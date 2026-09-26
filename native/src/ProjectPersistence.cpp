#include "DrillProject.h"

#include <QDateTime>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QPageSize>
#include <QPdfWriter>
#include <QSaveFile>
#include <QScopedValueRollback>
#include <QSizeF>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>
#include <QUuid>
#include <QXmlStreamReader>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <numeric>
#include <utility>

using MarchCraft::DrillSet;
using MarchCraft::Performer;
using MarchCraft::Placement;

#include "ProjectAlgorithms.h"
#include "ProjectStorage.h"

using namespace MarchCraft::ProjectAlgorithms;
using namespace MarchCraft::ProjectStorage;

QJsonObject DrillProject::toJson() const
{
    QJsonArray performers;
    for (const auto &person : m_performers) performers.push_back(person.toJson());
    QJsonArray sets;
    for (const auto &set : m_sets) sets.push_back(set.toJson());
    QJsonArray archivedSets;
    for (const auto &set : m_archivedSets) archivedSets.push_back(set.toJson());
    QJsonArray meters;
    for (const auto &region : m_meterRegions) meters.push_back(region.toJson());
    QJsonArray tempos;
    for (const auto &region : m_tempoRegions) tempos.push_back(region.toJson());
    QJsonArray props;
    for (const auto &prop : m_props) props.push_back(prop.toJson());
    QJsonArray musicSections;
    for (const auto &section : m_musicSections) {
        musicSections.push_back(QJsonObject{{QStringLiteral("id"), section.id},
            {QStringLiteral("name"), section.name}, {QStringLiteral("type"), section.type},
            {QStringLiteral("color"), section.color},
            {QStringLiteral("startMeasure"), section.startMeasure},
            {QStringLiteral("endMeasure"), section.endMeasure}});
    }
    QJsonArray timelineMarkers;
    for (const auto &marker : m_timelineMarkers) timelineMarkers.push_back(marker.toJson());
    QJsonArray selectedSets;
    QList<int> selectedIndices = m_selectedTimelineSets.values();
    std::sort(selectedIndices.begin(), selectedIndices.end());
    for (int index : selectedIndices) selectedSets.push_back(index);
    QJsonObject result{{QStringLiteral("format"), QStringLiteral("marchcraft")},
            {QStringLiteral("version"), 12},
            {QStringLiteral("showName"), m_showName},
            {QStringLiteral("fieldPreset"), m_fieldPreset},
            {QStringLiteral("audioSource"), m_audioSource},
            {QStringLiteral("audioOffsetMs"), m_audioOffsetMs},
            {QStringLiteral("music"), m_music.toJson()},
            {QStringLiteral("musicSections"), musicSections},
            {QStringLiteral("musicSelectionStart"), m_musicSelectionStart},
            {QStringLiteral("musicSelectionEnd"), m_musicSelectionEnd},
            {QStringLiteral("timelineMarkers"), timelineMarkers},
            {QStringLiteral("bpm"), m_bpm},
            {QStringLiteral("currentSet"), m_currentSet},
            {QStringLiteral("selectedSetStart"), m_selectedSetStart},
            {QStringLiteral("selectedSetEnd"), m_selectedSetEnd},
            {QStringLiteral("selectedSetIndices"), selectedSets},
            {QStringLiteral("timelineSelectionKind"), m_timelineSelectionKind},
            {QStringLiteral("selectedTransition"), m_selectedTransition},
            {QStringLiteral("timelineRangeStart"), m_timelineRangeStart},
            {QStringLiteral("timelineRangeEnd"), m_timelineRangeEnd},
            {QStringLiteral("playbackSource"), m_playbackSource},
            {QStringLiteral("midiMasterVolume"), m_midiMasterVolume},
            {QStringLiteral("loopEnabled"), m_loopEnabled},
            {QStringLiteral("openingBehavior"), m_openingBehavior},
            {QStringLiteral("openingCounts"), m_openingCounts},
            {QStringLiteral("capabilityProfile"), m_capability.toJson()},
            {QStringLiteral("performers"), performers},
            {QStringLiteral("sets"), sets},
            {QStringLiteral("archivedSets"), archivedSets},
            {QStringLiteral("meterRegions"), meters},
            {QStringLiteral("tempoRegions"), tempos},
            {QStringLiteral("venue"), m_venue.toJson()},
            {QStringLiteral("props"), props}};
    // The active movement stays in the legacy top-level fields. Only inactive
    // movements have a stored state, so there is never a second active copy.
    QJsonArray movements;
    for (int i = 0; i < m_movements.size(); ++i) {
        QJsonObject entry{{QStringLiteral("id"), m_movements[i].id},
                          {QStringLiteral("name"), m_movements[i].name}};
        if (i != m_currentMovement) entry.insert(QStringLiteral("state"), m_movements[i].state);
        movements.append(entry);
    }
    result.insert(QStringLiteral("exportBranding"), m_exportBranding);
    result.insert(QStringLiteral("movements"), movements);
    result.insert(QStringLiteral("currentMovement"), m_currentMovement);
    return result;
}

bool DrillProject::restoreJson(const QJsonObject &object, bool preservePath)
{
    if (object.value(QStringLiteral("format")).toString() != QStringLiteral("marchcraft")
        || object.value(QStringLiteral("version")).toInt() > 12) {
        setStatus(QStringLiteral("Unsupported MarchCraft project format"));
        return false;
    }
    QVector<Movement> movements;
    int activeMovement = 0;
    if (object.value(QStringLiteral("version")).toInt() >= 11) {
        const auto entries = object.value(QStringLiteral("movements")).toArray();
        activeMovement = object.value(QStringLiteral("currentMovement")).toInt(-1);
        QSet<QString> ids;
        if (entries.isEmpty() || activeMovement < 0 || activeMovement >= entries.size()) {
            setStatus(QStringLiteral("Invalid movement list")); return false;
        }
        for (int i = 0; i < entries.size(); ++i) {
            const auto entry = entries[i].toObject();
            const QString id = entry.value(QStringLiteral("id")).toString();
            const QString name = entry.value(QStringLiteral("name")).toString().simplified().left(80);
            const auto state = entry.value(QStringLiteral("state")).toObject();
            if (id.isEmpty() || ids.contains(id) || name.isEmpty()
                || (i != activeMovement && state.value(QStringLiteral("sets")).toArray().isEmpty())) {
                setStatus(QStringLiteral("Invalid movement data")); return false;
            }
            ids.insert(id); movements.push_back({id, name, movementState(state)});
        }
    } else movements.push_back({QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("Movement 1"), {}});
    QScopedValueRollback<bool> restoring(m_restoringSnapshot, true);
    beginResetModel();
    m_movements = std::move(movements); m_currentMovement = activeMovement;
    QVector<Performer> performers;
    QVector<DrillSet> sets;
    QVector<DrillSet> archivedSets;
    QVector<MarchCraft::MeterRegion> meterRegions;
    QVector<MarchCraft::TempoRegion> tempoRegions;
    QVector<MarchCraft::PropInstance> props;
    QVector<MusicSection> musicSections;
    QVector<MarchCraft::TimelineMarker> timelineMarkers;
    for (const auto &value : object.value(QStringLiteral("performers")).toArray())
        performers.push_back(Performer::fromJson(value.toObject()));
    for (const auto &value : object.value(QStringLiteral("sets")).toArray())
        sets.push_back(DrillSet::fromJson(value.toObject()));
    for (const auto &value : object.value(QStringLiteral("archivedSets")).toArray())
        archivedSets.push_back(DrillSet::fromJson(value.toObject()));
    for (const auto &value : object.value(QStringLiteral("meterRegions")).toArray())
        meterRegions.push_back(MarchCraft::MeterRegion::fromJson(value.toObject()));
    for (const auto &value : object.value(QStringLiteral("tempoRegions")).toArray())
        tempoRegions.push_back(MarchCraft::TempoRegion::fromJson(value.toObject()));
    for (const auto &value : object.value(QStringLiteral("props")).toArray())
        props.push_back(MarchCraft::PropInstance::fromJson(value.toObject()));
    for (const auto &value : object.value(QStringLiteral("musicSections")).toArray()) {
        const auto section = value.toObject();
        const QString name = section.value(QStringLiteral("name")).toString().simplified().left(60);
        if (name.isEmpty()) continue;
        const QString type = section.value(QStringLiteral("type")).toString() == QStringLiteral("part")
            ? QStringLiteral("part") : QStringLiteral("movement");
        QColor color(section.value(QStringLiteral("color")).toString());
        if (!color.isValid()) color = QColor(QStringLiteral("#8b5cf6"));
        musicSections.push_back({section.value(QStringLiteral("id")).toString(
                                     QUuid::createUuid().toString(QUuid::WithoutBraces)),
                                 name, type, color.name(),
                                 section.value(QStringLiteral("startMeasure")).toInt(),
                                 section.value(QStringLiteral("endMeasure")).toInt()});
    }
    QSet<QString> markerIds;
    for (const auto &value : object.value(QStringLiteral("timelineMarkers")).toArray()) {
        auto marker = MarchCraft::TimelineMarker::fromJson(value.toObject());
        if (marker.id.isEmpty()) marker.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (marker.name.isEmpty() || markerIds.contains(marker.id)) continue;
        markerIds.insert(marker.id);
        timelineMarkers.push_back(std::move(marker));
    }
    for (int i = 0; i < sets.size(); ++i)
        if (sets[i].number.isEmpty()) sets[i].number = QString::number(i + 1);
    if (sets.isEmpty()) {
        DrillSet first;
        first.activeVariant().name = QStringLiteral("Set 1");
        sets.push_back(first);
    }
    m_performers = std::move(performers);
    m_sets = std::move(sets);
    m_archivedSets = std::move(archivedSets);
    m_exportBranding = object.value(QStringLiteral("exportBranding")).toObject();
    m_showName = object.value(QStringLiteral("showName")).toString(QStringLiteral("Untitled Show"));
    m_fieldPreset = MarchCraft::fieldGeometry(object.value(QStringLiteral("fieldPreset")).toString()).id;
    m_audioSource = object.value(QStringLiteral("audioSource")).toString();
    m_audioOffsetMs = object.value(QStringLiteral("audioOffsetMs")).toDouble();
    m_music = MarchCraft::MusicDocument::fromJson(object.value(QStringLiteral("music")).toObject());
    m_musicSelectionStart = qBound(-1, object.value(QStringLiteral("musicSelectionStart")).toInt(-1), m_music.measures.size() - 1);
    m_musicSelectionEnd = qBound(-1, object.value(QStringLiteral("musicSelectionEnd")).toInt(-1), m_music.measures.size() - 1);
    m_musicSections.clear();
    for (auto section : musicSections) {
        if (m_music.measures.isEmpty()) break;
        section.startMeasure = qBound(0, section.startMeasure, m_music.measures.size() - 1);
        section.endMeasure = qBound(section.startMeasure, section.endMeasure, m_music.measures.size() - 1);
        m_musicSections.push_back(std::move(section));
    }
    m_timelineMarkers = std::move(timelineMarkers);
    std::stable_sort(m_timelineMarkers.begin(), m_timelineMarkers.end(), [](const auto &a, const auto &b) {
        return a.tick < b.tick;
    });
    m_setPlanCandidates.clear(); m_setPlanPreviewActive = false;
    if (m_music.sourceType == QStringLiteral("midi") && QFileInfo::exists(m_music.sourcePath)) {
        const auto reparsed = MarchCraft::parseMidiFile(m_music.sourcePath);
        if (reparsed.ok) {
            m_music.playbackEvents = reparsed.document.playbackEvents;
            if (m_music.attackEvents.isEmpty()) m_music.attackEvents = reparsed.document.attackEvents;
        }
    }
    m_venue = MarchCraft::VenueConfiguration::fromJson(object.value(QStringLiteral("venue")).toObject());
    m_props = std::move(props);
    m_bpm = object.value(QStringLiteral("bpm")).toDouble(120.0);
    // Imported timing is a fallback for legacy projects, not a replacement for
    // explicitly saved edits (also used by undo and immutable export snapshots).
    m_meterRegions = {MarchCraft::MeterRegion{}};
    m_tempoRegions = {MarchCraft::TempoRegion{0, std::numeric_limits<qint64>::max(), m_bpm, m_bpm, QStringLiteral("Tempo")}};
    if (m_music.loaded() && (meterRegions.isEmpty() || tempoRegions.isEmpty())) rebuildTimingFromMusic();
    if (!meterRegions.isEmpty()) m_meterRegions = std::move(meterRegions);
    if (!tempoRegions.isEmpty()) m_tempoRegions = std::move(tempoRegions);
    if (object.value(QStringLiteral("version")).toInt() < 3) {
        qint64 tick = 0;
        for (int i = 0; i < m_sets.size(); ++i) {
            m_sets[i].startTick = tick;
            if (i + 1 < m_sets.size()) tick = advancePulses(tick, qMax(1, m_sets[i + 1].counts));
        }
    }
    recalculateCounts();
    m_currentSet = std::clamp(object.value(QStringLiteral("currentSet")).toInt(),
                              0, static_cast<int>(m_sets.size()) - 1);
    m_selectedSetStart = qBound(0, object.value(QStringLiteral("selectedSetStart")).toInt(m_currentSet), m_sets.size()-1);
    m_selectedSetEnd = qBound(0, object.value(QStringLiteral("selectedSetEnd")).toInt(m_selectedSetStart), m_sets.size()-1);
    m_selectedTimelineSets.clear();
    for (const auto &value : object.value(QStringLiteral("selectedSetIndices")).toArray()) {
        const int selected = value.toInt(-1);
        if (selected >= 0 && selected < m_sets.size()) m_selectedTimelineSets.insert(selected);
    }
    static const QSet<QString> selectionKinds{QStringLiteral("none"), QStringLiteral("set"),
        QStringLiteral("transition"), QStringLiteral("time"), QStringLiteral("measure"),
        QStringLiteral("marker")};
    m_timelineSelectionKind = object.value(QStringLiteral("timelineSelectionKind")).toString(QStringLiteral("set"));
    if (!selectionKinds.contains(m_timelineSelectionKind)) m_timelineSelectionKind = QStringLiteral("set");
    if (m_timelineSelectionKind == QStringLiteral("set") && m_selectedTimelineSets.isEmpty()) {
        for (int selected = qMin(m_selectedSetStart, m_selectedSetEnd);
             selected <= qMax(m_selectedSetStart, m_selectedSetEnd); ++selected)
            m_selectedTimelineSets.insert(selected);
    } else if (m_timelineSelectionKind != QStringLiteral("set")) {
        m_selectedTimelineSets.clear();
    }
    m_selectedTransition = qBound(-1, object.value(QStringLiteral("selectedTransition")).toInt(-1), m_sets.size() - 1);
    if (m_selectedTransition <= 0 && m_timelineSelectionKind == QStringLiteral("transition"))
        m_timelineSelectionKind = QStringLiteral("set");
    m_timelineRangeStart = qMax<qint64>(0, object.value(QStringLiteral("timelineRangeStart")).toVariant().toLongLong());
    m_timelineRangeEnd = qMax(m_timelineRangeStart,
        object.value(QStringLiteral("timelineRangeEnd")).toVariant().toLongLong());
    m_playbackSource = object.value(QStringLiteral("playbackSource")).toString(
        m_audioSource.isEmpty() ? QStringLiteral("midi") : QStringLiteral("rehearsal"));
    m_midiMasterVolume = qBound(0.0, object.value(QStringLiteral("midiMasterVolume")).toDouble(0.75), 1.0);
    m_loopEnabled = object.value(QStringLiteral("loopEnabled")).toBool();
    if (object.value(QStringLiteral("version")).toInt() >= 9) {
        m_openingBehavior = object.value(QStringLiteral("openingBehavior")).toString(QStringLiteral("hold")) == QStringLiteral("hold")
            ? QStringLiteral("hold") : QStringLiteral("move");
        m_openingCounts = qBound(1, object.value(QStringLiteral("openingCounts")).toInt(8), 256);
    } else {
        m_openingBehavior = QStringLiteral("move"); m_openingCounts = 8;
    }
    m_capability = MarchCraft::CapabilityProfile::fromJson(object.value(QStringLiteral("capabilityProfile")).toObject());
    m_playhead = 0.0;
    m_playbackActive = false; m_playbackSet = -1;
    m_transitionPaths.clear();
    m_analyticsValid = false;
    ensurePlacements();
    endResetModel();
    emit performerCountChanged();
    if (!preservePath) m_projectPath.clear();
    markDirty();
    emit movementsChanged();
    emit projectChanged(); emit setsChanged(); emit currentSetChanged(); emit selectionChanged();
    emit sceneChanged(); emit propsChanged();
    emit playbackActiveChanged();
    emit playbackFrameChanged();
    emit timingChanged();
    emit musicChanged();
    emit setRangeChanged(); emit transportSettingsChanged();
    emit timelineSelectionChanged(); emit timelineMarkersChanged(); emit setPlanChanged();
    if (!m_backgroundWorkerClone) { invalidateClinic(); analyzeTransition(m_currentSet); startWaveformDecode(); }
    return true;
}

void DrillProject::loadDemo()
{
    resetMovements();
    m_openingBehavior = QStringLiteral("move"); m_openingCounts = 8;
    m_musicSections.clear();
    if (QFile::exists(QStringLiteral(":/samples/coordinates.json"))) {
        importCoordinateJson(QStringLiteral(":/samples/coordinates.json"));
        m_projectPath.clear();
        m_undo.clear();
        m_autosaveTimer.stop();
        m_dirty = false;
        emit dirtyChanged();
        emit projectChanged();
        setStatus(QStringLiteral("Rancho Bernardo 2025 test show loaded"));
        return;
    }

    beginResetModel();
    m_performers.clear();
    m_sets.clear();
    m_archivedSets.clear();
    const QStringList instruments = {QStringLiteral("Trumpet"), QStringLiteral("Mellophone"),
                                     QStringLiteral("Trombone"), QStringLiteral("Baritone"),
                                     QStringLiteral("Tuba"), QStringLiteral("Clarinet"),
                                     QStringLiteral("Alto Sax"), QStringLiteral("Guard")};
    for (int i = 0; i < 24; ++i) {
        Performer performer;
        performer.section = i < 16 ? QStringLiteral("Winds") : QStringLiteral("Guard");
        performer.instrument = instruments.at(i % instruments.size());
        performer.appearance.instrumentAssetId = instrumentAssetIdFor(performer.instrument);
        performer.label = QStringLiteral("%1%2").arg(performer.instrument.left(1).toUpper())
                              .arg(i + 1, 2, 10, QLatin1Char('0'));
        performer.name = QStringLiteral("Performer %1").arg(i + 1);
        performer.symbol = performer.instrument.left(1).toUpper();
        performer.color = sectionColor(performer.section);
        m_performers.push_back(performer);
    }
    for (int setIndex = 0; setIndex < 4; ++setIndex) {
        DrillSet set;
        set.activeVariant().name = QStringLiteral("Set %1").arg(setIndex + 1);
        set.measure = QStringLiteral("%1-%2").arg(setIndex * 4 + 1).arg(setIndex * 4 + 4);
        set.counts = setIndex == 0 ? 0 : 16;
        set.startTick = setIndex * 16 * MarchCraft::TicksPerQuarter;
        for (int i = 0; i < m_performers.size(); ++i) {
            const double angle = 2.0 * std::numbers::pi * i / m_performers.size();
            Placement placement;
            if (setIndex == 0)
                placement.position = {24.0 + (i % 12) * 10.0, 26.0 + (i / 12) * 12.0};
            else if (setIndex == 1)
                placement.position = {80.0 + std::cos(angle) * 45.0, 42.0 + std::sin(angle) * 24.0};
            else if (setIndex == 2)
                placement.position = {32.0 + (i % 8) * 13.5, 18.0 + (i / 8) * 20.0};
            else
                placement.position = {80.0 + std::cos(angle) * (18.0 + i * 1.1),
                                      42.0 + std::sin(angle) * (10.0 + i * 0.55)};
            placement.facing = 0.0;
            set.activeVariant().placements.insert(m_performers[i].id, placement);
        }
        m_sets.push_back(set);
    }
    m_showName = QStringLiteral("MarchCraft Demo");
    m_fieldPreset = QStringLiteral("hs");
    m_currentSet = 0;
    m_playhead = 0.0;
    m_meterRegions = {MarchCraft::MeterRegion{}};
    m_tempoRegions = {MarchCraft::TempoRegion{0, std::numeric_limits<qint64>::max(), m_bpm, m_bpm, QStringLiteral("Tempo")}};
    endResetModel();
    emit performerCountChanged();
    m_undo.clear();
    m_autosaveTimer.stop();
    m_projectPath.clear();
    m_dirty = false;
    emit dirtyChanged();
    setStatus(QStringLiteral("Demo loaded"));
    emit projectChanged();
    emit setsChanged();
    emit currentSetChanged();
    emit timingChanged();
}

bool DrillProject::importCoordinateJson(const QString &urlOrPath)
{
    QFile file(localPath(urlOrPath));
    if (!file.open(QIODevice::ReadOnly)) {
        setStatus(QStringLiteral("Could not open coordinate JSON"));
        return false;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        setStatus(QStringLiteral("Invalid coordinate JSON: %1").arg(error.errorString()));
        return false;
    }
    const auto root = document.object();
    const auto people = root.value(QStringLiteral("performers")).toArray();
    if (people.isEmpty()) {
        setStatus(QStringLiteral("Coordinate file contains no performers"));
        return false;
    }

    beginResetModel();
    resetMovements();
    // Coordinate sheets begin at a zero-count first set. A written hold is a
    // following set at the same coordinate, with that row's count value.
    m_openingBehavior = QStringLiteral("move");
    m_openingCounts = 8;
    m_performers.clear();
    m_sets.clear();
    m_archivedSets.clear();
    QHash<QString, int> setIndices;
    const auto geometry = MarchCraft::fieldGeometry(m_fieldPreset);
    for (const auto &personValue : people) {
        const auto personObject = personValue.toObject();
        Performer performer;
        performer.label = personObject.value(QStringLiteral("label")).toString();
        performer.instrument = personObject.value(QStringLiteral("instrument")).toString();
        performer.section = performer.instrument;
        performer.appearance.instrumentAssetId = instrumentAssetIdFor(performer.instrument);
        if (performer.appearance.instrumentAssetId == QStringLiteral("equipment.guard.flag"))
            performer.appearance.roleId = QStringLiteral("guard");
        performer.symbol = personObject.value(QStringLiteral("symbol")).toString(performer.label.left(1));
        performer.color = sectionColor(performer.section);
        m_performers.push_back(performer);
        for (const auto &setValue : personObject.value(QStringLiteral("sets")).toArray()) {
            const auto sourceSet = setValue.toObject();
            const QString key = QStringLiteral("%1:%2")
                                    .arg(sourceSet.value(QStringLiteral("part")).toInt())
                                    .arg(sourceSet.value(QStringLiteral("set")).toString());
            int destination = setIndices.value(key, -1);
            if (destination < 0) {
                DrillSet set;
                set.activeVariant().name = sourceSet.value(QStringLiteral("set")).toString();
                set.measure = sourceSet.value(QStringLiteral("measure")).toString();
                set.counts = qMax(1, sourceSet.value(QStringLiteral("counts")).toInt(8));
                destination = m_sets.size();
                setIndices.insert(key, destination);
                m_sets.push_back(set);
            }
            const auto lateral = sourceSet.value(QStringLiteral("lateral")).toObject();
            const auto vertical = sourceSet.value(QStringLiteral("vertical")).toObject();
            const int side = lateral.value(QStringLiteral("side")).toInt(1);
            const double yardLine = lateral.value(QStringLiteral("yardLine")).toDouble(50.0);
            double centeredX = (50.0 - yardLine) * 1.6 * (side == 1 ? -1.0 : 1.0);
            const QString lateralRelation = lateral.value(QStringLiteral("relation")).toString();
            if (lateralRelation != QStringLiteral("on")) {
                const double outward = lateralRelation == QStringLiteral("outside") ? -1.0 : 1.0;
                centeredX += lateral.value(QStringLiteral("offset")).toDouble()
                             * outward * (side == 1 ? 1.0 : -1.0);
            }
            const QString landmark = vertical.value(QStringLiteral("landmark")).toString();
            double y = 0.0;
            if (landmark == QStringLiteral("front hash")) y = geometry.frontHash;
            else if (landmark == QStringLiteral("back hash")) y = geometry.backHash;
            else if (landmark == QStringLiteral("back sideline")) y = geometry.depth;
            const QString verticalRelation = vertical.value(QStringLiteral("relation")).toString();
            const double offset = vertical.value(QStringLiteral("offset")).toDouble();
            if (verticalRelation == QStringLiteral("in front of")) y -= offset;
            else if (verticalRelation == QStringLiteral("behind")) y += offset;
            m_sets[destination].activeVariant().placements.insert(
                performer.id, Placement{{80.0 + centeredX, y}, 0.0});
        }
    }
    m_showName = root.value(QStringLiteral("show")).toString(QStringLiteral("Imported Show"));
    m_meterRegions = {MarchCraft::MeterRegion{}};
    m_tempoRegions = {MarchCraft::TempoRegion{0, std::numeric_limits<qint64>::max(), m_bpm, m_bpm, QStringLiteral("Tempo")}};
    qint64 tick = 0;
    for (int i = 0; i < m_sets.size(); ++i) {
        m_sets[i].number = QString::number(i + 1);
        m_sets[i].startTick = tick;
        if (i == 0) { m_sets[i].counts = 0; m_sets[i].stepMultiplier = 0.0; }
        if (i + 1 < m_sets.size()) tick = advancePulses(tick, qMax(1, m_sets[i + 1].counts));
    }
    m_currentSet = 0;
    m_playhead = 0.0;
    ensurePlacements();
    endResetModel();
    emit performerCountChanged();
    m_undo.clear();
    markDirty(QStringLiteral("Imported %1 performers and %2 sets")
                  .arg(m_performers.size()).arg(m_sets.size()));
    emit projectChanged();
    emit setsChanged();
    emit currentSetChanged();
    emit timingChanged();
    return true;
}

bool DrillProject::saveProject(const QString &urlOrPath)
{
    QString path = localPath(urlOrPath);
    if (path.isEmpty())
        path = m_projectPath;
    if (path.isEmpty()) {
        setStatus(QStringLiteral("Choose a project filename"));
        return false;
    }
    if (!path.endsWith(QStringLiteral(".marchcraft"), Qt::CaseInsensitive))
        path += QStringLiteral(".marchcraft");
    if (QFile::exists(path)) {
        const QString backup = path + QStringLiteral(".backup-")
            + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-hhmmss"));
        QFile::copy(path, backup);
    }
    QString databaseError;
    if (!writeSqliteProject(path, toJson(), &databaseError)) {
        setStatus(QStringLiteral("Could not save project database: %1").arg(databaseError));
        return false;
    }
    m_projectPath = path;
    m_autosaveTimer.stop();
    m_undo.setClean();
    m_dirty = false;
    emit dirtyChanged();
    emit projectChanged();
    const QString recoveryPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/recovery.marchcraft");
    if (QFileInfo(path).absoluteFilePath() != QFileInfo(recoveryPath).absoluteFilePath())
        QFile::remove(recoveryPath);
    setStatus(QStringLiteral("Saved %1").arg(QFileInfo(path).fileName()));
    return true;
}

bool DrillProject::loadProject(const QString &urlOrPath)
{
    const QString path = localPath(urlOrPath);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setStatus(QStringLiteral("Could not open project"));
        return false;
    }
    const QByteArray signature = file.peek(16); file.close();
    QJsonObject root; bool legacyJson = !signature.startsWith("SQLite format 3");
    if (legacyJson) {
        if (!file.open(QIODevice::ReadOnly)) return false;
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (!document.isObject() || parseError.error != QJsonParseError::NoError) {
            setStatus(QStringLiteral("Invalid project: %1").arg(parseError.errorString())); return false;
        }
        root = document.object();
    } else {
        QString databaseError;
        if (!readSqliteProject(path, &root, &databaseError)) {
            setStatus(QStringLiteral("Invalid project database: %1").arg(databaseError)); return false;
        }
    }
    if (!restoreJson(root, false))
        return false;
    m_projectPath = legacyJson ? QString{} : path;
    m_autosaveTimer.stop();
    m_dirty = false;
    m_undo.clear();
    emit dirtyChanged();
    emit projectChanged();
    setStatus(legacyJson ? QStringLiteral("Imported legacy project; save to create a .marchcraft database")
                         : QStringLiteral("Opened %1").arg(QFileInfo(path).fileName()));
    return true;
}

bool DrillProject::loadRecoveryProject(const QString &urlOrPath)
{
    if (!loadProject(urlOrPath))
        return false;
    m_projectPath.clear();
    emit projectChanged();
    markDirty(QStringLiteral("Recovered autosaved work — choose Save As to keep it"));
    return true;
}

bool DrillProject::exportCsv(const QString &urlOrPath) const
{
    QSaveFile file(localPath(urlOrPath));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream stream(&file);
    stream << QStringLiteral("Performer,Name,Instrument,Section,Set,Measure,Counts,Coordinate,Move steps,Total steps\n");
    for (int p = 0; p < m_performers.size(); ++p) {
        for (int s = 0; s < m_sets.size(); ++s) {
            const auto &person = m_performers[p];
            const auto &set = m_sets[s];
            stream << csvCell(person.label) << QLatin1Char(',') << csvCell(person.name) << QLatin1Char(',')
                   << csvCell(person.instrument) << QLatin1Char(',') << csvCell(person.section) << QLatin1Char(',')
                   << csvCell(set.activeVariant().name) << QLatin1Char(',') << csvCell(set.measure) << QLatin1Char(',')
                   << set.counts << QLatin1Char(',') << csvCell(coordinateFor(p, s)) << QLatin1Char(',')
                   << QString::number(transitionDistance(p, s), 'f', 2) << QLatin1Char(',')
                   << QString::number(performerTotalDistance(p), 'f', 2) << QLatin1Char('\n');
        }
    }
    return file.commit();
}

bool DrillProject::exportCoordinatePdf(const QString &urlOrPath) const
{
    const QString path = localPath(urlOrPath);
    QPdfWriter writer(path);
    writer.setTitle(m_showName + QStringLiteral(" Coordinate Sheets"));
    writer.setCreator(QStringLiteral("MarchCraft"));
    writer.setPageSize(QPageSize(QPageSize::Letter));
    writer.setPageOrientation(QPageLayout::Landscape);
    writer.setResolution(144);
    QPainter painter(&writer);
    if (!painter.isActive())
        return false;
    const QRect page = writer.pageLayout().paintRectPixels(writer.resolution());
    const int left = page.left() + 34;
    const int right = page.right() - 34;
    const int tableWidth = right - left;
    const int rowHeight = 25;
    const int tableTop = page.top() + 126;
    const int footerSpace = 42;
    const int rowsPerPage = qMax(1, (page.bottom() - footerSpace - tableTop - rowHeight) / rowHeight);
    const int pagesPerPerformer = qMax(1, static_cast<int>(std::ceil(m_sets.size() / static_cast<double>(rowsPerPage))));
    const int totalPages = pagesPerPerformer * m_performers.size();
    int outputPage = 0;

    QFont heading = painter.font();
    heading.setBold(true);
    heading.setPointSize(15);
    QFont performerFont = painter.font();
    performerFont.setBold(true);
    performerFont.setPointSize(10);
    QFont body = painter.font();
    body.setPointSize(7);
    QFont tableHeader = body;
    tableHeader.setBold(true);

    const QVector<double> columnFractions = {0.055, 0.085, 0.060, 0.300, 0.275, 0.095, 0.130};
    QStringList headers = {QStringLiteral("Set"), QStringLiteral("Measure"), QStringLiteral("Counts"),
                           QStringLiteral("Side-to-side"), QStringLiteral("Front-to-back"),
                           QStringLiteral("Move"), QStringLiteral("Step/count")};

    for (int p = 0; p < m_performers.size(); ++p) {
        const auto &person = m_performers[p];
        for (int performerPage = 0; performerPage < pagesPerPerformer; ++performerPage) {
            if (outputPage > 0)
                writer.newPage();
            ++outputPage;

            painter.fillRect(page, Qt::white);
            painter.setPen(QColor(QStringLiteral("#111827")));
            painter.setFont(heading);
            painter.drawText(left, page.top() + 34, QStringLiteral("MARCHCRAFT COORDINATE SHEET"));
            painter.setFont(performerFont);
            painter.drawText(left, page.top() + 64,
                             QStringLiteral("Performer: %1    Instrument: %2    Section: %3")
                                 .arg(person.label, person.instrument, person.section));
            painter.drawText(left, page.top() + 90,
                             QStringLiteral("Name: %1").arg(person.name.isEmpty() ? QStringLiteral("-") : person.name));
            painter.drawText(QRect(left, page.top() + 48, tableWidth, 28), Qt::AlignRight | Qt::AlignVCenter,
                             m_showName);
            painter.setFont(body);
            painter.drawText(QRect(left, page.top() + 78, tableWidth, 24), Qt::AlignRight | Qt::AlignVCenter,
                             QStringLiteral("Performer page %1 of %2").arg(performerPage + 1).arg(pagesPerPerformer));

            int x = left;
            painter.fillRect(QRect(left, tableTop, tableWidth, rowHeight), QColor(QStringLiteral("#17212b")));
            painter.setPen(Qt::white);
            painter.setFont(tableHeader);
            for (int c = 0; c < headers.size(); ++c) {
                const int width = c == headers.size() - 1
                    ? right - x : qRound(tableWidth * columnFractions[c]);
                painter.drawText(QRect(x + 5, tableTop, width - 10, rowHeight),
                                 Qt::AlignLeft | Qt::AlignVCenter, headers[c]);
                x += width;
            }

            painter.setFont(body);
            const int firstSet = performerPage * rowsPerPage;
            const int lastSet = qMin(firstSet + rowsPerPage, static_cast<int>(m_sets.size()));
            for (int s = firstSet; s < lastSet; ++s) {
                const int row = s - firstSet;
                const int y = tableTop + rowHeight * (row + 1);
                if (row % 2 == 1)
                    painter.fillRect(QRect(left, y, tableWidth, rowHeight), QColor(QStringLiteral("#eef2f4")));
                painter.setPen(QColor(QStringLiteral("#111827")));
                const auto &set = m_sets[s];
                const QString combined = coordinateFor(p, s);
                QString lateral = combined.section(QStringLiteral(" · "), 0, 0);
                const QString vertical = combined.section(QStringLiteral(" · "), 1, 1);
                const double move = transitionDistance(p, s);
                const double plannedSteps = set.counts * set.stepMultiplier;
                const double stepPerCount = plannedSteps > 0.0 ? move / plannedSteps
                                                               : (move > 0.0 ? std::numeric_limits<double>::infinity() : 0.0);
                const QStringList values = {set.activeVariant().name, set.measure, QString::number(set.counts), lateral,
                                            vertical, QStringLiteral("%1 st").arg(move, 0, 'f', 1),
                                            QString::number(stepPerCount, 'f', 2)};
                x = left;
                for (int c = 0; c < values.size(); ++c) {
                    const int width = c == values.size() - 1
                        ? right - x : qRound(tableWidth * columnFractions[c]);
                    const QString text = painter.fontMetrics().elidedText(values[c], Qt::ElideRight, width - 10);
                    painter.drawText(QRect(x + 5, y, width - 10, rowHeight),
                                     Qt::AlignLeft | Qt::AlignVCenter, text);
                    painter.setPen(QColor(QStringLiteral("#d1d5db")));
                    painter.drawLine(x + width, y, x + width, y + rowHeight);
                    painter.setPen(QColor(QStringLiteral("#111827")));
                    x += width;
                }
                painter.setPen(QColor(QStringLiteral("#d1d5db")));
                painter.drawLine(left, y + rowHeight, right, y + rowHeight);
            }

            painter.setPen(QColor(QStringLiteral("#64748b")));
            painter.setFont(body);
            painter.drawText(left, page.bottom() - 16,
                             QStringLiteral("Total distance: %1 steps / %2 yards")
                                 .arg(performerTotalDistance(p), 0, 'f', 1)
                                 .arg(performerTotalDistance(p) * 5.0 / 8.0, 0, 'f', 1));
            painter.drawText(QRect(left, page.bottom() - 31, tableWidth, 24),
                             Qt::AlignRight | Qt::AlignVCenter,
                             QStringLiteral("Page %1 of %2").arg(outputPage).arg(totalPages));
        }
    }
    painter.end();
    return true;
}

bool DrillProject::importMusicXml(const QString &urlOrPath)
{
    QFile file(localPath(urlOrPath));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStatus(QStringLiteral("Could not open MusicXML"));
        return false;
    }
    struct ImportedMeasure { QString number; qint64 tick = 0; qint64 duration = 0; int noteCount = 0; };
    QXmlStreamReader xml(&file);
    QVector<ImportedMeasure> measures;
    QVector<MarchCraft::MeterRegion> meters;
    QVector<MarchCraft::TempoRegion> tempos;
    int divisions = 1, beats = 4, beatType = 4, unsupported = 0;
    qint64 scoreTick = 0, cursor = 0, maximum = 0, lastNoteStart = 0;
    int measureNoteCount = 0;
    QVector<MarchCraft::MusicAttackEvent> attacks;
    QString measureNumber;
    bool inMeasure = false;
    auto sourceToTicks = [&divisions](int duration) {
        return qRound64(double(duration) * MarchCraft::TicksPerQuarter / qMax(1, divisions));
    };
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1StringView("measure")) {
            inMeasure = true; cursor = 0; maximum = 0; lastNoteStart = 0; measureNoteCount = 0;
            measureNumber = xml.attributes().value(QLatin1StringView("number")).toString();
        } else if (xml.isEndElement() && xml.name() == QLatin1StringView("measure")) {
            const qint64 nominal = qRound64(double(beats) * 4.0 / beatType * MarchCraft::TicksPerQuarter);
            const qint64 duration = maximum > 0 ? maximum : nominal;
            measures.push_back({measureNumber, scoreTick, duration, measureNoteCount});
            scoreTick += duration; inMeasure = false;
        } else if (!xml.isStartElement()) {
            continue;
        } else if (xml.name() == QLatin1StringView("divisions")) {
            divisions = qMax(1, xml.readElementText().toInt());
        } else if (xml.name() == QLatin1StringView("beats")) {
            beats = qMax(1, xml.readElementText().toInt());
        } else if (xml.name() == QLatin1StringView("beat-type")) {
            beatType = qMax(1, xml.readElementText().toInt());
            MarchCraft::MeterRegion r;
            r.startTick = scoreTick; r.numerator = beats; r.denominator = beatType;
            r.pulseTicks = (beats == 6 && beatType == 8) ? MarchCraft::TicksPerQuarter * 3 / 2
                                                        : MarchCraft::TicksPerQuarter * 4 / beatType;
            r.grouping = (beats == 6 && beatType == 8) ? QStringLiteral("3+3") : QString::number(beats);
            if (!meters.isEmpty()) meters.last().endTick = scoreTick;
            meters.push_back(r);
        } else if (xml.name() == QLatin1StringView("note")) {
            int rawDuration = 0; bool chord = false, grace = false, rest = false;
            while (!(xml.isEndElement() && xml.name() == QLatin1StringView("note")) && !xml.atEnd()) {
                xml.readNext();
                if (!xml.isStartElement()) continue;
                if (xml.name() == QLatin1StringView("duration")) rawDuration = xml.readElementText().toInt();
                else if (xml.name() == QLatin1StringView("chord")) chord = true;
                else if (xml.name() == QLatin1StringView("grace")) grace = true;
                else if (xml.name() == QLatin1StringView("rest")) rest = true;
            }
            const qint64 attackTick = scoreTick + (chord ? lastNoteStart : cursor);
            if (!rest && !grace) {
                attacks.push_back({attackTick, 0, 0, 80, false});
                ++measureNoteCount;
            }
            if (!chord && !grace) {
                lastNoteStart = cursor;
                cursor += sourceToTicks(rawDuration);
            }
            maximum = qMax(maximum, cursor);
        } else if (xml.name() == QLatin1StringView("backup") || xml.name() == QLatin1StringView("forward")) {
            const bool backward = xml.name() == QLatin1StringView("backup"); int rawDuration = 0;
            while (!(xml.isEndElement() && (xml.name() == QLatin1StringView("backup") || xml.name() == QLatin1StringView("forward"))) && !xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement() && xml.name() == QLatin1StringView("duration")) rawDuration = xml.readElementText().toInt();
            }
            cursor += (backward ? -1 : 1) * sourceToTicks(rawDuration); cursor = qMax<qint64>(0, cursor);
            maximum = qMax(maximum, cursor);
        } else if (xml.name() == QLatin1StringView("sound")) {
            bool ok = false; const double tempo = xml.attributes().value(QLatin1StringView("tempo")).toDouble(&ok);
            if (ok) {
                if (!tempos.isEmpty()) tempos.last().endTick = scoreTick + cursor;
                tempos.push_back({scoreTick + cursor, std::numeric_limits<qint64>::max(), tempo, tempo,
                                  QStringLiteral("Imported tempo")});
            }
        } else if (xml.name() == QLatin1StringView("repeat")) {
            ++unsupported;
        }
    }
    if (xml.hasError() || measures.isEmpty()) {
        setStatus(QStringLiteral("MusicXML did not contain readable measures"));
        return false;
    }
    if (meters.isEmpty()) meters = {MarchCraft::MeterRegion{}};
    if (tempos.isEmpty()) tempos = {MarchCraft::TempoRegion{0, std::numeric_limits<qint64>::max(), m_bpm, m_bpm, QStringLiteral("Tempo")}};
    MarchCraft::MusicDocument document;
    document.sourceType = QStringLiteral("musicxml"); document.sourcePath = localPath(urlOrPath);
    QFile hashFile(document.sourcePath);
    if (hashFile.open(QIODevice::ReadOnly))
        document.sourceHash = QString::fromLatin1(QCryptographicHash::hash(hashFile.readAll(), QCryptographicHash::Sha256).toHex());
    document.durationTick = scoreTick;
    document.tracks.push_back({0, QStringLiteral("Score"), static_cast<int>(attacks.size()), -1, -1, true});
    document.attackEvents = attacks;
    for (const auto &meter : std::as_const(meters))
        document.meters.push_back({meter.startTick, meter.numerator, meter.denominator, meter.pulseTicks, meter.grouping});
    for (const auto &tempo : std::as_const(tempos))
        document.tempos.push_back({tempo.startTick, tempo.startBpm});
    for (int i = 0; i < measures.size(); ++i) {
        const auto &source = measures[i];
        MarchCraft::MusicMeasure measure; measure.index = i;
        bool numberOk = false; measure.displayNumber = source.number.toInt(&numberOk); if (!numberOk) measure.displayNumber = i + 1;
        measure.startTick = source.tick; measure.endTick = source.tick + source.duration;
        const auto meter = std::find_if(document.meters.crbegin(), document.meters.crend(),
            [&](const MarchCraft::MusicMeterEvent &event) { return event.tick <= source.tick; });
        if (meter != document.meters.crend()) {
            measure.numerator = meter->numerator; measure.denominator = meter->denominator; measure.pulseTicks = meter->pulseTicks;
        }
        measure.counts = qMax(1, qRound(double(source.duration) / qMax<qint64>(1, measure.pulseTicks)));
        const qint64 nominal = qRound64(double(MarchCraft::TicksPerQuarter) * 4.0 * measure.numerator / measure.denominator);
        measure.partial = source.duration != nominal;
        measure.noteCount = source.noteCount;
        document.measures.push_back(measure);
    }
    document.firstMeasureNumber = document.measures.first().displayNumber;
    document.durationMs = document.millisecondsAt(document.durationTick);
    if (unsupported > 0) document.diagnostics.push_back(
        QStringLiteral("Written-order timing imported; %1 repeat instruction(s) require review").arg(unsupported));
    applyMusicDocument(std::move(document), QStringLiteral("Import MusicXML"));
    return true;
}


QJsonObject DrillProject::movementState(const QJsonObject &project)
{
    static const QStringList keys = {
        QStringLiteral("sets"), QStringLiteral("archivedSets"), QStringLiteral("currentSet"),
        QStringLiteral("selectedSetStart"), QStringLiteral("selectedSetEnd"),
        QStringLiteral("music"), QStringLiteral("musicSections"),
        QStringLiteral("musicSelectionStart"), QStringLiteral("musicSelectionEnd"),
        QStringLiteral("timelineMarkers"), QStringLiteral("selectedSetIndices"),
        QStringLiteral("timelineSelectionKind"), QStringLiteral("selectedTransition"),
        QStringLiteral("timelineRangeStart"), QStringLiteral("timelineRangeEnd"),
        QStringLiteral("audioSource"), QStringLiteral("audioOffsetMs"), QStringLiteral("bpm"),
        QStringLiteral("meterRegions"), QStringLiteral("tempoRegions"),
        QStringLiteral("playbackSource"), QStringLiteral("midiMasterVolume"), QStringLiteral("loopEnabled"),
        QStringLiteral("openingBehavior"), QStringLiteral("openingCounts")};
    QJsonObject result;
    for (const auto &key : keys) result.insert(key, project.value(key));
    return result;
}

void DrillProject::overlayMovement(QJsonObject &project, const QJsonObject &state)
{
    const auto local = movementState(state);
    const auto previous = movementState(project);
    for (auto it = previous.begin(); it != previous.end(); ++it) project.remove(it.key());
    for (auto it = local.begin(); it != local.end(); ++it) project.insert(it.key(), it.value());
}

void DrillProject::resetMovements()
{
    m_movements = {{QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("Movement 1"), {}}};
    m_currentMovement = 0;
    emit movementsChanged();
}

QVariantList DrillProject::movements() const
{
    QVariantList result;
    for (const auto &movement : m_movements)
        result.append(QVariantMap{{QStringLiteral("id"), movement.id}, {QStringLiteral("name"), movement.name}});
    return result;
}

QString DrillProject::currentMovementName() const
{
    return m_currentMovement >= 0 && m_currentMovement < m_movements.size()
        ? m_movements[m_currentMovement].name : QString();
}

void DrillProject::activateMovement(int index)
{
    if (index < 0 || index >= m_movements.size() || index == m_currentMovement) return;
    auto snapshot = toJson();
    auto entries = snapshot.value(QStringLiteral("movements")).toArray();
    auto leaving = entries[m_currentMovement].toObject();
    leaving.insert(QStringLiteral("state"), movementState(snapshot)); entries[m_currentMovement] = leaving;
    overlayMovement(snapshot, entries[index].toObject().value(QStringLiteral("state")).toObject());
    snapshot.insert(QStringLiteral("movements"), entries);
    snapshot.insert(QStringLiteral("currentMovement"), index);
    const bool wasDirty = m_dirty;
    if (!restoreJson(snapshot)) return;
    clearSelection(); cancelFormationPreview();
    // Workspace navigation must not create undo entries or dirty a saved show.
    if (!wasDirty) { m_dirty = false; m_autosaveTimer.stop(); emit dirtyChanged(); }
    setStatus(QStringLiteral("Editing %1").arg(currentMovementName()));
}

bool DrillProject::createMovement(const QString &name, bool duplicate)
{
    const QString title = name.simplified().left(80);
    if (title.isEmpty()) { setStatus(QStringLiteral("Enter a movement name")); return false; }
    const auto before = toJson();
    auto next = before;
    auto entries = before.value(QStringLiteral("movements")).toArray();
    auto leaving = entries[m_currentMovement].toObject();
    leaving.insert(QStringLiteral("state"), movementState(before)); entries[m_currentMovement] = leaving;
    if (!duplicate) {
        DrillSet first;
        first.number = QStringLiteral("1"); first.activeVariant().name = QStringLiteral("Set 1");
        first.counts = 0; first.stepMultiplier = 0.0; first.startTick = 0;
        if (m_currentSet >= 0 && m_currentSet < m_sets.size())
            first.activeVariant().placements = m_sets[m_currentSet].activeVariant().placements;
        QJsonObject fresh{{QStringLiteral("sets"), QJsonArray{first.toJson()}},
                          {QStringLiteral("bpm"), 120.0},
                          {QStringLiteral("openingBehavior"), QStringLiteral("move")},
                          {QStringLiteral("openingCounts"), 8}};
        overlayMovement(next, fresh);
    }
    entries.append(QJsonObject{{QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
                               {QStringLiteral("name"), title}});
    next.insert(QStringLiteral("movements"), entries);
    next.insert(QStringLiteral("currentMovement"), entries.size() - 1);
    if (!restoreJson(next)) return false;
    clearSelection(); cancelFormationPreview();
    commitSnapshot(before, duplicate ? QStringLiteral("Duplicate movement") : QStringLiteral("Create movement"));
    return true;
}

bool DrillProject::renameMovement(int index, const QString &name)
{
    const QString title = name.simplified().left(80);
    if (index < 0 || index >= m_movements.size() || title.isEmpty()) return false;
    if (m_movements[index].name == title) return true;
    const auto before = toJson();
    m_movements[index].name = title; emit movementsChanged();
    commitSnapshot(before, QStringLiteral("Rename movement")); return true;
}

void DrillProject::removeMovement(int index)
{
    if (m_movements.size() <= 1 || index < 0 || index >= m_movements.size()) return;
    const auto before = toJson();
    if (index == m_currentMovement) activateMovement(index == 0 ? 1 : index - 1);
    m_movements.removeAt(index);
    if (m_currentMovement > index) --m_currentMovement;
    emit movementsChanged(); commitSnapshot(before, QStringLiteral("Delete movement"));
}

void DrillProject::moveMovement(int from, int to)
{
    if (from < 0 || to < 0 || from >= m_movements.size() || to >= m_movements.size() || from == to) return;
    const auto before = toJson(); const QString active = m_movements[m_currentMovement].id;
    m_movements.move(from, to);
    for (int i = 0; i < m_movements.size(); ++i) if (m_movements[i].id == active) m_currentMovement = i;
    emit movementsChanged(); commitSnapshot(before, QStringLiteral("Reorder movements"));
}


void DrillProject::synchronizeMovementRoster()
{
    QSet<QString> ids;
    for (const auto &person : m_performers) ids.insert(person.id);
    for (int i = 0; i < m_movements.size(); ++i) {
        if (i == m_currentMovement) continue;
        for (const QString &key : {QStringLiteral("sets"), QStringLiteral("archivedSets")}) {
            QJsonArray updated;
            for (const auto &value : m_movements[i].state.value(key).toArray()) {
                auto set = DrillSet::fromJson(value.toObject());
                auto synchronize = [&](auto &variant) {
                    for (auto it = variant.placements.begin(); it != variant.placements.end();) {
                        if (!ids.contains(it.key())) it = variant.placements.erase(it); else ++it;
                    }
                    for (int row = 0; row < m_performers.size(); ++row)
                        if (!variant.placements.contains(m_performers[row].id))
                            variant.placements.insert(m_performers[row].id, placementAt(row, m_currentSet));
                    for (auto &group : variant.groups)
                        group.performerIds.removeIf([&](const QString &id) { return !ids.contains(id); });
                    variant.groups.removeIf([](const auto &group) { return group.performerIds.size() < 2; });
                    for (auto &shape : variant.shapes)
                        shape.performerIds.removeIf([&](const QString &id) { return !ids.contains(id); });
                    variant.shapes.removeIf([](const auto &shape) { return shape.performerIds.size() < 2; });
                };
                for (auto &variant : set.variants) synchronize(variant);
                for (auto &variant : set.archivedVariants) synchronize(variant);
                updated.append(set.toJson());
            }
            m_movements[i].state.insert(key, updated);
        }
    }
}
