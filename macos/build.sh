#!/bin/bash
#  ./build.sh          - build build/telex.app
#  ./build.sh engine   - run the tier 1 engine tests only
#  ./build.sh test     - run tier 1 then the tier 2 end-to-end tests
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

OUT="$ROOT/build"
APP="$OUT/telex.app"
E2E_APP="$OUT/TelexE2E.app"

bin_path() {
    swift build -c release --show-bin-path
}

# The repository carries no image files. The icon Finder shows is rendered at
# build time by the same code that draws the menu bar icon, so the two cannot
# drift apart. See Tools/makeicon.swift.
build_icons() {
    mkdir -p "$OUT"
    swiftc -O "$ROOT/Tools/makeicon.swift" "$ROOT/Sources/TelexApp/Icon.swift" \
        -o "$OUT/makeicon"
    "$OUT/makeicon" "$OUT" >/dev/null
    iconutil -c icns "$OUT/AppIcon.iconset" -o "$OUT/AppIcon.icns"
    # The Windows build has no Swift toolchain, so its .ico is generated here
    # and committed. Copying it every build keeps the two in step.
    cp "$OUT/telex.ico" "$ROOT/../windows/telex.ico"
}

# Assembles a .app around a SwiftPM executable. Accessibility permission is
# keyed on bundle id plus signature, so a loose binary asks again every launch.
bundle() {
    local app="$1" plist="$2" product="$3" exe="$4" icon="${5:-}"
    rm -rf "$app"
    mkdir -p "$app/Contents/MacOS"
    cp "$ROOT/Resources/$plist" "$app/Contents/Info.plist"
    cp "$(bin_path)/$product" "$app/Contents/MacOS/$exe"
    if [ -n "$icon" ]; then
        mkdir -p "$app/Contents/Resources"
        cp "$icon" "$app/Contents/Resources/AppIcon.icns"
    fi
    # Signing comes last: it covers the resources too, so anything copied in
    # afterwards would invalidate the signature.
    codesign --force --sign - --timestamp=none "$app" >/dev/null 2>&1
}

build_app() {
    echo "[build] telex.app"
    # A running instance would tap every key twice.
    pkill -x telex 2>/dev/null || true
    swift build -c release --product TelexApp
    build_icons
    bundle "$APP" Info.plist TelexApp telex "$OUT/AppIcon.icns"
    echo "Built $APP"
}

build_e2e() {
    echo "[build] TelexE2E.app"
    swift build -c release --product TelexE2E
    bundle "$E2E_APP" E2E-Info.plist TelexE2E TelexE2E
}

run_engine() {
    echo "[run] tier 1 engine tests"
    # The suite prints its own "N passed, M failed" line, the same way the
    # Windows build does, so the two totals can be compared directly.
    set +e
    swift test 2>&1 | grep -E "^(corpus:|FAIL|     |[0-9]+ passed)|error:"
    local status=${PIPESTATUS[0]}
    set -e
    return $status
}

run_e2e() {
    local runner="$E2E_APP/Contents/MacOS/TelexE2E"
    if ! "$runner" --check-permission; then
        cat <<EOF

SKIPPED: tier 2 needs Accessibility permission for TWO applications:

  1. $APP
  2. $E2E_APP

Open System Settings > Privacy & Security > Accessibility, press +, and add
both of the paths above. Then run ./build.sh test again.

Rebuilding changes the signature, so if they are already listed but tier 2
still skips, remove them with - and add them again.
See macos/docs/TESTING.md.
EOF
        return 1
    fi
    echo
    echo "[run] tier 2 end-to-end tests"
    "$runner" "$APP"
}

case "${1:-app}" in
    app)
        build_app
        ;;
    engine)
        run_engine
        ;;
    test)
        run_engine
        echo
        build_app
        build_e2e
        run_e2e
        ;;
    *)
        echo "usage: $0 [app|engine|test]" >&2
        exit 2
        ;;
esac
