#include "DrillProject.h"

#include <QColor>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

QString markerColor(const QString &type)
{
    if (type == QStringLiteral("impact") || type == QStringLiteral("hit"))
        return QStringLiteral("#f97316");
    if (type == QStringLiteral("phrase")) return QStringLiteral("#8b5cf6");
    if (type == QStringLiteral("tempo")) return QStringLiteral("#ec4899");
    if (type == QStringLiteral("rehearsal")) return QStringLiteral("#38bdf8");
    return QStringLiteral("#f59e0b");
}

QString formatClock(double milliseconds)
{
    const int total = qMax(0, qRound(milliseconds));
    const int minutes = total / 60000;
    const int seconds = (total / 1000) % 60;
    const int tenths = (total / 100) % 10;
    return QStringLiteral("%1:%2.%3").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0')).arg(tenths);
}

} // namespace

QVariantList DrillProject::selectedSetIndices() const
{
    QList<int> indices = m_selectedTimelineSets.values();
    std::sort(indices.begin(), indices.end());
    QVariantList result;
    result.reserve(indices.size());
    for (int index : indices) result.push_back(index);
    return result;
}

bool DrillProject::isSetSelected(int index) const
{
    return m_timelineSelectionKind == QStringLiteral("set")
        && m_selectedTimelineSets.contains(index);
}

void DrillProject::selectTimelineSet(int index, int mode)
{
    if (m_sets.isEmpty()) return;
    index = qBound(0, index, m_sets.size() - 1);
    if (mode == 1) {
        selectSetRange(index, true);
        return;
    }
    if (mode != 2) {
        selectSetRange(index, false);
        return;
    }

    if (m_selectedTimelineSets.contains(index) && m_selectedTimelineSets.size() > 1)
        m_selectedTimelineSets.remove(index);
    else
        m_selectedTimelineSets.insert(index);
    QList<int> ordered = m_selectedTimelineSets.values();
    std::sort(ordered.begin(), ordered.end());
    m_selectedSetStart = ordered.isEmpty() ? index : ordered.first();
    m_selectedSetEnd = ordered.isEmpty() ? index : ordered.last();
    m_timelineSelectionKind = QStringLiteral("set");
    m_selectedTransition = -1;
    m_selectedTimelineMarkerId.clear();
    const bool hadMusicSelection = m_musicSelectionStart >= 0 || m_musicSelectionEnd >= 0;
    m_musicSelectionStart = m_musicSelectionEnd = -1;
    setCurrentSetIndex(index);
    emit setRangeChanged();
    emit timelineSelectionChanged();
    if (hadMusicSelection) emit musicChanged();
}

void DrillProject::selectTimelineTransition(int destinationSet)
{
    if (destinationSet <= 0 || destinationSet >= m_sets.size()) return;
    if (m_timelineSelectionKind == QStringLiteral("transition")
        && m_selectedTransition == destinationSet) return;
    m_timelineSelectionKind = QStringLiteral("transition");
    m_selectedTransition = destinationSet;
    m_selectedTimelineMarkerId.clear();
    m_selectedTimelineSets.clear();
    const bool hadMusicSelection = m_musicSelectionStart >= 0 || m_musicSelectionEnd >= 0;
    m_musicSelectionStart = m_musicSelectionEnd = -1;
    emit timelineSelectionChanged();
    if (hadMusicSelection) emit musicChanged();
}

void DrillProject::selectTimelineMarker(const QString &id)
{
    QVariantMap selected;
    for (int index = 0; index < timelineMarkerCount(); ++index) {
        const auto marker = timelineMarkerInfo(index);
        if (marker.value(QStringLiteral("id")).toString() == id) {
            selected = marker;
            break;
        }
    }
    if (selected.isEmpty()) return;
    if (m_timelineSelectionKind == QStringLiteral("marker")
        && m_selectedTimelineMarkerId == id) return;

    m_timelineSelectionKind = QStringLiteral("marker");
    m_selectedTimelineMarkerId = id;
    m_selectedTransition = -1;
    m_selectedTimelineSets.clear();
    m_timelineRangeStart = m_timelineRangeEnd
        = selected.value(QStringLiteral("tick")).toLongLong();
    const bool hadMusicSelection = m_musicSelectionStart >= 0 || m_musicSelectionEnd >= 0;
    m_musicSelectionStart = m_musicSelectionEnd = -1;
    emit timelineSelectionChanged();
    if (hadMusicSelection) emit musicChanged();
}

void DrillProject::selectTimelineRange(qint64 startTick, qint64 endTick)
{
    startTick = qMax<qint64>(0, startTick);
    endTick = qMax<qint64>(0, endTick);
    m_timelineRangeStart = qMin(startTick, endTick);
    m_timelineRangeEnd = qMax(startTick, endTick);
    m_timelineSelectionKind = QStringLiteral("time");
    m_selectedTransition = -1;
    m_selectedTimelineMarkerId.clear();
    m_selectedTimelineSets.clear();
    const bool hadMusicSelection = m_musicSelectionStart >= 0 || m_musicSelectionEnd >= 0;
    m_musicSelectionStart = m_musicSelectionEnd = -1;
    emit timelineSelectionChanged();
    if (hadMusicSelection) emit musicChanged();
}

void DrillProject::clearTimelineSelection()
{
    if (m_timelineSelectionKind == QStringLiteral("none") && m_selectedTimelineSets.isEmpty()
        && m_selectedTimelineMarkerId.isEmpty()
        && m_musicSelectionStart < 0 && m_musicSelectionEnd < 0) return;
    const bool hadMusicSelection = m_musicSelectionStart >= 0 || m_musicSelectionEnd >= 0;
    m_timelineSelectionKind = QStringLiteral("none");
    m_selectedTransition = -1;
    m_selectedTimelineMarkerId.clear();
    m_selectedTimelineSets.clear();
    m_musicSelectionStart = m_musicSelectionEnd = -1;
    m_timelineRangeStart = m_timelineRangeEnd = 0;
    emit timelineSelectionChanged();
    if (hadMusicSelection) emit musicChanged();
}

int DrillProject::absoluteCountAtTick(qint64 tick) const
{
    return pulsesBetween(0, qMax<qint64>(0, tick));
}

qint64 DrillProject::tickAtAbsoluteCount(int count) const
{
    return advancePulses(0, qBound(0, count, 1000000));
}

void DrillProject::rebuildSetTicksFrom(int destinationSet, const QVector<int> &counts)
{
    if (destinationSet <= 0 || destinationSet >= m_sets.size()) return;
    for (int set = destinationSet; set < m_sets.size(); ++set) {
        const int duration = qBound(1, counts.value(set, qMax(1, m_sets[set].counts)), 2048);
        m_sets[set].counts = duration;
        m_sets[set].startTick = advancePulses(m_sets[set - 1].startTick, duration);
    }
}

bool DrillProject::applyTransitionCounts(int destinationSet, int counts, const QString &undoText)
{
    if (destinationSet <= 0 || destinationSet >= m_sets.size()) return false;
    counts = qBound(1, counts, 2048);
    if (m_sets[destinationSet].counts == counts) {
        selectTimelineTransition(destinationSet);
        return true;
    }
    const auto before = toJson();
    QVector<int> durations;
    durations.reserve(m_sets.size());
    for (const auto &set : std::as_const(m_sets)) durations.push_back(qMax(1, set.counts));
    durations[destinationSet] = counts;
    rebuildSetTicksFrom(destinationSet, durations);
    recalculateCounts();
    m_selectedTransition = destinationSet;
    m_timelineSelectionKind = QStringLiteral("transition");
    m_selectedTimelineMarkerId.clear();
    m_selectedTimelineSets.clear();
    emit timingChanged();
    emit timelineSelectionChanged();
    emitAllDataChanged();
    commitSnapshot(before, undoText);
    return true;
}

bool DrillProject::setTransitionCounts(int destinationSet, int counts)
{
    return applyTransitionCounts(destinationSet, counts, QStringLiteral("Resize transition"));
}

bool DrillProject::insertCountsBeforeSet(int setIndex, int counts)
{
    if (setIndex <= 0 || setIndex >= m_sets.size() || counts <= 0) return false;
    return applyTransitionCounts(setIndex, m_sets[setIndex].counts + counts,
                                 QStringLiteral("Insert counts before set"));
}

bool DrillProject::insertCountsAfterSet(int setIndex, int counts)
{
    if (setIndex < 0 || setIndex + 1 >= m_sets.size() || counts <= 0) return false;
    return applyTransitionCounts(setIndex + 1, m_sets[setIndex + 1].counts + counts,
                                 QStringLiteral("Insert counts after set"));
}

bool DrillProject::deleteCountsFromTransition(int destinationSet, int counts)
{
    if (destinationSet <= 0 || destinationSet >= m_sets.size() || counts <= 0) return false;
    return applyTransitionCounts(destinationSet, qMax(1, m_sets[destinationSet].counts - counts),
                                 QStringLiteral("Delete counts from transition"));
}

QVariantMap DrillProject::transitionInfo(int destinationSet) const
{
    if (destinationSet <= 0 || destinationSet >= m_sets.size()) return {};
    const auto &source = m_sets[destinationSet - 1];
    const auto &destination = m_sets[destinationSet];
    double totalDistance = 0.0;
    double maximumDistance = 0.0;
    for (int performer = 0; performer < m_performers.size(); ++performer) {
        const double distance = transitionDistance(performer, destinationSet);
        totalDistance += distance;
        maximumDistance = qMax(maximumDistance, distance);
    }
    const int measureIndex = musicMeasureAtTick(destination.startTick);
    int beat = 0;
    if (measureIndex >= 0) {
        const auto &measure = m_music.measures[measureIndex];
        beat = qMax(1, int((destination.startTick - measure.startTick)
            / qMax<qint64>(1, measure.pulseTicks)) + 1);
    }
    return {{QStringLiteral("destinationSet"), destinationSet},
            {QStringLiteral("sourceSet"), destinationSet - 1},
            {QStringLiteral("sourceNumber"), source.number},
            {QStringLiteral("destinationNumber"), destination.number},
            {QStringLiteral("sourceName"), source.activeVariant().name},
            {QStringLiteral("destinationName"), destination.activeVariant().name},
            {QStringLiteral("counts"), destination.counts},
            {QStringLiteral("startCount"), absoluteCountAtTick(source.startTick)},
            {QStringLiteral("endCount"), absoluteCountAtTick(destination.startTick)},
            {QStringLiteral("startTick"), source.startTick},
            {QStringLiteral("endTick"), destination.startTick},
            {QStringLiteral("durationMs"), transitionDurationMs(destinationSet)},
            {QStringLiteral("measure"), measureIndex >= 0
                ? m_music.measures[measureIndex].displayNumber : 0},
            {QStringLiteral("beat"), beat},
            {QStringLiteral("tempo"), effectiveTempoText(destinationSet)},
            {QStringLiteral("averageDistance"), m_performers.isEmpty()
                ? 0.0 : totalDistance / m_performers.size()},
            {QStringLiteral("maximumDistance"), maximumDistance}};
}

QVariantMap DrillProject::previewTransitionResize(int destinationSet, qint64 targetTick,
                                                   bool snapping) const
{
    if (destinationSet <= 0 || destinationSet >= m_sets.size()) return {};
    const qint64 startTick = m_sets[destinationSet - 1].startTick;
    targetTick = qMax(startTick + 1, targetTick);
    int counts = qBound(1, pulsesBetween(startTick, targetTick), 2048);
    qint64 resultTick = advancePulses(startTick, counts);
    QString snapType = QStringLiteral("count");
    QString snapLabel = QStringLiteral("Count %1").arg(absoluteCountAtTick(resultTick));

    struct Landmark { qint64 tick; QString type; QString label; };
    QVector<Landmark> landmarks;
    if (snapping) {
        for (const auto &measure : m_music.measures) {
            landmarks.push_back({measure.startTick, QStringLiteral("measure"),
                QStringLiteral("Measure %1").arg(measure.displayNumber)});
        }
        for (const auto &section : std::as_const(m_musicSections)) {
            if (section.startMeasure < 0 || section.startMeasure >= m_music.measures.size()) continue;
            landmarks.push_back({m_music.measures[section.startMeasure].startTick,
                QStringLiteral("section"), section.name.isEmpty()
                    ? QStringLiteral("Music section") : section.name});
        }
        for (const auto &anchor : std::as_const(m_music.audioAnchors)) {
            landmarks.push_back({anchor.musicTick, QStringLiteral("sync"),
                QStringLiteral("Audio sync anchor")});
        }
        for (const auto &tempo : std::as_const(m_tempoRegions)) {
            if (tempo.startTick > 0)
                landmarks.push_back({tempo.startTick, QStringLiteral("tempo"),
                    tempo.name.isEmpty() ? QStringLiteral("Tempo change") : tempo.name});
        }
        for (const auto &meter : std::as_const(m_meterRegions)) {
            if (meter.startTick > 0)
                landmarks.push_back({meter.startTick, QStringLiteral("meter"),
                    QStringLiteral("%1/%2 meter").arg(meter.numerator).arg(meter.denominator)});
        }
        for (int index = 0; index < timelineMarkerCount(); ++index) {
            const auto marker = timelineMarkerInfo(index);
            landmarks.push_back({marker.value(QStringLiteral("tick")).toLongLong(),
                QStringLiteral("marker"), marker.value(QStringLiteral("name")).toString()});
        }
        for (int index = 0; index < m_sets.size(); ++index) {
            if (index == destinationSet) continue;
            landmarks.push_back({m_sets[index].startTick, QStringLiteral("set"),
                QStringLiteral("Set %1").arg(m_sets[index].number)});
        }

        const qint64 pulse = qMax<qint64>(1, advancePulses(targetTick, 1) - targetTick);
        qint64 bestDistance = qRound64(pulse * 0.42);
        for (const auto &landmark : std::as_const(landmarks)) {
            if (landmark.tick <= startTick) continue;
            const int landmarkCounts = pulsesBetween(startTick, landmark.tick);
            if (landmarkCounts < 1 || landmarkCounts > 2048) continue;
            const qint64 quantized = advancePulses(startTick, landmarkCounts);
            if (qAbs(quantized - landmark.tick) > pulse / 2) continue;
            const qint64 distance = qAbs(targetTick - quantized);
            if (distance > bestDistance) continue;
            bestDistance = distance;
            counts = landmarkCounts;
            resultTick = quantized;
            snapType = landmark.type;
            snapLabel = landmark.label;
        }
    }

    const int measureIndex = musicMeasureAtTick(resultTick);
    int beat = 0;
    if (measureIndex >= 0) {
        const auto &measure = m_music.measures[measureIndex];
        beat = qMax(1, int((resultTick - measure.startTick)
            / qMax<qint64>(1, measure.pulseTicks)) + 1);
    }
    const double timeMs = openingDurationMs() + millisecondsBetween(0, resultTick);
    return {{QStringLiteral("destinationSet"), destinationSet},
            {QStringLiteral("counts"), counts}, {QStringLiteral("tick"), resultTick},
            {QStringLiteral("absoluteCount"), absoluteCountAtTick(resultTick)},
            {QStringLiteral("measure"), measureIndex >= 0
                ? m_music.measures[measureIndex].displayNumber : 0},
            {QStringLiteral("beat"), beat}, {QStringLiteral("timeMs"), timeMs},
            {QStringLiteral("timeText"), formatClock(timeMs)},
            {QStringLiteral("snapType"), snapType}, {QStringLiteral("snapLabel"), snapLabel}};
}

QVariantMap DrillProject::snapTimelinePosition(qint64 targetTick, bool includeLandmarks) const
{
    targetTick = qMax<qint64>(0, targetTick);
    qint64 resultTick = tickAtAbsoluteCount(absoluteCountAtTick(targetTick));
    QString snapType = QStringLiteral("count");
    QString snapLabel = QStringLiteral("Count %1").arg(absoluteCountAtTick(resultTick));

    struct Landmark { qint64 tick; QString type; QString label; };
    QVector<Landmark> landmarks;
    if (includeLandmarks) {
        for (const auto &measure : m_music.measures) {
            landmarks.push_back({measure.startTick, QStringLiteral("measure"),
                QStringLiteral("Measure %1").arg(measure.displayNumber)});
        }
        for (const auto &section : std::as_const(m_musicSections)) {
            if (section.startMeasure < 0 || section.startMeasure >= m_music.measures.size()) continue;
            landmarks.push_back({m_music.measures[section.startMeasure].startTick,
                QStringLiteral("section"), section.name.isEmpty()
                    ? QStringLiteral("Music section") : section.name});
        }
        for (const auto &anchor : std::as_const(m_music.audioAnchors))
            landmarks.push_back({anchor.musicTick, QStringLiteral("sync"), QStringLiteral("Audio sync anchor")});
        for (const auto &tempo : std::as_const(m_tempoRegions)) {
            if (tempo.startTick > 0)
                landmarks.push_back({tempo.startTick, QStringLiteral("tempo"),
                    tempo.name.isEmpty() ? QStringLiteral("Tempo change") : tempo.name});
        }
        for (const auto &meter : std::as_const(m_meterRegions)) {
            if (meter.startTick > 0)
                landmarks.push_back({meter.startTick, QStringLiteral("meter"),
                    QStringLiteral("%1/%2 meter").arg(meter.numerator).arg(meter.denominator)});
        }
        for (int index = 0; index < timelineMarkerCount(); ++index) {
            const auto marker = timelineMarkerInfo(index);
            landmarks.push_back({marker.value(QStringLiteral("tick")).toLongLong(),
                QStringLiteral("marker"), marker.value(QStringLiteral("name")).toString()});
        }
        for (const auto &set : std::as_const(m_sets))
            landmarks.push_back({set.startTick, QStringLiteral("set"), QStringLiteral("Set %1").arg(set.number)});

        const qint64 pulse = qMax<qint64>(1, advancePulses(targetTick, 1) - targetTick);
        qint64 bestDistance = qRound64(pulse * 0.42);
        for (const auto &landmark : std::as_const(landmarks)) {
            const qint64 distance = qAbs(targetTick - landmark.tick);
            if (distance > bestDistance) continue;
            bestDistance = distance;
            resultTick = landmark.tick;
            snapType = landmark.type;
            snapLabel = landmark.label;
        }
    }

    const int measureIndex = musicMeasureAtTick(resultTick);
    int beat = 0;
    if (measureIndex >= 0) {
        const auto &measure = m_music.measures[measureIndex];
        beat = qMax(1, int((resultTick - measure.startTick)
            / qMax<qint64>(1, measure.pulseTicks)) + 1);
    }
    const double timeMs = openingDurationMs() + millisecondsBetween(0, resultTick);
    return {{QStringLiteral("tick"), resultTick},
            {QStringLiteral("absoluteCount"), absoluteCountAtTick(resultTick)},
            {QStringLiteral("measure"), measureIndex >= 0
                ? m_music.measures[measureIndex].displayNumber : 0},
            {QStringLiteral("beat"), beat}, {QStringLiteral("timeMs"), timeMs},
            {QStringLiteral("timeText"), formatClock(timeMs)},
            {QStringLiteral("snapType"), snapType}, {QStringLiteral("snapLabel"), snapLabel}};
}

QVariantList DrillProject::timelineMarkers() const
{
    QVariantList result;
    for (int index = 0; index < timelineMarkerCount(); ++index)
        result.push_back(timelineMarkerInfo(index));
    std::sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("tick")).toLongLong()
            < b.toMap().value(QStringLiteral("tick")).toLongLong();
    });
    return result;
}

QVariantMap DrillProject::timelineMarkerInfo(int index) const
{
    struct MarkerView {
        QString id;
        qint64 tick;
        QString name;
        QString type;
        QString color;
        QString notes;
        bool readOnly;
    };
    QVector<MarkerView> markers;
    markers.reserve(timelineMarkerCount());
    for (int imported = 0; imported < m_music.markers.size(); ++imported) {
        const auto &marker = m_music.markers[imported];
        markers.push_back({QStringLiteral("score:%1").arg(imported), marker.tick,
            marker.text.isEmpty() ? QStringLiteral("Score marker") : marker.text,
            marker.kind.isEmpty() ? QStringLiteral("score") : marker.kind,
            markerColor(marker.kind), QStringLiteral("Imported with the score"), true});
    }
    for (const auto &marker : m_timelineMarkers)
        markers.push_back({marker.id, marker.tick, marker.name, marker.type,
                           marker.color, marker.notes, false});
    std::stable_sort(markers.begin(), markers.end(), [](const auto &a, const auto &b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        return a.id < b.id;
    });
    if (index < 0 || index >= markers.size()) return {};
    const auto &marker = markers[index];
    const int measureIndex = musicMeasureAtTick(marker.tick);
    int beat = 0;
    if (measureIndex >= 0) {
        const auto &measure = m_music.measures[measureIndex];
        beat = qMax(1, int((marker.tick - measure.startTick)
            / qMax<qint64>(1, measure.pulseTicks)) + 1);
    }
    return {{QStringLiteral("id"), marker.id}, {QStringLiteral("tick"), marker.tick},
            {QStringLiteral("name"), marker.name}, {QStringLiteral("type"), marker.type},
            {QStringLiteral("color"), marker.color}, {QStringLiteral("notes"), marker.notes},
            {QStringLiteral("readOnly"), marker.readOnly},
            {QStringLiteral("absoluteCount"), absoluteCountAtTick(marker.tick)},
            {QStringLiteral("measure"), measureIndex >= 0
                ? m_music.measures[measureIndex].displayNumber : 0},
            {QStringLiteral("beat"), beat},
            {QStringLiteral("timeMs"), openingDurationMs() + millisecondsBetween(0, marker.tick)}};
}

QString DrillProject::addTimelineMarker(qint64 tick, const QString &name, const QString &type,
                                        const QString &color, const QString &notes)
{
    const QString cleanName = name.simplified().left(80);
    if (cleanName.isEmpty()) return {};
    const auto before = toJson();
    MarchCraft::TimelineMarker marker;
    marker.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    marker.tick = qMax<qint64>(0, tick);
    marker.name = cleanName;
    marker.type = type.simplified().isEmpty() ? QStringLiteral("user") : type.simplified().left(32);
    const QColor parsed(color);
    marker.color = parsed.isValid() ? parsed.name() : markerColor(marker.type);
    marker.notes = notes.left(1000);
    m_timelineMarkers.push_back(marker);
    std::stable_sort(m_timelineMarkers.begin(), m_timelineMarkers.end(), [](const auto &a, const auto &b) {
        return a.tick < b.tick;
    });
    emit timelineMarkersChanged();
    emit musicChanged();
    commitSnapshot(before, QStringLiteral("Add timeline marker"));
    return marker.id;
}

bool DrillProject::updateTimelineMarker(const QString &id, qint64 tick, const QString &name,
                                        const QString &type, const QString &color,
                                        const QString &notes)
{
    const auto found = std::find_if(m_timelineMarkers.begin(), m_timelineMarkers.end(),
        [&](const auto &marker) { return marker.id == id; });
    const QString cleanName = name.simplified().left(80);
    if (found == m_timelineMarkers.end() || cleanName.isEmpty()) return false;
    const auto before = toJson();
    found->tick = qMax<qint64>(0, tick);
    const qint64 updatedTick = found->tick;
    found->name = cleanName;
    found->type = type.simplified().isEmpty() ? QStringLiteral("user") : type.simplified().left(32);
    const QColor parsed(color);
    found->color = parsed.isValid() ? parsed.name() : markerColor(found->type);
    found->notes = notes.left(1000);
    std::stable_sort(m_timelineMarkers.begin(), m_timelineMarkers.end(), [](const auto &a, const auto &b) {
        return a.tick < b.tick;
    });
    emit timelineMarkersChanged(); emit musicChanged();
    if (m_selectedTimelineMarkerId == id) {
        m_timelineRangeStart = m_timelineRangeEnd = updatedTick;
        emit timelineSelectionChanged();
    }
    commitSnapshot(before, QStringLiteral("Edit timeline marker"));
    return true;
}

bool DrillProject::removeTimelineMarker(const QString &id)
{
    const auto found = std::find_if(m_timelineMarkers.cbegin(), m_timelineMarkers.cend(),
        [&](const auto &marker) { return marker.id == id; });
    if (found == m_timelineMarkers.cend()) return false;
    const auto before = toJson();
    m_timelineMarkers.erase(found);
    emit timelineMarkersChanged(); emit musicChanged();
    if (m_selectedTimelineMarkerId == id) {
        m_selectedTimelineMarkerId.clear();
        m_timelineSelectionKind = QStringLiteral("none");
        m_timelineRangeStart = m_timelineRangeEnd = 0;
        emit timelineSelectionChanged();
    }
    commitSnapshot(before, QStringLiteral("Remove timeline marker"));
    return true;
}

QVariantList DrillProject::setPlanCandidates() const
{
    QVariantList result;
    result.reserve(m_setPlanCandidates.size());
    for (const auto &candidate : m_setPlanCandidates) {
        auto view = candidate.toVariant();
        view.insert(QStringLiteral("showTimeMs"), openingDurationMs() + candidate.timeMs);
        result.push_back(view);
    }
    return result;
}

int DrillProject::setPlanAcceptedNewSetCount() const
{
    return static_cast<int>(std::count_if(m_setPlanCandidates.cbegin(), m_setPlanCandidates.cend(),
        [](const auto &candidate) {
            return candidate.accepted && candidate.action == QStringLiteral("add");
        }));
}

QVariantMap DrillProject::setPlanCandidateInfo(int index) const
{
    if (index < 0 || index >= m_setPlanCandidates.size()) return {};
    auto view = m_setPlanCandidates[index].toVariant();
    view.insert(QStringLiteral("showTimeMs"), openingDurationMs() + m_setPlanCandidates[index].timeMs);
    return view;
}

bool DrillProject::analyzeMusicForSetPlan(const QString &density, const QString &priority,
                                          const QVariantList &preferredCounts, int maximumSets)
{
    if (!m_music.loaded() && m_timelineMarkers.isEmpty()) {
        setStatus(QStringLiteral("Import MIDI/MusicXML or add timeline markers before analysis"));
        return false;
    }
    MarchCraft::SetPlanOptions options;
    options.density = density;
    options.priority = priority;
    options.maximumSets = qBound(16, maximumSets, 512);
    options.preferredCounts.clear();
    for (const auto &value : preferredCounts) {
        const int count = value.toInt();
        if (count > 0 && count <= 256 && !options.preferredCounts.contains(count))
            options.preferredCounts.push_back(count);
    }
    if (options.preferredCounts.isEmpty()) options.preferredCounts = {8, 12, 16, 24, 32};

    QVector<MarchCraft::SetPlanSection> sections;
    for (const auto &section : std::as_const(m_musicSections)) {
        if (m_music.measures.isEmpty()) break;
        const int first = qBound(0, section.startMeasure, m_music.measures.size() - 1);
        const int last = qBound(first, section.endMeasure, m_music.measures.size() - 1);
        sections.push_back({section.name, section.type, m_music.measures[first].startTick,
                            m_music.measures[last].endTick});
    }
    QVector<MarchCraft::ExistingSetTiming> existing;
    existing.reserve(m_sets.size());
    for (int index = 0; index < m_sets.size(); ++index) {
        existing.push_back({index, m_sets[index].number.isEmpty()
            ? QString::number(index + 1) : m_sets[index].number,
            m_sets[index].startTick, index == 0 ? 0 : m_sets[index].counts});
    }
    m_setPlanCandidates = MarchCraft::SetPlanAnalyzer::analyze(
        m_music, m_timelineMarkers, sections, existing, options);
    m_setPlanPreviewActive = true;
    emit setPlanChanged();
    setStatus(QStringLiteral("Set plan: %1 new sets selected from %2 evidence items (%3-set budget)")
        .arg(setPlanAcceptedNewSetCount()).arg(m_setPlanCandidates.size()).arg(options.maximumSets));
    return true;
}

bool DrillProject::setSetPlanCandidateAccepted(int index, bool accepted)
{
    if (!m_setPlanPreviewActive || index < 0 || index >= m_setPlanCandidates.size()) return false;
    if (m_setPlanCandidates[index].action == QStringLiteral("review")) accepted = false;
    if (m_setPlanCandidates[index].accepted == accepted) return true;
    m_setPlanCandidates[index].accepted = accepted;
    emit setPlanChanged();
    return true;
}

bool DrillProject::moveSetPlanCandidate(int index, qint64 tick)
{
    if (!m_setPlanPreviewActive || index < 0 || index >= m_setPlanCandidates.size()) return false;
    auto &candidate = m_setPlanCandidates[index];
    if (candidate.action == QStringLiteral("review")) return false;
    qint64 previous = 0;
    for (const auto &set : std::as_const(m_sets))
        if (set.startTick < tick) previous = qMax(previous, set.startTick);
    for (int other = 0; other < m_setPlanCandidates.size(); ++other)
        if (other != index && m_setPlanCandidates[other].accepted
            && m_setPlanCandidates[other].tick < tick)
            previous = qMax(previous, m_setPlanCandidates[other].tick);
    const int counts = qMax(1, pulsesBetween(previous, qMax(previous + 1, tick)));
    candidate.tick = advancePulses(previous, counts);
    candidate.accepted = true;
    const int measureIndex = musicMeasureAtTick(candidate.tick);
    candidate.measure = measureIndex >= 0 ? m_music.measures[measureIndex].displayNumber : 0;
    candidate.beat = measureIndex >= 0 ? qMax(1, int((candidate.tick - m_music.measures[measureIndex].startTick)
        / qMax<qint64>(1, m_music.measures[measureIndex].pulseTicks)) + 1) : 0;
    candidate.timeMs = millisecondsBetween(0, candidate.tick);
    candidate.countsFromPrevious = counts;
    candidate.reason = QStringLiteral("Adjusted manually in the set-plan preview");
    emit setPlanChanged();
    return true;
}

bool DrillProject::setSetPlanCandidateCounts(int index, int counts)
{
    if (!m_setPlanPreviewActive || index < 0 || index >= m_setPlanCandidates.size()) return false;
    auto &candidate = m_setPlanCandidates[index];
    if (candidate.action == QStringLiteral("review")) return false;
    counts = qBound(1, counts, 2048);
    qint64 previous = 0;
    for (const auto &set : std::as_const(m_sets))
        if (set.startTick < candidate.tick) previous = qMax(previous, set.startTick);
    for (int other = 0; other < m_setPlanCandidates.size(); ++other) {
        if (other != index && m_setPlanCandidates[other].accepted
            && m_setPlanCandidates[other].tick < candidate.tick)
            previous = qMax(previous, m_setPlanCandidates[other].tick);
    }
    candidate.tick = advancePulses(previous, counts);
    candidate.accepted = true;
    candidate.countsFromPrevious = counts;
    const int measureIndex = musicMeasureAtTick(candidate.tick);
    candidate.measure = measureIndex >= 0 ? m_music.measures[measureIndex].displayNumber : 0;
    candidate.beat = measureIndex >= 0 ? qMax(1, int((candidate.tick - m_music.measures[measureIndex].startTick)
        / qMax<qint64>(1, m_music.measures[measureIndex].pulseTicks)) + 1) : 0;
    candidate.timeMs = millisecondsBetween(0, candidate.tick);
    candidate.reason = QStringLiteral("Adjusted to %1 counts in the set-plan preview").arg(counts);
    emit setPlanChanged();
    return true;
}

bool DrillProject::removeSetPlanCandidate(int index)
{
    if (!m_setPlanPreviewActive || index < 0 || index >= m_setPlanCandidates.size()) return false;
    m_setPlanCandidates.removeAt(index);
    emit setPlanChanged();
    return true;
}

bool DrillProject::addSetPlanCandidate(qint64 tick)
{
    if (!m_setPlanPreviewActive) return false;
    qint64 previous = 0;
    for (const auto &set : std::as_const(m_sets)) if (set.startTick < tick) previous = qMax(previous, set.startTick);
    const int counts = qMax(1, pulsesBetween(previous, qMax(previous + 1, tick)));
    MarchCraft::SetPlanCandidate candidate;
    candidate.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    candidate.tick = advancePulses(previous, counts);
    candidate.kind = QStringLiteral("userMarker");
    candidate.action = QStringLiteral("add");
    candidate.title = QStringLiteral("Manual set candidate");
    candidate.reason = QStringLiteral("Added manually in the set-plan preview");
    candidate.color = QStringLiteral("#f59e0b");
    candidate.confidence = 1.0;
    candidate.accepted = true;
    candidate.manual = true;
    candidate.countsFromPrevious = counts;
    const int measureIndex = musicMeasureAtTick(candidate.tick);
    candidate.measure = measureIndex >= 0 ? m_music.measures[measureIndex].displayNumber : 0;
    candidate.timeMs = millisecondsBetween(0, candidate.tick);
    m_setPlanCandidates.push_back(std::move(candidate));
    std::stable_sort(m_setPlanCandidates.begin(), m_setPlanCandidates.end(), [](const auto &a, const auto &b) {
        return a.tick < b.tick;
    });
    emit setPlanChanged();
    return true;
}

void DrillProject::cancelSetPlanPreview()
{
    if (!m_setPlanPreviewActive && m_setPlanCandidates.isEmpty()) return;
    m_setPlanPreviewActive = false;
    m_setPlanCandidates.clear();
    emit setPlanChanged();
}

bool DrillProject::applySetPlan()
{
    if (!m_setPlanPreviewActive) return false;
    const bool hasAccepted = std::any_of(m_setPlanCandidates.cbegin(), m_setPlanCandidates.cend(),
        [](const auto &candidate) { return candidate.accepted && candidate.action != QStringLiteral("review"); });
    if (!hasAccepted) return false;
    const auto before = toJson();
    m_applyingSetPlan = true;

    for (const auto &candidate : std::as_const(m_setPlanCandidates)) {
        if (!candidate.accepted || candidate.action != QStringLiteral("align")
            || candidate.existingSetIndex <= 0 || candidate.existingSetIndex >= m_sets.size()) continue;
        const int index = candidate.existingSetIndex;
        const qint64 minimum = advancePulses(m_sets[index - 1].startTick, 1);
        const qint64 maximum = index + 1 < m_sets.size()
            ? m_sets[index + 1].startTick - 1 : std::numeric_limits<qint64>::max();
        if (candidate.tick >= minimum && candidate.tick < maximum)
            m_sets[index].startTick = candidate.tick;
    }

    QVector<MarchCraft::SetPlanCandidate> additions;
    for (const auto &candidate : std::as_const(m_setPlanCandidates))
        if (candidate.accepted && candidate.action == QStringLiteral("add")) additions.push_back(candidate);
    std::stable_sort(additions.begin(), additions.end(), [](const auto &a, const auto &b) {
        return a.tick < b.tick;
    });
    for (const auto &candidate : std::as_const(additions)) {
        const bool duplicate = std::any_of(m_sets.cbegin(), m_sets.cend(), [&](const auto &set) {
            return set.startTick == candidate.tick;
        });
        if (duplicate) continue;
        int insertAt = 0;
        while (insertAt < m_sets.size() && m_sets[insertAt].startTick < candidate.tick) ++insertAt;
        if (insertAt <= 0) continue;
        MarchCraft::DrillSet set;
        set.startTick = candidate.tick;
        set.activeVariant().placements = m_sets[insertAt - 1].activeVariant().placements;
        set.activeVariant().name = candidate.measure > 0
            ? QStringLiteral("Suggested Set · M. %1").arg(candidate.measure)
            : QStringLiteral("Suggested Set");
        set.measure = candidate.measure > 0 ? QString::number(candidate.measure) : QString();
        m_sets.insert(insertAt, std::move(set));
    }

    std::stable_sort(m_sets.begin(), m_sets.end(), [](const auto &a, const auto &b) {
        return a.startTick < b.startTick;
    });
    recalculateCounts();
    for (int index = 0; index < m_sets.size(); ++index) {
        const QString oldNumber = m_sets[index].number;
        m_sets[index].number = QString::number(index + 1);
        if (m_sets[index].activeVariant().name.isEmpty()
            || m_sets[index].activeVariant().name == QStringLiteral("New set")
            || m_sets[index].activeVariant().name == QStringLiteral("Set %1").arg(oldNumber))
            m_sets[index].activeVariant().name = QStringLiteral("Set %1").arg(index + 1);
    }
    m_currentSet = qBound(0, m_currentSet, m_sets.size() - 1);
    m_selectedSetStart = m_selectedSetEnd = m_currentSet;
    m_selectedTimelineSets = {m_currentSet};
    m_timelineSelectionKind = QStringLiteral("set");
    m_selectedTransition = -1;
    m_selectedTimelineMarkerId.clear();
    m_setPlanPreviewActive = false;
    m_setPlanCandidates.clear();
    m_applyingSetPlan = false;
    emit setsChanged(); emit currentSetChanged(); emit timingChanged(); emit musicChanged();
    emit setRangeChanged(); emit timelineSelectionChanged(); emit setPlanChanged();
    emitAllDataChanged();
    commitSnapshot(before, QStringLiteral("Apply set plan"));
    return true;
}
