// Focused runtime checks for the shipped Qt image plugin set. The integration
// harness tests QingView itself; this checks EXIF and native color conversion.
#if defined(__aarch64__)
#include <arm_acle.h> // Qt 6.8.3's qyieldcpu.h needs this with new Apple Clang.
#endif
#include <QCoreApplication>
#include <QColorSpace>
#include <QDir>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <algorithm>
#include <cstdlib>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (argc != 2 && argc != 3) {
        QTextStream(stderr) << "Expected formats fixture directory and optional exclusive plugin directory\n";
        return 2;
    }
    if (argc == 3)
        QCoreApplication::setLibraryPaths({QString::fromLocal8Bit(argv[2])});
    const QDir fixtures(QString::fromLocal8Bit(argv[1]));
    QJsonArray checks;
    bool passed = true;
    const auto record = [&](const QString &name, bool ok, const QJsonObject &details = {}) {
        QJsonObject check(details);
        check.insert("name", name);
        check.insert("passed", ok);
        checks.append(check);
        passed = passed && ok;
    };
    const auto read = [&](const QString &name, bool transform = true) {
        QImageReader reader(fixtures.filePath(name));
        reader.setAutoTransform(transform);
        return reader.read();
    };

    for (const QString &name : {"sample.jpg", "sample.png", "sample.webp", "sample.heic",
                                "transparent.png", "中文 测试 图片.png"}) {
        QImageReader reader(fixtures.filePath(name));
        reader.setAutoTransform(true);
        const QByteArray detectedFormat = reader.format();
        const QImage image = reader.read();
        record("decode " + name, !image.isNull() && image.size() == QSize(96, 64),
               {{"format", QString::fromLatin1(detectedFormat)},
                {"width", image.width()}, {"height", image.height()},
                {"error", image.isNull() ? reader.errorString() : QString()}});
    }

    for (const QString &name : {"oriented.jpg", "oriented.heic"}) {
        const QImage original = read(name, false);
        const QImage oriented = read(name);
        // Original: red/green across the top; blue/yellow across the bottom.
        // EXIF 6 rotates clockwise: blue/red across the top, yellow/green below.
        bool colorsOK = false;
        if (!oriented.isNull() && oriented.size() == QSize(64, 96)) {
            const QColor topLeft = oriented.pixelColor(16, 24);
            const QColor topRight = oriented.pixelColor(48, 24);
            const QColor bottomLeft = oriented.pixelColor(16, 72);
            const QColor bottomRight = oriented.pixelColor(48, 72);
            colorsOK = topLeft.blue() > 220 && topLeft.red() < 30 &&
                       topRight.red() > 220 && topRight.green() < 30 &&
                       bottomLeft.red() > 220 && bottomLeft.green() > 220 &&
                       bottomRight.green() > 220 && bottomRight.red() < 30;
        }
        record("EXIF orientation " + name,
               original.size() == QSize(96, 64) && oriented.size() == QSize(64, 96) && colorsOK,
               {{"originalWidth", original.width()}, {"originalHeight", original.height()},
                {"displayWidth", oriented.width()}, {"displayHeight", oriented.height()},
                {"quadrantsCorrect", colorsOK}});
    }

    const QImage transparent = read("transparent.png");
    record("PNG alpha", !transparent.isNull() && transparent.pixelColor(72, 32).alpha() == 0 &&
                          transparent.pixelColor(24, 32).alpha() == 255);
    const QImage large = read("large.jpg");
    record("24 MP JPEG", large.size() == QSize(6000, 4000));
    QImageReader broken(fixtures.filePath("broken.jpg"));
    const QImage invalid = broken.read();
    record("invalid JPEG rejected", invalid.isNull(), {{"error", broken.errorString()}});

    QImageReader gif(fixtures.filePath("animated.gif"));
    QJsonArray frameColors;
    bool animationOK = gif.supportsAnimation() && gif.imageCount() == 3;
    for (int frame = 0; frame < 3; ++frame) {
        const QImage image = gif.read();
        if (image.isNull()) {
            animationOK = false;
            break;
        }
        const QColor color = image.pixelColor(48, 32);
        frameColors.append(color.name());
        const QColor expected[] = {Qt::red, Qt::green, Qt::blue};
        animationOK = animationOK && color == expected[frame];
    }
    record("GIF frames", animationOK,
           {{"supportsAnimation", gif.supportsAnimation()},
            {"imageCount", gif.imageCount()}, {"frameColors", frameColors}});

    QImage p3 = read("sample-p3.heic");
    QImage reference = read("sample-p3-reference.png");
    // qmacheif's CoreGraphics context has already converted its output to sRGB,
    // even though that QImage is untagged. Match qView's existing tagging step.
    if (!p3.isNull() && !p3.colorSpace().isValid())
        p3.setColorSpace(QColorSpace::SRgb);
    if (!reference.isNull() && !reference.colorSpace().isValid())
        reference.setColorSpace(QColorSpace::SRgb);
    int maximumError = 255;
    if (!p3.isNull() && !reference.isNull() && p3.size() == reference.size()) {
        p3.convertToColorSpace(QColorSpace::SRgb);
        reference.convertToColorSpace(QColorSpace::SRgb);
        const QColor a = p3.pixelColor(48, 32), b = reference.pixelColor(48, 32);
        maximumError = std::max({std::abs(a.red() - b.red()), std::abs(a.green() - b.green()),
                                 std::abs(a.blue() - b.blue())});
    }
    record("Display P3 HEIC to sRGB", maximumError <= 2, {{"maximumChannelError", maximumError}});

    QJsonArray supported;
    for (const QByteArray &format : QImageReader::supportedImageFormats())
        supported.append(QString::fromLatin1(format));
    QJsonArray pluginPaths;
    for (const QString &path : QCoreApplication::libraryPaths())
        pluginPaths.append(path);
    QJsonArray loadedLibraries;
#if defined(__APPLE__)
    for (uint32_t index = 0; index < _dyld_image_count(); ++index)
        loadedLibraries.append(QString::fromLocal8Bit(_dyld_get_image_name(index)));
#endif
    const QJsonObject result = {{"passed", passed}, {"qtVersion", qVersion()},
                                {"pluginSearchPaths", pluginPaths},
                                {"loadedLibraries", loadedLibraries},
                                {"supportedFormats", supported}, {"checks", checks}};
    QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Indented);
    return passed ? 0 : 1;
}
