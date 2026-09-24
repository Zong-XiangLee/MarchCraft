#include "DrillProject.h"
#include "ExportController.h"
#include "DrillTypes.h"
#include "AssetCatalog.h"
#include "SceneTypes.h"
#include "TransportController.h"
#include "MidiSynthEngine.h"
#include "WorkspaceController.h"
#include "ProjectStorage.h"
#include "TransitionPath.h"

#include <QFile>
#include <QDataStream>
#include <QLineF>
#include <QQmlContext>
#include <QJSValue>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickView>
#include <algorithm>
#include <cmath>
#include <QJsonDocument>
#include <QSet>
#include <QSettings>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>

namespace {
bool closeTo(double actual, double expected)
{
    return qAbs(actual - expected) < 0.0001;
}

void append16(QByteArray &data, quint16 value)
{
    data.append(char((value >> 8) & 0xff)); data.append(char(value & 0xff));
}
void append32(QByteArray &data, quint32 value)
{
    data.append(char((value >> 24) & 0xff)); data.append(char((value >> 16) & 0xff));
    data.append(char((value >> 8) & 0xff)); data.append(char(value & 0xff));
}
void appendVlq(QByteArray &data, quint32 value)
{
    quint8 bytes[5]; int count = 0; bytes[count++] = value & 0x7f;
    while ((value >>= 7) != 0) bytes[count++] = (value & 0x7f) | 0x80;
    while (count > 0) data.append(char(bytes[--count]));
}
QByteArray midiFixture()
{
    QByteArray conductor;
    conductor.append(char(0)); conductor += QByteArray::fromHex("ff510307a120");
    conductor.append(char(0)); conductor += QByteArray::fromHex("ff580403021808");
    appendVlq(conductor, 2880); conductor += QByteArray::fromHex("ff2f00");
    QByteArray notes;
    notes.append(char(0)); notes += QByteArray::fromHex("ff030452656564");
    notes.append(char(0)); notes += QByteArray::fromHex("c028");
    notes.append(char(0)); notes += QByteArray::fromHex("903c64");
    appendVlq(notes, 480); notes += QByteArray::fromHex("3c00");
    notes.append(char(0)); notes += QByteArray::fromHex("3e64");
    appendVlq(notes, 480); notes += QByteArray::fromHex("3e00");
    appendVlq(notes, 1920); notes += QByteArray::fromHex("ff2f00");
    QByteArray result("MThd", 4); append32(result, 6); append16(result, 1); append16(result, 2); append16(result, 480);
    result += QByteArrayLiteral("MTrk"); append32(result, conductor.size()); result += conductor;
    result += QByteArrayLiteral("MTrk"); append32(result, notes.size()); result += notes;
    return result;
}
}

class DrillProjectTest final : public QObject
{
    Q_OBJECT

private slots:
    void movementSwitchPreservesLoopRangeAndEditingPage()
    {
        DrillProject project;
        project.addSet(QStringLiteral("Second"), 8); project.addSet(QStringLiteral("Third"), 8);
        project.setPlaybackSource(QStringLiteral("mute"));
        TransportController transport(&project);
        transport.navigateToSet(0); transport.navigateToSet(2, true);
        project.setLoopEnabled(true); transport.navigateToSet(1);
        QVERIFY(project.loopEnabled());
        QVERIFY(project.createMovement(QStringLiteral("Ballad")));
        project.activateMovement(0);
        QVERIFY(project.loopEnabled()); QCOMPARE(project.currentSetIndex(), 1);
        QCOMPARE(project.selectedSetStartIndex(), 0); QCOMPARE(project.selectedSetEndIndex(), 2);
        QCOMPARE(transport.currentMs(), transport.setPositionMs(1));
        transport.seekMs(transport.setPositionMs(2) - 10); transport.playPause();
        QTest::qWait(70); QVERIFY(transport.currentMs() < transport.setPositionMs(1));
        QCOMPARE(project.currentSetIndex(), 1); transport.stop();
    }

    void movementRosterChangesReachInactivePagesAndUndo()
    {
        DrillProject project;
        project.addPerformer(QStringLiteral("A"), QStringLiteral("Trumpet"), QStringLiteral("Brass"), 20, 20);
        QVERIFY(project.createMovement(QStringLiteral("Second")));
        project.addPerformer(QStringLiteral("B"), QStringLiteral("Trumpet"), QStringLiteral("Brass"), 40, 30);
        project.activateMovement(0);
        QCOMPARE(project.performerCount(), 2);
        QCOMPARE(project.data(project.index(1), DrillProject::XRole).toDouble(), 40.0);
        project.selectPerformer(1, false); project.removeSelectedPerformers();
        project.activateMovement(1); QCOMPARE(project.performerCount(), 1);
        project.undo(); QCOMPARE(project.performerCount(), 2);
        project.activateMovement(1);
        QCOMPARE(project.data(project.index(1), DrillProject::XRole).toDouble(), 40.0);
    }

    void movementsKeepIndependentPagesMusicAndHistory()
    {
        QTemporaryDir directory;
        QFile midi(directory.filePath(QStringLiteral("movement.mid")));
        QVERIFY(midi.open(QIODevice::WriteOnly)); midi.write(midiFixture()); midi.close();
        DrillProject project;
        project.addPerformer(QStringLiteral("P1"), QStringLiteral("Trumpet"), QStringLiteral("Brass"), 20, 20);
        project.addSet(QStringLiteral("Finale"), 8);
        project.selectAll(); project.nudgeSelected(12, 0);
        QVERIFY(project.importMidi(midi.fileName()));
        project.setMusicSelection(1, 1);
        project.setAudioOffsetMs(125);
        QVERIFY(project.renameMovement(0, QStringLiteral("Opener")));
        TransportController transport(&project);
        QVERIFY(project.createMovement(QStringLiteral("Ballad")));
        QCOMPARE(project.movementCount(), 2); QCOMPARE(project.currentMovementIndex(), 1);
        QCOMPARE(project.setCount(), 1); QVERIFY(!project.musicLoaded());
        QCOMPARE(project.audioOffsetMs(), 0.0); QCOMPARE(transport.currentMs(), 0.0);
        QCOMPARE(project.data(project.index(0), DrillProject::XRole).toDouble(), 32.0);
        project.addSet(QStringLiteral("Ballad end"), 16);
        project.selectAll(); project.nudgeSelected(10, 0);
        const QString saved = directory.filePath(QStringLiteral("movements.marchcraft"));
        QVERIFY(project.saveProject(saved));
        transport.playPause(); project.activateMovement(0);
        QVERIFY(!transport.playing()); QVERIFY(!project.dirty());
        QVERIFY(project.musicLoaded()); QCOMPARE(project.audioOffsetMs(), 125.0);
        QCOMPARE(project.musicSelectionStart(), 1); QCOMPARE(project.setCount(), 2);
        QCOMPARE(project.currentSetName(), QStringLiteral("Finale"));
        QCOMPARE(project.data(project.index(0), DrillProject::XRole).toDouble(), 32.0);
        project.activateMovement(1); QVERIFY(!project.dirty());
        QCOMPARE(project.data(project.index(0), DrillProject::XRole).toDouble(), 42.0);
        project.undo(); // Switching tabs must not occupy an undo slot.
        QCOMPARE(project.data(project.index(0), DrillProject::XRole).toDouble(), 32.0);
        project.redo(); QCOMPARE(project.data(project.index(0), DrillProject::XRole).toDouble(), 42.0);
        DrillProject restored; QVERIFY(restored.loadProject(saved));
        QCOMPARE(restored.movementCount(), 2); QCOMPARE(restored.currentMovementName(), QStringLiteral("Ballad"));
        restored.activateMovement(0); QVERIFY(restored.musicLoaded());
        QCOMPARE(restored.currentMovementName(), QStringLiteral("Opener"));
        QVERIFY(restored.createMovement(QStringLiteral("Opener copy"), true));
        QCOMPARE(restored.setCount(), 2); QVERIFY(restored.musicLoaded());
        restored.moveMovement(2, 0); QCOMPARE(restored.currentMovementIndex(), 0);
        restored.removeMovement(0); QCOMPARE(restored.movementCount(), 2);
        restored.undo(); QCOMPARE(restored.movementCount(), 3);
        QCOMPARE(restored.currentMovementName(), QStringLiteral("Opener copy"));
        restored.newProject(); QCOMPARE(restored.movementCount(), 1);
    }

    void legacyShowBecomesOneMovementAndBadMovementDataIsRejected()
    {
        QTemporaryDir directory; DrillProject project;
        project.addSet(QStringLiteral("Legacy end"), 8);
        const QString path = directory.filePath(QStringLiteral("legacy.marchcraft"));
        QVERIFY(project.saveProject(path));
        QJsonObject root; QString error;
        QVERIFY(MarchCraft::ProjectStorage::readSqliteProject(path, &root, &error));
        QFile file(path);
        root.insert(QStringLiteral("version"), 10); root.remove(QStringLiteral("movements"));
        QVERIFY(file.open(QIODevice::WriteOnly)); file.write(QJsonDocument(root).toJson()); file.close();
        QVERIFY(project.loadProject(path)); QCOMPARE(project.movementCount(), 1); QCOMPARE(project.setCount(), 2);
        root.insert(QStringLiteral("version"), 11);
        QVERIFY(file.open(QIODevice::WriteOnly)); file.write(QJsonDocument(root).toJson()); file.close();
        QVERIFY(!project.loadProject(path)); QCOMPARE(project.setCount(), 2);
    }

    void midiContinuesWhileGuiThreadIsBlocked()
    {
        QTemporaryDir directory;
        QFile file(directory.filePath(QStringLiteral("smooth.mid")));
        QVERIFY(file.open(QIODevice::WriteOnly)); file.write(midiFixture()); file.close();
        const auto parsed = MarchCraft::parseMidiFile(file.fileName()); QVERIFY(parsed.ok);
        MidiSynthEngine synth;
        if (!synth.available()) QSKIP(qPrintable(synth.status()));
        QVERIFY(synth.load(parsed.document)); synth.play();
        QTRY_VERIFY_WITH_TIMEOUT(synth.positionMs() > 100, 3000);
        const double before = synth.positionMs(); const int underruns = synth.underruns();
        QThread::msleep(500); // Simulate expensive field/QML work without pumping GUI events.
        QVERIFY2(synth.positionMs() >= before + 350, "Audio must advance independently of GUI events");
        QCOMPARE(synth.underruns(), underruns);
        synth.pause(); const double paused = synth.positionMs();
        QTest::qWait(80); QVERIFY(qAbs(synth.positionMs() - paused) < 30);
        synth.seekTick(960); synth.play();
        QTRY_VERIFY_WITH_TIMEOUT(synth.positionMs() > 550, 1500);
        synth.stop();
    }

    void musicSeekSelectionUsesVisiblePerformersAndCorrectPage()
    {
        DrillProject project;
        project.addPerformer(QStringLiteral("A"), QStringLiteral("Trumpet"), QStringLiteral("Brass"), 20, 20);
        project.addPerformer(QStringLiteral("B"), QStringLiteral("Trumpet"), QStringLiteral("Brass"), 60, 20);
        project.addSet(QStringLiteral("Second"), 8);
        project.selectAll(); project.nudgeSelected(40, 0);
        project.setPlaybackSource(QStringLiteral("mute"));
        TransportController transport(&project); transport.navigateToSet(0);
        transport.seekTick(7680);
        QCOMPARE(project.currentSetIndex(), 1);
        project.selectInRect(59, 19, 61, 21, false);
        QVERIFY(project.data(project.index(0), DrillProject::SelectedRole).toBool());
        QVERIFY(!project.data(project.index(1), DrillProject::SelectedRole).toBool());
        transport.seekTick(3840);
        project.selectInPolygon({QPointF(39,19), QPointF(41,19), QPointF(41,21), QPointF(39,21)}, false);
        QCOMPARE(project.selectedCount(), 1);
        QVERIFY(project.data(project.index(0), DrillProject::SelectedRole).toBool());
        QCOMPARE(project.selectedBounds(true).value(QStringLiteral("centerX")).toDouble(), 40.0);
        transport.seekTick(7680); project.nudgeSelected(2, 0);
        transport.navigateToSet(0);
        QCOMPARE(project.data(project.index(0), DrillProject::XRole).toDouble(), 20.0);
        transport.navigateToSet(1);
        QCOMPARE(project.data(project.index(0), DrillProject::XRole).toDouble(), 62.0);
    }

    void transportAdvancesWithoutMidiAndStopsOnProjectReset()
    {
        DrillProject project; project.loadDemo();
        TransportController transport(&project);
        QVERIFY(!project.musicLoaded());
        transport.playCurrentTransition();
        QTRY_VERIFY_WITH_TIMEOUT(transport.currentTick() > 0 && project.playhead() > 0.001, 3000);
        project.newProject();
        QVERIFY(!transport.playing());
        QCOMPARE(transport.currentTick(), qint64(0));
        QTest::qWait(50);
        QCOMPARE(project.performerCount(), 0);
    }

    void pageNavigationKeepsPlaybackAndEditingIndependent()
    {
        QTemporaryDir directory;
        DrillProject project; project.newProject();
        project.addPerformer(QStringLiteral("P1"), QStringLiteral("Trumpet"), QStringLiteral("Brass"), 40, 40);
        project.addSet(QStringLiteral("Second"), 8);
        project.selectAll(); project.nudgeSelected(8, 0);
        project.addSet(QStringLiteral("Third"), 8); project.nudgeSelected(8, 0);
        project.setPlaybackSource(QStringLiteral("mute"));
        QVERIFY(project.saveProject(directory.filePath(QStringLiteral("navigation.marchcraft"))));
        TransportController transport(&project);
        transport.navigateToSet(0); transport.playPause();
        for (int page : {1, 0, 1, 0, 1}) {
            transport.navigateToSet(page);
            QVERIFY(transport.playing());
            QCOMPARE(project.currentSetIndex(), page);
            QVERIFY(qAbs(project.data(project.index(0, 0), DrillProject::XRole).toDouble() - (40 + page * 8)) < 0.001);
            const double target = transport.setPositionMs(page);
            QVERIFY(qAbs(transport.currentMs() - target) < 0.01);
            QTest::qWait(35);
            QVERIFY(transport.currentMs() >= target);
            QVERIFY(transport.currentMs() < target + 250);
            QCOMPARE(project.currentSetIndex(), page);
        }
        QVERIFY(!project.dirty());
        transport.editSet(1);
        QVERIFY(!transport.playing()); QVERIFY(!project.playbackActive());
        QCOMPARE(project.currentSetIndex(), 1);
        QCOMPARE(project.data(project.index(0, 0), DrillProject::XRole).toDouble(), 48.0);
        project.nudgeSelected(1, 0);
        transport.editSet(2);
        QCOMPARE(project.data(project.index(0, 0), DrillProject::XRole).toDouble(), 56.0);
        project.undo(); transport.editSet(1);
        QCOMPARE(project.data(project.index(0, 0), DrillProject::XRole).toDouble(), 48.0);
    }

    void timelineElapsedSeekingAndScrubResume()
    {
        DrillProject project; project.newProject();
        project.addSet(QStringLiteral("Second"), 8); project.addSet(QStringLiteral("Third"), 16);
        project.setPlaybackSource(QStringLiteral("mute")); project.setOpeningBehavior(QStringLiteral("hold"), 4);
        project.setTempoRegion(0, 7680, 120, 120, QStringLiteral("Fast"));
        project.setTempoRegion(7680, 23040, 60, 60, QStringLiteral("Slow"));
        TransportController transport(&project);
        for (double fraction : {0.0, 0.03, 0.2, 0.5, 0.8, 1.0}) {
            transport.seekNormalized(fraction);
            QVERIFY(qAbs(transport.normalizedPosition() - fraction) < 0.0001);
        }
        transport.seekMs(project.openingDurationMs() / 2);
        QCOMPARE(project.playbackSetIndex(), 0);
        const double held = transport.currentMs();
        transport.beginScrub(); transport.seekMs(held); transport.endScrub();
        QVERIFY(!transport.playing());
        transport.playPause(); transport.beginScrub(); QVERIFY(!transport.playing());
        transport.seekMs(transport.setPositionMs(1) + 100);
        transport.endScrub(); QVERIFY(transport.playing());
        QTest::qWait(40); QVERIFY(transport.currentMs() >= transport.setPositionMs(1) + 100);
        transport.stop();
    }

    void navigationExitsTransitionAndPreservesOrExitsLoop()
    {
        DrillProject project; project.newProject();
        project.addSet(QStringLiteral("Second"), 8); project.addSet(QStringLiteral("Third"), 8);
        project.addSet(QStringLiteral("Fourth"), 8); project.setPlaybackSource(QStringLiteral("mute"));
        TransportController transport(&project);
        transport.editSet(1); transport.playCurrentTransition();
        transport.navigateToSet(2);
        QTest::qWait(40); QVERIFY(transport.playing());
        QVERIFY(transport.currentMs() > transport.setPositionMs(2));
        transport.stop(); transport.navigateToSet(0); transport.navigateToSet(2, true);
        transport.toggleLoop(); transport.navigateToSet(1);
        QVERIFY(project.loopEnabled()); QCOMPARE(project.selectedSetStartIndex(), 0); QCOMPARE(project.selectedSetEndIndex(), 2);
        transport.seekMs(transport.setPositionMs(2) - 10); transport.playPause();
        QTest::qWait(65); QVERIFY(transport.currentMs() < transport.setPositionMs(1));
        transport.navigateToSet(3); QVERIFY(!project.loopEnabled()); transport.stop();
    }

    void timelineExtendsBeyondImportedMusicAndMapsAudio()
    {
        QTemporaryDir directory;
        QFile midi(directory.filePath(QStringLiteral("score.mid"))); QVERIFY(midi.open(QIODevice::WriteOnly));
        midi.write(midiFixture()); midi.close();
        DrillProject project; project.newProject(); QVERIFY(project.importMidi(midi.fileName()));
        project.addSet(QStringLiteral("Long drill"), 64);
        TransportController transport(&project);
        QVERIFY(transport.durationMs() > project.musicDurationMs());
        transport.seekNormalized(0.9);
        QVERIFY(transport.currentTick() > 5760);
        QVERIFY(qAbs(transport.normalizedPosition() - 0.9) < 0.001);
        project.setAudioOffsetMs(200);
        QVERIFY(qAbs(transport.audioMsAtShowMs(1200) - 1400) < 0.01);
        project.addAudioAnchor(1000, 960); project.addAudioAnchor(3000, 2880);
        for (double audioMs : {1200.0, 2200.0, 4200.0}) {
            const double show = transport.showMsAtAudioMs(audioMs);
            QVERIFY(qAbs(transport.audioMsAtShowMs(show) - audioMs) < 2.0);
        }
    }

    void rehearsalAudioSeekAndSilentTailWithoutScore()
    {
        QTemporaryDir directory;
        QFile wave(directory.filePath(QStringLiteral("silence.wav")));
        QVERIFY(wave.open(QIODevice::WriteOnly));
        QDataStream out(&wave); out.setByteOrder(QDataStream::LittleEndian);
        const QByteArray samples(16000, char(0)); // One second, mono PCM at 8 kHz.
        out.writeRawData("RIFF", 4); out << quint32(36 + samples.size()); out.writeRawData("WAVEfmt ", 8);
        out << quint32(16) << quint16(1) << quint16(1) << quint32(8000) << quint32(16000) << quint16(2) << quint16(16);
        out.writeRawData("data", 4); out << quint32(samples.size()); out.writeRawData(samples.constData(), samples.size()); wave.close();
        DrillProject project; project.newProject(); project.addSet(QStringLiteral("End"), 8);
        project.attachAudio(wave.fileName());
        TransportController transport(&project);
        QTRY_VERIFY_WITH_TIMEOUT(project.audioDurationMs() > 900, 5000);
        QVERIFY(qAbs(project.audioDurationMs() - 1000) < 2);
        QCOMPARE(project.waveformPeakAtMs(500), 0.0);
        transport.playPause();
        for (double target : {700.0, 100.0, 850.0, 200.0}) {
            transport.seekMs(target);
            QVERIFY(qAbs(transport.currentMs() - target) < 0.01);
            QTest::qWait(80);
            QVERIFY(transport.currentMs() >= target);
            QVERIFY(transport.currentMs() < target + 350);
        }
        transport.seekMs(950); QTest::qWait(250);
        QVERIFY(transport.playing()); QVERIFY(transport.currentMs() > 1050);
        transport.stop();
        project.setAudioOffsetMs(-300);
        QVERIFY(qAbs(transport.showMsAtAudioMs(0) - 300) < 0.01);
        transport.seekMs(0); transport.playPause(); QTest::qWait(380);
        QVERIFY(transport.currentMs() >= 300); transport.stop();
    }

    void onePageHoldAndEmptyShowAreSeekable()
    {
        DrillProject project; project.newProject();
        TransportController transport(&project);
        transport.seekNormalized(0.5); QVERIFY(std::isfinite(transport.currentMs()));
        transport.playPause(); QTest::qWait(35); QVERIFY(!transport.playing());
        project.setOpeningBehavior(QStringLiteral("hold"), 4);
        transport.seekNormalized(0.5); QCOMPARE(project.playbackSetIndex(), 0);
        QVERIFY(qAbs(transport.currentMs() - project.openingDurationMs() / 2) < 0.01);
        transport.playPause(); transport.navigateToSet(0); QVERIFY(transport.playing());
        QVERIFY(transport.currentMs() < 1); transport.stop();
    }

    void undoReturnsToSavedCleanState()
    {
        QTemporaryDir directory;
        DrillProject project;
        project.addPerformer(QStringLiteral("A"), QStringLiteral("Trumpet"), QStringLiteral("Brass"));
        QVERIFY(project.saveProject(directory.filePath(QStringLiteral("clean.marchcraft"))));
        project.selectAll(); project.nudgeSelected(1, 0);
        QVERIFY(project.dirty());
        project.undo(); QVERIFY(!project.dirty());
        project.redo(); QVERIFY(project.dirty());
    }

    void legacyAppearanceSurvivesEditingAndSaving()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("appearance.marchcraft"));
        DrillProject project;
        project.addPerformer(QStringLiteral("A"), QStringLiteral("Trumpet"), QStringLiteral("Brass"));
        project.setPerformerAppearance(0, QStringLiteral("performer.body.tall"),
            QStringLiteral("uniform.custom"), QStringLiteral("skin.deep"), QStringLiteral("instrument.tuba"), 1.91);
        QVERIFY(project.saveProject(path));
        QJsonObject before; QString error;
        QVERIFY(MarchCraft::ProjectStorage::readSqliteProject(path, &before, &error));
        DrillProject restored; QVERIFY(restored.loadProject(path));
        restored.savePerformerDetails(0, QStringLiteral("A"), QStringLiteral("Edited"), QStringLiteral("Trumpet"),
            QStringLiteral("Brass"), QStringLiteral("Preserve appearance"), QStringLiteral("instrument.tuba"));
        restored.undo(); restored.redo();
        QVERIFY(restored.saveProject(path));
        QJsonObject after;
        QVERIFY(MarchCraft::ProjectStorage::readSqliteProject(path, &after, &error));
        QCOMPARE(after.value(QStringLiteral("performers")).toArray().first().toObject().value(QStringLiteral("appearance")),
                 before.value(QStringLiteral("performers")).toArray().first().toObject().value(QStringLiteral("appearance")));
    }

    void malformedMidiIsRejectedAtTrackBoundary()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("bad.mid"));
        QByteArray midi("MThd", 4); append32(midi, 6); append16(midi, 1); append16(midi, 2); append16(midi, 480);
        midi += QByteArrayLiteral("MTrk"); append32(midi, 1); midi.append(char(0x80));
        midi += QByteArrayLiteral("MTrk"); append32(midi, 4); midi += QByteArray::fromHex("00ff2f00");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(midi); file.close();
        const auto result = MarchCraft::parseMidiFile(path);
        QVERIFY(!result.ok);
        QCOMPARE(result.error, QStringLiteral("Invalid MIDI delta time"));
    }

    void transitionTablesHonorDelayAndDegenerateSegments()
    {
        MarchCraft::Placement destination;
        destination.position = {8, 0}; destination.pathType = QStringLiteral("delayed");
        MarchCraft::TransitionPath delayed({}, destination);
        QCOMPARE(delayed.position(0.25), QPointF(0, 0));
        QCOMPARE(delayed.position(0.625), QPointF(4, 0));
        QCOMPARE(delayed.distance(), 8.0);
        destination.pathType = QStringLiteral("follow"); destination.pathPoints = {{0, 0}, {4, 0}, {4, 0}};
        MarchCraft::TransitionPath follow({}, destination);
        QCOMPARE(follow.position(0.5), QPointF(4, 0));
        QCOMPARE(follow.position(1), QPointF(8, 0));
    }

    void asynchronousResultsDoNotCrossProjectBoundaries()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("import.mid"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(midiFixture()); file.close();
        DrillProject project;
        project.importMidiAsync(path);
        project.newProject();
        QTRY_VERIFY_WITH_TIMEOUT(!project.midiImporting(), 5000);
        QCoreApplication::processEvents();
        QVERIFY(!project.musicLoaded());
        project.batchAddPerformers(QStringLiteral("P"), 24, QStringLiteral("Trumpet"), QStringLiteral("Brass"));
        project.selectAll();
        project.requestFormationPreview(QStringLiteral("line"), {}, QStringLiteral("shortestTotal"));
        project.newProject();
        QTest::qWait(300);
        QVERIFY(!project.formationPreviewActive());
        QVERIFY(!project.formationPreviewBusy());
        QCOMPARE(project.performerCount(), 0);
    }

    void performerDialogIsOneUndoableEdit()
    {
        DrillProject project;
        project.savePerformerDetails(-1, QStringLiteral("A"), QStringLiteral("Alex"),
            QStringLiteral("Trumpet"), QStringLiteral("Brass"), QStringLiteral("Notes"), QStringLiteral("instrument.trumpet"));
        QCOMPARE(project.performerInfo(0).value(QStringLiteral("name")).toString(), QStringLiteral("Alex"));
        QCOMPARE(project.performerInfo(0).value(QStringLiteral("notes")).toString(), QStringLiteral("Notes"));
        project.undo(); QCOMPARE(project.performerCount(), 0);
        project.redo(); QCOMPARE(project.performerCount(), 1);
    }

    void dismissedClinicMetadataIsSafe()
    {
        DrillProject project;
        project.addPerformer(QStringLiteral("A"), QStringLiteral("Trumpet"), QStringLiteral("Brass"), 10, 20);
        project.selectAll(); project.addSet(QStringLiteral("Long move"), 8);
        project.nudgeSelected(60, 0);
        const QString id = QStringLiteral("1:stride");
        project.dismissIssue(id);
        const auto issues = project.analyzeTransition();
        for (const auto &issue : issues) QVERIFY(issue.toMap().value(QStringLiteral("id")).toString() != id);
    }

    void playbackOnlyNotifiesMotionRoles()
    {
        DrillProject project; project.loadDemo();
        QSignalSpy changed(&project, &QAbstractItemModel::dataChanged);
        project.setPlayhead(0.25);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(qvariant_cast<QList<int>>(changed.first().at(2)), QList<int>({
            DrillProject::XRole, DrillProject::YRole, DrillProject::FacingRole,
            DrillProject::TravelHeadingRole, DrillProject::TravelStepsPerCountRole,
            DrillProject::LocomotionModeRole, DrillProject::GaitPhaseRole,
            DrillProject::GaitElapsedCountsRole,
            DrillProject::ClosingTransitionRole}));
    }

    void samplePerformance()
    {
        QTemporaryDir directory;
        DrillProject project;
        QElapsedTimer timer;
        timer.start();
        QVERIFY(project.importCoordinateJson(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath(QStringLiteral("../../data/coordinates.json"))));
        const auto loadMs = timer.restart();
        QVERIFY(project.saveProject(directory.filePath(QStringLiteral("sample.marchcraft"))));
        const auto saveMs = timer.restart();
        for (int i = 0; i < project.setCount(); ++i) project.setCurrentSetIndex(i);
        const auto switchMs = timer.restart();
        project.selectAll();
        project.nudgeSelected(0.25, 0);
        const auto editMs = timer.restart();
        project.previewFormation(QStringLiteral("line"), {}, QStringLiteral("balanced"));
        const auto formationMs = timer.restart();
        project.cancelFormationPreview();
        project.setPlaybackActive(true);
        for (int frame = 0; frame < 120; ++frame) {
            project.setPlayhead(frame / 119.0);
            for (int row = 0; row < project.performerCount(); ++row) {
                project.data(project.index(row), DrillProject::XRole);
                project.data(project.index(row), DrillProject::YRole);
                project.data(project.index(row), DrillProject::FacingRole);
            }
        }
        qInfo("Sample timings ms: load=%lld save=%lld sets=%lld edit=%lld formation=%lld playback120=%lld",
              loadMs, saveMs, switchMs, editMs, formationMs, timer.elapsed());
    }

    void cleanStartupAndExplicitDemo()
    {
        DrillProject project;
        QCOMPARE(project.performerCount(), 0);
        QCOMPARE(project.setCount(), 1);
        QCOMPARE(project.showName(), QStringLiteral("Untitled Show"));
        QVERIFY(!project.dirty());

        project.loadDemo();
        QCOMPARE(project.performerCount(), 24);
        QCOMPARE(project.setCount(), 4);
        QVERIFY(!project.dirty());
        QVERIFY(project.averageDistance() > 0.0);
    }

    void regulationFieldGeometry()
    {
        const auto highSchool = MarchCraft::fieldGeometry(QStringLiteral("hs"));
        const auto college = MarchCraft::fieldGeometry(QStringLiteral("college"));
        const auto professional = MarchCraft::fieldGeometry(QStringLiteral("nfl"));

        auto closeTo = [](double actual, double expected) {
            return qAbs(actual - expected) < 0.0001;
        };
        QVERIFY(closeTo(highSchool.depth, 256.0 / 3.0));
        QVERIFY(closeTo(highSchool.frontHash, 256.0 / 9.0));
        QVERIFY(closeTo(highSchool.backHash, 512.0 / 9.0));
        QVERIFY(closeTo(college.frontHash, 32.0));
        QVERIFY(closeTo(college.backHash, 160.0 / 3.0));
        QVERIFY(closeTo(professional.frontHash, 566.0 / 15.0));
        QVERIFY(closeTo(professional.backHash, 47.6));

        DrillProject project;
        QCOMPARE(project.fieldInsertCount(), 80);
        for (int span = 0; span < 20; ++span) {
            for (int insert = 0; insert < 4; ++insert) {
                const int column = span * 4 + insert;
                const double expected = span * 8.0 + (insert + 1) * 1.6;
                QVERIFY(closeTo(project.fieldInsertStep(column), expected));
            }
        }
        QCOMPARE(project.fieldInsertStep(-1), -1.0);
        QCOMPARE(project.fieldInsertStep(80), -1.0);
    }

    void fieldTransformUsesMeterWorldAndGroundPlane()
    {
        const double depth = MarchCraft::fieldGeometry(QStringLiteral("hs")).depth;
        const QPointF drillPosition{80.0, depth / 2.0};
        const QVector3D world = MarchCraft::FieldTransform::drillToWorld(drillPosition, depth);
        QCOMPARE(world, QVector3D(0, 0, 0));
        const QPointF sideOne = MarchCraft::FieldTransform::worldToDrill(
            QVector3D(-45.72f, 7.0f, static_cast<float>(depth / 2.0 * MarchCraft::FieldTransform::MetersPerStep)), depth);
        QVERIFY(qAbs(sideOne.x()) < 0.0001);
        QVERIFY(qAbs(sideOne.y()) < 0.0001);
        QCOMPARE(MarchCraft::FieldTransform::drillToWorld(sideOne, depth).y(), 0.0f);
    }

    void builtInAssetCatalogHonorsGroundContract()
    {
        AssetCatalog catalog;
        QVERIFY2(catalog.valid(), qPrintable(catalog.validationErrors().join(QStringLiteral("\n"))));
        QVERIFY(catalog.instruments().size() >= 4);
        QVERIFY(catalog.props().size() >= 4);
        QVERIFY(catalog.venues().size() >= 5);
        QVERIFY(catalog.contains(QStringLiteral("performer.body.standard")));
        QVERIFY(!catalog.bodyRigs().isEmpty());
        QCOMPARE(catalog.asset(QStringLiteral("missing.asset")).value(QStringLiteral("id")).toString(),
                 QStringLiteral("venue.rehearsal"));
    }

    void demoHasUsableContent()
    {
        DrillProject project;
        project.loadDemo();
        QCOMPARE(project.performerCount(), 24);
        QCOMPARE(project.setCount(), 4);
        QVERIFY(project.averageDistance() > 0.0);
    }

    void groupingCapabilitiesAndInvalidNoOp()
    {
        DrillProject project;
        project.newProject();
        project.batchAddPerformers(QStringLiteral("G"), 3, QStringLiteral("Trumpet"),
                                   QStringLiteral("Brass"));
        project.selectAll();
        QVERIFY(project.canGroupSelection());
        QVERIFY(!project.canRemoveSelectionFromGroup());
        QVERIFY(!project.canUngroupSelection());

        project.groupSelected();
        QVERIFY(project.selectionIsExactGroup());
        QVERIFY(!project.canGroupSelection());
        QVERIFY(!project.canRemoveSelectionFromGroup());
        QVERIFY(project.canUngroupSelection());

        project.groupSelected();
        project.undo();
        QVERIFY(project.performerGroupInfo(0).isEmpty());

        project.selectAll();
        project.groupSelected();
        project.selectPerformerMode(1, 1);
        project.selectPerformerMode(2, 1);
        QVERIFY(project.canRemoveSelectionFromGroup());
        project.removeSelectedFromGroup();
        QVERIFY(project.performerGroupInfo(0).isEmpty());
        QCOMPARE(project.performerGroupInfo(1).value(QStringLiteral("size")).toInt(), 2);

        project.selectPerformerMode(1, 0);
        QCOMPARE(project.selectedCount(), 2);
        QVERIFY(project.canUngroupSelection());
        project.ungroupSelected();
        QVERIFY(project.performerGroupInfo(1).isEmpty());
    }

    void recentProjectsPersistDeduplicateAndPrune()
    {
        QSettings settings(QSettings::NativeFormat, QSettings::UserScope,
                           QStringLiteral("MarchCraft"), QStringLiteral("MarchCraft"));
        const QVariant previousRecent = settings.value(QStringLiteral("workspace/recentProjects"));
        const QVariant previousSound = settings.value(QStringLiteral("workspace/startupSoundEnabled"));
        settings.remove(QStringLiteral("workspace/recentProjects"));
        settings.remove(QStringLiteral("workspace/startupSoundEnabled"));
        settings.sync();

        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QStringList paths;
        for (int index = 0; index < 10; ++index) {
            const QString path = temporary.filePath(QStringLiteral("project-%1.marchcraft").arg(index));
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("placeholder");
            file.close();
            paths.push_back(path);
        }

        {
            WorkspaceController workspace;
            QVERIFY(workspace.startupSoundEnabled());
            const bool systemAnimationsEnabled = workspace.systemAnimationsEnabled();
            workspace.refreshSystemPreferences();
            QCOMPARE(workspace.systemAnimationsEnabled(), systemAnimationsEnabled);
            for (int index = 0; index < paths.size(); ++index)
                workspace.recordRecentProject(paths[index], QStringLiteral("Project %1").arg(index));
            QCOMPARE(workspace.recentProjects().size(), 8);
            QCOMPARE(workspace.recentProjects().first().toMap().value(QStringLiteral("name")).toString(),
                     QStringLiteral("Project 9"));
            workspace.recordRecentProject(paths[5], QStringLiteral("Renamed Five"));
            QCOMPARE(workspace.recentProjects().size(), 8);
            QCOMPARE(workspace.recentProjects().first().toMap().value(QStringLiteral("name")).toString(),
                     QStringLiteral("Renamed Five"));
            workspace.setStartupSoundEnabled(false);
        }

        QFile::remove(paths[5]);
        {
            WorkspaceController restored;
            QVERIFY(!restored.startupSoundEnabled());
            QCOMPARE(restored.recentProjects().size(), 7);
            restored.removeRecentProject(paths[9]);
            QCOMPARE(restored.recentProjects().size(), 6);
        }

        if (previousRecent.isValid()) settings.setValue(QStringLiteral("workspace/recentProjects"), previousRecent);
        else settings.remove(QStringLiteral("workspace/recentProjects"));
        if (previousSound.isValid()) settings.setValue(QStringLiteral("workspace/startupSoundEnabled"), previousSound);
        else settings.remove(QStringLiteral("workspace/startupSoundEnabled"));
    }

    void movementDistanceAndUndo()
    {
        DrillProject project;
        project.newProject();
        project.addPerformer(QStringLiteral("T01"), QStringLiteral("Trumpet"),
                             QStringLiteral("Brass"), 20.0, 20.0);
        project.addSet(QStringLiteral("Set 2"), 8);
        project.selectPerformer(0, false);
        project.beginMove(0, false);
        project.previewMove(3.0, 4.0);
        project.endMove();
        QCOMPARE(project.data(project.index(0, 0), DrillProject::XRole).toDouble(), 23.0);
        QCOMPARE(project.data(project.index(0, 0), DrillProject::YRole).toDouble(), 24.0);
        project.setPlaybackActive(true);
        project.setPlayhead(0.0);
        QCOMPARE(project.data(project.index(0, 0), DrillProject::XRole).toDouble(), 20.0);
        QCOMPARE(project.data(project.index(0, 0), DrillProject::YRole).toDouble(), 20.0);
        project.setPlaybackActive(false);
        QCOMPARE(project.data(project.index(0, 0), DrillProject::XRole).toDouble(), 23.0);
        QCOMPARE(project.data(project.index(0, 0), DrillProject::SetDistanceRole).toDouble(), 5.0);
        project.undo();
        QCOMPARE(project.data(project.index(0, 0), DrillProject::SetDistanceRole).toDouble(), 0.0);
        project.redo();
        QCOMPARE(project.data(project.index(0, 0), DrillProject::SetDistanceRole).toDouble(), 5.0);
    }

    void audienceCoordinateSemantics()
    {
        DrillProject project;
        project.newProject();
        const auto geometry = MarchCraft::fieldGeometry(QStringLiteral("hs"));
        project.addPerformer(QStringLiteral("F"), QStringLiteral("Brass"),
                             QStringLiteral("Brass"), 64.0, geometry.frontHash - 2.0);
        project.addPerformer(QStringLiteral("B"), QStringLiteral("Brass"),
                             QStringLiteral("Brass"), 96.0, geometry.backHash - 2.0);
        project.addPerformer(QStringLiteral("R"), QStringLiteral("Brass"),
                             QStringLiteral("Brass"), 96.0, geometry.backHash + 2.0);
        QCOMPARE(project.coordinateFor(0), QStringLiteral("Side 1: On 40 yard line · 2 steps in front of front hash"));
        QCOMPARE(project.coordinateFor(1), QStringLiteral("Side 2: On 40 yard line · 2 steps in front of back hash"));
        QCOMPARE(project.coordinateFor(2), QStringLiteral("Side 2: On 40 yard line · 2 steps behind back hash"));
    }

    void formationAssignmentPreferencePersists()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString organization = QCoreApplication::organizationName();
        const QString application = QCoreApplication::applicationName();
        QCoreApplication::setOrganizationName(QStringLiteral("MarchCraftTests"));
        QCoreApplication::setApplicationName(QFileInfo(temporary.path()).fileName());
        const auto restoreSettings = qScopeGuard([&] {
            QSettings().clear();
            QCoreApplication::setOrganizationName(organization);
            QCoreApplication::setApplicationName(application);
        });
        DrillProject project;
        const QStringList modes{QStringLiteral("shortest"), QStringLiteral("preserveOrder"),
            QStringLiteral("evenEffort"), QStringLiteral("featureMove"), QStringLiteral("rosterOrder"),
            QStringLiteral("rehearsalSafe")};
        QSignalSpy settingsChanged(&project, &DrillProject::editorSettingsChanged);
        for (const auto &mode : modes) {
            project.setFormationAssignmentMode(mode);
            QCOMPARE(project.formationAssignmentMode(), mode);
            QCOMPARE(QSettings().value(QStringLiteral("formation/assignmentMode")).toString(), mode);
            DrillProject reopened;
            QCOMPARE(reopened.formationAssignmentMode(), mode);
            reopened.newProject();
            QCOMPARE(reopened.formationAssignmentMode(), mode);
        }
        QCOMPARE(settingsChanged.size(), modes.size());
        project.setFormationAssignmentMode(QStringLiteral("invalid"));
        QCOMPARE(project.formationAssignmentMode(), QStringLiteral("rehearsalSafe"));
        QCOMPARE(settingsChanged.size(), modes.size());
    }

    void fieldStylePreference()
    {
        QTemporaryDir temporary;
        const QString organization = QCoreApplication::organizationName();
        const QString application = QCoreApplication::applicationName();
        QCoreApplication::setOrganizationName(QStringLiteral("MarchCraftFieldStyleTest"));
        QCoreApplication::setApplicationName(QFileInfo(temporary.path()).fileName());
        const auto restoreSettings = qScopeGuard([&] {
            QSettings().clear();
            QCoreApplication::setOrganizationName(organization);
            QCoreApplication::setApplicationName(application);
        });
        DrillProject project;
        project.newProject();
        QCOMPARE(project.fieldStyle(), QStringLiteral("realistic"));
        const bool dirty = project.dirty();
        const bool undo = project.canUndo();
        const double width = project.fieldWidthSteps();
        QSignalSpy changed(&project, &DrillProject::editorSettingsChanged);
        project.setFieldStyle(QStringLiteral("editor"));
        QCOMPARE(changed.size(), 1);
        QCOMPARE(project.fieldStyle(), QStringLiteral("editor"));
        QCOMPARE(project.dirty(), dirty);
        QCOMPARE(project.canUndo(), undo);
        QCOMPARE(project.fieldWidthSteps(), width);
        project.setFieldStyle(QStringLiteral("editor"));
        project.setFieldStyle(QStringLiteral("invalid"));
        QCOMPARE(changed.size(), 1);
        DrillProject reopened;
        QCOMPARE(reopened.fieldStyle(), QStringLiteral("editor"));
        reopened.newProject();
        QCOMPARE(reopened.fieldStyle(), QStringLiteral("editor"));
        project.setFieldStyle(QStringLiteral("realistic"));
        QCOMPARE(changed.size(), 2);
        // A hidden optional overlay must not suppress or coarsen the editor graph.
        project.setShowFieldGrid(false);
        project.setFieldGridInterval(4);
        WorkspaceController workspace;
        QQuickView view;
        view.rootContext()->setContextProperty(QStringLiteral("workspaceController"), &workspace);
        view.rootContext()->setContextProperty(QStringLiteral("drillProject"), &project);
        view.setSource(QUrl::fromLocalFile(QFINDTESTDATA("../qml/FieldView.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        auto *grid = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("fieldStepGrid"));
        QVERIFY(grid);
        QVERIFY(!grid->isVisible());
        QCOMPARE(grid->property("stepInterval").toDouble(), 4.0);
        project.setFieldStyle(QStringLiteral("editor"));
        QVERIFY(grid->isVisible());
        QCOMPARE(grid->property("stepInterval").toDouble(), 1.0);
        QCOMPARE(8 * grid->property("stepInterval").toDouble() * MarchCraft::FieldTransform::MetersPerStep, 5 * 0.9144);
        project.setFieldStyle(QStringLiteral("realistic"));
        QVERIFY(!grid->isVisible());
        QCOMPARE(grid->property("stepInterval").toDouble(), 4.0);
        QSettings().setValue(QStringLiteral("view/fieldStyle"), QStringLiteral("invalid"));
        DrillProject invalid;
        QCOMPARE(invalid.fieldStyle(), QStringLiteral("realistic"));
    }

    void shapeDrawerMouseGestures_data()
    {
        QTest::addColumn<QString>("kind");
        for (const char *kind : {"line", "rectangle", "circle", "arc", "ellipse", "triangle",
                                 "diamond", "polygon", "star", "spiral", "block"})
            QTest::newRow(kind) << QString::fromLatin1(kind);
    }

    void shapeDrawerMouseGestures()
    {
        QFETCH(QString, kind);
        DrillProject project; project.newProject();
        project.batchAddPerformers(QStringLiteral("P"), 12, QStringLiteral("Guard"), QStringLiteral("Guard"));
        project.selectAll();
        WorkspaceController workspace;
        TransportController transport(&project);
        QQuickView view;
        view.rootContext()->setContextProperty(QStringLiteral("transport"), &transport);
        view.rootContext()->setContextProperty(QStringLiteral("workspaceController"), &workspace);
        view.rootContext()->setContextProperty(QStringLiteral("drillProject"), &project);
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.resize(900, 560);
        view.setSource(QUrl::fromLocalFile(QFINDTESTDATA("../qml/FieldView.qml")));
        QCOMPARE(view.status(), QQuickView::Ready);
        view.show(); QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto *root = view.rootObject();
        auto *area = root->findChild<QQuickItem *>(QStringLiteral("shapeDrawingArea"));
        QVERIFY(area);
        QSignalSpy completed(root, SIGNAL(shapeCompleted(QString,QVariant)));
        QSignalSpy canceled(root, SIGNAL(shapeDrawingCanceled()));
        QVERIFY(completed.isValid()); QVERIFY(canceled.isValid());
        for (double zoom : {0.7, 1.0, 2.0}) {
            root->setProperty("zoom", zoom);
            root->setProperty("shapeDrawMode", kind);
            QTest::qWait(10);
            const QPoint start = area->mapToScene(QPointF(area->width() * 0.25, area->height() * 0.25)).toPoint();
            const QPoint end = start + QPoint(95, 48);
            for (bool reversed : {false, true}) {
                completed.clear();
                QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, reversed ? end : start);
                QTest::mouseMove(&view, reversed ? start : end, 20);
                QTest::qWait(20);
                const QString captureDirectory = qEnvironmentVariable("MARCHCRAFT_QA_OUTPUT");
                if (!captureDirectory.isEmpty() && zoom == 1.0 && !reversed) {
                    QTest::qWait(100);
                    QVERIFY(view.grabWindow().save(captureDirectory + QLatin1Char('/') + kind + QStringLiteral(".png")));
                }
                QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, reversed ? start : end);
                QCOMPARE(completed.size(), 1);
                QCOMPARE(completed.first().at(0).toString(), kind);
                const auto options = qvariant_cast<QJSValue>(completed.first().at(1)).toVariant().toMap();
                QVERIFY(!options.isEmpty());
                QCOMPARE(project.formationGeometry(kind, options).value(QStringLiteral("placements")).toList().size(), 12);
                QCOMPARE(project.currentShapeCount(), 0);
            }
            completed.clear();
            QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, start);
            QCOMPARE(completed.size(), 0);
            QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, start);
            QTest::mouseMove(&view, end, 20);
            QTest::keyClick(&view, Qt::Key_Escape);
            QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, end);
            QCOMPARE(completed.size(), 0);
            QVERIFY(!area->property("selecting").toBool());
            QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, start);
            QTest::mouseMove(&view, end, 20);
            root->setProperty("shapeDrawMode", QString());
            QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, end);
            QCOMPARE(completed.size(), 0);
            root->setProperty("shapeDrawMode", kind);
            QTest::mouseClick(&view, Qt::RightButton, Qt::NoModifier, start);
            QVERIFY(!area->property("selecting").toBool());
        }
        QCOMPARE(canceled.size(), 6);
    }

    void shapeDrawerGeometryAndTransactions_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<QString>("mode");
        QTest::addColumn<int>("count");
        const QStringList kinds{QStringLiteral("line"), QStringLiteral("rectangle"), QStringLiteral("circle"),
            QStringLiteral("arc"), QStringLiteral("ellipse"), QStringLiteral("triangle"), QStringLiteral("diamond"),
            QStringLiteral("polygon"), QStringLiteral("star"), QStringLiteral("spiral"), QStringLiteral("block")};
        const QStringList modes{QStringLiteral("rehearsalSafe"), QStringLiteral("shortest"),
            QStringLiteral("preserveOrder"), QStringLiteral("evenEffort"), QStringLiteral("featureMove"), QStringLiteral("rosterOrder")};
        for (const auto &kind : kinds) for (const auto &mode : modes) for (int count : {1, 7, 24})
            QTest::newRow(qPrintable(kind + QLatin1Char('-') + mode + QString::number(count))) << kind << mode << count;
    }

    void shapeDrawerGeometryAndTransactions()
    {
        QFETCH(QString, kind); QFETCH(QString, mode); QFETCH(int, count);
        DrillProject project; project.newProject();
        project.batchAddPerformers(QStringLiteral("P"), count, QStringLiteral("Guard"), QStringLiteral("Guard"));
        project.selectAll(); project.addSet(QStringLiteral("Destination"), 16);
        const QVariantMap options{{QStringLiteral("centerX"), 0.0}, {QStringLiteral("centerY"), 0.0},
            {QStringLiteral("width"), 190.0}, {QStringLiteral("height"), 90.0},
            {QStringLiteral("radius"), 70.0}, {QStringLiteral("outerRadius"), 60.0},
            {QStringLiteral("rotation"), 37.0}, {QStringLiteral("sweepAngle"), -230.0},
            {QStringLiteral("rows"), 3}, {QStringLiteral("createGroup"), true}};
        const auto state = [&] {
            QVariantList result;
            for (int row = 0; row < count; ++row)
                result.push_back(QPointF(project.data(project.index(row, 0), DrillProject::XRole).toDouble(),
                                        project.data(project.index(row, 0), DrillProject::YRole).toDouble()));
            for (int i = 0; i < project.currentShapeCount(); ++i) result.push_back(project.shapeInfo(i));
            return result;
        };
        const auto before = state();
        const auto geometry = project.formationGeometry(kind, options);
        const auto destinations = geometry.value(QStringLiteral("placements")).toList();
        QCOMPARE(destinations.size(), count);
        QCOMPARE(state(), before);
        QVERIFY(!project.formationPreviewActive());
        for (const auto &value : destinations) {
            const auto point = value.toPointF();
            QVERIFY(std::isfinite(point.x()) && std::isfinite(point.y()));
            QVERIFY(point.x() >= project.canvasMinX() && point.x() <= project.canvasMaxX());
            QVERIFY(point.y() >= project.canvasMinY() && point.y() <= project.canvasMaxY());
        }
        project.requestFormationPreview(kind, options, mode);
        QTRY_VERIFY_WITH_TIMEOUT(project.formationPreviewActive(), 10000);
        QCOMPARE(state(), before);
        auto remaining = destinations;
        for (const auto &value : project.formationPreviewPoints()) {
            const auto point = value.toMap();
            const QPointF target(point.value(QStringLiteral("x")).toDouble(), point.value(QStringLiteral("y")).toDouble());
            const auto it = std::find_if(remaining.begin(), remaining.end(), [&](const auto &v) {
                return QLineF(v.toPointF(), target).length() < 0.00001;
            });
            QVERIFY(it != remaining.end()); remaining.erase(it);
        }
        QVERIFY(remaining.isEmpty());
        QVERIFY(project.commitFormationPreview());
        QCOMPARE(project.currentShapeCount(), 1);
        QCOMPARE(project.shapeInfo(0).value(QStringLiteral("memberCount")).toInt(), count);
        const auto after = state();
        project.undo(); QCOMPARE(state(), before);
        project.redo(); QCOMPARE(state(), after);
        project.requestFormationPreview(kind, options, mode);
        project.cancelFormationPreview();
        QTest::qWait(20);
        QVERIFY(!project.formationPreviewActive()); QVERIFY(!project.formationPreviewBusy());
        QVERIFY(!project.commitFormationPreview()); QCOMPARE(state(), after);
    }

    void formationDistribution()
    {
        DrillProject project;
        project.newProject();
        project.batchAddPerformers(QStringLiteral("C"), 3, QStringLiteral("Clarinet"),
                                   QStringLiteral("Woodwinds"));
        project.selectAll();
        project.distributeLine(10.0, 20.0, 30.0, 20.0);
        QCOMPARE(project.data(project.index(0, 0), DrillProject::XRole).toDouble(), 10.0);
        QCOMPARE(project.data(project.index(1, 0), DrillProject::XRole).toDouble(), 20.0);
        QCOMPARE(project.data(project.index(2, 0), DrillProject::XRole).toDouble(), 30.0);
    }

    void rectangleAndRangeSelection()
    {
        DrillProject project;
        project.newProject();
        project.addPerformer(QStringLiteral("A"), QStringLiteral("Trumpet"),
                             QStringLiteral("Brass"), 10.0, 10.0);
        project.addPerformer(QStringLiteral("B"), QStringLiteral("Trumpet"),
                             QStringLiteral("Brass"), 20.0, 20.0);
        project.addPerformer(QStringLiteral("C"), QStringLiteral("Trumpet"),
                             QStringLiteral("Brass"), 30.0, 30.0);

        project.selectInRect(5.0, 5.0, 22.0, 22.0, false);
        QCOMPARE(project.selectedCount(), 2);
        project.selectPerformerRange(1, 2, false);
        QCOMPARE(project.selectedCount(), 2);
        project.selectPerformerMode(0, 1);
        QCOMPARE(project.selectedCount(), 3);

        QVariantList polygon;
        polygon << QPointF(25.0, 25.0) << QPointF(35.0, 25.0)
                << QPointF(35.0, 35.0) << QPointF(25.0, 35.0);
        project.selectInPolygon(polygon, false);
        QCOMPARE(project.selectedCount(), 1);
    }

    void setVariantsAndArchive()
    {
        DrillProject project;
        project.newProject();
        project.addPerformer(QStringLiteral("T01"), QStringLiteral("Trumpet"),
                             QStringLiteral("Brass"), 20.0, 20.0);
        project.addSet(QStringLiteral("Set 2"), 8);
        QCOMPARE(project.setCount(), 2);
        QCOMPARE(project.currentVariantCount(), 1);

        project.createVariant(QStringLiteral("Set 2 alternate"), QStringLiteral("Candidate B"));
        QCOMPARE(project.currentVariantCount(), 2);
        QCOMPARE(project.currentVariantIndex(), 1);
        QCOMPARE(project.setInfo(1).value(QStringLiteral("caption")).toString(),
                 QStringLiteral("Candidate B"));

        project.archiveCurrentVariant();
        QCOMPARE(project.currentVariantCount(), 1);
        QCOMPARE(project.setInfo(1).value(QStringLiteral("archivedVariantCount")).toInt(), 1);
        project.restoreArchivedVariant(0);
        QCOMPARE(project.currentVariantCount(), 2);

        project.archiveCurrentSet();
        QCOMPARE(project.setCount(), 1);
        QCOMPARE(project.archivedSetCount(), 1);
        project.restoreArchivedSet(0);
        QCOMPARE(project.setCount(), 2);
        QCOMPARE(project.archivedSetCount(), 0);
        QCOMPARE(project.currentVariantCount(), 2);
    }

    void projectRoundTrip()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString path = temporary.filePath(QStringLiteral("show.marchcraft"));
        DrillProject source;
        source.newProject();
        source.setShowName(QStringLiteral("Round Trip"));
        source.addPerformer(QStringLiteral("G01"), QStringLiteral("Guard"),
                            QStringLiteral("Guard"), 44.0, 18.0);
        source.selectAll();
        source.createFormation(QStringLiteral("star"), {{QStringLiteral("points"), 7},
            {QStringLiteral("rotation"), 15.0}});
        QVERIFY(source.saveProject(path));

        DrillProject destination;
        QVERIFY(destination.loadProject(path));
        QCOMPARE(destination.showName(), QStringLiteral("Round Trip"));
        QCOMPARE(destination.performerCount(), 1);
        QCOMPARE(destination.coordinateFor(0), source.coordinateFor(0));
        QCOMPARE(destination.meterRegionCount(), source.meterRegionCount());
        QCOMPARE(destination.currentShapeCount(), 1);
        QCOMPARE(destination.shapeInfo(0).value(QStringLiteral("type")).toString(), QStringLiteral("star"));
        QCOMPARE(destination.shapeInfo(0).value(QStringLiteral("parameters")).toMap()
                     .value(QStringLiteral("points")).toInt(), 7);
        QFile database(path);
        QVERIFY(database.open(QIODevice::ReadOnly));
        QCOMPARE(database.read(15), QByteArray("SQLite format 3"));
    }

    void structuredTimingAndTempoRamp()
    {
        DrillProject project;
        project.newProject();
        project.addSet(QStringLiteral("Set 2"), 16);
        QCOMPARE(project.currentSetCounts(), 16);
        QCOMPARE(qRound(project.transitionDurationMs(1)), 8000); // 16 quarters at 120 BPM

        project.setMeterRegion(0, 5760, 6, 8, 480, QStringLiteral("1+1+1+1+1+1"));
        project.setCurrentSetCounts(12);
        QCOMPARE(project.currentSetCounts(), 12);
        project.setMeterRegion(0, 5760, 6, 8, 1440, QStringLiteral("3+3"));
        project.setCurrentSetCounts(4);
        QCOMPARE(project.currentSetCounts(), 4);

        project.setTempoRegion(0, 5760, 120.0, 180.0, QStringLiteral("Accelerando"));
        QVERIFY(project.transitionDurationMs(1) > 2000.0);
        QVERIFY(project.transitionDurationMs(1) < 3000.0);
        QVERIFY(project.effectiveTempoText(1).contains(QStringLiteral("120")));
    }

    void persistentShapesAndTransitionPaths()
    {
        DrillProject project;
        project.newProject();
        project.batchAddPerformers(QStringLiteral("P"), 4, QStringLiteral("Trumpet"), QStringLiteral("Brass"));
        project.selectAll();
        project.distributeLine(20, 20, 60, 20);
        QCOMPARE(project.currentShapeCount(), 1);
        QCOMPARE(project.shapeInfo(0).value(QStringLiteral("memberCount")).toInt(), 4);
        project.addSet(QStringLiteral("Set 2"), 8);
        project.nudgeSelected(0, 20);
        const double direct = project.data(project.index(0, 0), DrillProject::SetDistanceRole).toDouble();
        project.setSelectedTransitionPath(QStringLiteral("curved"), {QPointF(5, 30)});
        const double curved = project.data(project.index(0, 0), DrillProject::SetDistanceRole).toDouble();
        QVERIFY(curved > direct);
        QCOMPARE(project.transitionPathSamples(0, 12).size(), 13);
    }

    void closedShapesNeverDuplicateEndpoints()
    {
        DrillProject project;
        project.newProject();
        project.batchAddPerformers(QStringLiteral("C"), 500, QStringLiteral("Clarinet"), QStringLiteral("Winds"));
        project.selectAll();
        project.createFormation(QStringLiteral("circle"), {{QStringLiteral("centerX"), 80.0},
            {QStringLiteral("centerY"), 42.0}, {QStringLiteral("radius"), 28.0}});
        QSet<QString> coordinates;
        for (int row = 0; row < project.performerCount(); ++row) {
            const double x = project.data(project.index(row, 0), DrillProject::XRole).toDouble();
            const double y = project.data(project.index(row, 0), DrillProject::YRole).toDouble();
            coordinates.insert(QStringLiteral("%1,%2").arg(x, 0, 'f', 5).arg(y, 0, 'f', 5));
        }
        QCOMPARE(coordinates.size(), project.performerCount());

        project.createFormation(QStringLiteral("arc"), {{QStringLiteral("centerX"), 80.0},
            {QStringLiteral("centerY"), 42.0}, {QStringLiteral("radius"), 20.0},
            {QStringLiteral("startAngle"), 0.0}, {QStringLiteral("sweepAngle"), 180.0}});
        QVERIFY(qAbs(project.data(project.index(0, 0), DrillProject::XRole).toDouble() - 100.0) < 0.01);
        QVERIFY(qAbs(project.data(project.index(499, 0), DrillProject::XRole).toDouble() - 60.0) < 0.01);
    }

    void formationFitsWithoutCornerCollapse()
    {
        DrillProject project;
        project.newProject();
        project.batchAddPerformers(QStringLiteral("P"), 80, QStringLiteral("Trumpet"), QStringLiteral("Brass"));
        project.selectAll();
        project.createFormation(QStringLiteral("circle"), {{QStringLiteral("centerX"), 0.0},
            {QStringLiteral("centerY"), 0.0}, {QStringLiteral("radius"), 125.0},
            {QStringLiteral("width"), 250.0}, {QStringLiteral("height"), 250.0}});
        QSet<QString> coordinates;
        for (int row = 0; row < project.performerCount(); ++row) {
            const double x = project.data(project.index(row, 0), DrillProject::XRole).toDouble();
            const double y = project.data(project.index(row, 0), DrillProject::YRole).toDouble();
            QVERIFY(x >= project.canvasMinX() + 1.99 && x <= project.canvasMaxX() - 1.99);
            QVERIFY(y >= project.canvasMinY() + 1.99 && y <= project.canvasMaxY() - 1.99);
            coordinates.insert(QStringLiteral("%1,%2").arg(x, 0, 'f', 4).arg(y, 0, 'f', 4));
        }
        QCOMPARE(coordinates.size(), project.performerCount());
    }

    void commonShapePackAndGuideMovement()
    {
        const QStringList shapes{QStringLiteral("ellipse"), QStringLiteral("triangle"),
            QStringLiteral("diamond"), QStringLiteral("polygon"), QStringLiteral("star"),
            QStringLiteral("spiral"), QStringLiteral("block")};
        for (const auto &shape : shapes) {
            DrillProject project; project.newProject();
            project.batchAddPerformers(QStringLiteral("P"), 12, QStringLiteral("Guard"), QStringLiteral("Guard"));
            project.selectAll(); project.createFormation(shape);
            QCOMPARE(project.currentShapeCount(), 1);
        }
        DrillProject project; project.newProject();
        project.batchAddPerformers(QStringLiteral("P"), 6, QStringLiteral("Guard"), QStringLiteral("Guard"));
        project.selectAll(); project.createFormation(QStringLiteral("circle"));
        const QPointF before = project.shapeInfo(0).value(QStringLiteral("anchor")).toPointF();
        project.nudgeSelected(4, 2);
        QCOMPARE(project.shapeInfo(0).value(QStringLiteral("anchor")).toPointF(), before + QPointF(4, 2));
        project.clearSelection(); project.selectPerformerMode(0, 1);
        project.nudgeSelected(1, 0);
        QCOMPARE(project.shapeInfo(0).value(QStringLiteral("type")).toString(), QStringLiteral("detached"));
        QCOMPARE(project.shapeInfo(0).value(QStringLiteral("memberCount")).toInt(), 5);
    }

    void lockedAndHiddenPerformersAreNotSelectable()
    {
        DrillProject project;
        project.newProject();
        project.batchAddPerformers(QStringLiteral("P"), 3, QStringLiteral("Trumpet"), QStringLiteral("Brass"));
        project.selectPerformer(0, false);
        project.updateSelectedPerformers({}, {}, {}, 0, -1, 1);
        QCOMPARE(project.selectedCount(), 0);
        project.selectAll();
        QCOMPARE(project.selectedCount(), 2);
        project.updateSelectedPerformers({}, {}, {}, 0, 0, -1);
        QCOMPARE(project.selectedCount(), 0);
        project.selectInRect(0, 0, 160, 90, false);
        QCOMPARE(project.selectedCount(), 0);
    }

    void performerFacingIsPerSetAndInterpolatesShortestTurn()
    {
        DrillProject project; project.newProject();
        project.addPerformer(QStringLiteral("P1"), QStringLiteral("Trumpet"), QStringLiteral("Brass"));
        project.selectAll(); project.faceSelected(350.0);
        QCOMPARE(project.data(project.index(0, 0), DrillProject::FacingRole).toDouble(), 350.0);
        project.addSet(QStringLiteral("Set 2"), 8);
        project.faceSelected(10.0);
        QCOMPARE(project.data(project.index(0, 0), DrillProject::FacingRole).toDouble(), 10.0);
        project.setPlaybackActive(true); project.setPlayhead(0.5);
        QVERIFY(closeTo(project.data(project.index(0, 0), DrillProject::FacingRole).toDouble(), 0.0));
        project.setPlaybackActive(false); project.setCurrentSetIndex(0);
        QCOMPARE(project.data(project.index(0, 0), DrillProject::FacingRole).toDouble(), 350.0);

        project.setMarkerGeometry(QStringLiteral("dot"));
        QCOMPARE(project.markerGeometry(), QStringLiteral("dot"));
    }

    void variantGroupsClickAndPersistence()
    {
        QTemporaryDir temporary;
        const QString path = temporary.filePath(QStringLiteral("groups.marchcraft"));
        DrillProject project; project.newProject();
        project.batchAddPerformers(QStringLiteral("G"), 4, QStringLiteral("Trumpet"), QStringLiteral("Brass"));
        project.selectAll();
        project.createFormation(QStringLiteral("spiral"), {{QStringLiteral("turns"), 2.5},
            {QStringLiteral("clockwise"), true}, {QStringLiteral("createGroup"), true}});
        project.clearSelection(); project.selectPerformerMode(0, 0);
        QCOMPARE(project.selectedCount(), 4);
        project.selectPerformerMode(0, 1); // Ctrl targets one member, not the whole group.
        QCOMPARE(project.selectedCount(), 3);
        project.clearSelection(); project.selectPerformerMode(0, 1);
        project.removeSelectedFromGroup();
        QCOMPARE(project.performerGroupInfo(0).isEmpty(), true);
        QVERIFY(project.saveProject(path));
        DrillProject loaded; QVERIFY(loaded.loadProject(path));
        QVERIFY(loaded.performerGroupInfo(1).value(QStringLiteral("size")).toInt() == 3);
    }

    void stagingApronCoordinatesAndMovement()
    {
        DrillProject project; project.newProject();
        QCOMPARE(project.canvasMinX(), -16.0); QCOMPARE(project.canvasMaxX(), 176.0);
        project.addPerformer(QStringLiteral("A"), QStringLiteral("Guard"), QStringLiteral("Guard"), -7.0, -6.0);
        QVERIFY(project.coordinateFor(0).contains(QStringLiteral("Staging apron")));
        project.addPerformer(QStringLiteral("B"), QStringLiteral("Guard"), QStringLiteral("Guard"), -8.0, 20.0);
        QVERIFY(project.coordinateFor(1).contains(QStringLiteral("Side 1 end zone")));
        project.selectPerformer(0, false); project.nudgeSelected(-20, -20);
        QCOMPARE(project.data(project.index(0, 0), DrillProject::XRole).toDouble(), -16.0);
        QCOMPARE(project.data(project.index(0, 0), DrillProject::YRole).toDouble(), -8.0);
    }

    void constantSpeedCurvesAndAbsoluteShowClock()
    {
        DrillProject project; project.newProject();
        project.addPerformer(QStringLiteral("P"), QStringLiteral("Trumpet"), QStringLiteral("Brass"), 10, 10);
        project.addSet(QStringLiteral("Two"), 8); project.selectAll(); project.nudgeSelected(24, 0);
        project.setSelectedTransitionPath(QStringLiteral("curved"), {QPointF(22, 28)});
        const auto points=project.transitionPathSamples(0,8); QVector<double> lengths;
        for(int i=1;i<points.size();++i){const QPointF a=points[i-1].toPointF(),b=points[i].toPointF();lengths.push_back(std::hypot(b.x()-a.x(),b.y()-a.y()));}
        const auto [minimum,maximum]=std::minmax_element(lengths.cbegin(),lengths.cend());
        QVERIFY(*maximum / *minimum < 1.08);
        project.addSet(QStringLiteral("Three"),8);
        const double first=project.openingDurationMs() + project.transitionDurationMs(1);
        QVERIFY(!project.setShowTimeMs(first-1)); QCOMPARE(project.currentSetIndex(),1);
        QVERIFY(!project.setShowTimeMs(first+1)); QCOMPARE(project.currentSetIndex(),2);
    }

    void partialReshapeReplacesGuideAndGroup()
    {
        DrillProject project; project.newProject(); project.batchAddPerformers(QStringLiteral("R"),4,QStringLiteral("Guard"),QStringLiteral("Guard"));
        project.selectAll(); project.createFormation(QStringLiteral("rectangle"),{{QStringLiteral("createGroup"),true}});
        QCOMPARE(project.currentShapeCount(),1); QVERIFY(!project.performerGroupInfo(0).isEmpty());
        project.clearSelection(); project.selectPerformerMode(0,1); project.selectPerformerMode(1,1);
        project.createFormation(QStringLiteral("arc"));
        QCOMPARE(project.currentShapeCount(),1); QCOMPARE(project.shapeInfo(0).value(QStringLiteral("memberCount")).toInt(),2);
        QVERIFY(project.performerGroupInfo(0).isEmpty());
        project.undo(); QCOMPARE(project.currentShapeCount(),1); QVERIFY(!project.performerGroupInfo(0).isEmpty());
    }

    void setCopyMoveAndRenumber()
    {
        DrillProject project; project.newProject(); project.addSet(QStringLiteral("Two"),8); project.addSet(QStringLiteral("Three"),12);
        project.duplicateSetAt(1); QCOMPARE(project.setCount(),4); QCOMPARE(project.currentSetIndex(),2);
        project.moveSet(2,1); QCOMPARE(project.currentSetIndex(),1);
        project.renumberSets();
        for(int i=0;i<project.setCount();++i) QCOMPARE(project.setInfo(i).value(QStringLiteral("number")).toString(),QString::number(i+1));
        project.archiveSetAt(1); QCOMPARE(project.setCount(),3); QCOMPARE(project.archivedSetCount(),1);
    }

    void setRenumberPromptIsOnlyNeededForBrokenLabels()
    {
        DrillProject project; project.newProject(); project.addSet(QStringLiteral("Set 2"), 8); project.addSet(QStringLiteral("Set 3"), 8);
        QVERIFY(!project.setLabelsNeedRenumbering());
        project.archiveSetAt(2); QVERIFY(!project.setLabelsNeedRenumbering());
        project.addSet(QStringLiteral("Set 3"), 8); project.archiveSetAt(1); QVERIFY(project.setLabelsNeedRenumbering());
        project.renumberSets(); QVERIFY(!project.setLabelsNeedRenumbering());
        QCOMPARE(project.setInfo(1).value(QStringLiteral("name")).toString(), QStringLiteral("Set 2"));
    }

    void galaxySpiralRemainsStableAtManyTurns()
    {
        DrillProject project; project.newProject();
        project.batchAddPerformers(QStringLiteral("G"), 80, QStringLiteral("Guard"), QStringLiteral("Guard"));
        project.selectAll(); project.createFormation(QStringLiteral("spiral"), {{QStringLiteral("spiralStyle"), QStringLiteral("galaxy")},
            {QStringLiteral("turns"), 6.0}, {QStringLiteral("outerRadius"), 18.0}, {QStringLiteral("centerX"), 80.0}, {QStringLiteral("centerY"), 42.0}});
        QSet<QString> points;
        for (int row=0; row<project.performerCount(); ++row) {
            const double x=project.data(project.index(row,0),DrillProject::XRole).toDouble();
            const double y=project.data(project.index(row,0),DrillProject::YRole).toDouble();
            points.insert(QStringLiteral("%1,%2").arg(x,0,'f',4).arg(y,0,'f',4));
        }
        QCOMPARE(points.size(), project.performerCount());
    }

    void persistentShapeResizeAndUndo()
    {
        DrillProject project; project.newProject();
        project.batchAddPerformers(QStringLiteral("S"), 12, QStringLiteral("Guard"), QStringLiteral("Guard"));
        project.selectAll();
        project.createFormation(QStringLiteral("circle"), {{QStringLiteral("radius"), 10.0},
            {QStringLiteral("centerX"), 80.0}, {QStringLiteral("centerY"), 42.0}});
        QCOMPARE(project.selectedShapeIndex(), 0);
        const double spacingBefore = project.selectionMetrics().value(QStringLiteral("averageSpacing")).toDouble();
        const double widthBefore = project.shapeInfo(0).value(QStringLiteral("width")).toDouble();
        project.beginScale(); project.previewScale(1.75); project.endScale();
        QVERIFY(project.selectionMetrics().value(QStringLiteral("averageSpacing")).toDouble() > spacingBefore * 1.7);
        QVERIFY(project.shapeInfo(0).value(QStringLiteral("width")).toDouble() > widthBefore * 1.7);
        project.undo();
        QVERIFY(qAbs(project.shapeInfo(0).value(QStringLiteral("width")).toDouble() - widthBefore) < 0.001);
    }

    void freehandRecognitionAssignmentAndUnits()
    {
        DrillProject project; project.newProject();
        project.batchAddPerformers(QStringLiteral("F"), 8, QStringLiteral("Guard"), QStringLiteral("Guard"));
        project.selectAll();
        const QVariantList stroke{QPointF(20,20), QPointF(35,20.1), QPointF(50,19.9), QPointF(70,20)};
        project.createFreehandFormation(stroke, QStringLiteral("shortest"), true, QStringLiteral("auto"));
        QCOMPARE(project.currentShapeCount(), 1);
        QCOMPARE(project.shapeInfo(0).value(QStringLiteral("type")).toString(), QStringLiteral("line"));
        QCOMPARE(project.shapeInfo(0).value(QStringLiteral("memberCount")).toInt(), 8);
        QVERIFY(project.selectionMetrics().value(QStringLiteral("minimumSpacing")).toDouble() > 1.49);
        QVERIFY(!project.performerGroupInfo(0).isEmpty());
        project.setMeasurementUnit(QStringLiteral("yards"));
        QCOMPARE(project.formatDistance(1.6, 1), QStringLiteral("1.0 yd"));
        project.setMeasurementUnit(QStringLiteral("steps"));
        QCOMPARE(project.formatDistance(1.6, 1), QStringLiteral("1.6 st"));
    }

    void sparseFreehandUsesWholeStrokeAndComplexStrokeStaysOpen()
    {
        DrillProject project; project.newProject();
        project.batchAddPerformers(QStringLiteral("F"), 3, QStringLiteral("Guard"), QStringLiteral("Guard"));
        project.selectAll();
        const QVariantList stroke{QPointF(20, 20), QPointF(70, 20), QPointF(70, 60),
                                  QPointF(25, 60), QPointF(23, 20)};
        project.createFreehandFormation(stroke, QStringLiteral("rosterOrder"), false, QStringLiteral("preserve"));
        const QVariantMap shape = project.shapeInfo(0);
        QVERIFY(!shape.value(QStringLiteral("closed")).toBool());
        const QVariantList points = shape.value(QStringLiteral("points")).toList();
        QVERIFY(points.size() > 100);
        QVERIFY(std::hypot(points.first().toPointF().x() - 20.0,
                           points.first().toPointF().y() - 20.0) < 0.1);
        QVERIFY(std::hypot(points.last().toPointF().x() - 23.0,
                           points.last().toPointF().y() - 20.0) < 0.1);

        QSet<QString> destinations;
        for (int row = 0; row < project.performerCount(); ++row) {
            const QPointF point(project.data(project.index(row, 0), DrillProject::XRole).toDouble(),
                                project.data(project.index(row, 0), DrillProject::YRole).toDouble());
            destinations.insert(QStringLiteral("%1,%2").arg(point.x(), 0, 'f', 1).arg(point.y(), 0, 'f', 1));
        }
        QVERIFY(destinations.contains(QStringLiteral("20.0,20.0")));
        QVERIFY(destinations.contains(QStringLiteral("23.0,20.0")));
    }

    void directDragShapeKeepsFullLengthWithFewPerformers()
    {
        DrillProject project; project.newProject();
        project.batchAddPerformers(QStringLiteral("L"), 2, QStringLiteral("Guard"), QStringLiteral("Guard"));
        project.selectAll();
        project.distributeLine(20, 30, 100, 30);
        QSet<int> xCoordinates;
        for (int row = 0; row < project.performerCount(); ++row)
            xCoordinates.insert(qRound(project.data(project.index(row, 0), DrillProject::XRole).toDouble()));
        QVERIFY(xCoordinates.contains(20));
        QVERIFY(xCoordinates.contains(100));
    }

    void rectangleDistributionRetainsExactCorners()
    {
        DrillProject project; project.newProject();
        project.batchAddPerformers(QStringLiteral("R"), 4, QStringLiteral("Guard"), QStringLiteral("Guard"));
        project.selectAll();
        project.distributeRectangle(20, 30, 40, 20);
        QSet<QString> corners;
        for (int row = 0; row < project.performerCount(); ++row) {
            const QPointF point(project.data(project.index(row, 0), DrillProject::XRole).toDouble(),
                                project.data(project.index(row, 0), DrillProject::YRole).toDouble());
            corners.insert(QStringLiteral("%1,%2").arg(point.x(), 0, 'f', 1).arg(point.y(), 0, 'f', 1));
        }
        QCOMPARE(corners.size(), 4);
        QVERIFY(corners.contains(QStringLiteral("20.0,30.0")));
        QVERIFY(corners.contains(QStringLiteral("60.0,30.0")));
        QVERIFY(corners.contains(QStringLiteral("60.0,50.0")));
        QVERIFY(corners.contains(QStringLiteral("20.0,50.0")));
    }

    void formationOptimizerPreviewAndApply()
    {
        DrillProject project; project.newProject();
        project.addPerformer(QStringLiteral("P1"), QStringLiteral("Guard"), QStringLiteral("Guard"), 20, 20);
        project.addPerformer(QStringLiteral("P2"), QStringLiteral("Guard"), QStringLiteral("Guard"), 40, 20);
        project.addPerformer(QStringLiteral("P3"), QStringLiteral("Guard"), QStringLiteral("Guard"), 60, 20);
        project.addPerformer(QStringLiteral("P4"), QStringLiteral("Guard"), QStringLiteral("Guard"), 80, 20);
        project.selectAll(); project.addSet(QStringLiteral("Destination"), 8);
        QVariantList before;
        for (int row = 0; row < 4; ++row) before.push_back(QPointF(
            project.data(project.index(row, 0), DrillProject::XRole).toDouble(),
            project.data(project.index(row, 0), DrillProject::YRole).toDouble()));
        const auto metrics = project.previewFormation(QStringLiteral("line"),
            {{QStringLiteral("centerX"), 50.0}, {QStringLiteral("centerY"), 40.0},
             {QStringLiteral("width"), 60.0}}, QStringLiteral("shortestTotal"));
        QVERIFY(project.formationPreviewActive()); QCOMPARE(project.formationPreviewPoints().size(), 4);
        QVERIFY(metrics.value(QStringLiteral("averageMove")).toDouble() > 0.0);
        for (int row = 0; row < 4; ++row)
            QCOMPARE(QPointF(project.data(project.index(row, 0), DrillProject::XRole).toDouble(),
                project.data(project.index(row, 0), DrillProject::YRole).toDouble()), before[row].toPointF());
        project.cancelFormationPreview(); QCOMPARE(project.currentShapeCount(), 0);
        project.previewFormation(QStringLiteral("line"),
            {{QStringLiteral("centerX"), 50.0}, {QStringLiteral("centerY"), 40.0},
             {QStringLiteral("width"), 60.0}}, QStringLiteral("preserveOrder"));
        const auto preview = project.formationPreviewPoints();
        double previousX = -1e9;
        for (const auto &value : preview) { const double x = value.toMap().value(QStringLiteral("x")).toDouble(); QVERIFY(x >= previousX); previousX = x; }
        QVERIFY(project.commitFormationPreview());
        for (int row = 0; row < 4; ++row)
            QVERIFY(qAbs(project.data(project.index(row, 0), DrillProject::YRole).toDouble() - 40.0) < 0.01);
        project.undo(); QCOMPARE(project.currentShapeCount(), 0);
        for (int row = 0; row < 4; ++row)
            QCOMPARE(QPointF(project.data(project.index(row, 0), DrillProject::XRole).toDouble(),
                project.data(project.index(row, 0), DrillProject::YRole).toDouble()), before[row].toPointF());
    }

    void drillClinicProfilesIssuesAndNextSet()
    {
        QTemporaryDir temporary; QVERIFY(temporary.isValid());
        DrillProject project; project.newProject();
        project.addPerformer(QStringLiteral("A"), QStringLiteral("Guard"), QStringLiteral("Guard"), 40, 20);
        project.addPerformer(QStringLiteral("B"), QStringLiteral("Guard"), QStringLiteral("Guard"), 60, 20);
        project.addSet(QStringLiteral("Crossing"), 8);
        project.clearSelection(); project.selectPerformerMode(0, 0); project.nudgeSelected(20, 0);
        project.clearSelection(); project.selectPerformerMode(1, 0); project.nudgeSelected(-20, 0);
        const auto issues = project.analyzeTransition();
        bool foundCollision = false, foundCrossing = false;
        for (const auto &value : issues) {
            const QString type = value.toMap().value(QStringLiteral("type")).toString();
            foundCollision |= type == QStringLiteral("collision"); foundCrossing |= type == QStringLiteral("crossing");
        }
        QVERIFY(foundCollision);
        // Geometric crossings alone are intentional choreography; timed collisions are reported.
        QVERIFY(!foundCrossing);
        project.setCapabilityProfile(QStringLiteral("beginner")); QCOMPARE(project.maximumStepsPerCount(), 1.0);
        const QString path = temporary.filePath(QStringLiteral("clinic.marchcraft")); QVERIFY(project.saveProject(path));
        DrillProject restored; QVERIFY(restored.loadProject(path)); QCOMPARE(restored.capabilityProfile(), QStringLiteral("beginner"));
        restored.selectAll(); const int beforeSets = restored.setCount();
        const auto suggestions = restored.suggestNextSet(); QCOMPARE(suggestions.size(), 11);
        const auto candidate = suggestions.first().toMap();
        restored.previewFormation(candidate.value(QStringLiteral("type")).toString(),
            candidate.value(QStringLiteral("options")).toMap(), QStringLiteral("rehearsalSafe"));
        QVERIFY(restored.commitFormationPreview()); QCOMPARE(restored.setCount(), beforeSets + 1);
    }

    void asynchronousPreviewUsesLatestGeneration()
    {
        DrillProject project; project.newProject();
        project.batchAddPerformers(QStringLiteral("A"), 24, QStringLiteral("Guard"), QStringLiteral("Guard"));
        project.selectAll(); project.addSet(QStringLiteral("Destination"), 8);
        project.requestFormationPreview(QStringLiteral("line"),
            {{QStringLiteral("centerX"), 80.0}, {QStringLiteral("centerY"), 30.0}, {QStringLiteral("width"), 92.0}},
            QStringLiteral("shortestTotal"));
        QVERIFY(project.formationPreviewBusy());
        QTRY_VERIFY_WITH_TIMEOUT(project.formationPreviewActive(), 10000);
        QVERIFY(!project.formationPreviewBusy()); QCOMPARE(project.formationPreviewPoints().size(), 24);

        project.requestFormationPreview(QStringLiteral("circle"),
            {{QStringLiteral("centerX"), 80.0}, {QStringLiteral("centerY"), 30.0}, {QStringLiteral("radius"), 18.0}},
            QStringLiteral("evenEffort"));
        project.cancelFormationPreview();
        QTest::qWait(300);
        QVERIFY(!project.formationPreviewActive()); QVERIFY(!project.formationPreviewBusy());
    }

    void drillClinicUnderstandsEquipmentAndProps()
    {
        DrillProject equipment; equipment.newProject();
        equipment.addPerformer(QStringLiteral("Tuba"), QStringLiteral("Tuba"), QStringLiteral("Brass"), 20, 20);
        equipment.addPerformer(QStringLiteral("Drum"), QStringLiteral("Percussion"), QStringLiteral("Battery"), 40, 20);
        equipment.addSet(QStringLiteral("Swap"), 8);
        equipment.clearSelection(); equipment.selectPerformerMode(0, 0); equipment.nudgeSelected(20, 0);
        equipment.clearSelection(); equipment.selectPerformerMode(1, 0); equipment.nudgeSelected(-20, 0);
        bool foundEquipment = false;
        for (const auto &value : equipment.analyzeTransition())
            foundEquipment |= value.toMap().value(QStringLiteral("type")).toString() == QStringLiteral("equipmentCollision");
        QVERIFY(foundEquipment);

        DrillProject props; props.newProject();
        props.addPerformer(QStringLiteral("T1"), QStringLiteral("Tuba"), QStringLiteral("Brass"), 20, 20);
        props.addSet(QStringLiteral("At prop"), 8); props.selectAll(); props.nudgeSelected(20, 0);
        props.addProp(QStringLiteral("prop.box"), 40, 20);
        QVariantMap propIssue; QString shiftSuggestion;
        for (const auto &value : props.analyzeTransition()) {
            const auto issue = value.toMap(); if (issue.value(QStringLiteral("type")).toString() != QStringLiteral("propCollision")) continue;
            propIssue = issue;
            for (const auto &actionValue : issue.value(QStringLiteral("actions")).toList()) {
                const auto action = actionValue.toMap();
                if (action.value(QStringLiteral("type")).toString() == QStringLiteral("shiftProp")) shiftSuggestion = action.value(QStringLiteral("id")).toString();
            }
        }
        QVERIFY(!propIssue.isEmpty()); QVERIFY(!shiftSuggestion.isEmpty());
        QVERIFY(props.selectClinicIssue(propIssue.value(QStringLiteral("id")).toString()));
        QVERIFY(props.props().first().toMap().value(QStringLiteral("highlighted")).toBool());
        QVERIFY(props.previewSuggestion(shiftSuggestion)); QVERIFY(props.formationPreviewActive());
        QVERIFY(props.commitFormationPreview());
        bool stillCollides = false;
        for (const auto &value : props.analyzeTransition())
            stillCollides |= value.toMap().value(QStringLiteral("type")).toString() == QStringLiteral("propCollision");
        QVERIFY(!stillCollides);
    }

    void openingSetHoldAndImmediateMovement()
    {
        QTemporaryDir temporary; QVERIFY(temporary.isValid());
        DrillProject project; project.newProject(); project.addSet(QStringLiteral("Set 2"), 8);
        project.setOpeningBehavior(QStringLiteral("hold"), 8);
        QCOMPARE(project.openingBehavior(), QStringLiteral("hold")); QCOMPARE(project.openingCounts(), 8);
        QVERIFY(project.openingDurationMs() > 3900.0 && project.openingDurationMs() < 4100.0);
        QVERIFY(!project.setShowTimeMs(project.openingDurationMs() / 2.0));
        QCOMPARE(project.currentSetIndex(), 0); QCOMPARE(project.playhead(), 1.0);
        QVERIFY(!project.setShowTimeMs(project.openingDurationMs() + project.transitionDurationMs(1) / 2.0));
        QCOMPARE(project.currentSetIndex(), 1); QVERIFY(project.playhead() > 0.49 && project.playhead() < 0.51);

        TransportController transport(&project); project.selectSetRange(0, false); transport.playFromSelection();
        QCOMPARE(project.currentSetIndex(), 0); QCOMPARE(project.playhead(), 1.0); transport.stop();
        project.setOpeningBehavior(QStringLiteral("move"), 8); transport.playFromSelection();
        QCOMPARE(project.playbackSetIndex(), 1); QVERIFY(project.playhead() < 0.01); transport.stop();

        project.setOpeningBehavior(QStringLiteral("hold"), 12);
        const QString path = temporary.filePath(QStringLiteral("opening.marchcraft")); QVERIFY(project.saveProject(path));
        DrillProject restored; QVERIFY(restored.loadProject(path));
        QCOMPARE(restored.openingBehavior(), QStringLiteral("hold")); QCOMPARE(restored.openingCounts(), 12);
    }

    void musicHoldThenMoveUsesExactMeasureBoundaries()
    {
        DrillProject project; project.newProject();
        project.addPerformer(QStringLiteral("P1"), QStringLiteral("Trumpet"),
                             QStringLiteral("Brass"), 40.0, 40.0);
        project.selectPerformer(0, false);
        const QVariantMap hold{{QStringLiteral("startTick"), 0},
            {QStringLiteral("endTick"), 15360}, {QStringLiteral("startMeasure"), 1},
            {QStringLiteral("endMeasure"), 4}, {QStringLiteral("stepMultiplier"), 0.0}};
        const QVariantMap move{{QStringLiteral("startTick"), 15360},
            {QStringLiteral("endTick"), 30720}, {QStringLiteral("startMeasure"), 5},
            {QStringLiteral("endMeasure"), 8}, {QStringLiteral("stepMultiplier"), 1.0}};
        QVERIFY(project.commitSetGenerationPlan({hold, move}));
        QCOMPARE(project.setCount(), 3);
        QCOMPARE(project.setInfo(1).value(QStringLiteral("startTick")).toLongLong(), 15360);
        QCOMPARE(project.setInfo(1).value(QStringLiteral("stepMultiplier")).toDouble(), 0.0);
        QCOMPARE(project.setInfo(2).value(QStringLiteral("startTick")).toLongLong(), 30720);
        QCOMPARE(project.setInfo(2).value(QStringLiteral("stepMultiplier")).toDouble(), 1.0);
        QCOMPARE(project.setInfo(2).value(QStringLiteral("counts")).toInt(), 16);

        // Reapplying a movement mode to an existing boundary must update it.
        QVariantMap revisedMove = move;
        revisedMove.insert(QStringLiteral("stepMultiplier"), 0.0);
        QVERIFY(project.commitSetGenerationPlan({revisedMove}));
        QCOMPARE(project.setInfo(2).value(QStringLiteral("stepMultiplier")).toDouble(), 0.0);
        QVERIFY(project.commitSetGenerationPlan({move}));
        QCOMPARE(project.setInfo(2).value(QStringLiteral("stepMultiplier")).toDouble(), 1.0);

        project.setCurrentSetIndex(2);
        project.nudgeSelected(8.0, 0.0);
        TransportController transport(&project);
        transport.seekTick(15359);
        QCOMPARE(project.playbackSetIndex(), 1);
        QVERIFY(project.playhead() > 0.999);
        QCOMPARE(project.data(project.index(0, 0), DrillProject::XRole).toDouble(), 40.0);
        transport.seekTick(15360);
        QCOMPARE(project.playbackSetIndex(), 2);
        QCOMPARE(project.playhead(), 0.0);
        QCOMPARE(project.data(project.index(0, 0), DrillProject::XRole).toDouble(), 40.0);
        transport.seekTick(30719);
        QVERIFY(project.playhead() > 0.999);
        QVERIFY(project.data(project.index(0, 0), DrillProject::XRole).toDouble() > 47.99);
        transport.seekTick(30720);
        QCOMPARE(project.playhead(), 1.0);
        QCOMPARE(project.data(project.index(0, 0), DrillProject::XRole).toDouble(), 48.0);
    }

    void liveSetMovePreservesCardsAndDurations()
    {
        DrillProject project; project.newProject(); project.addSet(QStringLiteral("Second"), 8); project.addSet(QStringLiteral("Third"), 12);
        project.moveSet(0, 2);
        QCOMPARE(project.setInfo(0).value(QStringLiteral("name")).toString(), QStringLiteral("Second"));
        QCOMPARE(project.setInfo(1).value(QStringLiteral("name")).toString(), QStringLiteral("Third"));
        QCOMPARE(project.setInfo(2).value(QStringLiteral("name")).toString(), QStringLiteral("Set 1"));
        QCOMPARE(project.setInfo(1).value(QStringLiteral("counts")).toInt(), 12);
        QCOMPARE(project.setInfo(2).value(QStringLiteral("counts")).toInt(), 8);
        QCOMPARE(project.currentSetIndex(), 2); QCOMPARE(project.selectedSetStartIndex(), 2); QCOMPARE(project.selectedSetEndIndex(), 2);
        QCOMPARE(project.setInfo(0).value(QStringLiteral("openingBehavior")).toString(), QStringLiteral("move"));
        QCOMPARE(project.setInfo(0).value(QStringLiteral("counts")).toInt(), 0);
        QVERIFY(project.setInfo(1).value(QStringLiteral("startTick")).toLongLong() > 0);
        QVERIFY(project.setInfo(2).value(QStringLiteral("startTick")).toLongLong()
            > project.setInfo(1).value(QStringLiteral("startTick")).toLongLong());
        project.undo(); QCOMPARE(project.setInfo(0).value(QStringLiteral("name")).toString(), QStringLiteral("Set 1"));
    }

    void exportSelectionBrandingAndStateIsolation()
    {
        QTemporaryDir temp;
        DrillProject project;
        project.newProject();
        project.addPerformer(QStringLiteral("T01"), QStringLiteral("Trumpet"), QStringLiteral("Brass"), 80, 42);
        project.addSet(QStringLiteral("Second"), 8);
        project.selectAll();
        const int current = project.currentSetIndex();
        ExportController exporter(&project);
        QImage logo(120,60,QImage::Format_ARGB32); logo.fill(QColor(QStringLiteral("#287d85")));
        const QString logoPath=temp.filePath(QStringLiteral("company.png")); QVERIFY(logo.save(logoPath));
        QVERIFY(exporter.setBranding(QStringLiteral("Example Company"), logoPath));
        QCOMPARE(exporter.branding().value(QStringLiteral("company")).toString(), QStringLiteral("Example Company"));
        project.undo();
        QVERIFY(exporter.branding().value(QStringLiteral("company")).toString().isEmpty());
        project.redo();
        const QString saved = temp.filePath(QStringLiteral("show.marchcraft"));
        QVERIFY(project.saveProject(saved));
        DrillProject reopened;
        QVERIFY(reopened.loadProject(saved));
        ExportController restored(&reopened);
        QCOMPARE(restored.branding().value(QStringLiteral("company")).toString(), QStringLiteral("Example Company"));
        QCOMPARE(restored.branding().value(QStringLiteral("logo")),exporter.branding().value(QStringLiteral("logo")));
        QVERIFY(!restored.branding().value(QStringLiteral("logo")).toString().isEmpty());
        project.selectAll();
        QVariantMap options{{QStringLiteral("sets"), QStringLiteral("1-2")}, {QStringLiteral("format"), QStringLiteral("pdf")}};
        QVERIFY(exporter.prepare(options));
        QCOMPARE(exporter.pages().size(), 2);
        QCOMPARE(project.currentSetIndex(), current);
        QCOMPARE(project.selectedCount(), 1);
        QVERIFY(!project.dirty());
        QVERIFY(exporter.start(temp.filePath(QStringLiteral("charts.pdf"))));
        QTRY_VERIFY_WITH_TIMEOUT(!exporter.busy(), 15000);
        QCOMPARE(exporter.progress(), 1.0);
        QVERIFY(QFileInfo(temp.filePath(QStringLiteral("charts.pdf"))).size()>1000);
        QVERIFY(!exporter.start(temp.filePath(QStringLiteral("charts.pdf"))));
        options[QStringLiteral("format")]=QStringLiteral("csv");
        QVERIFY(exporter.prepare(options));
        QVERIFY(exporter.start(temp.filePath(QStringLiteral("analytics.csv"))));
        QTRY_VERIFY(!exporter.busy());
        QFile csv(temp.filePath(QStringLiteral("analytics.csv"))); QVERIFY(csv.open(QIODevice::ReadOnly));
        const auto data=csv.readAll(); QVERIFY(data.contains("T01")); QVERIFY(data.contains("Movement"));
        options[QStringLiteral("sets")]=QStringLiteral("missing");
        QVERIFY(!exporter.prepare(options));
        QVERIFY(!exporter.start(temp.filePath(QStringLiteral("empty.pdf"))));
        options[QStringLiteral("sets")]=QStringLiteral("2");options[QStringLiteral("format")]=QStringLiteral("png");
        QVERIFY(exporter.prepare(options));
        QCOMPARE(exporter.pages().size(),1);
        QVERIFY(exporter.start(temp.path()));
        exporter.cancel();
        QVERIFY(!QFileInfo::exists(temp.filePath(QStringLiteral("chart-0001.png"))));
        QCOMPARE(project.gridMidlineSteps(),4.0);
        QCOMPARE(project.yardLineSteps(),8.0);
    }

    void exportPaginationVariantsAndMovements()
    {
        QTemporaryDir temp;
        DrillProject project;
        project.newProject();
        project.addPerformer(QStringLiteral("P01"), QStringLiteral("Trumpet"), QStringLiteral("Brass"), 80, 42);
        QString notes;
        for(int i=0;i<80;++i)notes += QStringLiteral("Instruction %1: hold, then move.\n").arg(i);
        project.updateCurrentSet(QStringLiteral("1"), QStringLiteral("Opening"), notes, QStringLiteral("1-4"), 8, false);
        project.createVariant(QStringLiteral("Alternative"), QStringLiteral("Alternative notes"));
        ExportController exporter(&project);
        QVariantMap options{{QStringLiteral("variants"),QStringLiteral("all")}};
        QVERIFY(exporter.prepare(options));
        QVERIFY(exporter.pages().size()>3);
        QVERIFY(exporter.pages().join(QStringLiteral(" ")).contains(QStringLiteral("Instructions continued")));
        options[QStringLiteral("variants")]=QStringLiteral("active");
        QVERIFY(exporter.prepare(options));
        QCOMPARE(exporter.pages().size(),1);
        project.createMovement(QStringLiteral("Finale"));
        options[QStringLiteral("split")]=true;
        QVERIFY(exporter.prepare(options));
        QCOMPARE(exporter.plannedFiles(temp.path()).size(),2);
        QVERIFY(exporter.start(temp.path()));
        QTRY_VERIFY_WITH_TIMEOUT(!exporter.busy(),15000);
        QCOMPARE(exporter.progress(),1.0);
        for(const auto &file:exporter.files())QVERIFY(QFileInfo(file).size()>100);
        options[QStringLiteral("scope")]=QStringLiteral("active");
        options[QStringLiteral("format")]=QStringLiteral("png");
        options[QStringLiteral("dpi")]=150;
        QVERIFY(exporter.prepare(options));
        QCOMPARE(exporter.pages().size(),1);
        QVERIFY(exporter.start(temp.path()));
        QTRY_VERIFY_WITH_TIMEOUT(!exporter.busy(),15000);
        QImage image(exporter.files().first());
        QCOMPARE(image.size(),QSize(1650,1275));
        QVERIFY(qAbs(image.dotsPerMeterX()-5906)<2);
        options[QStringLiteral("framing")]=QStringLiteral("custom");
        options[QStringLiteral("cropWidth")]=0;
        QVERIFY(!exporter.prepare(options));
    }

    void exportWorkspacePairedDocumentsAndPreview()
    {
        QTemporaryDir temp;
        DrillProject project;
        project.newProject();
        project.addPerformer(QStringLiteral("P01"), QStringLiteral("Trumpet"), QStringLiteral("Brass"), 80, 42);
        project.setOpeningBehavior(QStringLiteral("hold"),2);
        project.addSet(QStringLiteral("Second"),8);
        ExportController exporter(&project);
        QCOMPARE(ExportOptions::fromMap({}).performerLabelSize,6.0);
        QCOMPARE(ExportOptions::fromMap({{QStringLiteral("performerLabelSize"),2}}).performerLabelSize,4.0);
        const auto organization=QCoreApplication::organizationName();
        const auto application=QCoreApplication::applicationName();
        QCoreApplication::setOrganizationName(QStringLiteral("MarchCraftExportTests"));
        QCoreApplication::setApplicationName(QFileInfo(temp.path()).fileName());
        const auto cleanup=qScopeGuard([&] {QSettings().clear();QCoreApplication::setOrganizationName(organization);QCoreApplication::setApplicationName(application);});
        exporter.savePreset(QStringLiteral("QA legacy label"),{{QStringLiteral("fontSize"),12}});
        QCOMPARE(exporter.loadPreset(QStringLiteral("QA legacy label")).value(QStringLiteral("performerLabelSize")).toInt(),12);
        QSettings().remove(QStringLiteral("export/presets/QA legacy label"));
        QVariantMap options{{QStringLiteral("content"),QStringLiteral("both")}, {QStringLiteral("basename"),QStringLiteral("Packet")}, {QStringLiteral("brandingCompany"),QStringLiteral("Preview only")}};
        const auto branding=exporter.branding();
        QVERIFY(exporter.prepare(options));
        QCOMPARE(exporter.branding(),branding);
        QCOMPARE(exporter.pages().size(),3);
        auto files=exporter.plannedFiles(temp.path());
        QCOMPARE(files.size(),2);
        QCOMPARE(exporter.firstPageForFile(0),0);
        QCOMPARE(exporter.firstPageForFile(1),2);
        QVERIFY(files[0].endsWith(QStringLiteral("Packet - Drill Charts.pdf")));
        QVERIFY(files[1].endsWith(QStringLiteral("Packet - Coordinates.pdf")));
        const int revision=exporter.previewRevision();
        QVERIFY(exporter.start(temp.path()));
        QVERIFY(!exporter.prepare({}));
        QCOMPARE(exporter.previewRevision(),revision);
        project.addSet(QStringLiteral("Later edit"),8);
        QTRY_VERIFY_WITH_TIMEOUT(!exporter.busy(),15000);
        QCOMPARE(exporter.progress(),1.0);
        for(const auto &file:files)QVERIFY(QFileInfo(file).size()>1000);
        project.createMovement(QStringLiteral("Finale"));
        options[QStringLiteral("split")]=true;
        QVERIFY(exporter.prepare(options));
        files=exporter.plannedFiles(temp.path());
        QCOMPARE(files.size(),4);
        QVERIFY(files[0].contains(QStringLiteral("Drill Charts")));
        QVERIFY(files[1].contains(QStringLiteral("Coordinates")));
        QVERIFY(files[2].contains(QStringLiteral("Drill Charts")));
        QVERIFY(exporter.start(temp.path()));
        QTRY_VERIFY_WITH_TIMEOUT(!exporter.busy(),15000);
        QCOMPARE(exporter.progress(),1.0);
        for(const auto &file:files)QVERIFY(QFileInfo(file).size()>1000);
        options[QStringLiteral("format")]=QStringLiteral("csv");
        QVERIFY(exporter.prepare(options));
        QVERIFY(exporter.tableRows().size()>1);
        QCOMPARE(exporter.tableRows()[0].toStringList().size(),12);
        QCOMPARE(exporter.tableRows()[1].toStringList()[1],QStringLiteral("P01"));
        QCOMPARE(exporter.tableRows()[1].toStringList()[8],QStringLiteral("2"));
        QVERIFY(exporter.suggestedName(QStringLiteral("charts"),QStringLiteral("pdf")).endsWith(QStringLiteral(" - Drill Charts.pdf")));
        options[QStringLiteral("format")]=QStringLiteral("video2d");
        QVERIFY(exporter.prepare(options));
        QVERIFY(exporter.duration()>0);
        exporter.previewTime(exporter.duration()/2);
        const QString preview=exporter.localPath(exporter.previewUrl());
        QVERIFY(QFileInfo::exists(preview));
        exporter.previewTime(0);
        QVERIFY(!QFileInfo::exists(preview));
        options[QStringLiteral("sets")]=QStringLiteral("missing");
        QVERIFY(!exporter.prepare(options));
        QVERIFY(exporter.previewUrl().isEmpty());
    }

    void exportsAreCreated()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        DrillProject project;
        project.loadDemo();
        const QString csv = temporary.filePath(QStringLiteral("analytics.csv"));
        const QString pdf = temporary.filePath(QStringLiteral("coordinates.pdf"));
        QVERIFY(project.exportCsv(csv));
        QVERIFY(project.exportCoordinatePdf(pdf));
        QVERIFY(QFileInfo(csv).size() > 100);
        QVERIFY(QFileInfo(pdf).size() > 100);
    }


    void sceneAppearancePropsAndVenueRoundTrip()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString path = temporary.filePath(QStringLiteral("scene.marchcraft"));
        DrillProject source;
        source.newProject();
        source.addPerformer(QStringLiteral("T01"), QStringLiteral("Trumpet"), QStringLiteral("Brass"), 80, 42);
        source.setPerformerAppearance(0, QStringLiteral("performer.body.tall"),
                                      QStringLiteral("uniform.marchcraft.default"), QStringLiteral("skin.deep"),
                                      QStringLiteral("instrument.trumpet"), 1.91);
        source.setVenuePreset(QStringLiteral("venue.high_school"));
        source.setLightingPreset(QStringLiteral("lighting.sunset"));
        source.setGraphicsProfile(QStringLiteral("balanced"));
        const QString propId = source.addProp(QStringLiteral("prop.panel"), 80, 42);
        source.updateProp(propId, 84, 40, 25, 1.2, 1.1, 0.9);
        QVERIFY(source.saveProject(path));

        DrillProject restored;
        QVERIFY(restored.loadProject(path));
        QCOMPARE(restored.venuePreset(), QStringLiteral("venue.high_school"));
        QCOMPARE(restored.lightingPreset(), QStringLiteral("lighting.sunset"));
        QCOMPARE(restored.graphicsProfile(), QStringLiteral("balanced"));
        QCOMPARE(restored.performerInfo(0).value(QStringLiteral("bodyRigId")).toString(),
                 QStringLiteral("performer.body.tall"));
        QCOMPARE(restored.performerInfo(0).value(QStringLiteral("instrumentAssetId")).toString(),
                 QStringLiteral("instrument.trumpet"));
        QCOMPARE(restored.props().size(), 1);
        const auto prop = restored.props().first().toMap();
        QCOMPARE(prop.value(QStringLiteral("definitionId")).toString(), QStringLiteral("prop.panel"));
        QVERIFY(qAbs(prop.value(QStringLiteral("worldX")).toDouble()
                     - (84.0 - 80.0) * MarchCraft::FieldTransform::MetersPerStep) < 0.0001);
    }

    void midiImportSetGenerationAndPersistence()
    {
        QTemporaryDir temporary; QVERIFY(temporary.isValid());
        const QString midiPath = temporary.filePath(QStringLiteral("running status.mid"));
        QFile midi(midiPath); QVERIFY(midi.open(QIODevice::WriteOnly)); QCOMPARE(midi.write(midiFixture()), midiFixture().size()); midi.close();
        DrillProject project; project.newProject();
        QVERIFY(project.importMidi(midiPath));
        const auto parsed = MarchCraft::parseMidiFile(midiPath);
        QVERIFY(parsed.ok); QCOMPARE(parsed.document.playbackEvents.size(), 5);
        QCOMPARE(parsed.document.playbackEvents[0].status & 0xf0, 0xc0);
        QCOMPARE(parsed.document.playbackEvents[1].status & 0xf0, 0x90);
        QVERIFY(project.musicLoaded()); QCOMPARE(project.musicSourceType(), QStringLiteral("midi"));
        QCOMPARE(project.musicMeasureCount(), 2); QCOMPARE(project.musicTrackCount(), 2);
        QCOMPARE(project.musicTrackInfo(1).value(QStringLiteral("noteCount")).toInt(), 2);
        QCOMPARE(project.musicMeasureInfo(0).value(QStringLiteral("numerator")).toInt(), 3);
        QVERIFY(qAbs(project.musicDurationMs() - 3000.0) < 0.01);
        const QString sectionId = project.addMusicSection(QStringLiteral("Opening"), QStringLiteral("movement"),
                                                           QStringLiteral("#8b5cf6"), 0, 1);
        QVERIFY(!sectionId.isEmpty()); QCOMPARE(project.musicSections().size(), 1);
        QCOMPARE(project.musicMeasureInfo(1).value(QStringLiteral("sections")).toList().size(), 1);

        const auto oneMove = project.previewSetGeneration(0, 1, QStringLiteral("oneMove"), 16, 1.0);
        QCOMPARE(oneMove.size(), 1); QCOMPARE(oneMove.first().toMap().value(QStringLiteral("counts")).toInt(), 6);
        const auto subdivided = project.previewSetGeneration(0, 1, QStringLiteral("subdivide"), 4, 0.5);
        QCOMPARE(subdivided.size(), 2);
        QCOMPARE(subdivided.first().toMap().value(QStringLiteral("steps")).toDouble(), 2.0);
        QCOMPARE(subdivided.last().toMap().value(QStringLiteral("steps")).toDouble(), 1.0);
        QVariantList customPlan = subdivided;
        auto finalSegment = customPlan.last().toMap(); finalSegment.insert(QStringLiteral("stepMultiplier"), 2.0);
        customPlan.last() = finalSegment;
        QVERIFY(project.commitSetGenerationPlan(customPlan));
        QCOMPARE(project.setCount(), 3); QCOMPARE(project.setInfo(1).value(QStringLiteral("stepMultiplier")).toDouble(), 0.5);
        QCOMPARE(project.setInfo(2).value(QStringLiteral("stepMultiplier")).toDouble(), 2.0);
        project.undo(); QCOMPARE(project.setCount(), 1); project.redo(); QCOMPARE(project.setCount(), 3);
        project.selectSetRange(0, false); project.selectSetRange(2, true);
        QVERIFY(project.musicMeasureInfo(0).value(QStringLiteral("inSetRange")).toBool());
        QVERIFY(project.musicMeasureInfo(1).value(QStringLiteral("inSetRange")).toBool());
        project.setLoopEnabled(true); project.setPlaybackSource(QStringLiteral("mute"));
        project.setMidiMasterVolume(0.42); project.setMusicTrackMuted(1, true);
        TransportController transport(&project);
        transport.seekNormalized(0.6); const qint64 soughtTick=transport.currentTick();
        QVERIFY(soughtTick > 0); QTest::qWait(30); QCOMPARE(transport.currentTick(), soughtTick);

        const QString projectPath = temporary.filePath(QStringLiteral("music.marchcraft"));
        QVERIFY(project.saveProject(projectPath));
        DrillProject restored; QVERIFY(restored.loadProject(projectPath));
        QCOMPARE(restored.musicMeasureCount(), 2); QCOMPARE(restored.setCount(), 3);
        QCOMPARE(restored.setInfo(2).value(QStringLiteral("steps")).toDouble(), 4.0);
        QCOMPARE(restored.selectedSetStartIndex(), 0); QCOMPARE(restored.selectedSetEndIndex(), 2);
        QVERIFY(restored.loopEnabled()); QCOMPARE(restored.playbackSource(), QStringLiteral("mute"));
        QVERIFY(qAbs(restored.midiMasterVolume()-0.42)<0.001);
        QVERIFY(restored.musicTrackInfo(1).value(QStringLiteral("muted")).toBool());
        QCOMPARE(restored.musicSections().size(), 1);
        QCOMPARE(restored.musicSections().first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Opening"));
    }

    void suppliedFocalPointMidiAcceptance()
    {
        const QString path = QStringLiteral("C:/Users/Andrew/Downloads/Focal_Point_-_A_Marching_Band_Show_(HQ).mid");
        if (!QFileInfo::exists(path)) QSKIP("Supplied acceptance MIDI is not available");
        DrillProject project; project.newProject();
        QVERIFY(project.importMidi(path));
        QCOMPARE(project.musicTrackCount(), 35);
        QCOMPARE(project.musicMeasureCount(), 346);
        QCOMPARE(project.meterRegionCount(), 16);
        QCOMPARE(project.tempoRegionCount(), 121);
        QVERIFY(qAbs(project.musicDurationMs() - 626695.0) < 10.0);
        const auto preview = project.previewSetGeneration(0, 9, QStringLiteral("subdivide"), 16, 1.0);
        QVERIFY(!preview.isEmpty());
        QCOMPARE(preview.first().toMap().value(QStringLiteral("counts")).toInt(), 16);
    }
    void performerMotionRolesFollowTravelAndFacing()
    {
        auto motionFor = [](double dx, double dy, double facing = 0.0,
                            const QString &pathType = QStringLiteral("direct"), double playhead = 0.5) {
            DrillProject project;
            project.newProject();
            project.addPerformer(QStringLiteral("P1"), QStringLiteral("Trumpet"),
                                 QStringLiteral("Brass"), 40.0, 40.0);
            project.selectPerformer(0, false);
            project.faceSelected(facing);
            project.addSet(QStringLiteral("Set 2"), 8);
            project.nudgeSelected(dx, -dy); // Positive helper dy means toward the audience.
            project.faceSelected(facing);
            project.setSelectedTransitionPath(pathType, {});
            project.setPlaybackActive(true);
            project.setPlayhead(playhead);
            const QModelIndex index = project.index(0, 0);
            return QVariantMap{{QStringLiteral("mode"), project.data(index, DrillProject::LocomotionModeRole)},
                {QStringLiteral("heading"), project.data(index, DrillProject::TravelHeadingRole)},
                {QStringLiteral("stride"), project.data(index, DrillProject::TravelStepsPerCountRole)},
                {QStringLiteral("phase"), project.data(index, DrillProject::GaitPhaseRole)},
                {QStringLiteral("pathType"), project.data(index, DrillProject::TravelPathTypeRole)},
                {QStringLiteral("closes"), project.data(index, DrillProject::ClosingTransitionRole)}};
        };

        const auto forward = motionFor(0.0, 8.0);
        QCOMPARE(forward.value(QStringLiteral("mode")).toString(), QStringLiteral("march.forward"));
        QVERIFY(qAbs(forward.value(QStringLiteral("heading")).toDouble()) < 0.01);
        QVERIFY(qAbs(forward.value(QStringLiteral("stride")).toDouble() - 1.0) < 0.01);
        QCOMPARE(motionFor(8.0, 0.0).value(QStringLiteral("mode")).toString(), QStringLiteral("slide.right"));
        QCOMPARE(motionFor(-8.0, 0.0).value(QStringLiteral("mode")).toString(), QStringLiteral("slide.left"));
        QCOMPARE(motionFor(0.0, -8.0).value(QStringLiteral("mode")).toString(), QStringLiteral("march.backward"));
        QCOMPARE(motionFor(8.0, -4.0).value(QStringLiteral("mode")).toString(), QStringLiteral("march.backward"));
        QCOMPARE(motionFor(-8.0, -4.0).value(QStringLiteral("mode")).toString(), QStringLiteral("march.backward"));
        QCOMPARE(motionFor(8.0, 2.0).value(QStringLiteral("mode")).toString(), QStringLiteral("march.forward"));
        QCOMPARE(motionFor(8.0, 0.0, 90.0).value(QStringLiteral("mode")).toString(), QStringLiteral("march.forward"));
        QVERIFY(qAbs(motionFor(0.0, 8.0, 0.0, QStringLiteral("direct"), 0.125)
                         .value(QStringLiteral("phase")).toDouble() - 0.5) < 0.001);
        QCOMPARE(motionFor(0.0, 8.0, 0.0, QStringLiteral("delayed"), 0.10)
                     .value(QStringLiteral("mode")).toString(), QStringLiteral("idle"));
        const auto follow = motionFor(0.0, 8.0, 90.0, QStringLiteral("follow"));
        QCOMPARE(follow.value(QStringLiteral("mode")).toString(), QStringLiteral("march.forward"));
        QVERIFY(follow.value(QStringLiteral("closes")).toBool());
    }

    void gaitContinuesAcrossOddCountSets()
    {
        DrillProject project;
        project.newProject();
        project.addPerformer(QStringLiteral("P1"), QStringLiteral("Trumpet"), QStringLiteral("Brass"), 80, 40);
        project.selectAll();
        project.addSet(QStringLiteral("Seven"), 7);
        project.nudgeSelected(0, -7);
        project.addSet(QStringLiteral("Eight"), 8);
        project.nudgeSelected(0, -8);
        project.setPlaybackActive(true);
        project.setCurrentSetIndex(1);
        project.setPlayhead(1);
        QCOMPARE(project.data(project.index(0), DrillProject::GaitPhaseRole).toDouble(), 0.5);
        project.setCurrentSetIndex(2);
        project.setPlayhead(0);
        QCOMPARE(project.data(project.index(0), DrillProject::GaitPhaseRole).toDouble(), 0.5);
        QCOMPARE(project.data(project.index(0), DrillProject::GaitElapsedCountsRole).toDouble(), 7.0);
        project.setPlayhead(0.125);
        QCOMPARE(project.data(project.index(0), DrillProject::GaitPhaseRole).toDouble(), 0.0);
    }

    void closingAndPlantedSetBehavior()
    {
        DrillProject project;
        project.newProject();
        project.addPerformer(QStringLiteral("P1"), QStringLiteral("Trumpet"),
                             QStringLiteral("Brass"), 40.0, 40.0);
        project.selectPerformer(0, false);
        project.addSet(QStringLiteral("Set 2"), 8);
        project.nudgeSelected(0.0, 8.0);
        project.addSet(QStringLiteral("Set 3"), 8);
        project.nudgeSelected(0.0, 8.0);
        project.setPlaybackActive(true);
        project.setCurrentSetIndex(1);
        QVERIFY(!project.data(project.index(0, 0), DrillProject::ClosingTransitionRole).toBool());
        project.setCurrentSetIndex(2);
        QVERIFY(project.data(project.index(0, 0), DrillProject::ClosingTransitionRole).toBool());

        DrillProject planted;
        planted.newProject();
        planted.addPerformer(QStringLiteral("P1"), QStringLiteral("Trumpet"),
                             QStringLiteral("Brass"), 40.0, 40.0);
        planted.selectPerformer(0, false);
        planted.addSet(QStringLiteral("Hold"), 8);
        planted.setPlaybackActive(true);
        planted.setPlayhead(0.5);
        QCOMPARE(planted.data(planted.index(0, 0), DrillProject::LocomotionModeRole).toString(),
                 QStringLiteral("idle"));
        planted.setPlaybackActive(false);
        planted.faceSelected(90.0);
        planted.setPlaybackActive(true);
        QCOMPARE(planted.data(planted.index(0, 0), DrillProject::LocomotionModeRole).toString(),
                 QStringLiteral("direction_change"));
    }
};

QTEST_MAIN(DrillProjectTest)
#include "tst_drillproject.moc"
