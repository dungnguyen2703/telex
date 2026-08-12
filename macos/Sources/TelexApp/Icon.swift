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

    /// The application icon — what Finder and the Dock show for telex.app.
    ///
    /// Same artwork as the menu bar, tuned for large sizes: Finder reads the
    /// icon out of the bundle rather than asking the app to draw it, so this one
    /// has to be baked into a file. `build.sh` renders it at build time (see
    /// Tools/makeicon.swift) so the repository still carries no image assets.
    static func appIcon(size: CGFloat) -> NSImage {
        NSImage(size: NSSize(width: size, height: size), flipped: false) { rect in
            // Proportions follow Apple's icon grid: the shape sits inside the
            // canvas with a margin, rather than filling it edge to edge.
            let plate = rect.insetBy(dx: size * 0.098, dy: size * 0.098)
            let radius = size * 0.185
            let path = NSBezierPath(roundedRect: plate, xRadius: radius, yRadius: radius)

            // A flat fill looks dead at 1024px; a slight vertical gradient does
            // not. Fixed colours, not system ones: an app icon must not change
            // with the appearance setting.
            let top = NSColor(srgbRed: 0.94, green: 0.29, blue: 0.26, alpha: 1)
            let bottom = NSColor(srgbRed: 0.80, green: 0.13, blue: 0.13, alpha: 1)
            NSGradient(starting: top, ending: bottom)?.draw(in: path, angle: -90)

            let attributes: [NSAttributedString.Key: Any] = [
                .font: NSFont.systemFont(ofSize: size * 0.50, weight: .bold),
                .foregroundColor: NSColor.white,
            ]
            let glyph = NSAttributedString(string: "V", attributes: attributes)
            let glyphSize = glyph.size()
            glyph.draw(at: NSPoint(x: rect.midX - glyphSize.width / 2,
                                   y: rect.midY - glyphSize.height / 2))
            return true
        }
    }
}
