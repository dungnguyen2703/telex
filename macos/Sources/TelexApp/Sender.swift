// Emits the edits the engine asked for. One event source for the whole batch,
// posted in order: applications that process input asynchronously reorder
// events that arrive from different sources (docs/DESIGN.md, pitfall 5).
import CoreGraphics

/// Stamped on every event we inject so the tap can ignore its own output.
/// Deliberately not a generic "synthetic" check: the e2e tests inject keys too
/// and those must be processed like real ones (macos/docs/DESIGN.md, pitfall 1).
let kSignature: Int64 = 0x5445_4C58  // 'TELX'

private let kVKDelete: CGKeyCode = 51

enum Sender {
    /// A private source so our injected keys never inherit a modifier the user
    /// happens to be holding, and never disturb the global modifier state.
    private static let source = CGEventSource(stateID: .privateState)

    private static func stamp(_ event: CGEvent?) -> CGEvent? {
        event?.setIntegerValueField(.eventSourceUserData, value: kSignature)
        return event
    }

    static func sendEdit(backspaces: Int, insert: String) {
        if backspaces <= 0 && insert.isEmpty { return }

        var events: [CGEvent] = []
        events.reserveCapacity(backspaces * 2 + insert.count * 2)

        for _ in 0..<max(0, backspaces) {
            if let down = stamp(CGEvent(keyboardEventSource: source, virtualKey: kVKDelete, keyDown: true)) {
                events.append(down)
            }
            if let up = stamp(CGEvent(keyboardEventSource: source, virtualKey: kVKDelete, keyDown: false)) {
                events.append(up)
            }
        }

        for ch in insert {
            var utf16 = Array(String(ch).utf16)
            // The Unicode string has to go on both halves of the pair: setting
            // it only on the key down makes some applications drop or double
            // the character (docs/DESIGN.md, pitfall 6).
            for isDown in [true, false] {
                guard let event = stamp(CGEvent(keyboardEventSource: source, virtualKey: 0, keyDown: isDown)) else {
                    continue
                }
                event.keyboardSetUnicodeString(stringLength: utf16.count, unicodeString: &utf16)
                events.append(event)
            }
        }

        for event in events {
            event.post(tap: .cgSessionEventTap)
        }
    }
}
