# 轻看 0.1.0 历史基线：应用包格式与依赖检查

> 公开版本说明：本机工程、Qt SDK 与用户缓存路径已分别标准化为 `${PROJECT_ROOT}`、`${QINGVIEW_QT_ROOT}`、`${QINGVIEW_BUILD_ROOT}`；系统库路径及测试结果、数量、时间、哈希保持原值。

本报告保留 0.1.0 的历史检查结果，不代表当前 0.1.2 版本。当前版本请查看 [0.1.2 验证报告](../Docs/Verification-0.1.2.md)、[0.1.2 格式检查结果](packaged-format-check-result-0.1.2.json) 与 [0.1.2 依赖检查结果](packaged-dependency-check-result-0.1.2.json)。

被检查的 0.1.0 应用包当时位于 `dist/release/轻看.app`；应用版本：0.1.0；Qt 版本：6.8.3。

本报告对应最终重建后的主程序 SHA-256：`a04fa6be208f2557043c24931d84a9ba928886dcbc76c7720236c4f475561a12`。两份 JSON 结果已在最终构建完成后重新生成。

## 格式检查：13 项全部通过

诊断工具显式调用 `QCoreApplication::setLibraryPaths`，唯一插件搜索目录为应用包 `Contents/PlugIns`；Qt frameworks 也由应用包 `Contents/Frameworks` 加载。dyld 的实际加载记录中，15 个库来自应用包，来自开发 SDK 的库为零。

- JPG、PNG、WebP、HEIC、中文文件名图片成功读取。
- JPEG 与 HEIC 的 EXIF 6 均正确顺时针旋转为 64 × 96，四象限颜色位置正确。
- GIF 的红、绿、蓝三帧读取正确，识别为动画。
- PNG 透明度、6000 × 4000 JPEG 和损坏 JPEG 拒绝处理通过。
- Display P3 HEIC 转 sRGB 与 ImageIO 参考图的最大 RGB 通道误差为 0。

详细证据：`packaged-format-check-result.json`，含独占插件路径和所有实际加载库路径。

## Mach-O 检查：20 个文件、275 条依赖全部通过

全部 20 个 Mach-O 均为单一 `arm64` 架构。所有非系统依赖均可解析到包内文件；所有 RPATH 均指向包内位置，没有开发 SDK 的绝对路径或外部第三方库依赖。最终包已移除 QtNetwork framework 及其附属网络插件。

Qt 插件保留的 `@loader_path/../../lib` 在包内不存在，但应用主程序的 `@executable_path/../Frameworks` 可解析其依赖；这不妨碍加载，也不会回退到 SDK。Apple 系统 framework 和 `/usr/lib` 的库可能通过 dyld shared cache 提供，因此无需其在文件系统中单独存在。Qt 旧版二进制的系统 AGL 依赖已在当前系统实际加载成功；本地编译时 SDK 移除 AGL 头文件/链接 stub 的问题与此不同。

应用包含 `Contents/Resources/qt.conf`，其中 `Plugins = PlugIns` 指向随包插件。

历史详细证据：`packaged-dependency-check-result.json`，含每个文件的 SHA-256、架构、依赖解析和 RPATH。如需检查当前本地构建，可写入单独的结果文件，以保留历史基线：

```sh
mkdir -p tests/results
python3 formats/audit_bundle.py 'dist/release/轻看.app' tests/results/packaged-dependency-check-result.local.json
```

本次检查没有修改应用包、重新签名或重新打包。UI 操作与播放时序由应用集成验收覆盖。

## Qt 完整许可证定位

当前二进制 SDK 未发现完整 LGPL 文本。已经核对的官方 Qt Image Formats v6.8.3 源码目录包含完整文本：

[Qt Image Formats v6.8.3 的 LGPLv3 全文](https://github.com/qt/qtimageformats/blob/v6.8.3/LICENSES/LGPL-3.0-only.txt)

同目录包含 `GPL-2.0-only.txt`、`GPL-3.0-only.txt`。Qt Image Formats 源码提交为 `3b6b1fb96041f1bc85a8f42593bef2ee7bcdca6c`；来源链接见 `README.md`。
