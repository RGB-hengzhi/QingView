#!/bin/bash
set -euo pipefail

test_dir=$(cd "$(dirname "$0")" && pwd)
project_dir=$(cd "$test_dir/.." && pwd)
qt_root=${QT_ROOT:-${HOME}/.cache/qingview-build/qt/6.8.3/macos}
stage_dir=${QINGVIEW_TEST_STAGE:-${HOME}/.cache/qingview-build/integration-source}
build_dir=${QINGVIEW_TEST_BUILD:-${HOME}/.cache/qingview-build/integration-build}
result_dir=${QINGVIEW_RESULTS_DIR:-$test_dir/results}

if [[ ! -x "$qt_root/bin/qmake" ]]; then
    echo "Qt qmake was not found at $qt_root/bin/qmake. Set QT_ROOT to a Qt SDK directory." >&2
    exit 2
fi
mkdir -p "$stage_dir/tests" "$build_dir" "$result_dir"
# Stage on the internal disk so exFAT metadata sidecars and filesystem latency
# cannot influence compilation. The fixture corpus stays outside the build.
rsync -a --exclude '._*' "$project_dir/src" "$project_dir/resources" "$project_dir/dist" "$stage_dir/"
cp "$test_dir/integration.pro" "$test_dir/tst_integration.cpp" "$stage_dir/tests/"
(
    cd "$build_dir"
    "$qt_root/bin/qmake" "$stage_dir/tests/integration.pro" CONFIG+=release
    make -j "${QINGVIEW_TEST_JOBS:-4}"
)
export QINGVIEW_FIXTURES=${QINGVIEW_FIXTURES:-$test_dir/fixtures}
export QINGVIEW_RESULTS_DIR=$result_dir
export QT_PLUGIN_PATH="$qt_root/plugins"
"$build_dir/bin/qingview-integration" -o "$result_dir/integration.txt",txt -o "$result_dir/integration.xml",junitxml "$@"
