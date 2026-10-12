#!/bin/bash
set -euo pipefail

test_dir=$(cd "$(dirname "$0")" && pwd)
project_dir=$(cd "$test_dir/.." && pwd)
source_dir=${QINGVIEW_RESOURCE_SOURCE:-$project_dir}
qt_root=${QT_ROOT:-${HOME}/.cache/qingview-build/qt/6.8.3/macos}
stage_dir=${QINGVIEW_RESOURCE_STAGE:-${HOME}/.cache/qingview-build/resource-source}
build_dir=${QINGVIEW_RESOURCE_BUILD:-${HOME}/.cache/qingview-build/resource-build}
result_dir=${QINGVIEW_RESULTS_DIR:-$test_dir/results}

if [[ ! -x "$qt_root/bin/qmake" ]]; then
    echo "Qt qmake was not found at $qt_root/bin/qmake. Set QT_ROOT." >&2
    exit 2
fi
if [[ "$source_dir" == "$stage_dir" ]]; then
    echo "Use different resource source and staging directories." >&2
    exit 2
fi
mkdir -p "$stage_dir/tests" "$build_dir" "$result_dir"
rsync -a --delete --exclude '._*' "$source_dir/src" "$source_dir/resources" "$source_dir/dist" "$stage_dir/"
cp "$test_dir/resource.pro" "$test_dir/resource_probe.cpp" "$stage_dir/tests/"
(
    cd "$build_dir"
    "$qt_root/bin/qmake" "$stage_dir/tests/resource.pro" CONFIG+=release
    make -j "${QINGVIEW_TEST_JOBS:-4}"
)
if [[ ${1:-} == --build-only ]]; then
    echo "$build_dir/bin/qingview-resource-probe"
    exit 0
fi

if [[ -f "$source_dir/BASELINE_REVISION.txt" ]]; then
    export QINGVIEW_PROBE_SOURCE_REVISION
    QINGVIEW_PROBE_SOURCE_REVISION=$(cat "$source_dir/BASELINE_REVISION.txt")
else
    export QINGVIEW_PROBE_SOURCE_REVISION
    QINGVIEW_PROBE_SOURCE_REVISION=$(git -C "$source_dir" rev-parse HEAD)
fi
export QINGVIEW_FIXTURES=${QINGVIEW_FIXTURES:-$test_dir/fixtures}
export QINGVIEW_PROBE_SOURCE_SHA256
QINGVIEW_PROBE_SOURCE_SHA256=$(python3 - "$stage_dir/src" <<'PY'
import hashlib, pathlib, sys
root = pathlib.Path(sys.argv[1])
digest = hashlib.sha256()
for path in sorted(root.rglob('*')):
    if path.is_file() and not path.name.startswith('._'):
        digest.update(str(path.relative_to(root)).encode('utf-8') + b'\0')
        digest.update(path.read_bytes())
print(digest.hexdigest())
PY
)
export QT_PLUGIN_PATH="$qt_root/plugins"
"$build_dir/bin/qingview-resource-probe" --output "$result_dir/resource-${QINGVIEW_RESOURCE_LABEL:-sample}.json" "$@"
