#!/bin/bash
set -euo pipefail
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
QINGVIEW_BUILD_ROOT="${QINGVIEW_BUILD_ROOT:-$HOME/.cache/qingview-build}"
QINGVIEW_QT_ROOT="${QINGVIEW_QT_ROOT:-$QINGVIEW_BUILD_ROOT/qt/6.8.3/macos}"
if [ ! -x "$QINGVIEW_QT_ROOT/bin/qmake" ]; then
    echo "Qt SDK missing: $QINGVIEW_QT_ROOT/bin/qmake" >&2
    exit 1
fi
mkdir -p "$QINGVIEW_BUILD_ROOT/build" "$PROJECT_ROOT/dist/release"
# Compile on APFS: avoid exFAT AppleDouble files and path escaping in generated Makefiles.
python3 - "$PROJECT_ROOT" "$QINGVIEW_BUILD_ROOT/source" <<'PY'
import pathlib, shutil, sys
source, target = map(pathlib.Path, sys.argv[1:])
if target.exists(): shutil.rmtree(target)
shutil.copytree(source, target, ignore=shutil.ignore_patterns('.git', '._*', '.DS_Store', 'release', 'fixtures', 'test-results'))
PY
swift "$QINGVIEW_BUILD_ROOT/source/scripts/make_icon.swift" "$QINGVIEW_BUILD_ROOT/QingView.iconset"
iconutil -c icns "$QINGVIEW_BUILD_ROOT/QingView.iconset" -o "$QINGVIEW_BUILD_ROOT/source/dist/mac/QingView.icns"
cd "$QINGVIEW_BUILD_ROOT/build"
"$QINGVIEW_QT_ROOT/bin/qmake" "$QINGVIEW_BUILD_ROOT/source/qView.pro" CONFIG+=release QMAKE_APPLE_DEVICE_ARCHS=arm64 QMAKE_MACOSX_DEPLOYMENT_TARGET=12.0
# qmake's generated plist target has no source prerequisites.
rm -f "$QINGVIEW_BUILD_ROOT/build/bin/QingView.app/Contents/Info.plist"
make -j4
APP="$QINGVIEW_BUILD_ROOT/build/bin/QingView.app"
rm -rf "$APP/Contents/Frameworks" "$APP/Contents/PlugIns"
"$QINGVIEW_QT_ROOT/bin/macdeployqt" "$APP" -always-overwrite
# Ship only the architecture used by this first edition.
python3 - "$APP" <<'PY'
import pathlib, subprocess, sys
root = pathlib.Path(sys.argv[1])
for p in root.rglob('*'):
    if not p.is_file() or p.is_symlink(): continue
    kind = subprocess.run(['/usr/bin/file','-b',str(p)],capture_output=True,text=True).stdout
    if 'Mach-O' in kind:
        arch = subprocess.check_output(['/usr/bin/lipo','-archs',str(p)],text=True).split()
        if 'arm64' not in arch: raise SystemExit('Non-arm64 dependency: '+str(p))
        if len(arch)>1:
            thin=p.with_name(p.name+'.thin')
            subprocess.run(['/usr/bin/lipo',str(p),'-thin','arm64','-output',str(thin)],check=True)
            thin.replace(p)
PY
mkdir -p "$APP/Contents/Resources/licenses"
cp "$PROJECT_ROOT/LICENSE" "$APP/Contents/Resources/licenses/qView-GPL-3.0.txt"
cp "$PROJECT_ROOT/resources/Lato-OFL.txt" "$APP/Contents/Resources/licenses/Lato-OFL.txt"
cp "$PROJECT_ROOT/resources/licenses/"*.txt "$APP/Contents/Resources/licenses/"
cp "$PROJECT_ROOT/THIRD_PARTY_NOTICES.md" "$APP/Contents/Resources/licenses/THIRD_PARTY_NOTICES.md"
cp "$PROJECT_ROOT/使用说明.txt" "$APP/Contents/Resources/使用说明.txt"
find "$APP" -type f -name '._*' -delete
xattr -cr "$APP"
codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict "$APP"
DELIVERY="$PROJECT_ROOT/dist/release/轻看.app"
rm -rf "$DELIVERY"
COPYFILE_DISABLE=1 ditto --norsrc --noextattr "$APP" "$DELIVERY"
find "$DELIVERY" -type f -name '._*' -delete
codesign --verify --deep --strict "$DELIVERY"
ARCHIVE_APP="$QINGVIEW_BUILD_ROOT/package/轻看.app"
mkdir -p "$(dirname "$ARCHIVE_APP")"
rm -rf "$ARCHIVE_APP"
COPYFILE_DISABLE=1 ditto --norsrc --noextattr "$APP" "$ARCHIVE_APP"
COPYFILE_DISABLE=1 ditto -c -k --keepParent --norsrc --noextattr "$ARCHIVE_APP" "$PROJECT_ROOT/dist/release/轻看-0.1.2-arm64.zip"
python3 "$PROJECT_ROOT/scripts/fix_zip_utf8.py" "$PROJECT_ROOT/dist/release/轻看-0.1.2-arm64.zip"
echo "Built: $DELIVERY"
