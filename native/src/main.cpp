#include "DrillProject.h"
#include "AssetCatalog.h"
#include "TransportController.h"

#include <QGuiApplication>
#include <QDateTime>
#include <QFile>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
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
    QGuiApplication application(argc, argv);
    application.setOrganizationName(QStringLiteral("MarchCraft"));
    application.setOrganizationDomain(QStringLiteral("marchcraft.local"));
    application.setApplicationName(QStringLiteral("MarchCraft"));
    application.setApplicationVersion(QStringLiteral("0.6.0"));
    qInstallMessageHandler(writeApplicationLog);
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    DrillProject project;
    TransportController transport(&project);
    AssetCatalog assetCatalog;
    const QStringList arguments = application.arguments();
    const int venueFlag = arguments.indexOf(QStringLiteral("--venue"));
    if (venueFlag >= 0 && venueFlag + 1 < arguments.size())
        project.setVenuePreset(arguments.at(venueFlag + 1));
    const int lightingFlag = arguments.indexOf(QStringLiteral("--lighting"));
    if (lightingFlag >= 0 && lightingFlag + 1 < arguments.size())
        project.setLightingPreset(arguments.at(lightingFlag + 1));
    const int qualityFlag = arguments.indexOf(QStringLiteral("--graphics-profile"));
    if (qualityFlag >= 0 && qualityFlag + 1 < arguments.size())
        project.setGraphicsProfile(arguments.at(qualityFlag + 1));
    if (arguments.contains(QStringLiteral("--ground-debug")))
        project.setDebug3D(true);
    const int midiFlag = arguments.indexOf(QStringLiteral("--midi"));
    if (midiFlag >= 0 && midiFlag + 1 < arguments.size())
        project.importMidi(arguments.at(midiFlag + 1));
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

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("drillProject"), &project);
    engine.rootContext()->setContextProperty(QStringLiteral("transport"), &transport);
    engine.rootContext()->setContextProperty(QStringLiteral("assetCatalog"), &assetCatalog);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &application, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("MarchCraft"), QStringLiteral("Main"));

    if (arguments.contains(QStringLiteral("--3d")) && !engine.rootObjects().isEmpty())
        engine.rootObjects().first()->setProperty("threeD", true);
    const int viewFlag = arguments.indexOf(QStringLiteral("--3d-view"));
    if (viewFlag >= 0 && viewFlag + 1 < arguments.size() && !engine.rootObjects().isEmpty())
        engine.rootObjects().first()->setProperty("qa3DView", arguments.at(viewFlag + 1));
    if (arguments.contains(QStringLiteral("--qa-set-drag-preview")) && !engine.rootObjects().isEmpty())
        engine.rootObjects().first()->setProperty("qaSetDragPreview", true);
    const int screenshotFlag = arguments.indexOf(QStringLiteral("--screenshot"));
    const bool screenshotRequested = screenshotFlag >= 0 && screenshotFlag + 1 < arguments.size();
    if (screenshotRequested) {
        const QString destination = arguments.at(screenshotFlag + 1);
        const int delayFlag = arguments.indexOf(QStringLiteral("--screenshot-delay"));
        const int screenshotDelay = delayFlag >= 0 && delayFlag + 1 < arguments.size()
                ? qBound(250, arguments.at(delayFlag + 1).toInt(), 10000) : 1800;
        QTimer::singleShot(screenshotDelay, &application, [&engine, destination] {
            if (!engine.rootObjects().isEmpty()) {
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
                const bool advancing = project.currentSetIndex() > 0 && project.playhead() > 0.01;
                if (!advancing || !screenshotRequested)
                    application.exit(advancing ? 0 : 3);
            });
        });
    }
    if (arguments.contains(QStringLiteral("--qa-midi-synth"))) {
        transport.playFromSelection();
        QTimer::singleShot(1200, &application, [&transport, &application] {
            qInfo().noquote() << QStringLiteral("MIDI synth QA: available=%1 tick=%2 status=%3")
                .arg(transport.synthAvailable()).arg(transport.currentTick()).arg(transport.audioStatus());
            application.exit(transport.synthAvailable() && transport.currentTick() > 0 ? 0 : 4);
        });
    }
    return application.exec();
}
