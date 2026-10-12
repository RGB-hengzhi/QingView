#include <QtTest>
#include <QFileOpenEvent>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QDataStream>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QUuid>
#include <QGraphicsPixmapItem>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QScopeGuard>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QToolButton>
#include <QWheelEvent>
#include <QtConcurrent/QtConcurrentRun>
#include <memory>

#include "qvapplication.h"
#include "qvrenamedialog.h"

class ScopedOptions
{
public:
    ScopedOptions(std::initializer_list<QPair<QString, QVariant>> values)
    {
        for (const auto &entry : values)
        {
            const QString key = "options/" + entry.first;
            previous.insert(key, settings.value(key));
            settings.setValue(key, entry.second);
        }
        settings.sync();
        qvApp->getSettingsManager().loadSettings();
    }
    ~ScopedOptions()
    {
        for (auto it = previous.cbegin(); it != previous.cend(); ++it)
        {
            if (it.value().isValid())
                settings.setValue(it.key(), it.value());
            else
                settings.remove(it.key());
        }
        settings.sync();
        qvApp->getSettingsManager().loadSettings();
    }
private:
    QSettings settings;
    QMap<QString, QVariant> previous;
};

// These tests compile the actual application sources. No private members,
// decoder stubs, or replacement view implementation are used.
class IntegrationTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void emptyWindow();
    void supportedFormats_data();
    void supportedFormats();
    void naturalSortAndNavigation();
    void zoomFitAndOriginalSize();
    void wheelZoomUsesFineSteps_data();
    void wheelZoomUsesFineSteps();
    void wheelZoomReversesAndKeepsButtonStep();
    void wheelZoomIsIndependentOfEventPartition();
    void wheelZoomSupportsPixelOnlyAndPrefersAngleDelta();
    void wheelZoomSettingTakesEffect_data();
    void wheelZoomSettingTakesEffect();
    void wheelZoomOptionSavesThroughApply();
    void rotateDisplayedPixels();
    void fullScreenAndEscape();
    void damagedFileAndRecovery();
    void animatedGifChangesDisplayedFrame();
    void finderFileOpenEvent();
    void toolbarButtonsAndStatus();
    void toolbarAutoHidesAndReappears();
    void remoteUrlAndPasteAreIgnored();
    void localUrlAndMixedDropStillOpen();
    void vectorSvgPreservesAspectAndPixels();
    void closingWindowsWithQueuedDecodeAndPreloadIsSafe();
    void reopeningExternallyChangedSameSizeImageShowsNewPixels();
    void rapidOpenShowsLastRequestedImage();
    void directoryOpenSkipsNonImagesAndMetadata();
    void emptyAndSingleImageDirectoriesRemainUsable();
    void folderSorting_data();
    void folderSorting();
    void naturalSortChangeKeepsStatusAndLoopBoundaries();
    void exifInfoMatchesUprightDimensions_data();
    void exifInfoMatchesUprightDimensions();
    void informationMegapixelsUseNearestTenth_data();
    void informationMegapixelsUseNearestTenth();
    void gifRotationPersistsAtActualSize_data();
    void gifRotationPersistsAtActualSize();
    void copyContainsDisplayedImageAndLocalPath();
    void renameTemporaryImage_data();
    void renameTemporaryImage();
    void renameCollisionPreservesImageAndAnimation_data();
    void renameCollisionPreservesImageAndAnimation();
    void trashCancelDeleteAndUndoTemporaryImage();
    void undoTrashCollisionRetainsRecovery();
    void staleDeleteConfirmationDoesNotDeleteAnotherImage();
    void slideshowStopsResumesAndHonorsBoundary();
    void settingsCancelAndOkPropagateToNewWindow();
    void openingEmptyWindowKeepsOtherWindowsOpenWithMenuEnabled();
    void missingFileReloadAndExternalRemovalRecover();
    void openFolderMenuLoadsChosenDirectory();
    void openFolderCancelKeepsCurrentImage();
    void colorSettingsDuringQueuedOpenKeepRequestedImage_data();
    void colorSettingsDuringQueuedOpenKeepRequestedImage();
    void preloadingSkipsOversizedInvalidAndExpensiveNeighbors();
    void preloadCompletionAfterCloseDoesNotRestoreOldCache();
    void mirrorAndFlipChangeVisibleQuadrantsAndRestore();
    void recentMenuOpensTemporaryImage();

private:
    QString fixture(const QString &relative) const;
    bool waitForLoad(const QString &relative, int timeout = 10000);
    bool openPath(const QString &path, int timeout = 10000);
    bool clickControl(const QString &key);
    void movePointer(QWidget *target, const QPoint &pos);
    QGraphicsPixmapItem *displayedItem() const;
    QRectF displayedRect() const;
    void saveScreenshot() const;
    bool writeVectorFixture(const QString &path) const;
    QMessageBox *visibleConfirmation() const;
    void sendWheelDelta(int verticalAngle, int verticalPixel = 0);

    QString fixtures;
    QPointer<MainWindow> window;
    QVGraphicsView *view = nullptr;
};

QString IntegrationTests::fixture(const QString &relative) const
{
    return QDir(fixtures).filePath(relative);
}

void IntegrationTests::initTestCase()
{
    fixtures = qEnvironmentVariable("QINGVIEW_FIXTURES");
    if (fixtures.isEmpty())
        fixtures = QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("fixtures");
    QVERIFY2(QFileInfo(fixture("navigation/1.png")).isFile(),
             qPrintable("Missing fixtures: " + fixtures + ". Run tests/generate_fixtures.swift first."));
    qInfo() << "Qt" << QT_VERSION_STR << "platform" << QGuiApplication::platformName()
            << "fixtures" << fixtures;
}

void IntegrationTests::init()
{
    window = QVApplication::newWindow();
    QVERIFY(window);
    window->resize(760, 520);
    window->activateWindow();
    view = window->findChild<QVGraphicsView *>();
    QVERIFY(view);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    // Exposed does not imply active on Cocoa. QAction keyboard shortcuts
    // require an active window; wait for that real state before sending keys.
    QVERIFY2(QTest::qWaitForWindowActive(window, 5000), "The test window must become active before keyboard input");
    view->setFocus();
    QTRY_VERIFY_WITH_TIMEOUT(view->hasFocus(), 3000);
    QTest::qWait(100);
}

void IntegrationTests::cleanup()
{
    saveScreenshot();
    // Preloading reads application-owned state from the thread pool. Let it
    // finish before tearing down the real window between test cases.
    QThreadPool::globalInstance()->waitForDone(10000);
    QCoreApplication::processEvents();
    if (window)
    {
        if (window->isFullScreen())
        {
            window->toggleFullScreen();
            QTest::qWait(600);
        }
        window->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    view = nullptr;
}

bool IntegrationTests::waitForLoad(const QString &relative, int timeout)
{
    return openPath(fixture(relative), timeout);
}

bool IntegrationTests::openPath(const QString &filePath, int timeout)
{
    const QString path = QFileInfo(filePath).absoluteFilePath();
    window->openFile(path);
    const bool loaded = QTest::qWaitFor([&]() {
        const auto &details = window->getCurrentFileDetails();
        return details.fileInfo.absoluteFilePath() == path && details.isPixmapLoaded;
    }, timeout);
    if (loaded)
        QTest::qWait(100);
    return loaded;
}

QGraphicsPixmapItem *IntegrationTests::displayedItem() const
{
    if (!view || !view->scene())
        return nullptr;
    for (auto *item : view->scene()->items())
        if (auto *pixmap = qgraphicsitem_cast<QGraphicsPixmapItem *>(item))
            return pixmap;
    return nullptr;
}

QRectF IntegrationTests::displayedRect() const
{
    auto *item = displayedItem();
    return item ? view->transform().mapRect(item->sceneBoundingRect()) : QRectF();
}

bool IntegrationTests::clickControl(const QString &key)
{
    auto *toolbar = window->findChild<QWidget *>("qingToolbar");
    auto *button = window->findChild<QToolButton *>("qingToolbar_" + key);
    if (!toolbar || !button || !button->isEnabled())
        return false;
    movePointer(view->viewport(), QPoint(25, 60));
    QCoreApplication::processEvents();
    if (!toolbar->isVisible())
    {
        QEnterEvent enter(QPointF(25, 60), QPointF(25, 60), view->viewport()->mapToGlobal(QPoint(25, 60)));
        QCoreApplication::sendEvent(view->viewport(), &enter);
    }
    if (!toolbar->isVisible())
        return false;
    QTest::mouseClick(button, Qt::LeftButton);
    QCoreApplication::processEvents();
    return true;
}

void IntegrationTests::movePointer(QWidget *target, const QPoint &pos)
{
    // Deliver only an event to this test widget. Do not warp or inject input
    // into the system pointer; native hover retention is checked separately
    // through the supported computer-use UI by the release audit.
    const QPoint global = target->mapToGlobal(pos);
    QMouseEvent move(QEvent::MouseMove, pos, global, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(target, &move);
    QCoreApplication::processEvents();
}

void IntegrationTests::saveScreenshot() const
{
    const QString dir = qEnvironmentVariable("QINGVIEW_RESULTS_DIR");
    if (dir.isEmpty() || !window)
        return;
    QDir().mkpath(dir);
    QString name = QString::fromLatin1(QTest::currentTestFunction());
    if (QTest::currentDataTag() && *QTest::currentDataTag())
        name += "_" + QString::fromLatin1(QTest::currentDataTag());
    window->grab().save(QDir(dir).filePath(name + ".png"));
}

void IntegrationTests::emptyWindow()
{
    auto *open = window->findChild<QPushButton *>("emptyOpenButton");
    auto *toolbar = window->findChild<QWidget *>("qingToolbar");
    QVERIFY(open);
    QVERIFY(toolbar);
    QVERIFY(open->isVisible());
    QVERIFY(!toolbar->isVisible());
    QVERIFY(!window->getIsPixmapLoaded());
    QVERIFY(view->viewport()->rect().contains(open->geometry()));
}

void IntegrationTests::supportedFormats_data()
{
    QTest::addColumn<QString>("relative");
    QTest::addColumn<QSize>("decodedSize");
    QTest::newRow("JPEG") << QString("formats/sample.jpg") << QSize(96, 64);
    QTest::newRow("PNG") << QString("formats/sample.png") << QSize(96, 64);
    QTest::newRow("WebP") << QString("formats/sample.webp") << QSize(96, 64);
    QTest::newRow("HEIC") << QString("formats/sample.heic") << QSize(96, 64);
    QTest::newRow("EXIF_orientation_6") << QString("formats/oriented.jpg") << QSize(64, 96);
    QTest::newRow("transparent_PNG") << QString("formats/transparent.png") << QSize(96, 64);
    QTest::newRow("Chinese_filename") << QString::fromUtf8("formats/中文 测试 图片.png") << QSize(96, 64);
    QTest::newRow("large_JPEG") << QString("formats/large.jpg") << QSize(6000, 4000);
}

void IntegrationTests::supportedFormats()
{
    QFETCH(QString, relative);
    QFETCH(QSize, decodedSize);
    QVERIFY2(QFileInfo(fixture(relative)).isFile(), qPrintable("Missing fixture: " + relative));
    QVERIFY2(waitForLoad(relative, 20000), qPrintable("Failed to display: " + relative));
    QCOMPARE(view->getLoadedPixmap().size(), decodedSize);
    QVERIFY(displayedItem());
    QVERIFY(!displayedItem()->pixmap().isNull());
    QVERIFY(!window->getCurrentFileDetails().errorData.hasError);
    if (relative.endsWith("transparent.png"))
    {
        const QImage image = view->getLoadedPixmap().toImage();
        QVERIFY(image.hasAlphaChannel());
        QVERIFY(qAlpha(image.pixel(75, 30)) < 255);
        QCOMPARE(qAlpha(image.pixel(10, 30)), 255);
    }
}

void IntegrationTests::naturalSortAndNavigation()
{
    QVERIFY(waitForLoad("navigation/1.png"));
    QStringList names;
    for (const auto &file : window->getCurrentFileDetails().folderFileInfoList)
        names << file.fileName;
    QCOMPARE(names, QStringList({"1.png", "2.png", "10.png"}));
    QCOMPARE(window->getCurrentFileDetails().loadedIndexInFolder, 0);

    qInfo() << "Navigation before keyboard: window active" << window->isActiveWindow()
            << "QApplication active window" << (QApplication::activeWindow() == window)
            << "view focus" << view->hasFocus() << "application state" << QGuiApplication::applicationState();
    QVERIFY(window->isActiveWindow());
    QVERIFY(view->hasFocus());
    QList<std::shared_ptr<QSignalSpy>> actionTriggers;
    for (auto *action : qvApp->getActionManager().getAllInstancesOfAction("nextfile"))
        actionTriggers << std::make_shared<QSignalSpy>(action, &QAction::triggered);
    // Keyboard events pass through MainWindow's real QAction/shortcut wiring.
    QTest::keyClick(view, Qt::Key_Right);
    int triggered = 0;
    for (const auto &spy : actionTriggers) triggered += spy->count();
    qInfo() << "First Right action triggers" << triggered
            << "active" << window->isActiveWindow() << "view focus" << view->hasFocus()
            << "loading" << window->getCurrentFileDetails().isLoadRequested;
    QVERIFY2(triggered > 0, "Right must activate a real navigation QAction in the focused test window");
    QTRY_COMPARE_WITH_TIMEOUT(window->getCurrentFileDetails().fileInfo.fileName(), QString("2.png"), 5000);
    QTest::keyClick(view, Qt::Key_Right);
    QTRY_COMPARE_WITH_TIMEOUT(window->getCurrentFileDetails().fileInfo.fileName(), QString("10.png"), 5000);
    QTest::keyClick(view, Qt::Key_Left);
    QTRY_COMPARE_WITH_TIMEOUT(window->getCurrentFileDetails().fileInfo.fileName(), QString("2.png"), 5000);
    window->firstFile();
    QTRY_COMPARE_WITH_TIMEOUT(window->getCurrentFileDetails().fileInfo.fileName(), QString("1.png"), 5000);
    window->lastFile();
    QTRY_COMPARE_WITH_TIMEOUT(window->getCurrentFileDetails().fileInfo.fileName(), QString("10.png"), 5000);
    QCOMPARE(window->getCurrentFileDetails().loadedIndexInFolder, 2);
}

void IntegrationTests::zoomFitAndOriginalSize()
{
    QVERIFY(waitForLoad("formats/large.jpg", 20000));
    window->resetZoom();
    QTest::qWait(150);
    const QRectF fitted = displayedRect();
    QVERIFY(fitted.width() > 0 && fitted.height() > 0);
    QVERIFY(fitted.width() <= view->viewport()->width() + 4);
    QVERIFY(fitted.height() <= view->viewport()->height() + 4);

    window->zoomIn();
    QTest::qWait(150);
    const qreal larger = displayedRect().width();
    QVERIFY(larger > fitted.width() * 1.15);
    window->zoomOut();
    QTest::qWait(150);
    QVERIFY(qAbs(displayedRect().width() - fitted.width()) < 3);

    // A real wheel event exercises viewport event forwarding and scroll zoom.
    const QPoint pos = view->viewport()->rect().center();
    QWheelEvent wheel(pos, view->viewport()->mapToGlobal(pos), QPoint(), QPoint(0, 120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(view->viewport(), &wheel);
    QTest::qWait(150);
    const qreal wheelRatio = displayedRect().width() / fitted.width();
    QVERIFY2(wheelRatio > 1.04 && wheelRatio < 1.06,
             "One standard wheel detent should zoom by about 5 percent");

    window->originalSize();
    QTest::qWait(100);
    QCOMPARE(displayedRect().size().toSize(), QSize(6000, 4000));
    window->resetZoom();
    QTest::qWait(150);
    QVERIFY(qAbs(displayedRect().width() - fitted.width()) < 3);
}

void IntegrationTests::sendWheelDelta(int verticalAngle, int verticalPixel)
{
    const QPoint pos = view->viewport()->rect().center();
    QWheelEvent wheel(pos, view->viewport()->mapToGlobal(pos), QPoint(0, verticalPixel), QPoint(0, verticalAngle),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(view->viewport(), &wheel);
    QCoreApplication::processEvents();
}

void IntegrationTests::wheelZoomUsesFineSteps_data()
{
    QTest::addColumn<int>("angle");
    QTest::addColumn<double>("expectedRatio");
    // User-visible contract: a 120-unit detent is 5%; high-resolution partial
    // detents contribute proportionally, and negative detents are reciprocal.
    QTest::newRow("zero") << 0 << 1.0;
    QTest::newRow("tiny_positive") << 1 << 1.000406667368;
    QTest::newRow("eighth_detent_positive") << 15 << 1.006117405887;
    QTest::newRow("half_detent_positive") << 60 << 1.024695076596;
    QTest::newRow("standard_detent_positive") << 120 << 1.05;
    QTest::newRow("two_detents_positive") << 240 << 1.1025;
    QTest::newRow("three_detents_positive") << 360 << 1.157625;
    QTest::newRow("tiny_negative") << -1 << 0.999593497943;
    QTest::newRow("eighth_detent_negative") << -15 << 0.993919789230;
    QTest::newRow("half_detent_negative") << -60 << 0.975900072949;
    QTest::newRow("standard_detent_negative") << -120 << 0.952380952381;
    QTest::newRow("two_detents_negative") << -240 << 0.907029478458;
    QTest::newRow("three_detents_negative") << -360 << 0.863837598531;
}

void IntegrationTests::wheelZoomUsesFineSteps()
{
    QFETCH(int, angle);
    QFETCH(double, expectedRatio);
    QVERIFY(waitForLoad("formats/sample.png"));
    window->originalSize();
    const qreal before = displayedRect().width();
    QVERIFY(before > 0);
    sendWheelDelta(angle);
    const qreal actualRatio = displayedRect().width() / before;
    QVERIFY2(qAbs(actualRatio - expectedRatio) < 0.000001,
             qPrintable(QString("Wheel delta %1 produced ratio %2; expected %3")
                        .arg(angle).arg(actualRatio, 0, 'f', 9).arg(expectedRatio, 0, 'f', 9)));
}

void IntegrationTests::wheelZoomReversesAndKeepsButtonStep()
{
    QVERIFY(waitForLoad("formats/sample.png"));
    window->originalSize();
    const qreal before = displayedRect().width();
    for (int delta : {1, 15, 60, 120, 240, 360})
    {
        sendWheelDelta(delta);
        QVERIFY(displayedRect().width() > before);
        sendWheelDelta(-delta);
        QVERIFY2(qAbs(displayedRect().width() - before) < 0.0001,
                 "Equal forward and reverse wheel deltas must restore the rendered size");
    }
    QVERIFY(clickControl("zoomin"));
    QVERIFY(qAbs(displayedRect().width() / before - 1.25) < 0.000001);
    QVERIFY(clickControl("zoomout"));
    QVERIFY(qAbs(displayedRect().width() - before) < 0.0001);
}

void IntegrationTests::wheelZoomIsIndependentOfEventPartition()
{
    QVERIFY(waitForLoad("formats/sample.png"));
    // Exercise delayed HiDPI resampling as well as the instantaneous transform.
    window->resetZoom();
    QTest::qWait(100);
    const qreal before = displayedRect().width();
    sendWheelDelta(120);
    QTest::qWait(100);
    const qreal oneEvent = displayedRect().width();
    QVERIFY(oneEvent > before * 1.04 && oneEvent < before * 1.06);
    window->resetZoom();
    QTest::qWait(100);
    for (int count = 0; count < 8; ++count)
        sendWheelDelta(15);
    QTest::qWait(100);
    QVERIFY2(qAbs(displayedRect().width() - oneEvent) <= 1.0,
             "Eight high-resolution eighth-detents must match one standard detent after resampling");
    window->resetZoom();
    QTest::qWait(100);
    sendWheelDelta(240);
    QTest::qWait(100);
    const qreal twoDetents = displayedRect().width();
    window->resetZoom();
    QTest::qWait(100);
    sendWheelDelta(120);
    sendWheelDelta(120);
    QTest::qWait(100);
    QVERIFY2(qAbs(displayedRect().width() - twoDetents) <= 1.0,
             "A two-detent event must match two single-detent events after resampling");
}

void IntegrationTests::wheelZoomSupportsPixelOnlyAndPrefersAngleDelta()
{
    QVERIFY(waitForLoad("formats/sample.png"));
    window->originalSize();
    const qreal before = displayedRect().width();
    sendWheelDelta(0, 15);
    QVERIFY(qAbs(displayedRect().width() / before - 1.006117405887) < 0.000001);
    sendWheelDelta(0, -15);
    QVERIFY(qAbs(displayedRect().width() - before) < 0.0001);
    sendWheelDelta(0, 120);
    QVERIFY(qAbs(displayedRect().width() / before - 1.05) < 0.000001);
    sendWheelDelta(0, -120);
    QVERIFY(qAbs(displayedRect().width() - before) < 0.0001);
    sendWheelDelta(120, 1200);
    QVERIFY2(qAbs(displayedRect().width() / before - 1.05) < 0.000001,
             "When both deltas are present, angle movement must take precedence");
    sendWheelDelta(-120, -1200);
    QVERIFY(qAbs(displayedRect().width() - before) < 0.0001);
}

void IntegrationTests::wheelZoomSettingTakesEffect_data()
{
    QTest::addColumn<int>("percent");
    QTest::addColumn<double>("expectedRatio");
    QTest::newRow("one_percent") << 1 << 1.01;
    QTest::newRow("ten_percent") << 10 << 1.10;
}

void IntegrationTests::wheelZoomSettingTakesEffect()
{
    QFETCH(int, percent);
    QFETCH(double, expectedRatio);
    QVERIFY(waitForLoad("formats/sample.png"));
    QSettings settings;
    const QVariant oldValue = settings.value("options/wheelzoompercent");
    const auto restore = qScopeGuard([&]() {
        if (oldValue.isValid())
            settings.setValue("options/wheelzoompercent", oldValue);
        else
            settings.remove("options/wheelzoompercent");
        settings.sync();
        qvApp->getSettingsManager().loadSettings();
    });
    QSignalSpy updates(&qvApp->getSettingsManager(), &SettingsManager::settingsUpdated);
    settings.setValue("options/wheelzoompercent", percent);
    settings.sync();
    qvApp->getSettingsManager().loadSettings();
    QCOMPARE(qvApp->getSettingsManager().getInteger("wheelzoompercent"), percent);
    QCOMPARE(updates.count(), 1);
    window->originalSize();
    const qreal before = displayedRect().width();
    sendWheelDelta(120);
    QVERIFY(qAbs(displayedRect().width() / before - expectedRatio) < 0.000001);
    sendWheelDelta(-120);
    QVERIFY(qAbs(displayedRect().width() - before) < 0.0001);
    window->zoomIn();
    QVERIFY2(qAbs(displayedRect().width() / before - 1.25) < 0.000001,
             "Changing wheel sensitivity must preserve the separate button/keyboard step");
}

void IntegrationTests::wheelZoomOptionSavesThroughApply()
{
    QVERIFY(waitForLoad("formats/sample.png"));
    QSettings settings;
    const QVariant oldValue = settings.value("options/wheelzoompercent");
    const auto restore = qScopeGuard([&]() {
        if (oldValue.isValid())
            settings.setValue("options/wheelzoompercent", oldValue);
        else
            settings.remove("options/wheelzoompercent");
        settings.sync();
        qvApp->getSettingsManager().loadSettings();
    });
    QPointer<QVOptionsDialog> dialog = new QVOptionsDialog(window);
    const auto closeDialog = qScopeGuard([&]() {
        if (dialog)
        {
            dialog->close();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
    });
    dialog->show();
    QVERIFY(QTest::qWaitForWindowExposed(dialog));
    auto *spin = dialog->findChild<QSpinBox *>("wheelZoomPercentSpinBox");
    auto *buttons = dialog->findChild<QDialogButtonBox *>("buttonBox");
    QVERIFY(spin && buttons);
    QCOMPARE(spin->minimum(), 1);
    QCOMPARE(spin->maximum(), 50);
    QCOMPARE(spin->value(), 5);
    spin->setValue(10);
    auto *apply = buttons->button(QDialogButtonBox::Apply);
    QVERIFY(apply && apply->isEnabled());
    QTest::mouseClick(apply, Qt::LeftButton);
    QTRY_COMPARE(qvApp->getSettingsManager().getInteger("wheelzoompercent"), 10);
    QCOMPARE(settings.value("options/wheelzoompercent").toInt(), 10);
    window->originalSize();
    const qreal before = displayedRect().width();
    sendWheelDelta(120);
    QVERIFY2(qAbs(displayedRect().width() / before - 1.10) < 0.000001,
             "The UI Apply action must change the existing viewer's wheel sensitivity");
}

void IntegrationTests::rotateDisplayedPixels()
{
    QVERIFY(waitForLoad("formats/sample.png"));
    const QImage before = view->getLoadedPixmap().toImage();
    QVERIFY(clickControl("rotateright"));
    QTRY_COMPARE(view->getLoadedPixmap().size(), QSize(64, 96));
    const QImage after = view->getLoadedPixmap().toImage();
    const QImage expected = before.transformed(QTransform().rotate(90));
    QCOMPARE(after, expected);
    window->originalSize();
    QTest::qWait(100);
    QCOMPARE(displayedItem()->pixmap().toImage(), after);
    QCOMPARE(displayedRect().size().toSize(), QSize(64, 96));
    window->rotateLeft();
    QTRY_COMPARE(view->getLoadedPixmap().size(), QSize(96, 64));
    QCOMPARE(view->getLoadedPixmap().toImage(), before);
}

void IntegrationTests::fullScreenAndEscape()
{
    QVERIFY(waitForLoad("formats/sample.png"));
    QVERIFY(clickControl("fullscreen"));
    QTRY_VERIFY_WITH_TIMEOUT(window->isFullScreen(), 5000);
    QTest::qWait(700); // native macOS space transition
    QTest::keyClick(view, Qt::Key_Escape);
    QTRY_VERIFY_WITH_TIMEOUT(!window->isFullScreen(), 5000);
    QVERIFY(window->getIsPixmapLoaded());
    QCOMPARE(window->getCurrentFileDetails().fileInfo.fileName(), QString("sample.png"));
}

void IntegrationTests::damagedFileAndRecovery()
{
    QVERIFY(waitForLoad("formats/sample.png"));
    QSignalSpy changed(view, &QVGraphicsView::fileChanged);
    window->openFile(fixture("formats/broken.jpg"));
    QTRY_VERIFY_WITH_TIMEOUT(changed.count() > 0, 5000);
    QVERIFY(window->getCurrentFileDetails().errorData.hasError);
    QVERIFY(!window->getIsPixmapLoaded());
    QVERIFY(view->getLoadedPixmap().isNull());
    QVERIFY(!window->getCurrentFileDetails().errorData.errorString.isEmpty());
    QVERIFY(window->isVisible());
    QVERIFY2(waitForLoad("navigation/2.png"), "A valid image must load after a damaged image");
    QVERIFY(!window->getCurrentFileDetails().errorData.hasError);
}

void IntegrationTests::animatedGifChangesDisplayedFrame()
{
    QVERIFY(waitForLoad("formats/animated.gif"));
    QVERIFY(window->getCurrentFileDetails().isMovieLoaded);
    const QMovie &movie = view->getLoadedMovie();
    QCOMPARE(movie.frameCount(), 3);
    QSignalSpy frames(&movie, &QMovie::frameChanged);
    const QImage initial = displayedItem()->pixmap().toImage();
    QTRY_VERIFY_WITH_TIMEOUT(frames.count() >= 2, 2500);
    QVERIFY(QTest::qWaitFor([&]() {
        return displayedItem()->pixmap().toImage() != initial;
    }, 1500));
    window->pause();
    QCOMPARE(movie.state(), QMovie::Paused);
    const int paused = movie.currentFrameNumber();
    QTest::qWait(350);
    QCOMPARE(movie.currentFrameNumber(), paused);
    window->nextFrame();
    QVERIFY(movie.currentFrameNumber() != paused);
    window->pause();
    QCOMPARE(movie.state(), QMovie::Running);
}

void IntegrationTests::finderFileOpenEvent()
{
    // macOS delivers Finder/Open With requests through QFileOpenEvent. Send
    // that OS-facing event to the real application rather than calling openFile.
    const QString path = fixture(QString::fromUtf8("formats/中文 测试 图片.png"));
    QFileOpenEvent event(path);
    QCoreApplication::sendEvent(qvApp, &event);
    QTRY_VERIFY_WITH_TIMEOUT(window->getIsPixmapLoaded(), 5000);
    QCOMPARE(window->getCurrentFileDetails().fileInfo.absoluteFilePath(), QFileInfo(path).absoluteFilePath());
    QVERIFY(!view->getLoadedPixmap().isNull());
}

void IntegrationTests::toolbarButtonsAndStatus()
{
    QVERIFY(waitForLoad("navigation/1.png"));
    auto *position = window->findChild<QLabel *>("qingPosition");
    auto *filename = window->findChild<QLabel *>("qingFilename");
    auto *zoom = window->findChild<QLabel *>("qingZoom");
    auto *toolbar = window->findChild<QWidget *>("qingToolbar");
    QVERIFY(position && filename && zoom && toolbar);
    QCOMPARE(position->text(), QString("1 / 3"));
    QCOMPARE(filename->toolTip(), QString("1.png"));
    QVERIFY(zoom->text().endsWith('%'));
    QVERIFY(view->viewport()->rect().contains(toolbar->geometry()));
    const QStringList controls = {"open", "previousfile", "nextfile", "zoomout", "zoomin", "resetzoom", "originalsize", "rotateright", "fullscreen"};
    for (const auto &key : controls)
    {
        auto *button = window->findChild<QToolButton *>("qingToolbar_" + key);
        QVERIFY2(button, qPrintable("Missing toolbar button: " + key));
        QVERIFY(!button->accessibleName().isEmpty());
        QVERIFY(toolbar->rect().contains(button->geometry()));
    }
    QVERIFY(clickControl("nextfile"));
    QTRY_COMPARE(window->getCurrentFileDetails().fileInfo.fileName(), QString("2.png"));
    QTRY_COMPARE(position->text(), QString("2 / 3"));
    QVERIFY(clickControl("previousfile"));
    QTRY_COMPARE(window->getCurrentFileDetails().fileInfo.fileName(), QString("1.png"));
    QVERIFY(clickControl("originalsize"));
    QCOMPARE(displayedRect().size().toSize(), QSize(96, 64));
    QVERIFY(clickControl("zoomin"));
    QVERIFY(displayedRect().width() > 96);
    QVERIFY(clickControl("zoomout"));
    QVERIFY(qAbs(displayedRect().width() - 96) < 2);
    QVERIFY(clickControl("resetzoom"));
    QTest::qWait(150);
    QVERIFY(displayedRect().width() <= view->viewport()->width() + 4);
}

void IntegrationTests::toolbarAutoHidesAndReappears()
{
    QVERIFY(waitForLoad("formats/sample.png"));
    auto *toolbar = window->findChild<QWidget *>("qingToolbar");
    auto *timer = window->findChild<QTimer *>("qingHideTimer");
    auto *button = window->findChild<QToolButton *>("qingToolbar_zoomin");
    auto *filename = window->findChild<QLabel *>("qingFilename");
    QVERIFY(toolbar && timer && button && filename);
    QCOMPARE(timer->interval(), 2000);
    const QSize viewportSize = view->viewport()->size();
    movePointer(view->viewport(), QPoint(40, 80));
    QVERIFY(toolbar->isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(!toolbar->isVisible(), 3500);
    QCOMPARE(view->viewport()->size(), viewportSize);

    movePointer(view->viewport(), QPoint(70, 90));
    QTRY_VERIFY(toolbar->isVisible());
    // Use normal events directed to this test's real button. No QCursor warp,
    // CoreGraphics input or global-pointer position is required.
    movePointer(button, button->rect().center());
    QTest::mouseClick(button, Qt::LeftButton);
    QTest::qWait(2500);
    QVERIFY2(toolbar->isVisible(), "Controls must remain visible while the last received pointer event is over a button");

    // Labels are transparent to mouse input, so their real hit target is the
    // toolbar beneath them. Deliver the label's position to that receiver.
    QVERIFY(filename->testAttribute(Qt::WA_TransparentForMouseEvents));
    movePointer(toolbar, filename->mapTo(toolbar, filename->rect().center()));
    QTest::qWait(2500);
    QVERIFY2(toolbar->isVisible(), "Hovering the filename/status row must keep the toolbar visible");

    movePointer(view->viewport(), QPoint(40, 80));
    QTest::qWait(2500);
    QVERIFY2(!toolbar->isVisible(), "After moving back to the image surface, controls must hide after the normal delay");
    QCOMPARE(view->viewport()->size(), viewportSize);
}

void IntegrationTests::remoteUrlAndPasteAreIgnored()
{
    QVERIFY(waitForLoad("navigation/1.png"));
    const QString originalPath = window->getCurrentFileDetails().fileInfo.absoluteFilePath();
    const QImage originalPixels = view->getLoadedPixmap().toImage();
    QSignalSpy changed(view, &QVGraphicsView::fileChanged);

    // Preserve all currently advertised clipboard formats before testing paste.
    auto *savedClipboard = new QMimeData();
    if (const auto *oldClipboard = QApplication::clipboard()->mimeData())
        for (const auto &format : oldClipboard->formats())
            savedClipboard->setData(format, oldClipboard->data(format));
    const auto restore = qScopeGuard([savedClipboard]() {
        QApplication::clipboard()->setMimeData(savedClipboard);
    });

    for (const auto &address : {"https://example.invalid/image.jpg", "http://example.invalid/image.png", "ftp://example.invalid/image.webp"})
    {
        const QUrl remote(QString::fromLatin1(address));
        window->openUrl(remote);

        auto *text = new QMimeData();
        text->setText(remote.toString());
        QApplication::clipboard()->setMimeData(text);
        window->paste();

        auto *urlClipboard = new QMimeData();
        urlClipboard->setUrls({remote});
        QApplication::clipboard()->setMimeData(urlClipboard);
        window->paste();

        QMimeData drop;
        drop.setUrls({remote});
        view->loadMimeData(&drop);
        QTest::qWait(100);
        QCOMPARE(window->getCurrentFileDetails().fileInfo.absoluteFilePath(), originalPath);
        QVERIFY(window->getIsPixmapLoaded());
        QCOMPARE(view->getLoadedPixmap().toImage(), originalPixels);
        QVERIFY(!window->getCurrentFileDetails().errorData.hasError);
    }
    QCOMPARE(changed.count(), 0);
}

void IntegrationTests::localUrlAndMixedDropStillOpen()
{
    QVERIFY(waitForLoad("navigation/1.png"));
    window->openUrl(QUrl::fromLocalFile(fixture("navigation/2.png")));
    QTRY_COMPARE_WITH_TIMEOUT(window->getCurrentFileDetails().fileInfo.fileName(), QString("2.png"), 5000);
    QVERIFY(window->getIsPixmapLoaded());

    // A rejected remote entry before a local file must not consume the first
    // accepted-file slot or prevent the local image from opening in this window.
    QMimeData mixed;
    mixed.setUrls({QUrl("https://example.invalid/ignored.png"),
                   QUrl::fromLocalFile(fixture("navigation/10.png")),
                   QUrl("ftp://example.invalid/also-ignored.jpg")});
    view->loadMimeData(&mixed);
    QTRY_COMPARE_WITH_TIMEOUT(window->getCurrentFileDetails().fileInfo.fileName(), QString("10.png"), 5000);
    QVERIFY(window->getIsPixmapLoaded());
    QCOMPARE(view->getLoadedPixmap().toImage().pixelColor(40, 30), QColor(Qt::blue));
}

bool IntegrationTests::writeVectorFixture(const QString &path) const
{
    // Non-square geometry makes accidental aspect distortion observable;
    // different quadrant colors verify actual rendered pixels after decoding.
    const QByteArray svg = R"(<svg xmlns="http://www.w3.org/2000/svg" width="96" height="64" viewBox="0 0 96 64">
<rect width="48" height="32" fill="#ff0000"/>
<rect x="48" width="48" height="32" fill="#00ff00"/>
<rect y="32" width="48" height="32" fill="#0000ff"/>
<rect x="48" y="32" width="48" height="32" fill="#ffff00"/>
</svg>)";
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(svg) == svg.size();
}

void IntegrationTests::vectorSvgPreservesAspectAndPixels()
{
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    const QString path = corpus.filePath("simple.svg");
    QVERIFY(writeVectorFixture(path));
    window->openFile(path);
    QTRY_VERIFY_WITH_TIMEOUT(window->getIsPixmapLoaded(), 5000);
    QCOMPARE(window->getCurrentFileDetails().fileInfo.absoluteFilePath(), path);
    QCOMPARE(window->getCurrentFileDetails().baseImageSize, QSize(96, 64));
    const QImage rendered = view->getLoadedPixmap().toImage();
    QVERIFY(rendered.width() >= 96);
    QVERIFY(qAbs(qreal(rendered.width()) / rendered.height() - 1.5) < 0.01);
    QCOMPARE(rendered.pixelColor(rendered.width() / 4, rendered.height() / 4), QColor(Qt::red));
    QCOMPARE(rendered.pixelColor(rendered.width() * 3 / 4, rendered.height() / 4), QColor(Qt::green));
    QCOMPARE(rendered.pixelColor(rendered.width() / 4, rendered.height() * 3 / 4), QColor(Qt::blue));
    QCOMPARE(rendered.pixelColor(rendered.width() * 3 / 4, rendered.height() * 3 / 4), QColor(Qt::yellow));
    QVERIFY(displayedItem() && !displayedItem()->pixmap().isNull());
}

void IntegrationTests::closingWindowsWithQueuedDecodeAndPreloadIsSafe()
{
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    auto *pool = QThreadPool::globalInstance();
    const int originalThreads = pool->maxThreadCount();
    pool->setMaxThreadCount(1);
    QSemaphore blockerStarted;
    QSemaphore unblockWorker;
    auto blocker = QtConcurrent::run([&]() {
        blockerStarted.release();
        unblockWorker.acquire();
    });
    const auto releasePool = qScopeGuard([&]() {
        unblockWorker.release();
        blocker.waitForFinished();
        pool->waitForDone(20000);
        pool->setMaxThreadCount(originalThreads);
    });
    QVERIFY(blockerStarted.tryAcquire(1, 5000));

    QList<QPointer<QObject>> destroyedObservers;
    for (int iteration = 0; iteration < 6; ++iteration)
    {
        const QString svgPath = corpus.filePath(QString("vector_%1.svg").arg(iteration));
        const QString jpegPath = corpus.filePath(QString("preload_%1.jpg").arg(iteration));
        QVERIFY(writeVectorFixture(svgPath));
        // Use eligible images whose combined reservations remain within budget;
        // blocked workers retain their memory reservation even after close.
        QVERIFY(QFile::copy(fixture("formats/sample.jpg"), jpegPath));
        QPointer<MainWindow> transient = QVApplication::newWindow();
        const auto closeTransient = qScopeGuard([&]() {
            if (transient) transient->close();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        });
        QVERIFY(transient);
        auto *core = transient->findChild<QVImageCore *>();
        QVERIFY(core);
        destroyedObservers << core;

        // Guarantee that decode and preload jobs are still queued when the
        // owning window is destroyed. Unique paths avoid cache-hit shortcuts.
        transient->openFile(svgPath);
        QVERIFY(transient->getCurrentFileDetails().isLoadRequested);
        QVERIFY(!transient->getIsPixmapLoaded());
        core->requestCachingFile(jpegPath, QColorSpace());
        const auto watchers = core->findChildren<QFutureWatcherBase *>();
        QVERIFY2(!watchers.isEmpty(), "Preload watchers must belong to the image core");
        for (auto *watcher : watchers)
            destroyedObservers << watcher;
        transient->requestPopulateOpenWithMenu();
        transient->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(transient.isNull());
    }
    for (const auto &observer : destroyedObservers)
        QVERIFY2(observer.isNull(), "Closing must destroy image cores and child preload watchers");

    // Release the queued work only after all six windows have been destroyed.
    // Unlike normal cleanup, there is deliberately no waitForDone before close.
    unblockWorker.release();
    blocker.waitForFinished();
    QVERIFY(pool->waitForDone(20000));
    QCoreApplication::processEvents();
    QVERIFY2(waitForLoad("navigation/2.png"), "The surviving real window must remain usable after queued workers finish");
}

void IntegrationTests::reopeningExternallyChangedSameSizeImageShowsNewPixels()
{
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    const QString path = corpus.filePath("edited.bmp");
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(Qt::red);
    QVERIFY(image.save(path, "BMP"));
    QVERIFY(openPath(path));
    QCOMPARE(view->getLoadedPixmap().toImage().pixelColor(10, 10), QColor(Qt::red));
    QVERIFY(QThreadPool::globalInstance()->waitForDone(10000));
    QCoreApplication::processEvents();
    const qint64 oldSize = QFileInfo(path).size();
    const QDateTime oldModified = QFileInfo(path).lastModified();

    // An editor can overwrite an uncompressed image without changing its size.
    // Advance mtime explicitly so the case is deterministic on coarse filesystems.
    image.fill(Qt::blue);
    QVERIFY(image.save(path, "BMP"));
    QFile edited(path);
    QVERIFY(edited.open(QIODevice::ReadWrite));
    QVERIFY(edited.setFileTime(oldModified.addSecs(2), QFileDevice::FileModificationTime));
    edited.close();
    QCOMPARE(QFileInfo(path).size(), oldSize);
    QVERIFY(QFileInfo(path).lastModified() != oldModified);
    QVERIFY(openPath(path));
    QCOMPARE(view->getLoadedPixmap().toImage().pixelColor(10, 10), QColor(Qt::blue));
}

void IntegrationTests::rapidOpenShowsLastRequestedImage()
{
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    const QString first = corpus.filePath("first.svg");
    const QString last = corpus.filePath("last.bmp");
    const QString middle = corpus.filePath("middle.bmp");
    QVERIFY(writeVectorFixture(first));
    QImage green(32, 24, QImage::Format_RGB32);
    green.fill(Qt::green);
    QVERIFY(green.save(middle, "BMP"));
    QImage blue(32, 24, QImage::Format_RGB32);
    blue.fill(Qt::blue);
    QVERIFY(blue.save(last, "BMP"));
    auto *pool = QThreadPool::globalInstance();
    QVERIFY(pool->waitForDone(10000));
    const int originalThreads = pool->maxThreadCount();
    pool->setMaxThreadCount(1);
    QSemaphore started;
    QSemaphore release;
    auto blocker = QtConcurrent::run([&]() { started.release(); release.acquire(); });
    const auto restore = qScopeGuard([&]() {
        release.release();
        blocker.waitForFinished();
        pool->waitForDone(10000);
        pool->setMaxThreadCount(originalThreads);
    });
    QVERIFY(started.tryAcquire(1, 5000));
    window->openFile(first);
    QVERIFY(window->getCurrentFileDetails().isLoadRequested);
    QVERIFY(!window->getIsPixmapLoaded());
    window->openFile(middle);
    window->openFile(last);
    release.release();
    blocker.waitForFinished();
    QTRY_VERIFY_WITH_TIMEOUT(window->getIsPixmapLoaded(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(window->getCurrentFileDetails().fileInfo.absoluteFilePath(), last, 5000);
    QCOMPARE(view->getLoadedPixmap().toImage().pixelColor(10, 10), QColor(Qt::blue));
}

void IntegrationTests::directoryOpenSkipsNonImagesAndMetadata()
{
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    const QList<QPair<QString, QString>> copies = {
        {"1.PNG", "navigation/1.png"}, {"2.png", "navigation/2.png"},
        {"10.png", "navigation/10.png"}, {".hidden.png", "navigation/1.png"},
        {"._metadata.png", "navigation/1.png"}};
    for (const auto &entry : copies)
        QVERIFY(QFile::copy(fixture(entry.second), corpus.filePath(entry.first)));
    QFile text(corpus.filePath("readme.txt"));
    QVERIFY(text.open(QIODevice::WriteOnly));
    text.write("not an image");
    text.close();
    QVERIFY(QDir(corpus.path()).mkdir("subfolder"));
    QVERIFY(QFile::copy(fixture("navigation/1.png"), corpus.filePath("subfolder/0.png")));
    window->openFile(corpus.path());
    QTRY_VERIFY(window->getIsPixmapLoaded());
    QCOMPARE(window->getCurrentFileDetails().fileInfo.fileName(), QString("1.PNG"));
    QStringList names;
    for (const auto &entry : window->getCurrentFileDetails().folderFileInfoList)
        names << entry.fileName;
    QCOMPARE(names, QStringList({"1.PNG", "2.png", "10.png"}));
    window->nextFile();
    QTRY_COMPARE(window->getCurrentFileDetails().fileInfo.fileName(), QString("2.png"));
    QCOMPARE(view->getLoadedPixmap().toImage().pixelColor(10, 10), QColor(Qt::green));
    ScopedOptions showHidden({{"skiphidden", false}});
    names.clear();
    for (const auto &entry : window->getCurrentFileDetails().folderFileInfoList)
        names << entry.fileName;
    QVERIFY(names.contains(".hidden.png"));
    QVERIFY(!names.contains("._metadata.png"));
    QCOMPARE(names.size(), 4);
}

void IntegrationTests::emptyAndSingleImageDirectoriesRemainUsable()
{
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    QVERIFY(waitForLoad("navigation/1.png"));
    window->openFile(corpus.path());
    QVERIFY(!window->getIsPixmapLoaded());
    QVERIFY(window->getCurrentFileDetails().folderFileInfoList.isEmpty());
    QVERIFY(window->findChild<QPushButton *>("emptyOpenButton")->isVisible());
    for (auto operation : {&MainWindow::firstFile, &MainWindow::nextFile,
                           &MainWindow::previousFile, &MainWindow::lastFile})
        (window.data()->*operation)();
    QVERIFY(!window->getIsPixmapLoaded());
    const QString sole = corpus.filePath("only.png");
    QVERIFY(QFile::copy(fixture("navigation/2.png"), sole));
    window->openFile(corpus.path());
    QTRY_VERIFY(window->getIsPixmapLoaded());
    QCOMPARE(window->getCurrentFileDetails().folderFileInfoList.size(), 1);
    window->nextFile();
    window->previousFile();
    QCOMPARE(window->getCurrentFileDetails().fileInfo.absoluteFilePath(), sole);
    QCOMPARE(view->getLoadedPixmap().toImage().pixelColor(10, 10), QColor(Qt::green));
    QVERIFY(QFile::remove(sole));
    window->openFile(corpus.path());
    QVERIFY(!window->getIsPixmapLoaded());
    QVERIFY(window->getCurrentFileDetails().folderFileInfoList.isEmpty());
    window->nextFile();
    window->previousFile();
    QVERIFY(!window->getIsPixmapLoaded());
    QVERIFY(waitForLoad("navigation/10.png"));
}

void IntegrationTests::folderSorting_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<bool>("descending");
    QTest::newRow("modified_ascending") << 1 << false;
    QTest::newRow("modified_descending") << 1 << true;
    QTest::newRow("created_ascending") << 2 << false;
    QTest::newRow("created_descending") << 2 << true;
    QTest::newRow("size_ascending") << 3 << false;
    QTest::newRow("size_descending") << 3 << true;
}

void IntegrationTests::folderSorting()
{
    QFETCH(int, mode);
    QFETCH(bool, descending);
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    const QString low = corpus.filePath("low.bmp");
    const QString high = corpus.filePath("high.bmp");
    QImage small(12, 8, QImage::Format_RGB32);
    small.fill(Qt::red);
    QVERIFY(small.save(low, "BMP"));
    QTest::qWait(100);
    QImage large(24, 16, QImage::Format_RGB32);
    large.fill(Qt::blue);
    QVERIFY(large.save(high, "BMP"));
    QFile older(low);
    QVERIFY(older.open(QIODevice::ReadWrite));
    QVERIFY(older.setFileTime(QDateTime::currentDateTimeUtc().addSecs(-600), QFileDevice::FileModificationTime));
    older.close();
    QVERIFY(QFileInfo(low).size() < QFileInfo(high).size());
    QVERIFY(QFileInfo(low).lastModified() < QFileInfo(high).lastModified());
    QVERIFY(QFileInfo(low).birthTime() < QFileInfo(high).birthTime());
    ScopedOptions sorting({{"sortmode", mode}, {"sortdescending", descending}});
    window->openFile(corpus.path());
    QTRY_VERIFY(window->getIsPixmapLoaded());
    // Ascending means oldest/smallest first, descending newest/largest first.
    const QStringList expected = descending ? QStringList({"high.bmp", "low.bmp"})
                                            : QStringList({"low.bmp", "high.bmp"});
    QStringList actual;
    for (const auto &entry : window->getCurrentFileDetails().folderFileInfoList)
        actual << entry.fileName;
    QCOMPARE(actual, expected);
    QCOMPARE(window->getCurrentFileDetails().fileInfo.fileName(), expected.first());
}

void IntegrationTests::naturalSortChangeKeepsStatusAndLoopBoundaries()
{
    QVERIFY(waitForLoad("navigation/1.png"));
    ScopedOptions descending({{"sortdescending", true}, {"loopfoldersenabled", false}});
    QCOMPARE(window->getCurrentFileDetails().loadedIndexInFolder, 2);
    auto *position = window->findChild<QLabel *>("qingPosition");
    QVERIFY(position);
    QTRY_COMPARE_WITH_TIMEOUT(position->text(), QString("3 / 3"), 1000);
    window->nextFile();
    QCOMPARE(window->getCurrentFileDetails().fileInfo.fileName(), QString("1.png"));
    window->firstFile();
    QTRY_COMPARE(window->getCurrentFileDetails().fileInfo.fileName(), QString("10.png"));
    window->previousFile();
    QCOMPARE(window->getCurrentFileDetails().fileInfo.fileName(), QString("10.png"));
    { ScopedOptions loop({{"loopfoldersenabled", true}});
      window->previousFile();
      QTRY_COMPARE(window->getCurrentFileDetails().fileInfo.fileName(), QString("1.png")); }
}

void IntegrationTests::exifInfoMatchesUprightDimensions_data()
{
    QTest::addColumn<QString>("relative");
    QTest::newRow("JPEG_EXIF6") << QString("formats/oriented.jpg");
    QTest::newRow("HEIC_EXIF6") << QString("formats/oriented.heic");
}

void IntegrationTests::exifInfoMatchesUprightDimensions()
{
    QFETCH(QString, relative);
    QVERIFY(waitForLoad(relative));
    QCOMPARE(view->getLoadedPixmap().size(), QSize(64, 96));
    window->showFileInfo();
    auto *info = window->findChild<QVInfoDialog *>();
    QVERIFY(info && info->isVisible());
    auto *dimensions = info->findChild<QLabel *>("dimensionsLabel");
    auto *ratio = info->findChild<QLabel *>("ratioLabel");
    QVERIFY(dimensions && ratio);
    QCOMPARE(window->getCurrentFileDetails().baseImageSize, QSize(64, 96));
    QVERIFY2(dimensions->text().startsWith("64 x 96"), qPrintable(dimensions->text()));
    QCOMPARE(ratio->text(), QString("2:3"));
    info->close();
}

void IntegrationTests::informationMegapixelsUseNearestTenth_data()
{
    QTest::addColumn<QString>("relative");
    QTest::addColumn<double>("megapixels");
    QTest::newRow("small_image_rounds_to_zero") << QString("formats/sample.jpg") << 0.0;
    QTest::newRow("24MP_image") << QString("formats/large.jpg") << 24.0;
}

void IntegrationTests::informationMegapixelsUseNearestTenth()
{
    QFETCH(QString, relative);
    QFETCH(double, megapixels);
    QVERIFY(waitForLoad(relative));
    window->showFileInfo();
    auto *info = window->findChild<QVInfoDialog *>();
    QVERIFY(info && info->isVisible());
    auto *dimensions = info->findChild<QLabel *>("dimensionsLabel");
    QVERIFY(dimensions);
    const auto number = QRegularExpression("\\(([0-9.]+) MP\\)").match(dimensions->text());
    QVERIFY2(number.hasMatch(), qPrintable(dimensions->text()));
    QCOMPARE(number.captured(1).toDouble(), megapixels);
    info->close();
}

void IntegrationTests::gifRotationPersistsAtActualSize_data()
{
    QTest::addColumn<bool>("scaling");
    QTest::newRow("resampling_enabled") << true;
    QTest::newRow("resampling_disabled") << false;
}

void IntegrationTests::gifRotationPersistsAtActualSize()
{
    QFETCH(bool, scaling);
    ScopedOptions options({{"scalingenabled", scaling}});
    QVERIFY(waitForLoad("formats/animated.gif"));
    window->pause();
    const QImage expected = view->getLoadedMovie().currentImage().transformed(QTransform().rotate(90));
    window->rotateRight();
    window->originalSize();
    QTest::qWait(100);
    QCOMPARE(displayedItem()->pixmap().size(), QSize(64, 96));
    QCOMPARE(displayedItem()->pixmap().toImage(), expected.convertToFormat(displayedItem()->pixmap().toImage().format()));
    QCOMPARE(displayedRect().size().toSize(), QSize(64, 96));
    window->pause();
    QSignalSpy frames(&view->getLoadedMovie(), &QMovie::frameChanged);
    QTRY_VERIFY_WITH_TIMEOUT(frames.count() >= 2, 1000);
    QCOMPARE(displayedItem()->pixmap().size(), QSize(64, 96));
    window->pause();
}

void IntegrationTests::copyContainsDisplayedImageAndLocalPath()
{
    auto *saved = new QMimeData();
    if (const auto *old = QApplication::clipboard()->mimeData())
        for (const auto &format : old->formats()) saved->setData(format, old->data(format));
    const auto restore = qScopeGuard([saved]() { QApplication::clipboard()->setMimeData(saved); });
    QVERIFY(waitForLoad("formats/sample.png"));
    window->rotateRight();
    window->copy();
    const auto *copied = QApplication::clipboard()->mimeData();
    QVERIFY(copied && copied->hasImage() && copied->hasUrls());
    QCOMPARE(copied->urls(), QList<QUrl>({QUrl::fromLocalFile(fixture("formats/sample.png"))}));
    QCOMPARE(qvariant_cast<QImage>(copied->imageData()), view->getLoadedPixmap().toImage());
    // Paste in a distinct window opens the local original rather than a remote URL.
    QPointer<MainWindow> other = QVApplication::newWindow();
    const auto close = qScopeGuard([&]() { if (other) other->close(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); });
    QVERIFY(other);
    other->paste();
    QTRY_VERIFY(other->getIsPixmapLoaded());
    QCOMPARE(other->getCurrentFileDetails().fileInfo.absoluteFilePath(), fixture("formats/sample.png"));
}

void IntegrationTests::renameTemporaryImage_data()
{
    QTest::addColumn<QString>("relative");
    QTest::newRow("static_PNG") << QString("formats/sample.png");
    QTest::newRow("animated_GIF") << QString("formats/animated.gif");
}

void IntegrationTests::renameTemporaryImage()
{
    QFETCH(QString, relative);
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    const QString extension = QFileInfo(relative).suffix();
    const QString original = corpus.filePath("before." + extension);
    const QString renamed = corpus.filePath(QString::fromUtf8("中文 重命名.") + extension);
    QVERIFY(QFile::copy(fixture(relative), original));
    QFile bytes(original);
    QVERIFY(bytes.open(QIODevice::ReadOnly));
    const QByteArray originalBytes = bytes.readAll();
    bytes.close();
    QVERIFY(openPath(original));
    window->rename();
    QPointer<QVRenameDialog> dialog = window->findChild<QVRenameDialog *>();
    QVERIFY(dialog);
    const auto reject = qScopeGuard([&]() { if (dialog) dialog->reject(); });
    QVERIFY(QTest::qWaitForWindowExposed(dialog));
    dialog->setTextValue(QFileInfo(renamed).fileName());
    auto *buttons = dialog->findChild<QDialogButtonBox *>();
    QVERIFY(buttons && buttons->button(QDialogButtonBox::Ok));
    QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton);
    QTRY_COMPARE(window->getCurrentFileDetails().fileInfo.absoluteFilePath(), renamed);
    QTRY_VERIFY(window->getIsPixmapLoaded());
    QVERIFY(!QFile::exists(original));
    QFile result(renamed);
    QVERIFY(result.open(QIODevice::ReadOnly));
    QCOMPARE(result.readAll(), originalBytes);
    result.close();
    QCOMPARE(window->findChild<QLabel *>("qingFilename")->toolTip(), QFileInfo(renamed).fileName());
    if (extension == "gif")
    {
        QVERIFY(window->getCurrentFileDetails().isMovieLoaded);
        QSignalSpy frames(&view->getLoadedMovie(), &QMovie::frameChanged);
        QTRY_VERIFY_WITH_TIMEOUT(frames.count() >= 2, 1000);
    }
    window->rename();
    QPointer<QVRenameDialog> canceled = window->findChild<QVRenameDialog *>();
    QVERIFY(canceled);
    canceled->setTextValue("canceled." + extension);
    canceled->reject();
    QVERIFY(QFile::exists(renamed));
    QVERIFY(!QFile::exists(corpus.filePath("canceled." + extension)));
}

void IntegrationTests::renameCollisionPreservesImageAndAnimation_data()
{
    QTest::addColumn<QString>("relative");
    QTest::newRow("static_PNG") << QString("formats/sample.png");
    QTest::newRow("animated_GIF") << QString("formats/animated.gif");
}

void IntegrationTests::renameCollisionPreservesImageAndAnimation()
{
    QFETCH(QString, relative);
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    const QString path = corpus.filePath("original." + QFileInfo(relative).suffix());
    const QString collision = corpus.filePath("already-exists.png");
    QVERIFY(QFile::copy(fixture(relative), path));
    QVERIFY(QFile::copy(fixture("navigation/2.png"), collision));
    QVERIFY(openPath(path));
    const QImage before = view->getLoadedPixmap().toImage();
    window->rename();
    QPointer<QVRenameDialog> dialog = window->findChild<QVRenameDialog *>();
    QVERIFY(dialog);
    const auto close = qScopeGuard([&]() { if (dialog) dialog->reject(); });
    QVERIFY(QTest::qWaitForWindowExposed(dialog));
    dialog->setTextValue(QFileInfo(collision).fileName());
    bool reported = false;
    QTimer dismissError;
    connect(&dismissError, &QTimer::timeout, window, [this, &reported]() {
        for (auto *box : window->findChildren<QMessageBox *>())
            if (box->icon() == QMessageBox::Critical)
            {
                reported = true;
                box->done(QMessageBox::Ok);
            }
    });
    dismissError.start(30);
    auto *buttons = dialog->findChild<QDialogButtonBox *>();
    QVERIFY(buttons && buttons->button(QDialogButtonBox::Ok));
    QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(reported, 1000);
    dismissError.stop();
    QVERIFY(QFile::exists(path));
    QFile unaffected(collision);
    QFile expected(fixture("navigation/2.png"));
    QVERIFY(unaffected.open(QIODevice::ReadOnly) && expected.open(QIODevice::ReadOnly));
    QCOMPARE(unaffected.readAll(), expected.readAll());
    QTRY_VERIFY(window->getIsPixmapLoaded());
    QCOMPARE(window->getCurrentFileDetails().fileInfo.absoluteFilePath(), path);
    if (relative.endsWith("gif"))
    {
        QVERIFY(window->getCurrentFileDetails().isMovieLoaded);
        QSignalSpy frames(&view->getLoadedMovie(), &QMovie::frameChanged);
        QTRY_VERIFY_WITH_TIMEOUT(frames.count() >= 2, 1000);
    }
    else
        QCOMPARE(view->getLoadedPixmap().toImage(), before);
}

QMessageBox *IntegrationTests::visibleConfirmation() const
{
    for (auto *box : window->findChildren<QMessageBox *>())
        if (box->isVisible() && box->icon() == QMessageBox::Question)
            return box;
    return nullptr;
}

void IntegrationTests::trashCancelDeleteAndUndoTemporaryImage()
{
    ScopedOptions options({{"askdelete", true}, {"afterdelete", 1}});
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    const QString path = corpus.filePath("qing-audit-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".png");
    const QString trashPath = QDir::home().filePath(".Trash/" + QFileInfo(path).fileName());
    const auto recover = qScopeGuard([&]() { if (QFile::exists(trashPath) && !QFile::exists(path)) QFile::rename(trashPath, path); });
    QVERIFY(QFile::copy(fixture("navigation/1.png"), path));
    QVERIFY(openPath(path));
    const QImage before = view->getLoadedPixmap().toImage();
    window->askDeleteFile(false);
    auto *confirmation = visibleConfirmation();
    QVERIFY(confirmation && confirmation->button(QMessageBox::No));
    QTest::mouseClick(confirmation->button(QMessageBox::No), Qt::LeftButton);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(QFile::exists(path));
    QCOMPARE(view->getLoadedPixmap().toImage(), before);
    window->askDeleteFile(false);
    confirmation = visibleConfirmation();
    QVERIFY(confirmation && confirmation->button(QMessageBox::Yes));
    QTest::mouseClick(confirmation->button(QMessageBox::Yes), Qt::LeftButton);
    QTRY_VERIFY(!QFile::exists(path));
    QVERIFY(!window->getIsPixmapLoaded());
    window->undoDelete();
    QTRY_VERIFY(QFile::exists(path));
    QTRY_VERIFY(window->getIsPixmapLoaded());
    QCOMPARE(view->getLoadedPixmap().toImage(), before);
    QCOMPARE(window->getCurrentFileDetails().fileInfo.absoluteFilePath(), path);
}

void IntegrationTests::undoTrashCollisionRetainsRecovery()
{
    ScopedOptions options({{"afterdelete", 1}});
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    const QString path = corpus.filePath("qing-audit-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".png");
    const QString trashPath = QDir::home().filePath(".Trash/" + QFileInfo(path).fileName());
    const auto recover = qScopeGuard([&]() {
        if (QFile::exists(trashPath)) { QFile::remove(path); QFile::rename(trashPath, path); }
    });
    QVERIFY(QFile::copy(fixture("navigation/1.png"), path));
    QVERIFY(openPath(path));
    window->deleteFile(false);
    QVERIFY(!QFile::exists(path));
    QVERIFY(QFile::copy(fixture("navigation/2.png"), path));
    QTimer dismissError;
    connect(&dismissError, &QTimer::timeout, window, [this]() {
        for (auto *box : window->findChildren<QMessageBox *>())
            if (box->icon() == QMessageBox::Critical) box->done(QMessageBox::Ok);
    });
    dismissError.start(30);
    window->undoDelete();
    QTest::qWait(150);
    QFile collision(path);
    QVERIFY(collision.open(QIODevice::ReadOnly));
    const QByteArray untouched = collision.readAll();
    collision.close();
    QFile green(fixture("navigation/2.png"));
    QVERIFY(green.open(QIODevice::ReadOnly));
    QCOMPARE(untouched, green.readAll());
    QVERIFY2(!window->getIsPixmapLoaded(), "Failed Undo must not open the unrelated file now occupying the original path");
    QVERIFY(QFile::remove(path));
    window->undoDelete();
    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(path), 1000);
    QTRY_VERIFY(window->getIsPixmapLoaded());
    QCOMPARE(view->getLoadedPixmap().toImage().pixelColor(10, 10), QColor(Qt::red));
}

void IntegrationTests::staleDeleteConfirmationDoesNotDeleteAnotherImage()
{
    ScopedOptions options({{"askdelete", true}, {"afterdelete", 1}});
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    const QString prefix = "qing-audit-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString first = corpus.filePath(prefix + "-A.png");
    const QString second = corpus.filePath(prefix + "-B.png");
    const auto recover = qScopeGuard([&]() {
        for (const auto &path : {first, second})
        {
            const QString trash = QDir::home().filePath(".Trash/" + QFileInfo(path).fileName());
            if (QFile::exists(trash) && !QFile::exists(path)) QFile::rename(trash, path);
        }
    });
    QVERIFY(QFile::copy(fixture("navigation/1.png"), first));
    QVERIFY(QFile::copy(fixture("navigation/2.png"), second));
    QVERIFY(openPath(first));
    window->askDeleteFile(false);
    auto *confirmation = visibleConfirmation();
    QVERIFY(confirmation && confirmation->text().contains(QFileInfo(first).fileName()));
    QVERIFY(openPath(second));
    QTest::mouseClick(confirmation->button(QMessageBox::Yes), Qt::LeftButton);
    QTest::qWait(150);
    QVERIFY(QFile::exists(first));
    QVERIFY2(QFile::exists(second), "Confirming deletion of A must never delete the newly loaded B");
    QVERIFY(window->getIsPixmapLoaded());
    QCOMPARE(window->getCurrentFileDetails().fileInfo.absoluteFilePath(), second);
    QCOMPARE(view->getLoadedPixmap().toImage().pixelColor(10, 10), QColor(Qt::green));
}

void IntegrationTests::slideshowStopsResumesAndHonorsBoundary()
{
    ScopedOptions options({{"slideshowtimer", 0.2}, {"slideshowreversed", false}, {"loopfoldersenabled", false}});
    QVERIFY(waitForLoad("navigation/1.png"));
    QSignalSpy changed(view, &QVGraphicsView::fileChanged);
    window->toggleSlideshow();
    QTRY_VERIFY_WITH_TIMEOUT(changed.count() >= 1, 1000);
    window->cancelSlideshow();
    const QString stopped = window->getCurrentFileDetails().fileInfo.absoluteFilePath();
    const int count = changed.count();
    QTest::qWait(650);
    QCOMPARE(window->getCurrentFileDetails().fileInfo.absoluteFilePath(), stopped);
    QCOMPARE(changed.count(), count);
    window->toggleSlideshow();
    QTRY_COMPARE_WITH_TIMEOUT(window->getCurrentFileDetails().fileInfo.fileName(), QString("10.png"), 1500);
    QTest::qWait(500);
    const int atBoundary = changed.count();
    QTest::qWait(500);
    QCOMPARE(changed.count(), atBoundary);
    { ScopedOptions reverse({{"slideshowreversed", true}});
      window->toggleSlideshow();
      QTRY_COMPARE_WITH_TIMEOUT(window->getCurrentFileDetails().fileInfo.fileName(), QString("1.png"), 1500);
      window->cancelSlideshow(); }
    QVERIFY(openPath(fixture("navigation/2.png")));
    window->toggleSlideshow();
    QVERIFY(openPath(fixture("navigation/1.png")));
    QTest::qWait(650);
    QCOMPARE(window->getCurrentFileDetails().fileInfo.fileName(), QString("1.png"));
}

void IntegrationTests::settingsCancelAndOkPropagateToNewWindow()
{
    ScopedOptions options({{"wheelzoompercent", 5}});
    QPointer<QVOptionsDialog> dialog = new QVOptionsDialog(window);
    const auto closeDialog = qScopeGuard([&]() { if (dialog) dialog->reject(); });
    dialog->show();
    QVERIFY(QTest::qWaitForWindowExposed(dialog));
    auto *spin = dialog->findChild<QSpinBox *>("wheelZoomPercentSpinBox");
    auto *buttons = dialog->findChild<QDialogButtonBox *>("buttonBox");
    QVERIFY(spin && buttons);
    spin->setValue(11);
    QTest::mouseClick(buttons->button(QDialogButtonBox::Cancel), Qt::LeftButton);
    QCOMPARE(qvApp->getSettingsManager().getInteger("wheelzoompercent"), 5);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    dialog = new QVOptionsDialog(window);
    dialog->show();
    QVERIFY(QTest::qWaitForWindowExposed(dialog));
    spin = dialog->findChild<QSpinBox *>("wheelZoomPercentSpinBox");
    buttons = dialog->findChild<QDialogButtonBox *>("buttonBox");
    QCOMPARE(spin->value(), 5);
    spin->setValue(7);
    QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton);
    QCOMPARE(qvApp->getSettingsManager().getInteger("wheelzoompercent"), 7);
    QCOMPARE(QSettings().value("options/wheelzoompercent").toInt(), 7);
    QPointer<MainWindow> other = QVApplication::newWindow();
    const auto closeOther = qScopeGuard([&]() { if (other) other->close(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); });
    other->openFile(fixture("navigation/1.png"));
    QTRY_VERIFY(other->getIsPixmapLoaded());
    auto *otherView = other->findChild<QVGraphicsView *>();
    QVERIFY(otherView);
    other->originalSize();
    const QPoint pos = otherView->viewport()->rect().center();
    QWheelEvent wheel(pos, otherView->viewport()->mapToGlobal(pos), QPoint(), QPoint(0, 120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(otherView->viewport(), &wheel);
    const auto items = otherView->scene()->items();
    bool measured = false;
    for (auto *item : items)
        if (auto *pixmap = qgraphicsitem_cast<QGraphicsPixmapItem *>(item))
        {
            const qreal width = otherView->transform().mapRect(pixmap->sceneBoundingRect()).width();
            QVERIFY(qAbs(width / 96 - 1.07) < 0.000001);
            measured = true;
        }
    QVERIFY(measured);
}

void IntegrationTests::openingEmptyWindowKeepsOtherWindowsOpenWithMenuEnabled()
{
    QVERIFY(waitForLoad("formats/sample.png"));
    const auto originalMenus = qvApp->getActionManager().getAllClonesOfMenu("openwith", window);
    QVERIFY(!originalMenus.isEmpty());
    for (auto *menu : originalMenus) QVERIFY(menu->isEnabled());
    QPointer<MainWindow> other = QVApplication::newWindow();
    const auto close = qScopeGuard([&]() { if (other) other->close(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); });
    QVERIFY(other && !other->getIsPixmapLoaded());
    for (auto *menu : originalMenus)
        QVERIFY2(menu->isEnabled(), "Creating an empty window must not disable Open With in an existing image window");
    const auto emptyMenus = qvApp->getActionManager().getAllClonesOfMenu("openwith", other);
    QVERIFY(!emptyMenus.isEmpty());
    for (auto *menu : emptyMenus) QVERIFY(!menu->isEnabled());
    other->close();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(other.isNull());
    QVERIFY(window->getIsPixmapLoaded());
    window->zoomIn();
    QVERIFY(!displayedRect().isEmpty());
}

void IntegrationTests::missingFileReloadAndExternalRemovalRecover()
{
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    const QString missing = corpus.filePath("missing.jpg");
    window->openFile(missing);
    QTRY_VERIFY_WITH_TIMEOUT(window->getCurrentFileDetails().errorData.hasError, 5000);
    QVERIFY(window->isVisible() && !window->getIsPixmapLoaded());
    const QString path = corpus.filePath("reloaded.bmp");
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(Qt::red);
    QVERIFY(image.save(path, "BMP"));
    QVERIFY(openPath(path));
    image.fill(Qt::blue);
    QVERIFY(image.save(path, "BMP"));
    QSignalSpy changed(view, &QVGraphicsView::fileChanged);
    window->reloadFile();
    QTRY_VERIFY_WITH_TIMEOUT(changed.count() > 0, 5000);
    QCOMPARE(view->getLoadedPixmap().toImage().pixelColor(10, 10), QColor(Qt::blue));
    QVERIFY(QFile::remove(path));
    window->reloadFile();
    QTRY_VERIFY_WITH_TIMEOUT(window->getCurrentFileDetails().errorData.hasError, 5000);
    QVERIFY(!window->getIsPixmapLoaded());
    QVERIFY(waitForLoad("navigation/2.png"));
}

void IntegrationTests::openFolderMenuLoadsChosenDirectory()
{
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    QVERIFY(QFile::copy(fixture("navigation/1.png"), corpus.filePath("1.png")));
    QVERIFY(QFile::copy(fixture("navigation/2.png"), corpus.filePath("2.png")));
    QSettings settings;
    const QVariant oldDirectory = settings.value("recents/lastFileDialogDir");
    const auto restoreDirectory = qScopeGuard([&]() {
        if (oldDirectory.isValid()) settings.setValue("recents/lastFileDialogDir", oldDirectory);
        else settings.remove("recents/lastFileDialogDir");
    });
    settings.setValue("recents/lastFileDialogDir", corpus.path());
    const auto actions = qvApp->getActionManager().getAllClonesOfAction("openfolder", window);
    QVERIFY(!actions.isEmpty());
    auto *action = actions.first();
    QVERIFY(action->isEnabled());
    QVERIFY(action->shortcuts().contains(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O)));
    action->trigger();
    QPointer<QFileDialog> dialog = window->findChild<QFileDialog *>();
    QVERIFY(dialog);
    const auto close = qScopeGuard([&]() { if (dialog) dialog->reject(); });
    QCOMPARE(dialog->fileMode(), QFileDialog::Directory);
    QVERIFY(dialog->testOption(QFileDialog::ShowDirsOnly));
    dialog->setDirectory(corpus.path());
    dialog->selectFile(corpus.path());
    // Invoke the actual dialog accept slot; no synthetic filesSelected signal.
    QVERIFY(QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection));
    QTRY_VERIFY_WITH_TIMEOUT(window->getIsPixmapLoaded(), 5000);
    QCOMPARE(window->getCurrentFileDetails().fileInfo.canonicalFilePath(), QFileInfo(corpus.filePath("1.png")).canonicalFilePath());
    QCOMPARE(window->getCurrentFileDetails().folderFileInfoList.size(), 2);
    QCOMPARE(QFileInfo(QSettings().value("recents/lastFileDialogDir").toString()).canonicalFilePath(),
             QFileInfo(corpus.path()).canonicalFilePath());
}

void IntegrationTests::openFolderCancelKeepsCurrentImage()
{
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    QSettings settings;
    const QVariant oldDirectory = settings.value("recents/lastFileDialogDir");
    const auto restoreDirectory = qScopeGuard([&]() {
        if (oldDirectory.isValid()) settings.setValue("recents/lastFileDialogDir", oldDirectory);
        else settings.remove("recents/lastFileDialogDir");
    });
    settings.setValue("recents/lastFileDialogDir", corpus.path());
    QVERIFY(waitForLoad("navigation/2.png"));
    const auto before = view->getLoadedPixmap().toImage();
    const QString path = window->getCurrentFileDetails().fileInfo.absoluteFilePath();
    const auto actions = qvApp->getActionManager().getAllClonesOfAction("openfolder", window);
    QVERIFY(!actions.isEmpty());
    actions.first()->trigger();
    QPointer<QFileDialog> dialog = window->findChild<QFileDialog *>();
    QVERIFY(dialog);
    dialog->reject();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(dialog.isNull());
    QCOMPARE(window->getCurrentFileDetails().fileInfo.absoluteFilePath(), path);
    QCOMPARE(view->getLoadedPixmap().toImage(), before);
}

void IntegrationTests::colorSettingsDuringQueuedOpenKeepRequestedImage_data()
{
    QTest::addColumn<bool>("hasPreviousImage");
    QTest::addColumn<int>("conversion");
    QTest::newRow("first_open_sRGB") << false << 2;
    QTest::newRow("first_open_P3") << false << 3;
    QTest::newRow("replace_image_sRGB") << true << 2;
    QTest::newRow("replace_image_P3") << true << 3;
}

void IntegrationTests::colorSettingsDuringQueuedOpenKeepRequestedImage()
{
    QFETCH(bool, hasPreviousImage);
    QFETCH(int, conversion);
    ScopedOptions options({{"preloadingmode", 0}, {"colorspaceconversion", 0}});
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    QVERIFY(QDir(corpus.path()).mkdir("A"));
    QVERIFY(QDir(corpus.path()).mkdir("B"));
    const QString old = corpus.filePath("A/old.bmp");
    const QString requested = corpus.filePath("B/requested.bmp");
    QImage red(12, 8, QImage::Format_RGB32);
    red.fill(Qt::red);
    QVERIFY(red.save(old, "BMP"));
    QImage blue(24, 16, QImage::Format_RGB32);
    blue.fill(QColor(20, 60, 230));
    QVERIFY(blue.save(requested, "BMP"));
    if (hasPreviousImage) QVERIFY(openPath(old));
    auto *pool = QThreadPool::globalInstance();
    QVERIFY(pool->waitForDone(10000));
    const int originalThreads = pool->maxThreadCount();
    pool->setMaxThreadCount(1);
    QSemaphore started;
    QSemaphore release;
    auto blocker = QtConcurrent::run([&]() { started.release(); release.acquire(); });
    const auto restore = qScopeGuard([&]() {
        release.release();
        blocker.waitForFinished();
        pool->waitForDone(10000);
        pool->setMaxThreadCount(originalThreads);
    });
    QVERIFY(started.tryAcquire(1, 5000));
    window->openFile(requested);
    ScopedOptions changedColor({{"colorspaceconversion", conversion}});
    release.release();
    blocker.waitForFinished();
    QTRY_COMPARE_WITH_TIMEOUT(window->getCurrentFileDetails().fileInfo.absoluteFilePath(), requested, 5000);
    QTRY_VERIFY(window->getIsPixmapLoaded());
    QCOMPARE(view->getLoadedPixmap().size(), blue.size());
    QImage expected(requested);
    expected.setColorSpace(QColorSpace::SRgb);
    const QColorSpace target(conversion == 2 ? QColorSpace::SRgb : QColorSpace::DisplayP3);
    expected.convertToColorSpace(target);
    const QImage actual = view->getLoadedPixmap().toImage();
    QCOMPARE(actual.pixelColor(10, 10), expected.pixelColor(10, 10));
    QCOMPARE(actual.colorSpace(), target);
}

static bool writeBmpHeader(const QString &path, int width, int height)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << quint16(0x4d42) << quint32(54) << quint32(0) << quint32(54)
           << quint32(40) << qint32(width) << qint32(height) << quint16(1)
           << quint16(24) << quint32(0) << quint32(0) << qint32(0)
           << qint32(0) << quint32(0) << quint32(0);
    return stream.status() == QDataStream::Ok;
}

static bool writeOversizedPngHeader(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    if (file.write(QByteArray::fromHex("89504e470d0a1a0a")) != 8) return false;
    auto writeChunk = [&](const QByteArray &type, const QByteArray &payload) {
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::BigEndian);
        stream << quint32(payload.size());
        stream.writeRawData(type.constData(), type.size());
        stream.writeRawData(payload.constData(), payload.size());
        quint32 crc = 0xffffffff;
        for (unsigned char byte : type + payload)
        {
            crc ^= byte;
            for (int bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
        }
        stream << quint32(~crc);
        return stream.status() == QDataStream::Ok;
    };
    QByteArray header;
    QDataStream dimensions(&header, QIODevice::WriteOnly);
    dimensions.setByteOrder(QDataStream::BigEndian);
    dimensions << quint32(30000) << quint32(30000) << quint8(8) << quint8(2)
               << quint8(0) << quint8(0) << quint8(0);
    // A valid IHDR plus IDAT lets Qt inspect dimensions without decoding.
    // The intentionally tiny payload is never used as a foreground image.
    return writeChunk("IHDR", header) &&
           writeChunk("IDAT", qCompress(QByteArray(4, '\0')).mid(4)) &&
           writeChunk("IEND", {});
}

void IntegrationTests::preloadingSkipsOversizedInvalidAndExpensiveNeighbors()
{
    ScopedOptions options({{"preloadingmode", 1}});
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    auto *core = window->findChild<QVImageCore *>();
    QVERIFY(core);
    const QString oversized = corpus.filePath("oversized.png");
    const QString empty = corpus.filePath("empty.bmp");
    const QString vector = corpus.filePath("vector.svg");
    const QString animation = corpus.filePath("animation.gif");
    const QString eligible = corpus.filePath("eligible.jpg");
    QVERIFY(writeOversizedPngHeader(oversized));
    QVERIFY(writeBmpHeader(empty, 0, 0));
    QVERIFY(writeVectorFixture(vector));
    QVERIFY(QFile::copy(fixture("formats/animated.gif"), animation));
    QVERIFY(QFile::copy(fixture("formats/sample.jpg"), eligible));
    QImageReader metadata(oversized);
    QCOMPARE(metadata.size(), QSize(30000, 30000));
    // These are neighbor preload requests, not user-requested foreground opens.
    // Headers predict 3.6 GB of decoded memory; no huge allocation is attempted.
    for (const auto &path : {oversized, empty, vector, animation})
        core->requestCachingFile(path, QColorSpace());
    QVERIFY(core->findChildren<QFutureWatcherBase *>().isEmpty());
    core->requestCachingFile(eligible, QColorSpace());
    QVERIFY2(!core->findChildren<QFutureWatcherBase *>().isEmpty(), "A valid small neighbor must still be eligible for preload");
    QVERIFY(QThreadPool::globalInstance()->waitForDone(10000));
    QCoreApplication::processEvents();
    QVERIFY(openPath(animation));
    QVERIFY(window->getCurrentFileDetails().isMovieLoaded);
    QVERIFY(openPath(vector));
    QCOMPARE(window->getCurrentFileDetails().baseImageSize, QSize(96, 64));
}

void IntegrationTests::preloadCompletionAfterCloseDoesNotRestoreOldCache()
{
    ScopedOptions options({{"preloadingmode", 1}});
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    const QString path = corpus.filePath("changed-after-close.bmp");
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(Qt::red);
    QVERIFY(image.save(path, "BMP"));
    const QDateTime modified = QFileInfo(path).lastModified();
    auto *core = window->findChild<QVImageCore *>();
    QVERIFY(core);
    core->requestCachingFile(path, QColorSpace());
    const auto watchers = core->findChildren<QFutureWatcherBase *>();
    QVERIFY(!watchers.isEmpty());
    // Wait for decoding without pumping the UI queue, leaving its finished
    // callback pending until after closeImage invalidates the old generation.
    QVERIFY(QThreadPool::globalInstance()->waitForDone(10000));
    QVERIFY(watchers.first()->isFinished());
    core->closeImage();
    image.fill(Qt::blue);
    QVERIFY(image.save(path, "BMP"));
    QFile edited(path);
    QVERIFY(edited.open(QIODevice::ReadWrite));
    QVERIFY(edited.setFileTime(modified, QFileDevice::FileModificationTime));
    edited.close();
    QCoreApplication::processEvents();
    QVERIFY(openPath(path));
    QCOMPARE(view->getLoadedPixmap().toImage().pixelColor(10, 10), QColor(Qt::blue));
}

void IntegrationTests::mirrorAndFlipChangeVisibleQuadrantsAndRestore()
{
    QVERIFY(waitForLoad("formats/sample.png"));
    window->originalSize();
    QTest::qWait(100);
    auto quadrants = [&]() {
        const QRect bounds = view->mapFromScene(displayedItem()->sceneBoundingRect()).boundingRect();
        const QPixmap screenshot = view->viewport()->grab();
        const QImage pixels = screenshot.toImage();
        const qreal ratio = screenshot.devicePixelRatioF();
        QList<QColor> colors;
        for (const auto &fraction : {QPointF(0.25, 0.25), QPointF(0.75, 0.25),
                                    QPointF(0.25, 0.75), QPointF(0.75, 0.75)})
        {
            const QPoint point(qRound((bounds.x() + bounds.width() * fraction.x()) * ratio),
                               qRound((bounds.y() + bounds.height() * fraction.y()) * ratio));
            colors << (pixels.rect().contains(point) ? pixels.pixelColor(point) : QColor());
        }
        return colors;
    };
    const QList<QColor> original = {Qt::red, Qt::green, Qt::blue, Qt::yellow};
    QCOMPARE(quadrants(), original);
    auto trigger = [&](const QString &key) {
        const auto actions = qvApp->getActionManager().getAllClonesOfAction(key, window);
        if (actions.isEmpty() || !actions.first()->isEnabled()) return false;
        actions.first()->trigger();
        QTest::qWait(100);
        return true;
    };
    QVERIFY(trigger("mirror"));
    QCOMPARE(quadrants(), QList<QColor>({Qt::green, Qt::red, Qt::yellow, Qt::blue}));
    QVERIFY(trigger("mirror"));
    QCOMPARE(quadrants(), original);
    QVERIFY(trigger("flip"));
    QCOMPARE(quadrants(), QList<QColor>({Qt::blue, Qt::yellow, Qt::red, Qt::green}));
    QVERIFY(trigger("flip"));
    QCOMPARE(quadrants(), original);
}

void IntegrationTests::recentMenuOpensTemporaryImage()
{
    ScopedOptions options({{"saverecents", true}});
    QTemporaryDir corpus;
    QVERIFY(corpus.isValid());
    const QString first = corpus.filePath(QString::fromUtf8("recent 中文A.png"));
    const QString second = corpus.filePath("recent-B.png");
    QVERIFY(QFile::copy(fixture("navigation/1.png"), first));
    QVERIFY(QFile::copy(fixture("navigation/2.png"), second));
    QVERIFY(openPath(first));
    QVERIFY(openPath(second));
    int index = -1;
    const auto recents = qvApp->getActionManager().getRecentsList();
    for (int i = 0; i < recents.size(); ++i)
        if (recents[i].filePath == first) index = i;
    QVERIFY(index >= 0);
    const auto actions = qvApp->getActionManager().getAllClonesOfAction("recent" + QString::number(index), window);
    QVERIFY(!actions.isEmpty() && actions.first()->isEnabled());
    QVERIFY(actions.first()->text().contains(QFileInfo(first).fileName()));
    const auto initialWidgets = QApplication::topLevelWidgets();
    const auto closeAdditional = qScopeGuard([&]() {
        for (auto *widget : QApplication::topLevelWidgets())
            if (!initialWidgets.contains(widget))
                if (auto *extra = qobject_cast<MainWindow *>(widget)) extra->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    });
    actions.first()->trigger();
    QPointer<MainWindow> opened;
    QVERIFY(QTest::qWaitFor([&]() {
        for (auto *widget : QApplication::topLevelWidgets())
            if (auto *candidate = qobject_cast<MainWindow *>(widget))
                if (candidate->getIsPixmapLoaded() && candidate->getCurrentFileDetails().fileInfo.absoluteFilePath() == first)
                {
                    opened = candidate;
                    return true;
                }
        return false;
    }, 5000));
    QVERIFY(opened);
    auto *openedView = opened->findChild<QVGraphicsView *>();
    QVERIFY(openedView);
    QCOMPARE(openedView->getLoadedPixmap().toImage().pixelColor(10, 10), QColor(Qt::red));
}

int main(int argc, char *argv[])
{
    QTemporaryDir settingsDir;
    if (!settingsDir.isValid())
        return 2;
    QCoreApplication::setOrganizationName("QingViewIntegration");
    QCoreApplication::setApplicationName("QingViewIntegration");
    QCoreApplication::setApplicationVersion("0.1.2");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    {
        QSettings settings;
        settings.setFallbacksEnabled(false);
        settings.setValue("firstlaunch", true);
        settings.setValue("options/updatenotifications", false);
        settings.setValue("options/quitonlastwindow", false);
        settings.setValue("options/windowresizemode", 0);
        settings.setValue("options/sortmode", 0);
        settings.setValue("options/sortdescending", false);
        settings.setValue("options/colorspaceconversion", 0);
        settings.sync();
    }
    QVApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    IntegrationTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_integration.moc"
