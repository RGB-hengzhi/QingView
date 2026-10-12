#!/bin/bash
set -euo pipefail

test_dir=$(cd "$(dirname "$0")" && pwd)
qt_root=${QT_ROOT:-${QINGVIEW_QT_ROOT:-${HOME}/.cache/qingview-build/qt/6.8.3/macos}}
fixtures_dir=${QINGVIEW_FIXTURES:-$test_dir/fixtures}
fixture_build_root=${QINGVIEW_FIXTURE_BUILD:-${HOME}/.cache/qingview-build/fixture-generator}

if [[ ! -x "$qt_root/bin/qmake" ]]; then
    echo "Qt qmake was not found at $qt_root/bin/qmake. Set QT_ROOT or QINGVIEW_QT_ROOT." >&2
    exit 2
fi
if ! command -v swift >/dev/null; then
    echo "Swift was not found. Install macOS Command Line Tools first." >&2
    exit 2
fi
# Keep compilation on the internal disk; all generated images stay in the
# requested corpus directory and never replace a user's photo collection.
mkdir -p "$fixture_build_root/source" "$fixture_build_root/build" "$fixtures_dir"
swift "$test_dir/generate_fixtures.swift" "$fixtures_dir"
cp "$test_dir/generate_webp.pro" "$test_dir/generate_webp_fixture.cpp" "$fixture_build_root/source/"
(
    cd "$fixture_build_root/build"
    "$qt_root/bin/qmake" "$fixture_build_root/source/generate_webp.pro" CONFIG+=release
    make -j "${QINGVIEW_TEST_JOBS:-4}"
)
QT_PLUGIN_PATH="$qt_root/plugins" "$fixture_build_root/build/bin/generate_webp_fixture" \
    "$fixtures_dir/formats/sample.png" "$fixtures_dir/formats/sample.webp"
echo "Generated fixtures: $fixtures_dir"
