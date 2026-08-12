# DESIGN — The macOS build

The rules that apply to both builds are in [docs/DESIGN.md](../../docs/DESIGN.md)
and the input rules in [docs/TELEX.md](../../docs/TELEX.md). This file is only
the macOS half.

This build is a **clone of the behaviour, not of the code**. It shares nothing
with the Windows sources — no headers, no bridging, no C++. What keeps the two
honest is tier 1: the same required coverage and the same
[corpus.txt](../../docs/corpus.txt).

## Choices

| Item | Choice | Why |
| --- | --- | --- |
| Language | Swift 6, `swiftLanguageMode(.v5)` on the app target | Native, no runtime to ship; v5 mode because the tap callback is a C function pointer over global state |
| Build | SwiftPM + `build.sh` | `Package.swift` is reviewable text; no `.xcodeproj` blob, nothing extra to install |
| Key interception | `CGEvent.tapCreate` on `.cgSessionEventTap` | The macOS equivalent of a low-level hook; needs Accessibility |
| Output | `CGEvent.keyboardSetUnicodeString` | Independent of keyboard layout, same as `KEYEVENTF_UNICODE` |
| Toggle | `Control + Space` | The chord macOS itself uses to switch input source |
| Indicator | `NSStatusItem` + `.accessory` activation policy | Menu bar, no Dock icon, no main window |
| Exclusion identity | Bundle identifier (`com.microsoft.VSCode`) | Stable, no spaces, unambiguous — the macOS analogue of `code.exe` |
| Exclusion file | `~/Library/Application Support/telex/exclude.txt` | Nothing may be written inside the `.app` |
| Packaging | `telex.app`, ad-hoc signed | Accessibility permission is bound to the bundle, not to a loose binary |

We do not write an Input Method Kit component. That is the "correct" macOS
approach and the analogue of TSF on Windows: it means a separate input source the
user has to select, an installed bundle under `~/Library/Input Methods`, and a
much larger surface. Interception is what the Windows build does and what UniKey
does by default.

## Layout

Paths are relative to `macos/`.

```
Package.swift
Sources/
  TelexEngine/       <- pure Swift. No AppKit, no I/O. Testable offline.
    Tables.swift        vowel/tone <-> precomposed character, both cases
    Syllable.swift      onset/nucleus/coda split, validity, tone placement
    Engine.swift        the input state machine
  TelexApp/          <- the macOS layer, as thin as possible
    main.swift          NSApplication lifetime, permission gate, teardown
    EventTap.swift      CGEventTap, key filtering, key swallowing
    Sender.swift        emits backspaces + characters
    StatusItem.swift    NSStatusItem and its menu
    Icon.swift          draws the two icons at runtime, no binary assets
    Exclusion.swift     reads exclude.txt, resolves the frontmost bundle id
Tests/
  TelexEngineTests/  <- tier 1, the same coverage as the Windows build
Resources/
  Info.plist          LSUIElement, bundle id, version
build.sh
build/telex.app       <- build output, and the committed download
```

`TelexEngine` must not `import AppKit` or `import Foundation`-dependent I/O. The
test target links it alone.

## Accessibility permission

Without it, `CGEvent.tapCreate` returns `nil` and the app can do nothing at all.
This has no Windows equivalent and is the one sanctioned exception to the shared
*Out of scope* list.

- Check with `AXIsProcessTrustedWithOptions([kAXTrustedCheckOptionPrompt: true])`
  at startup. That call both reports the state and raises the system prompt.
- If untrusted: show one alert explaining what is needed, open
  `x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility`,
  and keep polling `AXIsProcessTrusted()` until it flips, then install the tap.
  Do not exit — the user grants the permission while the app is running and
  expects it to start working.
- The permission is keyed on **bundle identifier plus code signature**. This is
  why the build must produce a `.app` and sign it, even ad-hoc: an unsigned loose
  binary asks again on every rebuild, and stale entries pile up in the list.
- Rebuilding changes the signature. Expect to re-grant after a rebuild; the
  developer workflow is in [TESTING.md](TESTING.md).
- The App Sandbox must stay **off**. Sandboxed processes cannot create event taps.

## macOS pitfalls that must be handled (missing one breaks the app)

1. **Tap recursion.** Our own posted events come back through the tap. Stamp
   every event we post with
   `setIntegerValueField(.eventSourceUserData, value: kSignature)` (`0x54454C58`)
   and skip those at the top of the callback. Skip *only* by this signature —
   never by the event source state or a generic "synthetic" check, because the
   tier 2 tests inject keys too and those must be processed like real ones. Same
   rule, same reason as the Windows build.
2. **The tap gets disabled and macOS tells you.** If the callback is too slow, or
   the user does something drastic, the system delivers
   `.tapDisabledByTimeout` or `.tapDisabledByUserInput` and **stops sending
   anything else**. Both must be handled by calling
   `CGEvent.tapEnable(tap:enable:true)` and returning. This is the counterpart of
   the Windows hook timeout, except Windows unhooks silently and macOS gives you
   a chance to recover — miss it and the app looks alive while doing nothing.
3. **The callback is a C function pointer.** `@convention(c)`: it cannot capture
   anything. State goes through the `refcon` pointer
   (`Unmanaged.passUnretained(...).toOpaque()` in, `takeUnretainedValue()` out).
   Writing it as an ordinary Swift closure does not compile, and reaching for a
   global to dodge the problem fights Swift 6 concurrency checking instead.
4. **Keep the callback cheap.** Same rule as Windows: no file I/O, no locks, no
   process queries. Reading `exclude.txt` and resolving the frontmost app happen
   elsewhere and publish one atomic flag.
5. **Post the whole edit in order, from one source.** macOS has no batch
   equivalent of `SendInput`. Create a single `CGEventSource` and post every
   backspace and character through it, in order, without pausing. Events from one
   source keep their order; mixing sources, or recreating the source per event,
   is what produces scrambled text in Chromium apps.
6. **Set the Unicode string on both key down and key up.** Build the pair with
   `CGEvent(keyboardEventSource:virtualKey:keyDown:)` using virtual key `0`, then
   call `keyboardSetUnicodeString` on **each** event. Setting it only on the down
   event makes some applications drop or double characters.
7. **Read the character from the event, not from the key code.** Windows has to
   reconstruct the character from the virtual key plus Shift and Caps Lock;
   macOS already did the layout work, so use
   `keyboardGetUnicodeString`. But detect the **toggle chord by key code**
   (`kVK_Space` = 49) and the modifier flags, before any character lookup: a
   chord is a key, not a character.
8. **Control+Space must be swallowed whole.** Return `nil` from the callback for
   both the key down and the matching key up, so no application sees half a
   chord and no stray space is typed. This chord is also macOS's own
   *Select the previous input source* shortcut: swallowing it in a session tap
   is what stops the system acting on it as well, which is the behaviour we
   want — telex replaces that switch rather than fighting it. There is no macOS
   equivalent of the Windows Alt menu-bar trap, so no dummy key is needed.
9. **Secure Event Input.** While a password field is focused, or a terminal has
   secure keyboard entry on, the system stops delivering keys to taps. Nothing
   can be done about it; it is the counterpart of the Windows "elevated windows"
   limitation and belongs in the README, not in a workaround.
10. **One tap, not two.** Ask the same tap for `.leftMouseDown`, `.rightMouseDown`
    and `.otherMouseDown` alongside the key events, and reset the engine on them.
    A second tap costs another callback on the input path for nothing.
11. **Single instance.** Two taps mean every key is processed twice. Check
    `NSRunningApplication.runningApplications(withBundleIdentifier:)` at startup
    and exit if another copy is live.
12. **No Dock icon, no menu bar takeover.** `LSUIElement` in `Info.plist` and
    `NSApp.setActivationPolicy(.accessory)`. Without it the app steals focus when
    it shows its permission alert.
13. **Teardown.** Disable the tap, remove the run loop source, and drop the
    status item on `applicationWillTerminate`, so no dead icon is left behind.

## Exclusion list

Shared semantics are in [docs/DESIGN.md](../../docs/DESIGN.md). macOS specifics:

- The file lives at `~/Library/Application Support/telex/exclude.txt`, created
  with a fully commented template the first time the menu opens it. The `.app`
  bundle is read-only and may not be written into.
- One **bundle identifier** per line, compared case-insensitively:
  `com.microsoft.VSCode`, `com.apple.Terminal`, `com.tinyspeck.slackmacgap`.
  Bundle identifiers never contain spaces, so anything after the first
  whitespace on a line is a description and is ignored — the same rule the
  Windows build applies after `.exe`.
- The frontmost application comes from
  `NSWorkspace.shared.frontmostApplication?.bundleIdentifier`. A worker resolves
  it and publishes one atomic flag; the tap callback only reads the flag.
- The list is polled once a second, and
  `NSWorkspace.didActivateApplicationNotification` asks for an immediate
  re-check. Polling is what makes "save the file and it applies" true even when
  the frontmost application never changes.
- To find an application's bundle identifier:
  `osascript -e 'id of app "Visual Studio Code"'`. The template says so.

## Menu bar item

- Two icons drawn with `NSImage`/`NSBezierPath` at runtime, no binary assets, the
  same artwork as the Windows tray: a **V** on a rounded square, red when on and
  grey when off. Drawn as non-template images so the colour survives.
- The item's tooltip states the current mode and the chord.
- Left click toggles. Right click (or click on the menu) opens two items: *Open
  exclusion list* and *Quit*.
- Updating the status item must be marshalled to the main queue — the tap
  callback runs on the run loop that owns the tap and must not touch AppKit.

## Known limitations, macOS only

- Secure Event Input (pitfall 9): password fields and terminals with secure
  keyboard entry receive raw keys.
- `Control + Space` no longer switches the system input source while telex is
  running, because telex swallows it.
- The app is ad-hoc signed and not notarised, so Gatekeeper requires the
  right-click → Open dance on first launch.
- Accessibility must be re-granted after a rebuild, because the signature
  changes.
