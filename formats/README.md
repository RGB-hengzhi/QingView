# macOS image formats

QingView uses Qt 6.8.3 image I/O and the system decoder. No custom HEIC plugin is needed.

| Format | Runtime implementation |
| --- | --- |
| JPEG / PNG / GIF | Qt GUI image handlers (`qjpeg`, `qgif`, and the built-in PNG handler) |
| WebP | Qt Image Formats `qwebp`, using its bundled libwebp |
| HEIC / HEIF | Qt Image Formats `qmacheif`, using macOS ImageIO / CoreGraphics |

`qmacheif` must be present in the shipped app's `Contents/PlugIns/imageformats` directory alongside `qwebp` and `qgif`/`qjpeg`. Plugin names can vary by packaging convention (for example `libqmacheif.dylib`).

qView's existing `QVImageCore::readFile` enables `QImageReader::setAutoTransform(true)`, so the native HEIC handler's `ImageTransformation` option applies EXIF orientation once. User rotation uses the display pixmap and does not overwrite the source file.

Qt 6.8.3's `qt_mac_toQImage` draws the native decoded CGImage into an explicit sRGB context. The application then marks untagged pixels as sRGB and converts to its target color space. This is color-managed SDR conversion; it does not retain the original wide-gamut or HDR pixel encoding. The fixtures include a Display P3 HEIC and an ImageIO-generated sRGB reference for testing this path.

## Reproducible fixtures

Fixtures are generated locally and are not included in a fresh clone. From the repository root, set `QINGVIEW_QT_ROOT` to the installed Qt 6.8.3 macOS SDK directory, then run:

```sh
QT_ROOT="$QINGVIEW_QT_ROOT" bash tests/generate_fixtures.sh
```

This creates synthetic JPEG, PNG, HEIC, a three-frame GIF, an EXIF-orientation JPEG, a transparent PNG, a 6000 × 4000 JPEG, a deliberately invalid JPEG, Chinese filenames, and naturally sorted folder samples. The same script builds `tests/generate_webp_fixture.cpp` with `tests/generate_webp.pro` and the selected Qt SDK, generates the lossless WebP sample, and verifies its round trip. It uses `tests/generate_fixtures.swift` for the other samples. Set `QINGVIEW_FIXTURES` to choose another output directory.

## Runtime verification

`format-check-result.json` records 13 passing checks with the Qt 6.8.3 SDK plugins on arm64. These are test cases, not 13 independently verified file-format types. JPEG, PNG, WebP, HEIC, and the Chinese filename decode successfully; JPEG and HEIC EXIF 6 both rotate clockwise to 64 × 96 with the expected quadrant colors; transparency, all three GIF color frames, the 24 MP JPEG, and invalid-file rejection pass. The Display P3 HEIC conversion matches the ImageIO sRGB reference with a maximum RGB channel difference of zero. The current packaged-release checks are in [packaged-format-check-result-0.1.2.json](packaged-format-check-result-0.1.2.json) and [the 0.1.2 verification report](../Docs/Verification-0.1.2.md).

The standalone diagnostic builds use `formats/format_probe.pro` and `formats/generate_webp_fixture.pro`. With the macOS 27 SDK, the tools include `<arm_acle.h>` before Qt headers to supply the old Qt `__yield` declaration; they disable qmake `link_prl` and override `QMAKE_LIBS_OPENGL` to avoid the removed AGL framework. The production build in `qView.pro` uses the same compatibility settings, as do the integration and resource harnesses. These are build-time SDK workarounds; final app bundle plugin deployment and UI playback are checked separately.

## Sources

- Qt Image Formats v6.8.3 commit `3b6b1fb96041f1bc85a8f42593bef2ee7bcdca6c`:
  - [HEIC handler](https://github.com/qt/qtimageformats/blob/v6.8.3/src/plugins/imageformats/macheif/qmacheifhandler.cpp)
  - [Plugin format keys](https://github.com/qt/qtimageformats/blob/v6.8.3/src/plugins/imageformats/macheif/macheif.json)
  - [Native ImageIO bridge and orientation mapping](https://github.com/qt/qtimageformats/blob/v6.8.3/src/plugins/imageformats/shared/qiiofhelpers.cpp)
  - [WebP handler](https://github.com/qt/qtimageformats/blob/v6.8.3/src/plugins/imageformats/webp/qwebphandler.cpp)
- [Qt 6.8.3 CGImage-to-QImage conversion and sRGB context](https://github.com/qt/qtbase/blob/v6.8.3/src/gui/painting/qcoregraphics.mm)
- [Qt Image Formats documentation](https://doc.qt.io/qt-6.8/qtimageformats-index.html)

Use the exact plugin binaries from the selected Qt SDK; a plugin from an unrelated Qt version can fail to load.
