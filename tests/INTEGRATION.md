# QingView integration acceptance

> 公开版本说明：本机工程、Qt SDK 与用户缓存路径已分别标准化为 `${PROJECT_ROOT}`、`${QINGVIEW_QT_ROOT}`、`${QINGVIEW_BUILD_ROOT}`；系统库路径及测试结果、数量、时间、哈希保持原值。

The harness links the actual application sources and uses a real `QVApplication`,
`MainWindow`, `QVGraphicsView`, image decoders, Qt event loop, and native macOS
window platform. It does not use mock windows or access private implementation
members. Settings are isolated in a temporary INI directory; update checking is
excluded at compilation.

Fixtures and test results are generated locally; neither directory is included
in a fresh clone. Install the build dependencies described in the repository
README, set `QINGVIEW_QT_ROOT` to the Qt 6.8.3 macOS SDK directory, and run from
the repository root:

```sh
QT_ROOT="$QINGVIEW_QT_ROOT" bash tests/generate_fixtures.sh
QT_ROOT="$QINGVIEW_QT_ROOT" bash tests/run_integration.sh
```

The fixture entry point creates the Swift samples, builds the WebP generator
with that same Qt SDK and its image plugins, and verifies the WebP round trip.
`QT_ROOT` takes precedence over `QINGVIEW_QT_ROOT`; if neither is set, the scripts
use `$HOME/.cache/qingview-build/qt/6.8.3/macos`. A custom `QINGVIEW_FIXTURES` value
must be the same for both generation and the test run. All sample pictures are
synthetic; no user photos are required.

The script stages sources on the internal disk, builds a standalone test binary,
and writes text/XML results and window snapshots to `tests/results/`. It uses the
native Cocoa platform; fullscreen and keyboard tests need an unlocked graphical
desktop. An offscreen run does not prove native fullscreen behavior.
The harness sends mouse events only to its own widgets and does not move the
system pointer. Toolbar reveal, timer hiding, button/label hover retention and
leaving the controls are automated with normal events to the test's own widgets.
Native delivery and visual layout are checked separately through the supported
computer-use UI.

Covered behaviors: empty state; JPEG/PNG/WebP/HEIC/large JPEG; EXIF rotation;
transparency and Chinese filenames; natural `1, 2, 10` ordering; real keyboard
navigation; fit/zoom/100% and wheel zoom; visible rotated pixels; native fullscreen
and Escape; damaged-file recovery; GIF frame changes, pause and frame stepping;
the `QFileOpenEvent` path used for Finder opens; toolbar controls, status labels,
pointer reveal, auto-hide, and preserving the image viewport size.
It also checks that remote URLs and HTTP clipboard text leave the current image
unchanged while local file URLs still open, verifies SVG aspect ratio and rendered
quadrant pixels, and destroys six windows while their image decode/preload/menu
jobs are deliberately queued. Those workers run only after their owners are gone;
child preload watchers must be destroyed with their image core.
Wheel regressions send actual viewport wheel events for tiny, partial, standard,
multiple, negative, and zero deltas. They check 5% per standard detent, reciprocal
reversal, equal accumulated movement across differently partitioned events,
delayed HiDPI resampling, and retaining the toolbar's 25% zoom buttons.
Additional cases cover pixel-only movement, angle-delta priority, immediate 1%
and 10% sensitivity changes through `QSettings`, and persisting the new options
spin box by clicking the real dialog's Apply button.

`QFileOpenEvent` tests validate the OS-facing application handler. They do not
register the built application with Launch Services or change default file
associations. The harness reports no startup, memory, or performance benchmark;
measure those on the packaged release separately.

Override `QINGVIEW_FIXTURES`, `QINGVIEW_RESULTS_DIR`, `QINGVIEW_TEST_STAGE`, or
`QINGVIEW_TEST_BUILD` to relocate inputs/results/build. QtTest arguments can be
passed to the script; for example append `naturalSortAndNavigation` to run one
case. A fresh build can be obtained by removing the chosen test build directory.

## 2026-10-12 common-operation audit

The expanded audit creates every mutable file in a fresh `QTemporaryDir` and
continues to isolate preferences under `QingViewIntegration`. Fixtures are only
read or copied. Trash tests operate on UUID-named copies, exercise the real
confirmation/Undo paths, and restore only the exact test file if a failure leaves
it in Trash. They never enumerate or empty the user's Trash. Clipboard tests
preserve and restore the clipboard's advertised formats.

The additional cases check:

- Opening a directory, ignoring text files/subfolders/macOS metadata, hidden-file
  preferences, uppercase extensions, and empty/single-image directory recovery.
- Natural-sort reversal with correct toolbar position; folder loop boundaries;
  ascending/descending size, modified-time, and creation-time order.
- Upright EXIF JPEG/HEIC dimensions and ratio, plus correctly rounded megapixel
  counts in the real information dialog;
  rotating GIFs at 100% with resampling both enabled and disabled.
- Copying displayed pixels plus a local file URL and pasting into another window.
- Successful PNG/GIF rename, Unicode names, canceled rename, and a name collision
  that preserves both files and resumes GIF playback.
- Canceling Trash confirmation, deleting and restoring, failed restoration to an
  occupied path followed by retry, and an obsolete confirmation after a different
  image finishes loading. Only temporary test copies are used.
- Slideshow start/stop/resume, reverse direction, end-of-folder stopping, and an
  explicit file open stopping the slideshow.
- Real settings Cancel/OK behavior, persistence and sensitivity in a new window;
  independent Open With menus; missing-file/reload/external-removal recovery.
- Reopening an externally changed image whose byte size is unchanged and honoring
  the final open request while background decoding is deliberately queued.

Run with an audit-specific output directory:

```sh
QINGVIEW_RESULTS_DIR="$PWD/tests/results/audit-2026-10-12/final" bash tests/run_integration.sh
```

`baseline/` preserves the 43-case starting regression. `edge-repro/` preserves
initial failures for stale cache and dropped open requests. `common-repro/`
preserves the intermediate common-operation run, including stale position and
EXIF information failures. `final/` is the complete run against the final source
snapshot. Reproduction reports are historical evidence, not current pass claims.
Native mouse hover retention and Launch Services/Finder associations remain
separate UI/release checks. This harness does not claim exhaustive image-format
support, coverage of every preference, or performance measurements.

The completed final 0.1.2 native Cocoa run is **82 passed, 0 failed, 0 skipped**
in 51.345 seconds. The 60 staged source/resource/test files matched the workspace
byte-for-byte at completion; hashes are saved in `final/source-snapshot.json`.
The earlier 72-case green run is preserved under
`accepted-72-before-final-additions/`. The first complete run's only failure was a
historical harness assumption that six queued 24MP neighbors must all be eligible
for preload. `pre-budget-harness-adjustment/`
preserves it. The close-while-queued test now uses a small eligible JPEG, retaining
all six window-destruction, watcher-ownership and worker-completion checks.

Final 0.1.2 additions exercise the real Open Folder action and directory dialog
accept/cancel paths; color-space changes during a blocked first/replacement open
for sRGB and Display P3; safe rejection of oversized/empty/GIF/SVG neighbor
preload requests while foreground GIF/SVG opens remain usable; and an old preload
callback finishing after `closeImage` without restoring an invalidated cache.
The Open Folder selection is made through the actual QFileDialog accept slot,
without synthesizing its file-selection signal. Its native panel's visual layout
remains a separate UI check.

Mirror/flip are checked against pixels captured from the real viewport: all four
quadrants must swap horizontally/vertically and restore after repeating the
operation. The real Recent menu action must reopen its own temporary Unicode-
named image. These are runtime checks, not just source inspection.

`raw-82-before-active-precondition/` preserves a run with 81 passes and one
keyboard-navigation failure. That harness waited for window exposure but did not
verify Cocoa activation or focus before sending a key. Its original report did
not record activation state, so it cannot prove what caused that missed shortcut.
The harness now waits for a genuinely active window and focused view before
keyboard input. `navigation-focus-diagnostic/` verifies the same preceding format
sequence, navigation and Escape, with 12 passes. Logs record active-window/focus
state and exactly one real navigation QAction trigger for the first Right key;
no failed key is silently resent. The final 82-case run includes those checks.

The hover regression keeps a real toolbar button and then the mouse-transparent
filename row visible for 2.5 seconds each, then moves the event position into the
image viewport and requires hiding after 2.5 seconds. It neither reads the global
cursor to decide a result nor warps/injects the system pointer. It verifies the
same received-event position used by the application's native mouse handler.

`accepted-82-before-hover-fix/` preserves the preceding 82-case run. The final
report and source snapshot include the subsequent event-position hover fix.
The corresponding `hover-fixed-focused/` run passed 6/6 checks in 11.588 seconds;
the final complete native Cocoa run passed 82/82 in 51.345 seconds.
