// The exclusion list: bundle identifiers, one per line, in
// ~/Library/Application Support/telex/exclude.txt. Reloaded when the file
// changes; the result is a single flag the event tap can read for free.
import AppKit
import Foundation
import Synchronization

final class Exclusion: @unchecked Sendable {
    /// The one value the tap callback reads. Never a lock on that path.
    private let excluded = Atomic<Bool>(false)

    // Owned by the worker thread.
    private var names: [String] = []
    private var lastModified: Date?
    private var loaded = false

    // Written on the main thread by the workspace observer, read by the worker.
    private let frontLock = NSLock()
    private var frontBundleID: String?

    private let wake = DispatchSemaphore(value: 0)
    private let quit = Atomic<Bool>(false)
    private var worker: Thread?

    static let path: String = {
        let dir = FileManager.default
            .urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("telex", isDirectory: true)
        return dir.appendingPathComponent("exclude.txt").path
    }()

    var isExcluded: Bool { excluded.load(ordering: .relaxed) }

    // MARK: - Lifetime

    func start() {
        updateFrontmost(NSWorkspace.shared.frontmostApplication?.bundleIdentifier)
        NSWorkspace.shared.notificationCenter.addObserver(
            forName: NSWorkspace.didActivateApplicationNotification,
            object: nil, queue: .main
        ) { [weak self] note in
            let app = note.userInfo?[NSWorkspace.applicationUserInfoKey] as? NSRunningApplication
            self?.updateFrontmost(app?.bundleIdentifier)
        }

        let thread = Thread { [weak self] in self?.workerMain() }
        thread.name = "telex.exclusion"
        thread.stackSize = 128 * 1024
        worker = thread
        thread.start()
    }

    func stop() {
        quit.store(true, ordering: .relaxed)
        wake.signal()
    }

    /// Asks the worker to look again now instead of waiting for its next poll.
    func refresh() { wake.signal() }

    private func updateFrontmost(_ bundleID: String?) {
        frontLock.lock()
        frontBundleID = bundleID
        frontLock.unlock()
        refresh()
    }

    // MARK: - Worker

    // Reading the file is slow enough to matter - it may sit in a cloud-synced
    // folder - and it must never run on the thread that owns the event tap.
    private func workerMain() {
        while !quit.load(ordering: .relaxed) {
            reloadIfChanged()

            var isOut = false
            if !names.isEmpty {
                frontLock.lock()
                let front = frontBundleID?.lowercased()
                frontLock.unlock()
                if let front, !front.isEmpty {
                    isOut = names.contains(front)
                }
            }
            excluded.store(isOut, ordering: .relaxed)

            _ = wake.wait(timeout: .now() + 1.0)
        }
    }

    private func reloadIfChanged() {
        let attrs = try? FileManager.default.attributesOfItem(atPath: Self.path)
        guard let attrs, let modified = attrs[.modificationDate] as? Date else {
            if !loaded || !names.isEmpty {
                names = []          // no file means no exclusions
                loaded = true
                lastModified = nil
            }
            return
        }
        if loaded, lastModified == modified { return }
        lastModified = modified
        loaded = true

        guard let text = try? String(contentsOfFile: Self.path, encoding: .utf8) else { return }
        names = Exclusion.parse(text)
    }

    /// One bundle identifier per line. Blank lines and `#` comments are ignored.
    /// Bundle identifiers never contain whitespace, so anything after the first
    /// whitespace on a line is a description and is dropped.
    static func parse(_ text: String) -> [String] {
        var out: [String] = []
        for rawLine in text.split(separator: "\n", omittingEmptySubsequences: false) {
            let line = rawLine.trimmingCharacters(in: .whitespacesAndNewlines)
            if line.isEmpty || line.hasPrefix("#") { continue }
            let name = line.split(whereSeparator: { $0 == " " || $0 == "\t" }).first
            if let name, !name.isEmpty { out.append(name.lowercased()) }
        }
        return out
    }

    // MARK: - The menu item

    func openFile() {
        let url = URL(fileURLWithPath: Self.path)
        if !FileManager.default.fileExists(atPath: Self.path) {
            try? FileManager.default.createDirectory(
                at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
            try? Self.template.write(to: url, atomically: true, encoding: .utf8)
        }
        NSWorkspace.shared.open(url)
    }

    // Pre-filled with the usual suspects so nobody has to go hunting for bundle
    // identifiers: delete the # in front of a line to switch telex off inside
    // that application.
    private static let template = """
        # telex - exclusion list
        #
        # While one of these applications is the active one, telex stays out of
        # the way and your keys come out exactly as typed.
        #
        # Remove the # in front of a line to turn it on. Saving the file takes
        # effect at once - no restart. One bundle identifier per line; anything
        # after the identifier on the same line is ignored.

        # --- editors and IDEs ---------------------------------------
        # com.microsoft.VSCode              Visual Studio Code
        # com.todesktop.230313mzl4w4u92     Cursor
        # com.exafunction.windsurf          Windsurf
        # com.apple.dt.Xcode                Xcode
        # com.jetbrains.intellij            IntelliJ IDEA
        # com.jetbrains.pycharm             PyCharm
        # com.jetbrains.WebStorm            WebStorm
        # com.sublimetext.4                 Sublime Text
        # com.neovim.neovide                Neovide

        # --- terminals -----------------------------------------------
        # com.apple.Terminal                Terminal
        # com.googlecode.iterm2             iTerm2
        # dev.warp.Warp-Stable              Warp
        # net.kovidgoyal.kitty              kitty
        # com.github.wez.wezterm            WezTerm
        # dev.zed.Zed                       Zed

        # --- browsers ------------------------------------------------
        # com.apple.Safari                  Safari
        # com.google.Chrome                 Google Chrome
        # com.microsoft.edgemac             Microsoft Edge
        # org.mozilla.firefox               Firefox
        # com.brave.Browser                 Brave
        # company.thebrowser.Browser        Arc

        # --- chat and mail -------------------------------------------
        # com.tinyspeck.slackmacgap         Slack
        # com.hnc.Discord                   Discord
        # ru.keepcoder.Telegram             Telegram
        # com.apple.MobileSMS               Messages
        # com.apple.mail                    Mail
        # com.microsoft.teams2              Microsoft Teams
        # us.zoom.xos                       Zoom
        # com.vng.zalo                      Zalo

        # --- office and notes ----------------------------------------
        # com.apple.Notes                   Notes
        # com.apple.TextEdit                TextEdit
        # com.microsoft.Word                Word
        # com.microsoft.Excel               Excel
        # com.microsoft.Powerpoint          PowerPoint
        # notion.id                         Notion
        # md.obsidian                       Obsidian

        # --- design and 3D -------------------------------------------
        # com.adobe.Photoshop               Adobe Photoshop
        # com.adobe.illustrator             Adobe Illustrator
        # com.figma.Desktop                 Figma
        # org.blenderfoundation.blender     Blender

        # --- other ---------------------------------------------------
        # com.apple.finder                  Finder
        # com.spotify.client                Spotify
        # org.videolan.vlc                  VLC
        # com.valvesoftware.steam           Steam

        # Not in the list? Run this in Terminal, with the app's name:
        #   osascript -e 'id of app "Visual Studio Code"'

        """
}
