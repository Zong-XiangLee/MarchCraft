#include "DrillProject.h"
#include "AssetCatalog.h"
#include "TransportController.h"
#include "WorkspaceController.h"

#include <QApplication>
#include "ExportController.h"
#include <QColor>
#include <QDateTime>
#include <QFile>
#include <QIcon>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QPalette>
#include <QTimer>
#include <QTextStream>

namespace {
void writeApplicationLog(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    QFile file(QCoreApplication::applicationDirPath() + QStringLiteral("/marchcraft-startup.log"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;
    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << QLatin1Char(' ')
           << (type == QtFatalMsg ? QStringLiteral("FATAL") : type == QtCriticalMsg ? QStringLiteral("CRITICAL")
               : type == QtWarningMsg ? QStringLiteral("WARNING") : QStringLiteral("INFO"))
           << QStringLiteral(": ") << message << QLatin1Char('\n');
}
}

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setOrganizationName(QStringLiteral("MarchCraft"));
    application.setOrganizationDomain(QStringLiteral("marchcraft.local"));
    application.setApplicationName(QStringLiteral("MarchCraft"));
    application.setApplicationVersion(QStringLiteral("0.6.0"));
    application.setWindowIcon(QIcon(QStringLiteral(":/branding/marchcraft-logo.png")));
    qInstallMessageHandler(writeApplicationLog);
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#202328")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#f1f3f5")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#16181c")));
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#292d33")));
    palette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#89929e")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#f1f3f5")));
    palette.setColor(QPalette::Button, QColor(QStringLiteral("#292d33")));
    palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#f1f3f5")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#3f78bd")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
    palette.setColor(QPalette::Mid, QColor(QStringLiteral("#3a4049")));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(QStringLiteral("#68717d")));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(QStringLiteral("#68717d")));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(QStringLiteral("#68717d")));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(QStringLiteral("#30343a")));
    application.setPalette(palette);

    DrillProject project;
    TransportController transport(&project);
    ExportController exportController(&project);
    AssetCatalog assetCatalog;
    WorkspaceController workspaceController;
    const QStringList arguments = application.arguments();
    const bool qaHome = arguments.contains(QStringLiteral("--qa-home"));
    const bool qaNewProject = arguments.contains(QStringLiteral("--qa-new-project"));
    const bool qaShapes = arguments.contains(QStringLiteral("--qa-shapes"));
    const bool qaMovements = arguments.contains(QStringLiteral("--qa-movements"));
    const bool qaMinimum = arguments.contains(QStringLiteral("--qa-minimum"));
    const QStringList editorFlags{
        QStringLiteral("--screenshot"), QStringLiteral("--3d"), QStringLiteral("--3d-view"),
        QStringLiteral("--qa-set-drag-preview"), QStringLiteral("--qa-current-transition"),
        QStringLiteral("--qa-midi-synth"), QStringLiteral("--qa-coordinate-pdf"), QStringLiteral("--qa-export"),
        QStringLiteral("--field-style"), QStringLiteral("--venue"), QStringLiteral("--lighting"),
        QStringLiteral("--graphics-profile"),
        QStringLiteral("--midi"), QStringLiteral("--qa-minimum")
    };
    bool startInEditor = false;
    for (const QString &flag : editorFlags)
        startInEditor |= arguments.contains(flag);
    startInEditor |= qaShapes || qaMovements;
    startInEditor &= !(qaHome || qaNewProject);
    if (startInEditor)
        project.loadDemo();
    if (qaShapes)
        project.selectAll();
    const int fieldPresetFlag = arguments.indexOf(QStringLiteral("--field-preset"));
    if (fieldPresetFlag >= 0 && fieldPresetFlag + 1 < arguments.size())
        project.setFieldPreset(arguments.at(fieldPresetFlag + 1));
    const int fieldStyleFlag = arguments.indexOf(QStringLiteral("--field-style"));
    if (fieldStyleFlag >= 0 && fieldStyleFlag + 1 < arguments.size())
        project.setFieldStyle(arguments.at(fieldStyleFlag + 1));
    const int venueFlag = arguments.indexOf(QStringLiteral("--venue"));
    if (venueFlag >= 0 && venueFlag + 1 < arguments.size())
        project.setVenuePreset(arguments.at(venueFlag + 1));
    const int lightingFlag = arguments.indexOf(QStringLiteral("--lighting"));
    if (lightingFlag >= 0 && lightingFlag + 1 < arguments.size())
        project.setLightingPreset(arguments.at(lightingFlag + 1));
    const int qualityFlag = arguments.indexOf(QStringLiteral("--graphics-profile"));
    if (qualityFlag >= 0 && qualityFlag + 1 < arguments.size())
        project.setGraphicsProfile(arguments.at(qualityFlag + 1));
    if (arguments.contains(QStringLiteral("--qa-export-fixture"))) {
        project.newProject();
        project.setShowName(QStringLiteral("Export Verification"));
        project.addPerformer(QStringLiteral("T01"), QStringLiteral("Trumpet"), QStringLiteral("Brass"), 72, 42);
        project.setOpeningBehavior(QStringLiteral("hold"), 2);
        project.selectAll();project.addSet(QStringLiteral("Move"), 4);project.nudgeSelected(8, 0);
        project.clearSelection();
    }
    const int midiFlag = arguments.indexOf(QStringLiteral("--midi"));
    if (midiFlag >= 0 && midiFlag + 1 < arguments.size())
        project.importMidi(arguments.at(midiFlag + 1));
    const int audioFlag = arguments.indexOf(QStringLiteral("--audio"));
    if (audioFlag >= 0 && audioFlag + 1 < arguments.size())
        project.attachAudio(arguments.at(audioFlag + 1));
    if (qaMovements) {
        project.renameMovement(0, QStringLiteral("Opener — a long movement title for layout QA"));
        project.createMovement(QStringLiteral("Ballad"));
        project.createMovement(QStringLiteral("Finale"));
        project.activateMovement(0);
    }
    const int qaPdfFlag = arguments.indexOf(QStringLiteral("--qa-coordinate-pdf"));
    if (qaPdfFlag >= 0 && qaPdfFlag + 1 < arguments.size()) {
        project.newProject();
        project.setShowName(QStringLiteral("Coordinate Sheet Layout Test"));
        project.addPerformer(QStringLiteral("T01"), QStringLiteral("Trumpet"),
                             QStringLiteral("Brass"), 32.0, 20.0);
        project.selectPerformer(0, false);
        for (int set = 2; set <= 42; ++set) {
            project.addSet(QStringLiteral("Set %1").arg(set), 8 + (set % 3) * 4);
            project.nudgeSelected(0.75, (set % 2 == 0) ? 0.25 : -0.25);
        }
        return project.exportCoordinatePdf(arguments.at(qaPdfFlag + 1)) ? 0 : 2;
    }

    const bool qaHuman = arguments.contains(QStringLiteral("--qa-human"));
    if (qaHuman) {
        startInEditor = true;
        project.newProject();
        const int phaseFlag = arguments.indexOf(QStringLiteral("--qa-human-count"));
        const int headingFlag = arguments.indexOf(QStringLiteral("--qa-human-heading"));
        const double heading = headingFlag >= 0 && headingFlag + 1 < arguments.size()
            ? arguments.at(headingFlag + 1).toDouble() * 3.141592653589793 / 180.0 : 0.0;
        const double count = phaseFlag >= 0 && phaseFlag + 1 < arguments.size()
            ? qBound(0.0, arguments.at(phaseFlag + 1).toDouble(), 8.0) : 0.0;
        const bool hold = arguments.contains(QStringLiteral("--qa-human-mark-time"));
        const double dx = hold ? 0.0 : std::sin(heading);
        const double dy = hold ? 0.0 : -std::cos(heading);
        project.addPerformer(QStringLiteral("Human QA"), QStringLiteral("Trumpet"),
                             QStringLiteral("Brass"), 80.0 - dx * count,
                             project.fieldDepthSteps() / 2.0 - dy * count);
        if (phaseFlag >= 0 && phaseFlag + 1 < arguments.size()) {
            project.selectAll();
            project.addSet(QStringLiteral("March"), 8);
            project.nudgeSelected(dx * 8, dy * 8);
            project.addSet(QStringLiteral("Continue"), 8);
            project.nudgeSelected(dx * 8, dy * 8);
            project.setCurrentSetIndex(1);
            project.clearSelection();
        }
        if (qualityFlag >= 0 && qualityFlag + 1 < arguments.size())
            project.setGraphicsProfile(arguments.at(qualityFlag + 1));
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("exportController"), &exportController);
    engine.rootContext()->setContextProperty(QStringLiteral("drillProject"), &project);
    engine.rootContext()->setContextProperty(QStringLiteral("transport"), &transport);
    engine.rootContext()->setContextProperty(QStringLiteral("assetCatalog"), &assetCatalog);
    engine.rootContext()->setContextProperty(QStringLiteral("workspaceController"), &workspaceController);
    engine.rootContext()->setContextProperty(QStringLiteral("initialWorkspaceActive"), startInEditor);
    engine.rootContext()->setContextProperty(QStringLiteral("qaMode"), startInEditor || qaHome || qaNewProject);
    engine.rootContext()->setContextProperty(QStringLiteral("initialHomeMode"),
        qaNewProject ? QStringLiteral("new") : QStringLiteral("dashboard"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &application, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("MarchCraft"), QStringLiteral("Main"));

    if (qaMinimum && !engine.rootObjects().isEmpty()) {
        engine.rootObjects().first()->setProperty("width", 1120);
        engine.rootObjects().first()->setProperty("height", 720);
    }

    if (arguments.contains(QStringLiteral("--3d")) && !engine.rootObjects().isEmpty())
        engine.rootObjects().first()->setProperty("threeD", true);
    if (qaHuman && !engine.rootObjects().isEmpty())
        engine.rootObjects().first()->setProperty("qa3DView", QStringLiteral("performer"));
    if (qaHuman && !engine.rootObjects().isEmpty()) {
        // Apply the sampled count after model/transport initialization has settled.
        const int phaseFlag = arguments.indexOf(QStringLiteral("--qa-human-count"));
        if (phaseFlag >= 0 && phaseFlag + 1 < arguments.size()) {
            const double count = qBound(0.0, arguments.at(phaseFlag + 1).toDouble(), 8.0);
            QTimer::singleShot(400, &application, [&project, count] {
                project.setCurrentSetIndex(1);
                project.setPlaybackActive(true);
                project.setPlayhead(count / 8.0);
                qInfo() << "Human QA: count=" << count << "active=" << project.playbackActive()
                        << "phase=" << project.data(project.index(0), DrillProject::GaitPhaseRole);
            });
        }
        if (auto *view = engine.rootObjects().first()->findChild<QObject *>(QStringLiteral("performerView"))) {
            view->setProperty("carriagePose", arguments.contains(QStringLiteral("--qa-human-horns-down"))
                              ? QStringLiteral("horn.down") : QStringLiteral("horn.up"));
            view->setProperty("markTimeDuringHolds", arguments.contains(QStringLiteral("--qa-human-mark-time")));
        }
    }
    const int themeFlag = arguments.indexOf(QStringLiteral("--qa-theme"));
    if (themeFlag >= 0 && themeFlag + 1 < arguments.size() && !engine.rootObjects().isEmpty()) {
        QMetaObject::invokeMethod(engine.rootObjects().first(), "setQaTheme",
                                  Q_ARG(QVariant, arguments.at(themeFlag + 1)));
    }
    const int viewFlag = arguments.indexOf(QStringLiteral("--3d-view"));
    if (viewFlag >= 0 && viewFlag + 1 < arguments.size() && !engine.rootObjects().isEmpty())
        engine.rootObjects().first()->setProperty("qa3DView", arguments.at(viewFlag + 1));
    if (arguments.contains(QStringLiteral("--qa-set-drag-preview")) && !engine.rootObjects().isEmpty())
        engine.rootObjects().first()->setProperty("qaSetDragPreview", true);
    if (qaShapes && !engine.rootObjects().isEmpty())
        engine.rootObjects().first()->setProperty("qaShapePalette", true);
    for (const auto &surface : {QStringLiteral("export-dialog"), QStringLiteral("preferences"), QStringLiteral("performer"), QStringLiteral("music"), QStringLiteral("formation")}) {
        if (arguments.contains(QStringLiteral("--qa-") + surface) && !engine.rootObjects().isEmpty()) {
            QObject *root = engine.rootObjects().first();
            QTimer::singleShot(350, &application, [root, surface] {
                QMetaObject::invokeMethod(root, "showQaSurface", Q_ARG(QVariant, surface));
            });
        }
    }
    const int presetFlag = arguments.indexOf(QStringLiteral("--qa-export-preset"));
    if(presetFlag>=0 && presetFlag+1<arguments.size() && !engine.rootObjects().isEmpty()) {
        auto *root=engine.rootObjects().first();
        const auto preset=arguments[presetFlag+1];
        QTimer::singleShot(400,&application,[root,preset] {QMetaObject::invokeMethod(root,"showQaExport",Q_ARG(QVariant,preset));});
    }
    const int exportFlag = arguments.indexOf(QStringLiteral("--qa-export"));
    if (exportFlag >= 0 && exportFlag + 1 < arguments.size()) {
        QVariantMap options{{QStringLiteral("format"), QStringLiteral("pdf")}, {QStringLiteral("scope"), QStringLiteral("active")}, {QStringLiteral("sets"), QStringLiteral("1-2")}};
        const int formatFlag = arguments.indexOf(QStringLiteral("--export-format"));
        if (formatFlag >= 0 && formatFlag + 1 < arguments.size()) options[QStringLiteral("format")] = arguments[formatFlag + 1];
        const int contentFlag=arguments.indexOf(QStringLiteral("--export-content"));
        if(contentFlag>=0 && contentFlag+1<arguments.size())options[QStringLiteral("content")]=arguments[contentFlag+1];
        const int soundFlag = arguments.indexOf(QStringLiteral("--export-audio"));
        if (soundFlag >= 0 && soundFlag + 1 < arguments.size()) options[QStringLiteral("audio")] = arguments[soundFlag + 1];
        const int encoderFlag = arguments.indexOf(QStringLiteral("--ffmpeg"));
        if (encoderFlag >= 0 && encoderFlag + 1 < arguments.size()) exportController.setFfmpeg(arguments[encoderFlag + 1]);
        QObject::connect(&exportController, &ExportController::finished, &application, [&application](bool ok) { application.exit(ok ? 0 : 5); });
        QTimer::singleShot(700, &application, [&exportController, &application, options, arguments, exportFlag] {
            if (!exportController.prepare(options) || !exportController.start(arguments[exportFlag + 1], true)) application.exit(5);
        });
    }
    const int screenshotFlag = arguments.indexOf(QStringLiteral("--screenshot"));
    const bool screenshotRequested = screenshotFlag >= 0 && screenshotFlag + 1 < arguments.size();
    if (screenshotRequested) {
        const QString destination = arguments.at(screenshotFlag + 1);
        const int delayFlag = arguments.indexOf(QStringLiteral("--screenshot-delay"));
        const int screenshotDelay = delayFlag >= 0 && delayFlag + 1 < arguments.size()
                ? qBound(250, arguments.at(delayFlag + 1).toInt(), 10000) : 1800;
        QTimer::singleShot(screenshotDelay, &application, [&engine, destination] {
            if (!engine.rootObjects().isEmpty()) {
                // Screenshot fixtures may be dirty; never open an unsaved-work
                // confirmation for this explicit capture-and-exit command.
                engine.rootObjects().first()->setProperty("forceClosing", true);
                if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first()))
                    window->grabWindow().save(destination);
            }
            QCoreApplication::quit();
        });
    }
    if (arguments.contains(QStringLiteral("--qa-current-transition")) && !engine.rootObjects().isEmpty()) {
        QObject *root = engine.rootObjects().first();
        QTimer::singleShot(400, &application, [root, &project, &application, screenshotRequested] {
            const bool invoked = QMetaObject::invokeMethod(root, "playCurrentTransition", Qt::DirectConnection);
            if (!invoked) {
                application.exit(2);
                return;
            }
            QTimer::singleShot(900, &application, [&project, &application, screenshotRequested] {
                const bool advancing = project.playbackSetIndex() > 0 && project.playhead() > 0.01;
                if (!advancing || !screenshotRequested)
                    application.exit(advancing ? 0 : 3);
            });
        });
    }
    if (arguments.contains(QStringLiteral("--qa-midi-synth"))) {
        transport.playFromSelection();
        QTimer::singleShot(4000, &application, [&transport, &application] {
            qInfo().noquote() << QStringLiteral("MIDI synth QA: available=%1 tick=%2 underruns=%3 status=%4")
                .arg(transport.synthAvailable()).arg(transport.currentTick()).arg(transport.underrunCount()).arg(transport.audioStatus());
            application.exit(transport.synthAvailable() && transport.currentTick() > 0 && transport.underrunCount() == 0 ? 0 : 4);
        });
    }
    return application.exec();
}
