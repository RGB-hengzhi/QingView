// Generate WebP with the exact Qt image plugin set used by QingView.
// Usage: generate_webp_fixture tests/fixtures/formats/sample.png tests/fixtures/formats/sample.webp
#if defined(__aarch64__)
#include <arm_acle.h> // Qt 6.8.3's qyieldcpu.h needs this with new Apple Clang.
#endif
#include <QCoreApplication>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QDebug>

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (argc != 3) {
        qCritical() << "Expected source PNG and destination WebP paths";
        return 2;
    }
    QImageReader reader(QString::fromLocal8Bit(argv[1]));
    const QImage image = reader.read();
    if (image.isNull()) {
        qCritical() << reader.errorString();
        return 1;
    }
    QImageWriter writer(QString::fromLocal8Bit(argv[2]), "webp");
    writer.setQuality(100); // Qt WebP lossless mode, preserving quadrant pixels.
    if (!writer.write(image)) {
        qCritical() << writer.errorString();
        return 1;
    }
    QImageReader verification(QString::fromLocal8Bit(argv[2]));
    const QImage decoded = verification.read();
    if (decoded.isNull() || decoded.size() != image.size()) {
        qCritical() << "WebP round-trip failed:" << verification.errorString();
        return 1;
    }
    qInfo() << "Generated" << QString::fromLocal8Bit(argv[2]) << decoded.size();
    return 0;
}
