// Runs the whole tier 1 suite as one XCTest case and reports the same
// "N passed, M failed" line the Windows build prints, so the two totals can be
// compared directly. See docs/TESTING.md.
import Foundation
import XCTest

@testable import TelexEngine

final class TierOneTests: XCTestCase {
    /// docs/corpus.txt, found relative to this source file so the test does not
    /// depend on the working directory.
    private var corpusPath: String {
        URL(fileURLWithPath: #filePath)      // .../macos/Tests/TelexEngineTests/TierOneTests.swift
            .deletingLastPathComponent()      // .../macos/Tests/TelexEngineTests
            .deletingLastPathComponent()      // .../macos/Tests
            .deletingLastPathComponent()      // .../macos
            .deletingLastPathComponent()      // .../telex
            .appendingPathComponent("docs/corpus.txt")
            .path
    }

    func testTierOne() {
        let suite = TierOne()
        suite.runAll()
        let syllables = suite.corpus(at: corpusPath)

        print("corpus: \(syllables) syllables")
        for f in suite.failures {
            print("FAIL \(f.keys)\n     want \"\(f.want)\"\n     got  \"\(f.got)\"")
        }
        print("\n\(suite.passed) passed, \(suite.failures.count) failed")

        XCTAssertEqual(suite.failures.count, 0, "\(suite.failures.count) tier 1 case(s) failed")

        // The Windows build is the reference. A different total means a group
        // was dropped in the port, not that the platform differs.
        XCTAssertEqual(suite.total, 662,
                       "tier 1 must run the same 662 assertions as the Windows build")
    }
}
