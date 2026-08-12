// Entry point: single instance, the Accessibility gate, the menu bar item and
// the lifetime of the event tap. See macos/docs/DESIGN.md.
import AppKit
import ApplicationServices

final class AppDelegate: NSObject, NSApplicationDelegate {
    private let exclusion = Exclusion()
    private lazy var tap = EventTap(exclusion: exclusion)
    private let status = StatusItem()
    private var permissionTimer: Timer?

    func applicationDidFinishLaunching(_ notification: Notification) {
        // No Dock icon and no menu bar of our own: without this the permission
        // alert steals focus from whatever the user is typing into.
        NSApp.setActivationPolicy(.accessory)

        guard !anotherInstanceIsRunning() else {
            // A second instance would tap the keyboard twice and double every key.
            let alert = NSAlert()
            alert.messageText = "telex is already running."
            alert.informativeText = "Look for the V in the menu bar."
            alert.runModal()
            NSApp.terminate(nil)
            return
        }

        status.isEnabled = { [weak self] in self?.tap.isEnabled ?? false }
        status.onToggle = { [weak self] in self?.tap.toggleEnabled() }
        status.onOpenExclusions = { [weak self] in self?.exclusion.openFile() }
        status.onQuit = { NSApp.terminate(nil) }
        status.create()

        tap.onStateChanged = { [weak self] in self?.status.update() }
        exclusion.start()

        // The frontmost application changed: the half-typed word is gone, and a
        // different application may or may not be excluded.
        NSWorkspace.shared.notificationCenter.addObserver(
            forName: NSWorkspace.didActivateApplicationNotification,
            object: nil, queue: .main
        ) { [weak self] _ in
            self?.tap.resetEngine()
        }

        startTapWhenPermitted()
    }

    func applicationWillTerminate(_ notification: Notification) {
        permissionTimer?.invalidate()
        tap.remove()
        exclusion.stop()
        status.destroy()
    }

    // MARK: - Accessibility

    private func startTapWhenPermitted() {
        // This call both reports the state and raises the system prompt.
        let options = [kAXTrustedCheckOptionPrompt.takeUnretainedValue(): true] as CFDictionary
        if AXIsProcessTrustedWithOptions(options), tap.install() {
            return
        }

        explainPermission()
        // The user grants it while we are running and expects us to start
        // working, so poll rather than exit.
        permissionTimer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { [weak self] timer in
            guard let self else { return }
            guard AXIsProcessTrusted(), self.tap.install() else { return }
            timer.invalidate()
            self.permissionTimer = nil
            self.status.update()
        }
    }

    private func explainPermission() {
        let alert = NSAlert()
        alert.messageText = "telex needs Accessibility permission"
        alert.informativeText = """
            telex reads and rewrites keystrokes, which macOS only allows with \
            Accessibility permission.

            Open System Settings → Privacy & Security → Accessibility, then \
            switch telex on. It starts working the moment you do — no restart.
            """
        alert.addButton(withTitle: "Open System Settings")
        alert.addButton(withTitle: "Later")
        NSApp.activate(ignoringOtherApps: true)
        if alert.runModal() == .alertFirstButtonReturn {
            let url = URL(string:
                "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility")!
            NSWorkspace.shared.open(url)
        }
    }

    private func anotherInstanceIsRunning() -> Bool {
        guard let id = Bundle.main.bundleIdentifier else { return false }
        let mine = ProcessInfo.processInfo.processIdentifier
        return NSRunningApplication
            .runningApplications(withBundleIdentifier: id)
            .contains { $0.processIdentifier != mine }
    }
}

let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.run()
