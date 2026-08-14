// Tier 2 tests: launch the real telex.app, inject real keystrokes into a real
// NSTextField and read the text back. See macos/docs/TESTING.md.
import AppKit
import ApplicationServices

// MARK: - Reporting

var passCount = 0
var failCount = 0
var stage = "startup"

func report(_ name: String, want: String, got: String) {
    if want == got {
        passCount += 1
        print("  ok   \(name)")
    } else {
        failCount += 1
        print("  FAIL \(name)\n         want \"\(want)\"\n         got  \"\(got)\"")
    }
    fflush(stdout)
}

// Injecting global input can wedge if something else grabs the front-most
// position, and a test that hangs is worse than one that fails.
func startWatchdog() {
    DispatchQueue.global().asyncAfter(deadline: .now() + 120) {
        print("\nTIMEOUT while running: \(stage)")
        fflush(stdout)
        killTelex()
        exit(3)
    }
}

let telexBundleID = "com.telex.macos"

func runningTelex() -> NSRunningApplication? {
    NSRunningApplication.runningApplications(withBundleIdentifier: telexBundleID).first
}

func killTelex() {
    // Leave nothing behind: a surviving telex holds the Accessibility grant and
    // taps the next test run's keys too.
    for app in NSRunningApplication.runningApplications(withBundleIdentifier: telexBundleID) {
        app.forceTerminate()
    }
}

// MARK: - Key injection

// US ANSI virtual key codes. Injecting real key codes rather than bare Unicode
// events is the point: it exercises the same path a real keyboard takes.
let keyCodes: [Character: CGKeyCode] = [
    "a": 0, "s": 1, "d": 2, "f": 3, "h": 4, "g": 5, "z": 6, "x": 7, "c": 8,
    "v": 9, "b": 11, "q": 12, "w": 13, "e": 14, "r": 15, "y": 16, "t": 17,
    "1": 18, "2": 19, "3": 20, "4": 21, "6": 22, "5": 23, "9": 25, "7": 26,
    "8": 28, "0": 29, "o": 31, "u": 32, "i": 34, "p": 35, "l": 37, "j": 38,
    "k": 40, ",": 43, "n": 45, "m": 46, ".": 47, " ": 49,
]

let kVKDelete: CGKeyCode = 51
let kVKSpace: CGKeyCode = 49
let kVKEscape: CGKeyCode = 53

let injectSource = CGEventSource(stateID: .privateState)

func post(_ keyCode: CGKeyCode, flags: CGEventFlags = [], down: Bool) {
    guard let event = CGEvent(keyboardEventSource: injectSource,
                              virtualKey: keyCode, keyDown: down) else { return }
    event.flags = flags
    event.post(tap: .cgSessionEventTap)
}

func pump(_ seconds: Double) {
    let deadline = Date().addingTimeInterval(seconds)
    while Date() < deadline {
        let until = min(deadline, Date().addingTimeInterval(0.004))
        if let event = NSApp.nextEvent(matching: .any, until: until,
                                       inMode: .default, dequeue: true) {
            NSApp.sendEvent(event)
        }
    }
}

/// "\u{8}" means Backspace. Uppercase letters are typed with Shift.
func sendKeys(_ text: String, delay: Double = 0.010) {
    for ch in text {
        if ch == "\u{8}" {
            post(kVKDelete, down: true)
            post(kVKDelete, down: false)
            pump(delay)
            continue
        }
        let lower = Character(ch.lowercased())
        guard let code = keyCodes[lower] else { continue }
        let flags: CGEventFlags = ch.isUppercase ? .maskShift : []
        post(code, flags: flags, down: true)
        post(code, flags: flags, down: false)
        pump(delay)
    }
    pump(0.12)
}

func sendControlSpace() {
    post(kVKSpace, flags: .maskControl, down: true)
    post(kVKSpace, flags: .maskControl, down: false)
    pump(0.25)
}

// MARK: - The test window

var window: NSWindow!
var field: NSTextField!

func makeWindow() {
    window = NSWindow(contentRect: NSRect(x: 200, y: 400, width: 520, height: 90),
                      styleMask: [.titled], backing: .buffered, defer: false)
    window.title = "telex e2e"
    field = NSTextField(frame: NSRect(x: 12, y: 20, width: 496, height: 32))
    field.isEditable = true
    window.contentView?.addSubview(field)
    window.makeKeyAndOrderFront(nil)
    NSApp.activate(ignoringOtherApps: true)
    window.makeFirstResponder(field)
    pump(0.5)
}

func clearField() {
    // Escape is a boundary key: it makes telex forget the word it thinks is on
    // screen. Without it the engine keeps the previous scenario's state while
    // the field is empty, and every later edit is computed against text that is
    // no longer there. The Windows harness does exactly the same.
    post(kVKEscape, down: true)
    post(kVKEscape, down: false)
    pump(0.05)
    field.stringValue = ""
    window.makeFirstResponder(field)
    pump(0.15)
}

func text() -> String { field.stringValue }

// MARK: - Exclusion file

let excludePath = NSString(string: "~/Library/Application Support/telex/exclude.txt")
    .expandingTildeInPath

func writeExclude(_ contents: String) {
    let url = URL(fileURLWithPath: excludePath)
    try? FileManager.default.createDirectory(at: url.deletingLastPathComponent(),
                                             withIntermediateDirectories: true)
    try? contents.write(to: url, atomically: true, encoding: .utf8)
    pump(1.6)  // the worker polls once a second
}

// MARK: - Scenarios

func scenarioTyping() {
    clearField()
    sendKeys("tieengs vieejt")
    report("1. types Vietnamese", want: "tiếng việt", got: text())
}

func scenarioToggleOff() {
    sendControlSpace()
    clearField()
    sendKeys("tieengs vieejt")
    report("2. Control+Space turns it off", want: "tieengs vieejt", got: text())
}

func scenarioToggleOn() {
    sendControlSpace()
    clearField()
    sendKeys("tieengs vieejt")
    report("3. Control+Space turns it back on", want: "tiếng việt", got: text())
}

func scenarioExcluded() {
    writeExclude("com.telex.e2e\n")
    clearField()
    sendKeys("tieengs vieejt")
    report("4. excluded app types literally", want: "tieengs vieejt", got: text())
}

func scenarioNotExcludedAnyMore() {
    writeExclude("# nothing excluded\n")
    clearField()
    sendKeys("tieengs vieejt")
    report("5. removing the entry applies without a restart",
           want: "tiếng việt", got: text())
}

func scenarioBackspace() {
    clearField()
    sendKeys("hoas\u{8}\u{8}")
    report("6. backspace over a rewritten word", want: "h", got: text())

    clearField()
    sendKeys("vay roi\u{8}\u{8}\u{8}\u{8}a")
    report("6b. backspace back into an earlier word", want: "vây", got: text())

    clearField()
    sendKeys("tieengs\u{8}gs")
    report("6c. putting a mark back after a backspace", want: "tiếng", got: text())
}

func scenarioBurst() {
    clearField()
    var keys = ""
    var expected = ""
    for _ in 0..<25 {
        keys += "tieengs "
        expected += "tiếng "
    }
    sendKeys(keys, delay: 0.003)  // far faster than anyone types
    pump(1.0)
    report("7. 200 keys with no pause", want: expected, got: text())
}

func scenarioSoak() {
    clearField()
    let end = Date().addingTimeInterval(3)
    while Date() < end {
        sendKeys("dduwowngf ", delay: 0.001)
    }
    clearField()
    sendKeys("tieengs")
    report("8. tap still alive after sustained typing", want: "tiếng", got: text())
}

// MARK: - Scenarios that exercise state invalidation (selection, app switch)
//
// These three mirror windows/tests/e2e_test.cpp scenarios 9-11. They were
// written and reasoned through without access to a Mac to run them on, unlike
// the rest of this file — verify them (and adjust the click coordinates in
// scenarioAppSwitch if anything is covered by a real window on the test
// machine) the first time this suite runs on real hardware.

/// Cmd+A is a Command chord, so telex resets on it via EventTap's
/// control/option/command branch (the same branch Ctrl+A hits on Windows).
/// This bare test harness has no Edit menu, so the keystroke alone would not
/// actually select anything the way it would in a real app; selectAll(_:) on
/// the field editor reproduces the selection a real app's menu would have
/// produced.
func scenarioSelectAllRetype() {
    clearField()
    sendKeys("tieengs")  // "tiếng" on screen, word not yet ended
    post(0, flags: .maskCommand, down: true)  // keycode 0 is 'a'
    post(0, flags: .maskCommand, down: false)
    pump(0.05)
    field.currentEditor()?.selectAll(nil)
    pump(0.05)
    sendKeys("hoas")  // first key replaces the selection
    report("9. select-all then retype composes fresh", want: "hóa", got: text())
}

/// Home is a boundary key (EventTap.swift, boundaryKeys), so telex resets on
/// it regardless of whether Shift is held — same reasoning as scenario 9.
func scenarioSelectionBackspace() {
    clearField()
    sendKeys("tieengs")
    let kVKHome: CGKeyCode = 115
    post(kVKHome, down: true)
    post(kVKHome, down: false)
    pump(0.05)
    if let editor = field.currentEditor() {
        editor.selectedRange = NSRange(location: 0, length: (editor.string as NSString).length)
    }
    pump(0.05)
    post(kVKDelete, down: true)
    post(kVKDelete, down: false)
    pump(0.05)
    sendKeys("vieejt")
    report("10. backspacing over a selection, then typing fresh", want: "việt", got: text())
}

var window2: NSWindow!
var field2: NSTextField!

func makeWindow2() {
    window2 = NSWindow(contentRect: NSRect(x: 200, y: 200, width: 520, height: 90),
                       styleMask: [.titled], backing: .buffered, defer: false)
    window2.title = "telex e2e #2"
    field2 = NSTextField(frame: NSRect(x: 12, y: 20, width: 496, height: 32))
    field2.isEditable = true
    window2.contentView?.addSubview(field2)
}

/// A real left-click anywhere on screen, purely to drive telex's own mouseDown
/// reset path (EventTap.swift) — the tap is session-wide, so the click does
/// not need to land on either test window to have that effect.
func clickToTriggerReset() {
    let point = CGPoint(x: 40, y: 700)  // an out-of-the-way spot near the bottom
    let down = CGEvent(mouseEventSource: nil, mouseType: .leftMouseDown,
                       mouseCursorPosition: point, mouseButton: .left)
    let up = CGEvent(mouseEventSource: nil, mouseType: .leftMouseUp,
                     mouseCursorPosition: point, mouseButton: .left)
    down?.post(tap: .cgSessionEventTap)
    up?.post(tap: .cgSessionEventTap)
    pump(0.1)
}

/// Half-type a word, switch to a second window and type something unrelated,
/// then switch back — the half-typed word must still be sitting in window 1
/// untouched (nothing asked telex to delete it) but telex's *memory* of it
/// must be gone, otherwise the next key in window 1 could emit a backspace
/// count sized for a word that is no longer what the caret is after.
///
/// Windows detects this by a foreground-window change alone, even between two
/// windows of the *same* process. On macOS, NSWorkspace's
/// didActivateApplicationNotification (main.swift) only fires on a change of
/// *application* — switching between two windows of this same test process
/// does not trigger it. What actually covers this case here is the mouseDown
/// reset path (EventTap.swift), same as a real user clicking to switch
/// windows would hit; clickToTriggerReset() stands in for that click.
func scenarioAppSwitch() {
    clearField()
    sendKeys("tieengs")  // "tiếng" on screen in window 1, word not ended

    window2.makeKeyAndOrderFront(nil)
    clickToTriggerReset()
    window2.makeFirstResponder(field2)
    pump(0.15)
    field2.stringValue = ""
    sendKeys("vieejt")
    report("11a. typing in the second window is unaffected",
           want: "việt", got: field2.stringValue)

    window.makeKeyAndOrderFront(nil)
    clickToTriggerReset()
    window.makeFirstResponder(field)
    pump(0.15)
    // If telex still thought "tiếng" was the live word, 's' here would emit a
    // backspace count sized for it. It must instead treat 'a' then 's' as a
    // brand new word appended after whatever is already on screen.
    sendKeys("as")
    report("11b. returning to the first window does not corrupt it",
           want: "tiếngá", got: text())
}

func scenarioShutdown() {
    guard let app = runningTelex() else {
        failCount += 1
        print("  FAIL 12. telex was not running at shutdown")
        return
    }
    app.terminate()
    let deadline = Date().addingTimeInterval(5)
    while Date() < deadline, !app.isTerminated { pump(0.1) }
    if app.isTerminated {
        passCount += 1
        print("  ok   12. shuts down cleanly")
    } else {
        failCount += 1
        print("  FAIL 12. did not shut down")
        app.forceTerminate()
    }
}

// MARK: - Entry point

let arguments = CommandLine.arguments

if arguments.contains("--check-permission") {
    exit(AXIsProcessTrusted() ? 0 : 1)
}

guard arguments.count >= 2 else {
    print("usage: TelexE2E <path to telex.app>")
    exit(2)
}
let appPath = arguments[1]

let nsApp = NSApplication.shared
nsApp.setActivationPolicy(.regular)

startWatchdog()

guard AXIsProcessTrusted() else {
    print("Accessibility permission missing for the test runner itself.")
    print("See macos/docs/TESTING.md.")
    exit(2)
}

stage = "starting telex.app"
killTelex()
pump(0.4)
writeExclude("# clean slate\n")

let config = NSWorkspace.OpenConfiguration()
config.activates = false
let launched = DispatchSemaphore(value: 0)
var launchError: Error?
NSWorkspace.shared.openApplication(at: URL(fileURLWithPath: appPath),
                                   configuration: config) { _, error in
    launchError = error
    launched.signal()
}
while launched.wait(timeout: .now()) == .timedOut { pump(0.05) }
if let launchError {
    print("could not launch \(appPath): \(launchError.localizedDescription)")
    exit(2)
}
pump(2.0)

guard runningTelex() != nil else {
    print("telex.app did not start. If this is its first run, grant it")
    print("Accessibility permission and try again.")
    exit(2)
}

stage = "creating the test window"
makeWindow()
makeWindow2()

let steps: [(String, () -> Void)] = [
    ("typing", scenarioTyping),
    ("toggle off", scenarioToggleOff),
    ("toggle on", scenarioToggleOn),
    ("excluded", scenarioExcluded),
    ("no longer excluded", scenarioNotExcludedAnyMore),
    ("backspace", scenarioBackspace),
    ("burst", scenarioBurst),
    ("soak", scenarioSoak),
    ("select-all retype", scenarioSelectAllRetype),
    ("selection backspace", scenarioSelectionBackspace),
    ("app switch", scenarioAppSwitch),
    ("shutdown", scenarioShutdown),
]

for step in steps {
    stage = step.0
    step.1()
}

killTelex()
try? FileManager.default.removeItem(atPath: excludePath)

print("\n\(passCount) passed, \(failCount) failed")
exit(failCount == 0 ? 0 : 1)
