#include "DrillProject.h"

#include <QSizeF>
#include <QSet>
#include <QUuid>
#include <QtConcurrent>
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

bool DrillProject::importMidi(const QString &urlOrPath)
{
    const auto result = MarchCraft::parseMidiFile(localPath(urlOrPath));
    if (!result.ok) { setStatus(result.error); return false; }
    applyMusicDocument(result.document, QStringLiteral("Import MIDI"));
    return true;
}

void DrillProject::importMidiAsync(const QString &urlOrPath)
{
    if (m_midiWatcher->isRunning()) { setStatus(QStringLiteral("A MIDI import is already running")); return; }
    const QString path = localPath(urlOrPath);
    setStatus(QStringLiteral("Reading MIDI…")); emit musicChanged();
    m_midiImportRevision = m_projectRevision;
    m_midiWatcher->setFuture(QtConcurrent::run([path] { return MarchCraft::parseMidiFile(path); }));
}

void DrillProject::applyMusicDocument(MarchCraft::MusicDocument document, const QString &undoText)
{
    const auto before = toJson();
    m_music = std::move(document);
    m_musicSections.clear();
    if (m_music.sourceType == QStringLiteral("midi")) {
        m_playbackSource = QStringLiteral("midi");
        emit transportSettingsChanged();
    }
    m_musicSelectionStart = m_music.measures.isEmpty() ? -1 : 0;
    m_musicSelectionEnd = m_musicSelectionStart;
    rebuildTimingFromMusic();
    emit projectChanged(); emit timingChanged(); emit musicChanged();
    commitSnapshot(before, undoText);
    setStatus(QStringLiteral("Imported %1 measures, %2 tracks, %3 tempo events")
        .arg(m_music.measures.size()).arg(m_music.tracks.size()).arg(m_music.tempos.size()));
}

void DrillProject::rebuildTimingFromMusic()
{
    if (!m_music.loaded()) return;
    m_meterRegions.clear();
    for (int i = 0; i < m_music.meters.size(); ++i) {
        const auto &event = m_music.meters[i];
        const qint64 end = i + 1 < m_music.meters.size() ? m_music.meters[i + 1].tick
                                                        : std::numeric_limits<qint64>::max();
        m_meterRegions.push_back({event.tick, end, event.numerator, event.denominator,
                                  event.pulseTicks, event.grouping});
    }
    m_tempoRegions.clear();
    for (int i = 0; i < m_music.tempos.size(); ++i) {
        const auto &event = m_music.tempos[i];
        const qint64 end = i + 1 < m_music.tempos.size() ? m_music.tempos[i + 1].tick
                                                        : std::numeric_limits<qint64>::max();
        m_tempoRegions.push_back({event.tick, end, event.bpm, event.bpm, QStringLiteral("MIDI tempo")});
    }
    if (!m_music.tempos.isEmpty()) m_bpm = m_music.tempos.first().bpm;
}

QVariantList DrillProject::musicSections() const
{
    QVariantList result;
    result.reserve(m_musicSections.size());
    for (const auto &section : m_musicSections) {
        result.push_back(QVariantMap{{QStringLiteral("id"), section.id},
            {QStringLiteral("name"), section.name}, {QStringLiteral("type"), section.type},
            {QStringLiteral("color"), section.color},
            {QStringLiteral("startMeasure"), section.startMeasure},
            {QStringLiteral("endMeasure"), section.endMeasure},
            {QStringLiteral("startNumber"), section.startMeasure < m_music.measures.size()
                ? m_music.measures[section.startMeasure].displayNumber : section.startMeasure + 1},
            {QStringLiteral("endNumber"), section.endMeasure < m_music.measures.size()
                ? m_music.measures[section.endMeasure].displayNumber : section.endMeasure + 1}});
    }
    return result;
}

QVariantMap DrillProject::musicMeasureInfo(int index) const
{
    if (index < 0 || index >= m_music.measures.size()) return {};
    const auto &measure = m_music.measures[index];
    double bpm = m_bpm;
    for (const auto &tempo : m_music.tempos) { if (tempo.tick > measure.startTick) break; bpm = tempo.bpm; }
    int setIndex = -1;
    for (int i = 0; i < m_sets.size(); ++i)
        if (m_sets[i].startTick >= measure.startTick && m_sets[i].startTick < measure.endTick) { setIndex = i; break; }
    const int rangeA=m_sets.isEmpty()?0:qBound(0,qMin(m_selectedSetStart,m_selectedSetEnd),m_sets.size()-1);
    const int rangeB=m_sets.isEmpty()?0:qBound(0,qMax(m_selectedSetStart,m_selectedSetEnd),m_sets.size()-1);
    const qint64 rangeStart = m_sets.isEmpty() ? -1 : m_sets[rangeA].startTick;
    const qint64 rangeEnd = m_sets.isEmpty() ? -1 : m_sets[rangeB].startTick;
    QVariantList sections;
    for (const auto &section : m_musicSections) {
        if (index >= section.startMeasure && index <= section.endMeasure)
            sections.push_back(QVariantMap{{QStringLiteral("id"), section.id},
                {QStringLiteral("name"), section.name}, {QStringLiteral("type"), section.type},
                {QStringLiteral("color"), section.color},
                {QStringLiteral("first"), index == section.startMeasure},
                {QStringLiteral("last"), index == section.endMeasure}});
    }
    const bool singleSetMeasure = !m_sets.isEmpty() && rangeA == rangeB
        && m_sets[rangeA].startTick >= measure.startTick && m_sets[rangeA].startTick < measure.endTick;
    return {{QStringLiteral("index"), index}, {QStringLiteral("number"), measure.displayNumber},
            {QStringLiteral("startTick"), measure.startTick}, {QStringLiteral("endTick"), measure.endTick},
            {QStringLiteral("numerator"), measure.numerator}, {QStringLiteral("denominator"), measure.denominator},
            {QStringLiteral("counts"), measure.counts},
            {QStringLiteral("beatTicks"), MarchCraft::TicksPerQuarter * 4 / qMax(1, measure.denominator)},
            {QStringLiteral("noteCount"), measure.noteCount},
            {QStringLiteral("density"), qMin(1.0, measure.noteCount / 160.0)},
            {QStringLiteral("tempo"), bpm}, {QStringLiteral("partial"), measure.partial},
            {QStringLiteral("selected"), index >= qMin(m_musicSelectionStart, m_musicSelectionEnd)
                && index <= qMax(m_musicSelectionStart, m_musicSelectionEnd)},
            {QStringLiteral("setIndex"), setIndex},
            {QStringLiteral("sections"), sections},
            {QStringLiteral("inSetRange"), singleSetMeasure
                || (rangeEnd > rangeStart && measure.endTick > rangeStart && measure.startTick < rangeEnd)}};
}

QVariantMap DrillProject::musicTrackInfo(int index) const
{
    if (index < 0 || index >= m_music.tracks.size()) return {};
    const auto &track = m_music.tracks[index];
    return {{QStringLiteral("index"), track.index}, {QStringLiteral("name"), track.name},
            {QStringLiteral("noteCount"), track.noteCount}, {QStringLiteral("channel"), track.channel},
            {QStringLiteral("program"), track.program}, {QStringLiteral("selected"), track.selected},
            {QStringLiteral("muted"), track.muted}, {QStringLiteral("solo"), track.solo},
            {QStringLiteral("volume"), track.volume}};
}

void DrillProject::setMusicTrackSelected(int index, bool selected)
{
    if (index < 0 || index >= m_music.tracks.size() || m_music.tracks[index].selected == selected) return;
    const auto before = toJson(); m_music.tracks[index].selected = selected;
    emit musicChanged(); commitSnapshot(before, QStringLiteral("Choose score tracks"));
}

void DrillProject::setMusicTrackMuted(int index, bool muted)
{
    if (index < 0 || index >= m_music.tracks.size() || m_music.tracks[index].muted == muted) return;
    const auto before=toJson(); m_music.tracks[index].muted=muted; emit musicChanged();
    commitSnapshot(before, QStringLiteral("Mute MIDI track"));
}

void DrillProject::setMusicTrackSolo(int index, bool solo)
{
    if (index < 0 || index >= m_music.tracks.size() || m_music.tracks[index].solo == solo) return;
    const auto before=toJson(); m_music.tracks[index].solo=solo; emit musicChanged();
    commitSnapshot(before, QStringLiteral("Solo MIDI track"));
}

void DrillProject::setMusicTrackVolume(int index, double volume)
{
    if (index < 0 || index >= m_music.tracks.size()) return; volume=qBound(0.0,volume,1.0);
    if (qFuzzyCompare(m_music.tracks[index].volume,volume)) return;
    const auto before=toJson(); m_music.tracks[index].volume=volume; emit musicChanged();
    commitSnapshot(before, QStringLiteral("Change MIDI track volume"));
}

void DrillProject::setMusicSelection(int startMeasure, int endMeasure)
{
    if (m_music.measures.isEmpty()) return;
    startMeasure = qBound(0, startMeasure, m_music.measures.size() - 1);
    endMeasure = qBound(0, endMeasure, m_music.measures.size() - 1);
    if (startMeasure == m_musicSelectionStart && endMeasure == m_musicSelectionEnd) return;
    m_musicSelectionStart = startMeasure; m_musicSelectionEnd = endMeasure; emit musicChanged();
}

QString DrillProject::addMusicSection(const QString &name, const QString &type,
                                      const QString &color, int startMeasure, int endMeasure)
{
    if (m_music.measures.isEmpty()) return {};
    const int requestedStart = qMin(startMeasure, endMeasure);
    const int requestedEnd = qMax(startMeasure, endMeasure);
    startMeasure = qBound(0, requestedStart, m_music.measures.size() - 1);
    endMeasure = qBound(startMeasure, requestedEnd, m_music.measures.size() - 1);
    const QString cleanName = name.simplified().left(60);
    if (cleanName.isEmpty()) return {};
    const QString cleanType = type == QStringLiteral("part") ? QStringLiteral("part") : QStringLiteral("movement");
    const QColor parsedColor(color);
    const QString cleanColor = parsedColor.isValid() ? parsedColor.name() : QStringLiteral("#8b5cf6");
    const auto before = toJson();
    MusicSection section{QUuid::createUuid().toString(QUuid::WithoutBraces), cleanName,
                         cleanType, cleanColor, startMeasure, endMeasure};
    m_musicSections.push_back(section);
    std::sort(m_musicSections.begin(), m_musicSections.end(), [](const auto &a, const auto &b) {
        if (a.startMeasure != b.startMeasure) return a.startMeasure < b.startMeasure;
        return a.endMeasure < b.endMeasure;
    });
    emit musicChanged();
    commitSnapshot(before, QStringLiteral("Group music measures"));
    return section.id;
}

void DrillProject::removeMusicSection(const QString &id)
{
    const auto it = std::find_if(m_musicSections.cbegin(), m_musicSections.cend(),
                                 [&](const auto &section) { return section.id == id; });
    if (it == m_musicSections.cend()) return;
    const auto before = toJson();
    m_musicSections.erase(it);
    emit musicChanged();
    commitSnapshot(before, QStringLiteral("Remove music group"));
}

QVariantList DrillProject::previewSetGeneration(int startMeasure, int endMeasure,
                                                 const QString &mode, int subdivision,
                                                 double stepMultiplier) const
{
    QVariantList result;
    if (m_music.measures.isEmpty()) return result;
    const int requestedStart = qMin(startMeasure, endMeasure);
    const int requestedEnd = qMax(startMeasure, endMeasure);
    startMeasure = qBound(0, requestedStart, m_music.measures.size() - 1);
    endMeasure = qBound(startMeasure, requestedEnd, m_music.measures.size() - 1);
    subdivision = qBound(1, subdivision, 256); stepMultiplier = qBound(0.0, stepMultiplier, 2.0);
    const qint64 selectionStart = m_music.measures[startMeasure].startTick;
    const qint64 selectionEnd = m_music.measures[endMeasure].endTick;
    QVector<qint64> boundaries{selectionStart};
    if (mode == QStringLiteral("oneMove")) boundaries.push_back(selectionEnd);
    else {
        qint64 cursor = selectionStart;
        while (cursor < selectionEnd) {
            const qint64 next = qMin(selectionEnd, advancePulses(cursor, subdivision));
            if (next <= cursor) break;
            boundaries.push_back(next); cursor = next;
        }
        if (boundaries.last() != selectionEnd) boundaries.push_back(selectionEnd);
    }
    for (int i = 1; i < boundaries.size(); ++i) {
        const int first = musicMeasureAtTick(boundaries[i - 1]);
        const int last = musicMeasureAtTick(qMax<qint64>(boundaries[i - 1], boundaries[i] - 1));
        const int counts = qMax(1, pulsesBetween(boundaries[i - 1], boundaries[i]));
        result.push_back(QVariantMap{{QStringLiteral("startTick"), boundaries[i - 1]},
            {QStringLiteral("endTick"), boundaries[i]}, {QStringLiteral("startMeasure"), first + m_music.firstMeasureNumber},
            {QStringLiteral("endMeasure"), last + m_music.firstMeasureNumber}, {QStringLiteral("counts"), counts},
            {QStringLiteral("durationMs"), millisecondsBetween(boundaries[i - 1], boundaries[i])},
            {QStringLiteral("stepMultiplier"), stepMultiplier}, {QStringLiteral("steps"), counts * stepMultiplier}});
    }
    return result;
}

bool DrillProject::commitSetGeneration(int startMeasure, int endMeasure, const QString &mode,
                                        int subdivision, double stepMultiplier)
{
    return commitSetGenerationPlan(previewSetGeneration(startMeasure, endMeasure, mode, subdivision, stepMultiplier));
}

bool DrillProject::commitSetGenerationPlan(const QVariantList &segments)
{
    if (segments.isEmpty()) return false;
    qint64 previousEnd = -1;
    for (const auto &value : segments) {
        const auto segment = value.toMap();
        const qint64 start = segment.value(QStringLiteral("startTick")).toLongLong();
        const qint64 end = segment.value(QStringLiteral("endTick")).toLongLong();
        if (end <= start || (previousEnd >= 0 && start != previousEnd)) {
            setStatus(QStringLiteral("Set-generation segments must be contiguous and chronological")); return false;
        }
        previousEnd = end;
    }
    const auto before = toJson();
    auto insertMarker = [this](qint64 tick, double multiplier, const QString &measureText,
                               bool updateExisting) {
        for (int i = 0; i < m_sets.size(); ++i) {
            if (m_sets[i].startTick != tick) continue;
            if (updateExisting) {
                m_sets[i].stepMultiplier = multiplier;
                m_sets[i].measure = measureText;
            }
            return i;
        }
        int insertAt = 0; while (insertAt < m_sets.size() && m_sets[insertAt].startTick < tick) ++insertAt;
        const int source = qBound(0, insertAt - 1, m_sets.size() - 1);
        DrillSet set;
        set.startTick = tick; set.stepMultiplier = multiplier; set.measure = measureText;
        set.activeVariant().name = QStringLiteral("Music set %1").arg(insertAt + 1);
        if (!m_sets.isEmpty()) set.activeVariant().placements = m_sets[source].activeVariant().placements;
        m_sets.insert(insertAt, set); return insertAt;
    };
    const auto first = segments.first().toMap();
    insertMarker(first.value(QStringLiteral("startTick")).toLongLong(), 0.0,
                 QString::number(first.value(QStringLiteral("startMeasure")).toInt()), false);
    int lastIndex = 0;
    for (const auto &value : segments) {
        const auto segment = value.toMap();
        const double multiplier = qBound(0.0, segment.value(QStringLiteral("stepMultiplier"), 1.0).toDouble(), 2.0);
        lastIndex = insertMarker(segment.value(QStringLiteral("endTick")).toLongLong(), multiplier,
            QStringLiteral("%1-%2").arg(segment.value(QStringLiteral("startMeasure")).toInt())
                                      .arg(segment.value(QStringLiteral("endMeasure")).toInt()), true);
    }
    for (int i = 0; i < m_sets.size(); ++i) m_sets[i].number = QString::number(i + 1);
    if (!m_sets.isEmpty()) m_sets.first().stepMultiplier = 0.0;
    m_currentSet = qBound(0, lastIndex, m_sets.size() - 1); recalculateCounts();
    emit setsChanged(); emit currentSetChanged(); emitAllDataChanged();
    commitSnapshot(before, QStringLiteral("Create sets from music")); return true;
}

QVariantList DrillProject::previewSetMapping() const
{
    QVariantList result;
    for (int i = 0; i < m_sets.size(); ++i) {
        const int measure = musicMeasureAtTick(m_sets[i].startTick);
        result.push_back(QVariantMap{{QStringLiteral("setIndex"), i}, {QStringLiteral("setName"), m_sets[i].activeVariant().name},
            {QStringLiteral("measureIndex"), measure}, {QStringLiteral("measureNumber"), measure >= 0 ? measure + m_music.firstMeasureNumber : 0},
            {QStringLiteral("startTick"), m_sets[i].startTick}});
    }
    return result;
}

bool DrillProject::applySetMapping(const QVariantList &measureIndices)
{
    if (measureIndices.size() != m_sets.size() || m_music.measures.isEmpty()) return false;
    QVector<int> indices; indices.reserve(measureIndices.size());
    for (const auto &value : measureIndices) indices.push_back(qBound(0, value.toInt(), m_music.measures.size() - 1));
    if (!std::is_sorted(indices.cbegin(), indices.cend())
        || std::adjacent_find(indices.cbegin(), indices.cend()) != indices.cend()) {
        setStatus(QStringLiteral("Each set must map to a later measure")); return false;
    }
    const auto before = toJson();
    for (int i = 0; i < m_sets.size(); ++i) {
        m_sets[i].startTick = m_music.measures[indices[i]].startTick;
        m_sets[i].measure = QString::number(m_music.measures[indices[i]].displayNumber);
    }
    recalculateCounts(); emit setsChanged(); emit timingChanged(); emit musicChanged();
    commitSnapshot(before, QStringLiteral("Map sets to music")); return true;
}

void DrillProject::setSetStepMultiplier(int setIndex, double multiplier)
{
    if (setIndex < 0 || setIndex >= m_sets.size()) return;
    multiplier = qBound(0.0, multiplier, 2.0);
    if (qFuzzyCompare(m_sets[setIndex].stepMultiplier, multiplier)) return;
    const auto before = toJson(); m_sets[setIndex].stepMultiplier = multiplier;
    m_analyticsValid = false; emit setsChanged(); emit statisticsChanged();
    commitSnapshot(before, QStringLiteral("Change marching step mode"));
}

int DrillProject::musicMeasureAtTick(qint64 tick) const
{
    if (m_music.measures.isEmpty()) return -1;
    auto it = std::upper_bound(m_music.measures.cbegin(), m_music.measures.cend(), tick,
        [](qint64 value, const MarchCraft::MusicMeasure &measure) { return value < measure.startTick; });
    return qBound(0, int(std::distance(m_music.measures.cbegin(), it)) - 1, m_music.measures.size() - 1);
}

QVariantMap DrillProject::meterRegionInfo(int index) const
{
    if (index < 0 || index >= m_meterRegions.size()) return {};
    const auto &r = m_meterRegions[index];
    return {{QStringLiteral("startTick"), r.startTick}, {QStringLiteral("endTick"), r.endTick},
            {QStringLiteral("numerator"), r.numerator}, {QStringLiteral("denominator"), r.denominator},
            {QStringLiteral("pulseTicks"), r.pulseTicks}, {QStringLiteral("grouping"), r.grouping}};
}

QVariantMap DrillProject::tempoRegionInfo(int index) const
{
    if (index < 0 || index >= m_tempoRegions.size()) return {};
    const auto &r = m_tempoRegions[index];
    return {{QStringLiteral("startTick"), r.startTick}, {QStringLiteral("endTick"), r.endTick},
            {QStringLiteral("startBpm"), r.startBpm}, {QStringLiteral("endBpm"), r.endBpm},
            {QStringLiteral("name"), r.name}};
}

void DrillProject::setMeterRegion(qint64 startTick, qint64 endTick, int numerator,
                                  int denominator, qint64 pulseTicks, const QString &grouping)
{
    startTick = qMax<qint64>(0, startTick); endTick = qMax(startTick + 1, endTick);
    const auto before = toJson();
    MarchCraft::MeterRegion region{startTick, endTick, qMax(1, numerator), qMax(1, denominator),
                                   qMax<qint64>(1, pulseTicks), grouping};
    QVector<MarchCraft::MeterRegion> next;
    for (auto existing : m_meterRegions) {
        if (existing.endTick <= startTick || existing.startTick >= endTick) next.push_back(existing);
        else {
            if (existing.startTick < startTick) { existing.endTick = startTick; next.push_back(existing); }
            if (existing.endTick > endTick) { existing.startTick = endTick; next.push_back(existing); }
        }
    }
    next.push_back(region);
    std::sort(next.begin(), next.end(), [](const auto &a, const auto &b){ return a.startTick < b.startTick; });
    m_meterRegions = std::move(next);
    recalculateCounts();
    emit timingChanged(); emit projectChanged();
    commitSnapshot(before, QStringLiteral("Edit meter region"));
}

void DrillProject::setTempoRegion(qint64 startTick, qint64 endTick, double startBpm,
                                  double endBpm, const QString &name)
{
    startTick = qMax<qint64>(0, startTick); endTick = qMax(startTick + 1, endTick);
    const auto before = toJson();
    MarchCraft::TempoRegion region{startTick, endTick, qBound(20.0, startBpm, 400.0),
        qBound(20.0, endBpm, 400.0), name.isEmpty() ? QStringLiteral("Tempo") : name};
    QVector<MarchCraft::TempoRegion> next;
    for (auto existing : m_tempoRegions) {
        if (existing.endTick <= startTick || existing.startTick >= endTick) next.push_back(existing);
        else {
            if (existing.startTick < startTick) { existing.endTick = startTick; next.push_back(existing); }
            if (existing.endTick > endTick) { existing.startTick = endTick; next.push_back(existing); }
        }
    }
    next.push_back(region);
    std::sort(next.begin(), next.end(), [](const auto &a, const auto &b){ return a.startTick < b.startTick; });
    m_tempoRegions = std::move(next);
    emit timingChanged(); emit projectChanged();
    commitSnapshot(before, QStringLiteral("Edit tempo region"));
}

void DrillProject::removeMeterRegion(int index)
{
    if (index < 0 || index >= m_meterRegions.size() || m_meterRegions.size() == 1) return;
    const auto before = toJson(); m_meterRegions.removeAt(index); recalculateCounts();
    emit timingChanged(); commitSnapshot(before, QStringLiteral("Remove meter region"));
}

void DrillProject::removeTempoRegion(int index)
{
    if (index < 0 || index >= m_tempoRegions.size() || m_tempoRegions.size() == 1) return;
    const auto before = toJson(); m_tempoRegions.removeAt(index);
    emit timingChanged(); commitSnapshot(before, QStringLiteral("Remove tempo region"));
}

int DrillProject::pulsesBetween(qint64 startTick, qint64 endTick) const
{
    if (endTick <= startTick) return 0;
    double pulses = 0.0; qint64 cursor = startTick;
    while (cursor < endTick) {
        const MarchCraft::MeterRegion *chosen = nullptr;
        for (const auto &r : m_meterRegions)
            if (r.startTick <= cursor && cursor < r.endTick) chosen = &r;
        const qint64 boundary = chosen ? qMin(endTick, chosen->endTick) : endTick;
        const qint64 pulse = chosen ? chosen->pulseTicks : MarchCraft::TicksPerQuarter;
        pulses += double(boundary - cursor) / double(qMax<qint64>(1, pulse)); cursor = boundary;
    }
    return qRound(pulses);
}

qint64 DrillProject::advancePulses(qint64 startTick, int pulses) const
{
    qint64 tick = startTick;
    for (int i = 0; i < pulses; ++i) {
        qint64 pulse = MarchCraft::TicksPerQuarter;
        for (const auto &r : m_meterRegions)
            if (r.startTick <= tick && tick < r.endTick) { pulse = r.pulseTicks; break; }
        tick += pulse;
    }
    return tick;
}

void DrillProject::recalculateCounts()
{
    if (m_sets.isEmpty()) return;
    m_sets[0].stepMultiplier = 0.0;
    for (int i = 1; i < m_sets.size(); ++i)
        m_sets[i].counts = pulsesBetween(m_sets[i - 1].startTick, m_sets[i].startTick);
    emit currentSetChanged(); emit setsChanged();
}

void DrillProject::setCurrentSetCounts(int counts)
{
    if (m_currentSet <= 0 || m_currentSet >= m_sets.size()) return;
    counts = qBound(1, counts, 2048); const auto before = toJson();
    m_sets[m_currentSet].startTick = advancePulses(m_sets[m_currentSet - 1].startTick, counts);
    for (int i = m_currentSet + 1; i < m_sets.size(); ++i)
        if (m_sets[i].startTick <= m_sets[i - 1].startTick)
            m_sets[i].startTick = advancePulses(m_sets[i - 1].startTick, qMax(1, m_sets[i].counts));
    recalculateCounts(); emit timingChanged();
    commitSnapshot(before, QStringLiteral("Move musical set marker"));
}

double DrillProject::openingDurationMs() const
{
    if (m_openingBehavior != QStringLiteral("hold") || m_openingCounts <= 0) return 0.0;
    return millisecondsBetween(0, advancePulses(0, m_openingCounts));
}

void DrillProject::setOpeningBehavior(const QString &behavior, int counts)
{
    const QString next = behavior == QStringLiteral("hold") ? QStringLiteral("hold") : QStringLiteral("move");
    counts = qBound(1, counts, 256);
    if (next == m_openingBehavior && counts == m_openingCounts) return;
    const auto before = toJson(); m_openingBehavior = next; m_openingCounts = counts;
    emit setsChanged(); emit currentSetChanged(); emit timingChanged();
    commitSnapshot(before, QStringLiteral("Change opening set behavior"));
}

double DrillProject::millisecondsBetween(qint64 startTick, qint64 endTick) const
{
    if (endTick <= startTick) return 0.0;
    double ms = 0.0; qint64 cursor = startTick;
    while (cursor < endTick) {
        const MarchCraft::TempoRegion *chosen = nullptr;
        for (const auto &r : m_tempoRegions)
            if (r.startTick <= cursor && cursor < r.endTick) chosen = &r;
        const qint64 boundary = chosen ? qMin(endTick, chosen->endTick) : endTick;
        if (!chosen) ms += (boundary - cursor) * 60000.0 / (MarchCraft::TicksPerQuarter * m_bpm);
        else {
            const double span = qMax<qint64>(1, chosen->endTick - chosen->startTick);
            const auto bpmAt = [&](qint64 t) { return chosen->startBpm + (chosen->endBpm - chosen->startBpm)
                * double(t - chosen->startTick) / span; };
            const double b0 = bpmAt(cursor), b1 = bpmAt(boundary);
            if (qAbs(b1 - b0) < 0.000001)
                ms += (boundary - cursor) * 60000.0 / (MarchCraft::TicksPerQuarter * b0);
            else
                ms += 60000.0 * (boundary - cursor) / MarchCraft::TicksPerQuarter
                    * std::log(b1 / b0) / (b1 - b0);
        }
        cursor = boundary;
    }
    return ms;
}

double DrillProject::transitionDurationMs(int destinationSet) const
{
    if (destinationSet <= 0 || destinationSet >= m_sets.size()) return 0.0;
    return millisecondsBetween(m_sets[destinationSet - 1].startTick, m_sets[destinationSet].startTick);
}

double DrillProject::showDurationMs() const
{
    double total = openingDurationMs();
    for (int set = 1; set < m_sets.size(); ++set) total += transitionDurationMs(set);
    return total;
}

bool DrillProject::setShowTimeMs(double milliseconds)
{
    if (m_sets.isEmpty()) return true;
    milliseconds = qMax(0.0, milliseconds);
    double cursor = openingDurationMs();
    if (milliseconds < cursor) {
        if (m_currentSet != 0) setCurrentSetIndex(0);
        setPlaybackActive(true); setPlayhead(1.0); return false;
    }
    if (m_sets.size() < 2) return milliseconds >= cursor;
    for (int destination = 1; destination < m_sets.size(); ++destination) {
        const double duration = qMax(1.0, transitionDurationMs(destination));
        if (milliseconds < cursor + duration || destination == m_sets.size() - 1) {
            if (m_currentSet != destination) setCurrentSetIndex(destination);
            setPlaybackActive(true);
            setPlayhead(qBound(0.0, (milliseconds - cursor) / duration, 1.0));
            return milliseconds >= showDurationMs();
        }
        cursor += duration;
    }
    return true;
}

bool DrillProject::setShowAudioTimeMs(double audioMilliseconds)
{
    if (!m_music.loaded()) return setShowTimeMs(openingDurationMs() + audioMilliseconds - m_audioOffsetMs);
    const qint64 tick = musicTickForAudioMs(audioMilliseconds);
    const qint64 firstTick = m_sets.isEmpty() ? 0 : m_sets.first().startTick;
    return setShowTimeMs(openingDurationMs() + qMax(0.0, m_music.millisecondsAt(tick) - m_music.millisecondsAt(firstTick)));
}

QString DrillProject::effectiveTempoText(int destinationSet) const
{
    if (destinationSet <= 0 || destinationSet >= m_sets.size()) return QString::number(m_bpm, 'f', 0) + QStringLiteral(" BPM");
    const qint64 a = m_sets[destinationSet - 1].startTick, b = m_sets[destinationSet].startTick;
    for (const auto &r : m_tempoRegions) if (r.startTick < b && r.endTick > a)
        return qFuzzyCompare(r.startBpm, r.endBpm) ? QString::number(r.startBpm, 'f', 0) + QStringLiteral(" BPM")
            : QStringLiteral("%1–%2 BPM").arg(r.startBpm, 0, 'f', 0).arg(r.endBpm, 0, 'f', 0);
    return QString::number(m_bpm, 'f', 0) + QStringLiteral(" BPM");
}

int DrillProject::selectedCount() const
{
    return static_cast<int>(std::count_if(m_performers.cbegin(), m_performers.cend(),
                                          [](const Performer &p) { return p.selected; }));
}

int DrillProject::selectedGroupedCount() const
{
    if(m_currentSet<0)return 0; QSet<QString> grouped;
    for(const auto&group:m_sets[m_currentSet].activeVariant().groups)for(const auto&id:group.performerIds)grouped.insert(id);
    int count=0;for(const auto&p:m_performers)if(p.selected&&grouped.contains(p.id))++count;return count;
}

bool DrillProject::selectionIsExactGroup() const
{
    if(m_currentSet<0)return false; QSet<QString> selected;for(const auto&p:m_performers)if(p.selected)selected.insert(p.id);
    for(const auto&group:m_sets[m_currentSet].activeVariant().groups){if(group.performerIds.size()!=selected.size())continue;bool same=true;for(const auto&id:group.performerIds)same=same&&selected.contains(id);if(same)return true;}return false;
}

bool DrillProject::canGroupSelection() const
{
    return selectedCount() >= 2 && !selectionIsExactGroup();
}

bool DrillProject::canRemoveSelectionFromGroup() const
{
    if (m_currentSet < 0) return false;
    QSet<QString> selected;
    for (const auto &performer : m_performers)
        if (performer.selected) selected.insert(performer.id);
    for (const auto &group : m_sets[m_currentSet].activeVariant().groups) {
        bool selectedMember = false;
        bool unselectedMember = false;
        for (const auto &id : group.performerIds) {
            selectedMember |= selected.contains(id);
            unselectedMember |= !selected.contains(id);
        }
        if (selectedMember && unselectedMember) return true;
    }
    return false;
}

bool DrillProject::canUngroupSelection() const
{
    return selectedGroupedCount() > 0;
}

double DrillProject::averageDistance() const
{
    return m_performers.isEmpty() ? 0.0 : totalDistance() / m_performers.size();
}

double DrillProject::totalDistance() const
{
    ensureAnalyticsCache(); return m_cachedEnsembleTotal;
}

double DrillProject::longestDistance() const
{
    ensureAnalyticsCache(); return m_cachedLongestMove;
}

int DrillProject::warningCount() const
{
    ensureAnalyticsCache(); return m_cachedWarningCount;
}

QVariantMap DrillProject::selectionMetrics() const
{
    QVector<int> rows; for(int i=0;i<m_performers.size();++i)if(m_performers[i].selected)rows.push_back(i);
    if(rows.isEmpty())return {{QStringLiteral("count"),0}};
    double nearestTotal=0.0,minSpacing=std::numeric_limits<double>::max(),maxNearest=0.0,moveTotal=0.0,maxMove=0.0;
    int collisions=0;
    for(int row:rows){const QPointF p=placementAt(row,m_currentSet).position;double nearest=std::numeric_limits<double>::max();
        for(int other:rows)if(other!=row){const QPointF q=placementAt(other,m_currentSet).position;nearest=qMin(nearest,std::hypot(p.x()-q.x(),p.y()-q.y()));}
        if(rows.size()==1)nearest=0.0;nearestTotal+=nearest;minSpacing=qMin(minSpacing,nearest);maxNearest=qMax(maxNearest,nearest);if(nearest>0&&nearest<1.5)++collisions;
        const double move=transitionDistance(row,m_currentSet);moveTotal+=move;maxMove=qMax(maxMove,move);
    }
    const auto bounds=selectedBounds(); const int shapeIndex=selectedShapeIndex();
    QString shapeType; if(shapeIndex>=0)shapeType=m_sets[m_currentSet].activeVariant().shapes[shapeIndex].type;
    return {{QStringLiteral("count"),rows.size()},{QStringLiteral("shapeIndex"),shapeIndex},{QStringLiteral("shapeType"),shapeType},
            {QStringLiteral("averageSpacing"),nearestTotal/rows.size()},{QStringLiteral("minimumSpacing"),minSpacing==std::numeric_limits<double>::max()?0.0:minSpacing},
            {QStringLiteral("maximumNearestSpacing"),maxNearest},{QStringLiteral("averageMove"),moveTotal/rows.size()},
            {QStringLiteral("maximumMove"),maxMove},{QStringLiteral("collisionCount"),collisions},
            {QStringLiteral("width"),bounds.value(QStringLiteral("right")).toDouble()-bounds.value(QStringLiteral("left")).toDouble()},
            {QStringLiteral("height"),bounds.value(QStringLiteral("bottom")).toDouble()-bounds.value(QStringLiteral("top")).toDouble()}};
}
