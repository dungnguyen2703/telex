// swift-tools-version: 6.0
// The macOS build of telex. See docs/DESIGN.md.
import PackageDescription

let package = Package(
    name: "telex",
    // macOS 15 for Synchronization.Atomic: the exclusion flag is written by a
    // worker and read on the input path, and that read may not take a lock.
    platforms: [.macOS(.v15)],
    targets: [
        // Pure Swift. No AppKit, no I/O. This is the half that is tested
        // offline and that must behave exactly like the Windows engine.
        .target(name: "TelexEngine"),

        // The macOS layer. Language mode v5 because the event tap callback is a
        // C function pointer over shared state (see docs/DESIGN.md, pitfall 3).
        .executableTarget(
            name: "TelexApp",
            dependencies: ["TelexEngine"],
            swiftSettings: [.swiftLanguageMode(.v5)]
        ),

        .testTarget(name: "TelexEngineTests", dependencies: ["TelexEngine"]),

        // Tier 2. An executable rather than a test target: it launches the real
        // app and injects real keystrokes, and it needs its own bundle identity
        // so the exclusion scenarios have something to exclude.
        .executableTarget(
            name: "TelexE2E",
            path: "Tests/E2E",
            swiftSettings: [.swiftLanguageMode(.v5)]
        ),
    ]
)
