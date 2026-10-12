#include "qvapplication.h"

#include <QCommandLineParser>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSaveFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QTimer>

#include <functional>
#include <stdexcept>
#include <sys/resource.h>
#ifdef Q_OS_MACOS
#include <mach/mach.h>
#include <malloc/malloc.h>
#endif

// This is an opt-in diagnostic executable, separate from the application.
// It uses real windows, public application APIs, temporary files and settings.
// No cursor warping, synthetic mouse input, decoder replacement or private
// cache access is used. The sampled peak is a lower bound at 100 ms intervals.
namespace {
struct ResourceReading
{
    quint64 footprint = 0;
    quint64 resident = 0;
    quint64 maxResident = 0;
    double cpuSeconds = 0;
    bool memorySupported = false;
};

ResourceReading readResources()
{
    ResourceReading value;
    struct rusage usage = {};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        value.cpuSeconds = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6
            + usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6;
#ifdef Q_OS_MACOS
        value.maxResident = static_cast<quint64>(usage.ru_maxrss);
#else
        value.maxResident = static_cast<quint64>(usage.ru_maxrss) * 1024;
#endif
    }
#ifdef Q_OS_MACOS
    task_vm_info_data_t info = {};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
        value.footprint = info.phys_footprint;
        value.resident = info.resident_size;
        value.memorySupported = true;
    }
#endif
    return value;
}

int windowCount()
{
    int count = 0;
    for (QWidget *widget : QApplication::topLevelWidgets())
        if (qobject_cast<MainWindow *>(widget))
            ++count;
    return count;
}

void ensure(bool valid, const QString &message)
{
    if (!valid)
        throw std::runtime_error(message.toStdString());
}

class ResourceProbe
{
public:
    ResourceProbe(QVApplication &application, const QString &fixture,
                  int roundCount, int idleDuration, bool stressWorkload)
        : app(application), sourceFixture(fixture), rounds(roundCount), idleMs(idleDuration),
          stressOnly(stressWorkload)
    {
        elapsed.start();
        lastSampleTime = elapsed.nsecsElapsed();
        lastCpu = readResources().cpuSeconds;
        sampler.setInterval(100);
        QObject::connect(&sampler, &QTimer::timeout, &app, [this] { sample("periodic"); });
    }

    QJsonObject run()
    {
        sampler.start();
        QJsonObject result;
        result.insert("schema", 1);
        result.insert("workload", stressOnly ? "two_files_high_compression_neighbor" : "eight_files_24mp_jpeg");
        result.insert("started_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        result.insert("source_revision", qEnvironmentVariable("QINGVIEW_PROBE_SOURCE_REVISION"));
        result.insert("source_snapshot_sha256", qEnvironmentVariable("QINGVIEW_PROBE_SOURCE_SHA256"));
        result.insert("qt_version", QT_VERSION_STR);
        result.insert("platform", QGuiApplication::platformName());
        result.insert("settings", "Fresh temporary INI, application defaults, no production preferences");
        result.insert("sampling_interval_ms", 100);
        result.insert("idle_duration_ms", idleMs);
        result.insert("navigation_rounds", rounds);
        result.insert("thread_pool_max_threads", QThreadPool::globalInstance()->maxThreadCount());
        result.insert("image_allocation_limit_mib", QImageReader::allocationLimit());
        result.insert("timing_note", "Window construction/show excludes process launch and QVApplication construction; this is not a cold-start benchmark. Load completion is the real fileChanged signal. Repeated paths can use application, filesystem and OS caches.");
        result.insert("memory_note", "Bytes. phys_footprint is TASK_VM_INFO.phys_footprint; RSS is resident_size. getrusage max RSS is a process lifetime high-water mark. 100 ms sampled peaks can miss brief allocations. Allocators may retain released memory.");
        result.insert("cpu_note", "CPU is getrusage user+system delta divided by wall time; 100 percent means one core. Includes this probe's 100 ms sampler and event handling.");
        result.insert("event_loop", "Workload starts via QTimer::singleShot(0) inside QApplication::exec; individual operations use nested event loops.");

        bool success = true;
        try {
            ensure(corpus.isValid(), "Could not create temporary fixture directory");
            if (stressOnly) {
                runStress(result);
            } else {
            ensure(QFileInfo(sourceFixture).isFile(), "Missing 24 MP JPEG fixture: " + sourceFixture);
            QImageReader metadata(sourceFixture);
            const QSize size = metadata.size();
            ensure(size.width() == 6000 && size.height() == 4000,
                   "The probe requires the same 6000x4000 JPEG fixture for both runs");
            result.insert("fixture_size", QJsonArray{size.width(), size.height()});
            result.insert("fixture_file_bytes", static_cast<double>(QFileInfo(sourceFixture).size()));
            result.insert("fixture_count", 8);
            QDir temp(corpus.path());
            ensure(temp.mkdir("single") && temp.mkdir("eight"), "Could not create corpus folders");
            const QString singlePath = temp.filePath("single/large.jpg");
            ensure(QFile::copy(sourceFixture, singlePath), "Could not copy single-image fixture");
            QStringList paths;
            for (int i = 0; i < 8; ++i) {
                const QString path = temp.filePath(QString("eight/%1.jpg").arg(i + 1, 2, 10, QChar('0')));
                ensure(QFile::copy(sourceFixture, path), "Could not copy navigation fixture");
                paths << path;
            }

            beginPhase("empty_window");
            QElapsedTimer construction;
            construction.start();
            QPointer<MainWindow> window = QVApplication::newWindow();
            result.insert("window_construct_and_show_ms", construction.nsecsElapsed() / 1e6);
            ensure(window, "Could not construct the real application window");
            window->resize(760, 520);
            pump(250);
            sample("empty_window_settled");
            finishPhase();

            beginPhase("empty_idle");
            pump(idleMs);
            sample("empty_idle_complete");
            finishPhase();

            beginPhase("single_24mp_load");
            load(window, singlePath, "single_first_request", -1, 0,
                 [window, singlePath] { window->openFile(singlePath); });
            ensure(waitForPool(), "Thread pool did not finish after single-image load");
            pump(250);
            sample("single_24mp_settled");
            finishPhase();

            beginPhase("single_24mp_idle");
            pump(idleMs);
            sample("single_24mp_idle_complete");
            finishPhase();

            beginPhase("navigation_initial_request");
            load(window, paths.first(), "navigation_first_request", -1, 0,
                 [window, paths] { window->openFile(paths.first()); });
            finishPhase();
            for (int round = 0; round < rounds; ++round) {
                beginPhase(QString("navigation_round_%1").arg(round + 1));
                for (int step = 1; step <= paths.size(); ++step) {
                    const int index = step % paths.size();
                    load(window, paths.at(index), "next_file", round, index,
                         [window] { window->nextFile(); });
                }
                ensure(waitForPool(), "Thread pool did not finish after navigation round");
                pump(250);
                sample("navigation_round_settled");
                finishPhase();
            }

            beginPhase("navigation_idle");
            pump(idleMs);
            sample("navigation_idle_complete");
            finishPhase();

            beginPhase("close_navigation_window");
            window->close();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            sample("closed_before_pool_drain");
            ensure(waitForPool(), "Thread pool did not finish after window close");
            pump(250);
            ensure(window.isNull(), "The navigation window was not destroyed");
            sample("closed_after_pool_drain");
            finishPhase();

            beginPhase("explicit_cache_clear_reference");
            clearCacheViaPublicApi();
            pump(250);
            sample("cache_clear_reference_complete");
            finishPhase();

            beginPhase("three_windows");
            QList<QPointer<MainWindow>> multiple;
            for (int i = 0; i < 3; ++i) {
                QPointer<MainWindow> other = QVApplication::newWindow();
                other->resize(760, 520);
                multiple << other;
                const QString path = paths.at(i * 3);
                load(other, path, "multiple_window_request", -1, i * 3,
                     [other, path] { other->openFile(path); });
                sample("multiple_window_loaded");
            }
            ensure(waitForPool(), "Thread pool did not finish after three windows loaded");
            pump(1000);
            sample("three_windows_settled");
            finishPhase();

            beginPhase("close_three_windows");
            for (const auto &other : multiple)
                if (other)
                    other->close();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            sample("three_closed_before_pool_drain");
            ensure(waitForPool(), "Thread pool did not finish after three windows closed");
            pump(250);
            ensure(windowCount() == 0, "Main windows survived close");
            sample("three_closed_after_pool_drain");
            finishPhase();

            beginPhase("final_cache_clear_reference");
            clearCacheViaPublicApi();
            pump(250);
            sample("final_cache_clear_complete");
            finishPhase();
            }
        } catch (const std::exception &error) {
            success = false;
            errors.append(QString::fromUtf8(error.what()));
            closeAllWindows();
            waitForPool();
            sample("failure_cleanup");
            finishPhase();
        }
        sampler.stop();
        result.insert("success", success);
        result.insert("errors", errors);
        result.insert("samples", samples);
        result.insert("sample_count", samples.size());
        result.insert("phases", phases);
        result.insert("load_latencies", loads);
        result.insert("load_sample_count", loads.size());
        result.insert("final_window_count", windowCount());
        result.insert("elapsed_ms", elapsed.nsecsElapsed() / 1e6);
        return result;
    }

    void captureAfterOuterLoop(QJsonObject &result, const QString &reason)
    {
        // Observe once the long workload callback has returned to the real
        // outer Cocoa event loop, including its autorelease handling.
        phase = "outer_loop_reference";
        sample(reason);
        phase.clear();
        result.insert("samples", samples);
        result.insert("sample_count", samples.size());
        result.insert("elapsed_with_outer_loop_reference_ms", elapsed.nsecsElapsed() / 1e6);
    }

    void captureAllocatorReliefReference(QJsonObject &result)
    {
#ifdef Q_OS_MACOS
        const size_t bytes = malloc_zone_pressure_relief(nullptr, 0);
        result.insert("allocator_relief_reported_bytes", static_cast<double>(bytes));
#endif
        result.insert("allocator_relief_note", "Diagnostic-only system malloc pressure relief after windows/jobs/cache and timing phases ended; this call is not part of product behavior.");
        captureAfterOuterLoop(result, "after_allocator_pressure_relief");
    }

private:
    void runStress(QJsonObject &result)
    {
        const QString largePng = qEnvironmentVariable("QINGVIEW_STRESS_PNG");
        const QString smallPng = QDir(QFileInfo(sourceFixture).dir().path()).filePath("../navigation/1.png");
        ensure(QFileInfo(largePng).isFile(), "Set QINGVIEW_STRESS_PNG to a separately generated 8000x6000 PNG");
        ensure(QFileInfo(smallPng).isFile(), "Missing small PNG fixture");
        QImageReader metadata(largePng);
        ensure(metadata.size() == QSize(8000, 6000), "Stress PNG must be 8000x6000");
        result.insert("fixture_count", 2);
        result.insert("neighbor_size", QJsonArray{8000, 6000});
        result.insert("neighbor_decoded_bytes", 8000.0 * 6000 * 4);
        result.insert("neighbor_compressed_bytes", static_cast<double>(QFileInfo(largePng).size()));
        result.insert("stress_note", "The large PNG is generated by a separate process before sampling. Only the small PNG is opened; default looping makes both neighboring indices address the same PNG.");
        const QString small = corpus.filePath("01-small.png");
        const QString large = corpus.filePath("02-large.png");
        ensure(QFile::copy(smallPng, small) && QFile::copy(largePng, large), "Could not copy two-file stress corpus");

        beginPhase("stress_empty_window");
        QPointer<MainWindow> window = QVApplication::newWindow();
        window->resize(760, 520);
        pump(250);
        sample("stress_empty_window_settled");
        finishPhase();

        beginPhase("small_image_large_neighbor");
        load(window, small, "small_image_only", -1, 0, [window, small] { window->openFile(small); });
        ensure(waitForPool(), "Thread pool did not finish the two-file stress preload");
        pump(250);
        sample("large_neighbor_jobs_complete");
        finishPhase();

        beginPhase("stress_idle");
        pump(idleMs);
        sample("stress_idle_complete");
        finishPhase();

        beginPhase("stress_close");
        window->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        sample("stress_closed_before_pool_drain");
        ensure(waitForPool(), "Thread pool did not finish after stress close");
        pump(250);
        ensure(window.isNull(), "Stress window was not destroyed");
        sample("stress_closed_after_pool_drain");
        finishPhase();
    }

    void beginPhase(const QString &name)
    {
        phase = name;
        phaseClock.start();
        phaseCpu = readResources().cpuSeconds;
        phasePeakFootprint = 0;
        phasePeakResident = 0;
        phaseSamples = 0;
        sample("phase_start");
    }

    void finishPhase()
    {
        if (phase.isEmpty())
            return;
        sample("phase_end");
        const double wallMs = phaseClock.nsecsElapsed() / 1e6;
        const double cpu = readResources().cpuSeconds - phaseCpu;
        phases.append(QJsonObject{{"phase", phase}, {"wall_ms", wallMs},
                                  {"cpu_seconds", cpu}, {"cpu_percent", wallMs > 0 ? cpu * 100000 / wallMs : 0},
                                  {"sampled_peak_footprint_bytes", static_cast<double>(phasePeakFootprint)},
                                  {"sampled_peak_rss_bytes", static_cast<double>(phasePeakResident)},
                                  {"sample_count", phaseSamples}});
        phase.clear();
    }

    void sample(const QString &reason)
    {
        const ResourceReading reading = readResources();
        const qint64 now = elapsed.nsecsElapsed();
        const double intervalMs = (now - lastSampleTime) / 1e6;
        QJsonObject sample{{"phase", phase}, {"reason", reason}, {"elapsed_ms", now / 1e6},
                           {"memory_supported", reading.memorySupported},
                           {"footprint_bytes", static_cast<double>(reading.footprint)},
                           {"rss_bytes", static_cast<double>(reading.resident)},
                           {"process_max_rss_bytes", static_cast<double>(reading.maxResident)},
                           {"process_cpu_seconds", reading.cpuSeconds},
                           {"interval_wall_ms", intervalMs},
                           {"interval_cpu_seconds", reading.cpuSeconds - lastCpu},
                           {"window_count", windowCount()},
                           {"thread_pool_active_threads", QThreadPool::globalInstance()->activeThreadCount()}};
        if (intervalMs >= 50)
            sample.insert("interval_cpu_percent", (reading.cpuSeconds - lastCpu) * 100000 / intervalMs);
        samples.append(sample);
        phasePeakFootprint = qMax(phasePeakFootprint, reading.footprint);
        phasePeakResident = qMax(phasePeakResident, reading.resident);
        ++phaseSamples;
        lastSampleTime = now;
        lastCpu = reading.cpuSeconds;
    }

    void pump(int duration)
    {
        QEventLoop loop;
        QTimer::singleShot(duration, &loop, &QEventLoop::quit);
        loop.exec();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    bool waitForPool(int timeout = 30000)
    {
        QEventLoop loop;
        QTimer poll;
        QTimer deadline;
        bool completed = false;
        poll.setInterval(25);
        deadline.setSingleShot(true);
        QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
            if (QThreadPool::globalInstance()->waitForDone(0)) {
                completed = true;
                loop.quit();
            }
        });
        QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
        poll.start();
        deadline.start(timeout);
        loop.exec();
        QCoreApplication::processEvents();
        return completed;
    }

    void load(QPointer<MainWindow> window, const QString &expected,
              const QString &kind, int round, int index, const std::function<void()> &request)
    {
        ensure(window, "Window disappeared before load");
        auto *view = window->findChild<QVGraphicsView *>();
        ensure(view, "Real QVGraphicsView not found");
        const QString path = QFileInfo(expected).absoluteFilePath();
        QEventLoop loop;
        QTimer deadline;
        deadline.setSingleShot(true);
        bool finished = false;
        bool loaded = false;
        double latency = 0;
        QString error;
        QElapsedTimer timer;
        QObject::connect(view, &QVGraphicsView::fileChanged, &loop, [&] {
            if (!window)
                return;
            const auto &details = window->getCurrentFileDetails();
            if (details.fileInfo.absoluteFilePath() != path)
                return;
            finished = true;
            loaded = details.isPixmapLoaded;
            error = details.errorData.errorString;
            latency = timer.nsecsElapsed() / 1e6;
            loop.quit();
        });
        QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start();
        request();
        if (!finished) {
            deadline.start(30000);
            loop.exec();
        }
        if (!finished)
            latency = timer.nsecsElapsed() / 1e6;
        loads.append(QJsonObject{{"phase", phase}, {"kind", kind}, {"round", round},
                                 {"file_index", index}, {"file_name", QFileInfo(path).fileName()},
                                 {"latency_ms", latency}, {"completed", finished},
                                 {"loaded", loaded}, {"error", error}});
        sample("load_complete");
        ensure(finished && loaded, "Image load failed or timed out: " + QFileInfo(path).fileName() + " " + error);
    }

    void clearCacheViaPublicApi()
    {
        // A separate reference checkpoint, not an assertion that closing
        // windows automatically clears the static cache in every revision.
        QSettings settings;
        settings.setValue("options/preloadingmode", 0);
        app.getSettingsManager().loadSettings();
        QPointer<MainWindow> temporary = QVApplication::newWindow();
        auto *core = temporary->findChild<QVImageCore *>();
        ensure(core, "Could not obtain the real image core for cache-clear reference");
        core->requestCaching();
        temporary->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        settings.setValue("options/preloadingmode", 1);
        app.getSettingsManager().loadSettings();
    }

    void closeAllWindows()
    {
        QList<QPointer<MainWindow>> windows;
        for (QWidget *widget : QApplication::topLevelWidgets())
            if (auto *window = qobject_cast<MainWindow *>(widget))
                windows << window;
        for (const auto &window : windows)
            if (window)
                window->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    QVApplication &app;
    QString sourceFixture;
    int rounds;
    int idleMs;
    bool stressOnly;
    QTemporaryDir corpus;
    QTimer sampler;
    QElapsedTimer elapsed;
    QElapsedTimer phaseClock;
    QString phase;
    qint64 lastSampleTime = 0;
    double lastCpu = 0;
    double phaseCpu = 0;
    quint64 phasePeakFootprint = 0;
    quint64 phasePeakResident = 0;
    int phaseSamples = 0;
    QJsonArray samples;
    QJsonArray phases;
    QJsonArray loads;
    QJsonArray errors;
};
} // namespace

int main(int argc, char **argv)
{
    QTemporaryDir settingsDirectory;
    if (!settingsDirectory.isValid())
        return 2;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settingsDirectory.path());
    QCoreApplication::setOrganizationName("QingViewResourceProbe");
    QCoreApplication::setApplicationName("QingViewResourceProbe");
    QGuiApplication::setApplicationDisplayName("QingView Resource Probe");
    QVApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription("Opt-in same-condition resource sampling of real QingView windows; uses temporary files/settings.");
    parser.addHelpOption();
    parser.addOption({"output", "JSON output path", "path"});
    parser.addOption({"rounds", "Complete rounds through eight copied 24 MP JPEGs", "count", "3"});
    parser.addOption({"idle-ms", "Idle interval for empty/static/navigation states", "milliseconds", "10000"});
    parser.addOption({"stress-only", "Only test a small PNG with a separately generated 48 MP high-compression neighbor"});
    parser.addOption({"closing-diagnostic", "Keep the closed process alive for vmmap/heap capture, then measure diagnostic allocator pressure relief"});
    parser.process(app);
    bool roundsValid = false;
    bool idleValid = false;
    const int rounds = parser.value("rounds").toInt(&roundsValid);
    const int idle = parser.value("idle-ms").toInt(&idleValid);
    if (!roundsValid || rounds < 1 || rounds > 20 || !idleValid || idle < 100 || idle > 60000
        || parser.value("output").isEmpty()) {
        qCritical("Provide --output; rounds must be 1..20 and idle-ms 100..60000.");
        return 2;
    }
    const QString fixtureRoot = qEnvironmentVariable("QINGVIEW_FIXTURES");
    if (fixtureRoot.isEmpty()) {
        qCritical("Set QINGVIEW_FIXTURES to the generated fixture corpus.");
        return 2;
    }
    ResourceProbe probe(app, QDir(fixtureRoot).filePath("formats/large.jpg"), rounds, idle,
                        parser.isSet("stress-only"));
    QJsonObject result;
    const bool closingDiagnostic = parser.isSet("closing-diagnostic");
    QTimer::singleShot(0, &app, [&] {
        result = probe.run();
        QTimer::singleShot(100, &app, [&] {
            probe.captureAfterOuterLoop(result, "outer_loop_after_100ms");
        });
        QTimer::singleShot(1000, &app, [&] {
            probe.captureAfterOuterLoop(result, "outer_loop_after_1000ms");
            if (!closingDiagnostic) {
                app.quit();
                return;
            }
            result.insert("closing_diagnostic", true);
            const QString acknowledgement = settingsDirectory.filePath("diagnostic-done");
            qInfo().noquote().nospace() << "CLOSING_DIAGNOSTIC_READY pid="
                << QCoreApplication::applicationPid() << " ack=" << acknowledgement;
            auto *poll = new QTimer(&app);
            poll->setInterval(100);
            QElapsedTimer *deadline = new QElapsedTimer;
            deadline->start();
            QObject::connect(poll, &QTimer::timeout, &app, [&, poll, deadline, acknowledgement] {
                if (!QFileInfo::exists(acknowledgement) && deadline->elapsed() < 30000)
                    return;
                poll->stop();
                poll->deleteLater();
                delete deadline;
                probe.captureAllocatorReliefReference(result);
                QTimer::singleShot(250, &app, [&] {
                    probe.captureAfterOuterLoop(result, "allocator_relief_after_250ms");
                    app.quit();
                });
            });
            poll->start();
        });
    });
    app.exec();
    QSaveFile output(parser.value("output"));
    if (!output.open(QIODevice::WriteOnly)) {
        qCritical() << "Cannot write probe result:" << output.errorString();
        return 2;
    }
    output.write(QJsonDocument(result).toJson(QJsonDocument::Indented));
    if (!output.commit()) {
        qCritical() << "Cannot commit probe result:" << output.errorString();
        return 2;
    }
    qInfo().noquote() << "Resource probe JSON:" << parser.value("output");
    return result.value("success").toBool() ? 0 : 1;
}
