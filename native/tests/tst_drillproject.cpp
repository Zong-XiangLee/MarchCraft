#include "DrillProject.h"
#include "DrillTypes.h"
#include "AssetCatalog.h"
#include "SceneTypes.h"
#include "TransportController.h"
#include "WorkspaceController.h"
#include "ProjectStorage.h"
#include "TransitionPath.h"

#include <QFile>
#include <QJsonDocument>
#include <QSet>
#include <QSettings>
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
        QCOMPARE(qvariant_cast<QList<int>>(changed.first().at(2)), QList<int>({DrillProject::XRole, DrillProject::YRole, DrillProject::FacingRole}));
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
        QVERIFY(!catalog.contains(QStringLiteral("performer.body.standard")));
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
        QCOMPARE(project.currentSetIndex(), 1); QVERIFY(project.playhead() < 0.01); transport.stop();

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
        QCOMPARE(project.currentSetIndex(), 1);
        QVERIFY(project.playhead() > 0.999);
        QCOMPARE(project.data(project.index(0, 0), DrillProject::XRole).toDouble(), 40.0);
        transport.seekTick(15360);
        QCOMPARE(project.currentSetIndex(), 2);
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

    void historyRecoveryAndStrictImportPreserveWork()
    {
        QTemporaryDir temporary;
        const QString path = temporary.filePath(QStringLiteral("reliable.marchcraft"));
        DrillProject project;
        project.addPerformer(QStringLiteral("A"), QStringLiteral("Trumpet"), QStringLiteral("Brass"));
        QVERIFY(project.saveProject(path));
        QTest::qWait(5);
        project.addPerformer(QStringLiteral("B"), QStringLiteral("Trumpet"), QStringLiteral("Brass"));
        QVERIFY(project.saveProject(path));
        project.refreshProjectHistory();
        QCOMPARE(project.projectHistory().size(), 2);
        QVERIFY(project.restoreHistoryVersion(1));
        QCOMPARE(project.performerCount(), 1);
        project.undo();
        QCOMPARE(project.performerCount(), 2);

        const QString invalid = temporary.filePath(QStringLiteral("invalid.json"));
        QFile bad(invalid); QVERIFY(bad.open(QIODevice::WriteOnly)); bad.write("{\"performers\":[{}]}"); bad.close();
        QVERIFY(!project.importCoordinateJson(invalid));
        QCOMPARE(project.performerCount(), 2);
        QVERIFY(!project.diagnostics().isEmpty());

        project.selectAll(); project.nudgeSelected(1.0, 0.0);
        QTest::qWait(1100);
        project.refreshRecoveryCandidates();
        int recoveryIndex = -1;
        for (int i = 0; i < project.recoveryCandidates().size(); ++i)
            if (project.recoveryCandidates().at(i).toMap().value(QStringLiteral("projectPath")).toString() == QFileInfo(path).absoluteFilePath()) recoveryIndex = i;
        QVERIFY(recoveryIndex >= 0);
        DrillProject recovered;
        recovered.refreshRecoveryCandidates();
        QVERIFY(recovered.restoreRecovery(recoveryIndex));
        QCOMPARE(recovered.performerCount(), 2);
        project.discardRecovery(recoveryIndex);
    }

    void sampleShowLongEditingSessionSurvivesSaveReopenUndoRedo()
    {
        QTemporaryDir temporary;
        const QString path = temporary.filePath(QStringLiteral("session.marchcraft"));
        DrillProject project;
        QVERIFY(project.importCoordinateJson(QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath(QStringLiteral("../../data/coordinates.json"))));
        QCOMPARE(project.performerCount(), 204);
        for (int i = 0; i < 200; ++i)
            project.setCurrentSetIndex(i % project.setCount());
        // Full-document undo snapshots intentionally make this a representative, bounded smoke test.
        for (int i = 0; i < 8; ++i) {
            project.selectPerformer(i % project.performerCount(), false);
            project.nudgeSelected((i % 2 == 0) ? 0.25 : -0.25, 0.0);
            if (i == 3 || i == 7) QVERIFY(project.saveProject(path));
        }
        QVERIFY(project.saveProject(path));
        DrillProject reopened;
        QVERIFY(reopened.loadProject(path));
        QCOMPARE(reopened.performerCount(), 204);
        reopened.selectPerformer(0, false); reopened.nudgeSelected(0.5, 0.0);
        QVERIFY(reopened.canUndo()); reopened.undo(); QVERIFY(reopened.canRedo()); reopened.redo();
    }
};

QTEST_MAIN(DrillProjectTest)
#include "tst_drillproject.moc"
