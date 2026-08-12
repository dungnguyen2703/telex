// Draws the menu bar icons at runtime so the build needs no binary assets.
// Same artwork as the Windows tray: a white V on a rounded square, red when on
// and grey when off.
import AppKit

enum Icon {
    static let size: CGFloat = 18

    static func make(enabled: Bool) -> NSImage {
        let box = NSSize(width: size, height: size)
        let image = NSImage(size: box, flipped: false) { rect in
            let background = enabled
                ? NSColor.systemRed
                : NSColor.secondaryLabelColor.withAlphaComponent(0.55)
            let plate = NSBezierPath(roundedRect: rect.insetBy(dx: 0.5, dy: 1.5),
                                     xRadius: 4, yRadius: 4)
            background.setFill()
            plate.fill()

            let attributes: [NSAttributedString.Key: Any] = [
                .font: NSFont.systemFont(ofSize: 11, weight: .bold),
                .foregroundColor: NSColor.white,
            ]
            let glyph = NSAttributedString(string: "V", attributes: attributes)
            let glyphSize = glyph.size()
            glyph.draw(at: NSPoint(x: rect.midX - glyphSize.width / 2,
                                   y: rect.midY - glyphSize.height / 2))
            return true
        }
        // Not a template image: the point of the icon is that the colour tells
        // you the state at a glance.
        image.isTemplate = false
        return image
    }
}
