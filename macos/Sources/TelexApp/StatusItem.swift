// The menu bar item and its two-item menu. The artwork lives in Icon.swift.
import AppKit

final class StatusItem: NSObject, NSMenuDelegate {
    private var item: NSStatusItem?
    private let iconOn = Icon.make(enabled: true)
    private let iconOff = Icon.make(enabled: false)

    var isEnabled: () -> Bool = { true }
    var onToggle: () -> Void = {}
    var onOpenExclusions: () -> Void = {}
    var onQuit: () -> Void = {}

    func create() {
        let item = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        if let button = item.button {
            button.target = self
            button.action = #selector(clicked(_:))
            button.sendAction(on: [.leftMouseUp, .rightMouseUp])
        }
        self.item = item
        update()
    }

    func destroy() {
        if let item { NSStatusBar.system.removeStatusItem(item) }
        item = nil
    }

    /// Safe to call from anywhere: AppKit work is marshalled to the main queue.
    func update() {
        if Thread.isMainThread {
            applyState()
        } else {
            DispatchQueue.main.async { [weak self] in self?.applyState() }
        }
    }

    private func applyState() {
        guard let button = item?.button else { return }
        let on = isEnabled()
        button.image = on ? iconOn : iconOff
        button.toolTip = on
            ? "telex: ON  (Control+Space to turn off)"
            : "telex: OFF  (Control+Space to turn on)"
    }

    @objc private func clicked(_ sender: NSStatusBarButton) {
        let rightClick = NSApp.currentEvent?.type == .rightMouseUp
        if rightClick {
            showMenu()
        } else {
            onToggle()
        }
    }

    private func showMenu() {
        let menu = NSMenu()
        menu.addItem(withTitle: "Open exclusion list",
                     action: #selector(openExclusions), keyEquivalent: "").target = self
        menu.addItem(.separator())
        menu.addItem(withTitle: "Quit telex",
                     action: #selector(quit), keyEquivalent: "q").target = self
        // Attaching the menu makes the next click open it; clearing it again
        // afterwards keeps left click on the button as a plain toggle.
        item?.menu = menu
        item?.button?.performClick(nil)
        item?.menu = nil
    }

    @objc private func openExclusions() { onOpenExclusions() }
    @objc private func quit() { onQuit() }
}
