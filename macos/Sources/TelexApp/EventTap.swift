// The global event tap. Everything here runs on the run loop that owns the tap
// and must stay fast: macOS disables a tap whose callback takes too long
// (macos/docs/DESIGN.md, pitfall 2).
import CoreGraphics
import TelexEngine

// Virtual key codes we care about. Values from <Carbon/HIToolbox/Events.h>.
private let kVKSpace: Int64 = 49
private let kVKDeleteKey: Int64 = 51

// Word boundaries that are not characters: anything that moves the caret or
// edits elsewhere invalidates what we think is on screen.
private let boundaryKeys: Set<Int64> = [
    36,   // Return
    48,   // Tab
    53,   // Escape
    76,   // Keypad Enter
    114,  // Help / Insert
    115,  // Home
    116,  // Page Up
    117,  // Forward Delete
    119,  // End
    121,  // Page Down
    123, 124, 125, 126,  // arrows
]

private func tapCallback(
    proxy: CGEventTapProxy,
    type: CGEventType,
    event: CGEvent,
    refcon: UnsafeMutableRawPointer?
) -> Unmanaged<CGEvent>? {
    guard let refcon else { return Unmanaged.passUnretained(event) }
    // The callback is @convention(c) and cannot capture, so the tap comes
    // through refcon (macos/docs/DESIGN.md, pitfall 3).
    let tap = Unmanaged<EventTap>.fromOpaque(refcon).takeUnretainedValue()
    return tap.handle(type: type, event: event)
}

final class EventTap {
    private let engine = Engine()
    private let exclusion: Exclusion

    private var machPort: CFMachPort?
    private var runLoopSource: CFRunLoopSource?

    private var swallowNextSpaceUp = false
    private var wasExcluded = false

    private(set) var isEnabled = true
    /// Called when the ON/OFF state changes. The status item listens; it must
    /// marshal its own work to the main queue rather than doing it here.
    var onStateChanged: (() -> Void)?

    init(exclusion: Exclusion) {
        self.exclusion = exclusion
    }

    // MARK: - Lifetime

    func install() -> Bool {
        let mask: CGEventMask =
            (1 << CGEventType.keyDown.rawValue) |
            (1 << CGEventType.keyUp.rawValue) |
            (1 << CGEventType.leftMouseDown.rawValue) |
            (1 << CGEventType.rightMouseDown.rawValue) |
            (1 << CGEventType.otherMouseDown.rawValue) |
            (1 << CGEventType.tapDisabledByTimeout.rawValue) |
            (1 << CGEventType.tapDisabledByUserInput.rawValue)

        guard let port = CGEvent.tapCreate(
            tap: .cgSessionEventTap,
            place: .headInsertEventTap,
            options: .defaultTap,
            eventsOfInterest: mask,
            callback: tapCallback,
            userInfo: Unmanaged.passUnretained(self).toOpaque()
        ) else {
            return false  // almost always: Accessibility not granted
        }

        machPort = port
        let source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, port, 0)
        runLoopSource = source
        CFRunLoopAddSource(CFRunLoopGetMain(), source, .commonModes)
        CGEvent.tapEnable(tap: port, enable: true)
        return true
    }

    func remove() {
        if let port = machPort {
            CGEvent.tapEnable(tap: port, enable: false)
            CFMachPortInvalidate(port)
        }
        if let source = runLoopSource {
            CFRunLoopRemoveSource(CFRunLoopGetMain(), source, .commonModes)
        }
        machPort = nil
        runLoopSource = nil
    }

    func resetEngine() { engine.reset() }

    func setEnabled(_ on: Bool) {
        if isEnabled == on { return }
        isEnabled = on
        engine.reset()
        onStateChanged?()
    }

    func toggleEnabled() { setEnabled(!isEnabled) }

    // MARK: - The input path

    fileprivate func handle(type: CGEventType, event: CGEvent) -> Unmanaged<CGEvent>? {
        let keep = Unmanaged.passUnretained(event)

        // macOS tells us when it drops the tap, unlike Windows. Take the offer.
        if type == .tapDisabledByTimeout || type == .tapDisabledByUserInput {
            if let port = machPort { CGEvent.tapEnable(tap: port, enable: true) }
            return keep
        }

        if type == .leftMouseDown || type == .rightMouseDown || type == .otherMouseDown {
            // The caret moved somewhere we cannot see; forget the current word.
            engine.reset()
            return keep
        }

        guard type == .keyDown || type == .keyUp else { return keep }

        if event.getIntegerValueField(.eventSourceUserData) == kSignature {
            return keep  // our own output
        }

        let down = (type == .keyDown)
        let keyCode = event.getIntegerValueField(.keyboardEventKeycode)
        let flags = event.flags
        let control = flags.contains(.maskControl)
        let option = flags.contains(.maskAlternate)
        let command = flags.contains(.maskCommand)

        // Control+Space toggles: the chord macOS itself uses for switching input
        // source, so it is the one Vietnamese typists already reach for. Both
        // halves are swallowed, so no application sees a stray space and the
        // system's own input-source shortcut never fires either. Swallowing it
        // in a session tap is what stops the system from acting on it.
        if keyCode == kVKSpace && control && !option && !command {
            if down {
                toggleEnabled()
                swallowNextSpaceUp = true
                return nil
            }
            if swallowNextSpaceUp {
                swallowNextSpaceUp = false
                return nil
            }
        }
        if keyCode == kVKSpace && !down && swallowNextSpaceUp {
            swallowNextSpaceUp = false  // Control was released first
            return nil
        }

        if !down { return keep }

        // Keys typed while we were standing aside never reached the engine, so
        // what it remembers about the text is no longer true.
        let excluded = exclusion.isExcluded
        if excluded != wasExcluded {
            wasExcluded = excluded
            engine.reset()
        }
        if !isEnabled || excluded { return keep }

        if control || option || command {
            engine.reset()
            return keep
        }

        if keyCode == kVKDeleteKey {
            _ = engine.onBackspace()
            return keep
        }

        if boundaryKeys.contains(keyCode) {
            // The caret is going somewhere we cannot follow; forget the text.
            engine.reset()
            return keep
        }

        // macOS has already applied the layout, Shift and Caps Lock for us, so
        // unlike the Windows build there is no character to reconstruct.
        guard let ch = character(of: event) else {
            engine.reset()
            return keep
        }

        if !isAsciiLetter(ch) {
            // Space, digits, punctuation: they end the word, but the caret
            // stays put, so backspacing back into what we typed still works.
            engine.endWord(ch)
            return keep
        }

        let r = engine.onKey(ch)
        if r.action == .passThrough { return keep }
        Sender.sendEdit(backspaces: r.backspaces, insert: r.insert)
        return nil
    }

    private func character(of event: CGEvent) -> Character? {
        var length = 0
        var buffer = [UniChar](repeating: 0, count: 4)
        event.keyboardGetUnicodeString(maxStringLength: 4,
                                       actualStringLength: &length,
                                       unicodeString: &buffer)
        guard length == 1 else { return nil }
        guard let scalar = Unicode.Scalar(buffer[0]) else { return nil }
        return Character(scalar)
    }
}
