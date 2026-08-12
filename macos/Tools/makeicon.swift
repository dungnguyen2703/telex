// Renders the application icon at build time.
//
// The repository carries no image files: this tool is compiled against
// Sources/TelexApp/Icon.swift, so the icon Finder shows and the icon in the
// menu bar are drawn by the same code and cannot drift apart.
//
//   swiftc Tools/makeicon.swift Sources/TelexApp/Icon.swift -o build/makeicon
//   build/makeicon <output directory>
//
// Writes AppIcon.iconset/ (for iconutil) and telex.ico (for the Windows build).
import AppKit
import Foundation

func png(_ image: NSImage, _ pixels: Int) -> Data {
    let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: pixels, pixelsHigh: pixels,
                              bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true,
                              isPlanar: false, colorSpaceName: .deviceRGB,
                              bytesPerRow: 0, bitsPerPixel: 0)!
    rep.size = NSSize(width: pixels, height: pixels)
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
    Icon.appIcon(size: CGFloat(pixels)).draw(in: NSRect(x: 0, y: 0, width: pixels, height: pixels))
    NSGraphicsContext.restoreGraphicsState()
    return rep.representation(using: .png, properties: [:])!
}

/// A .ico is a small header plus one entry per size. Windows has accepted
/// PNG-compressed entries since Vista, so the same bytes serve both platforms.
func ico(_ sizes: [Int]) -> Data {
    var directory = Data()
    var images = Data()
    var offset = 6 + 16 * sizes.count

    directory.append(contentsOf: [0, 0, 1, 0])                       // reserved, type = icon
    directory.append(contentsOf: [UInt8(sizes.count & 0xFF), UInt8(sizes.count >> 8)])

    for size in sizes {
        let data = png(Icon.appIcon(size: CGFloat(size)), size)
        // 0 means 256 in the directory entry.
        directory.append(UInt8(size >= 256 ? 0 : size))
        directory.append(UInt8(size >= 256 ? 0 : size))
        directory.append(contentsOf: [0, 0])                         // palette, reserved
        directory.append(contentsOf: [1, 0])                         // colour planes
        directory.append(contentsOf: [32, 0])                        // bits per pixel
        var length = UInt32(data.count).littleEndian
        var start = UInt32(offset).littleEndian
        withUnsafeBytes(of: &length) { directory.append(contentsOf: $0) }
        withUnsafeBytes(of: &start) { directory.append(contentsOf: $0) }
        images.append(data)
        offset += data.count
    }
    return directory + images
}

// Top-level code is only allowed in a file called main.swift, and this tool is
// compiled together with Icon.swift, so the entry point is explicit.
@main
enum MakeIcon {
    static func main() throws {
        let arguments = CommandLine.arguments
        guard arguments.count >= 2 else {
            FileHandle.standardError.write(Data("usage: makeicon <output directory>\n".utf8))
            exit(2)
        }
        let out = URL(fileURLWithPath: arguments[1])
        let iconset = out.appendingPathComponent("AppIcon.iconset")
        try? FileManager.default.createDirectory(at: iconset, withIntermediateDirectories: true)

        // The names are fixed by iconutil.
        let macSizes: [(name: String, pixels: Int)] = [
            ("icon_16x16", 16), ("icon_16x16@2x", 32),
            ("icon_32x32", 32), ("icon_32x32@2x", 64),
            ("icon_128x128", 128), ("icon_128x128@2x", 256),
            ("icon_256x256", 256), ("icon_256x256@2x", 512),
            ("icon_512x512", 512), ("icon_512x512@2x", 1024),
        ]
        for entry in macSizes {
            let data = png(Icon.appIcon(size: CGFloat(entry.pixels)), entry.pixels)
            try data.write(to: iconset.appendingPathComponent("\(entry.name).png"))
        }

        try ico([16, 32, 48, 64, 128, 256])
            .write(to: out.appendingPathComponent("telex.ico"))

        print("icons written to \(out.path)")
    }
}
