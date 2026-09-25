#include "TimelinePlanner.h"

#include <QColor>
#include <QSet>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace MarchCraft {
namespace {

qint64 pulseAt(const MusicDocument &music, qint64 tick)
{
    qint64 pulse = TicksPerQuarter;
    for (const auto &meter : music.meters) {
        if (meter.tick > tick) break;
        pulse = qMax<qint64>(1, meter.pulseTicks);
    }
    return pulse;
}

qint64 meterStartAt(const MusicDocument &music, qint64 tick)
{
    qint64 start = 0;
    for (const auto &meter : music.meters) {
        if (meter.tick > tick) break;
        start = meter.tick;
    }
    return start;
}

qint64 quantizedTick(const MusicDocument &music, qint64 tick)
{
    tick = qMax<qint64>(0, tick);
    const qint64 pulse = pulseAt(music, tick);
    const qint64 origin = meterStartAt(music, tick);
    return qMax<qint64>(0, origin + qRound64(double(tick - origin) / pulse) * pulse);
}

int pulsesBetween(const MusicDocument &music, qint64 start, qint64 end)
{
    if (end <= start) return 0;
    double pulses = 0.0;
    qint64 cursor = start;
    while (cursor < end) {
        qint64 boundary = end;
        qint64 pulse = pulseAt(music, cursor);
        for (const auto &meter : music.meters) {
            if (meter.tick > cursor) {
                boundary = qMin(boundary, meter.tick);
                break;
            }
        }
        pulses += double(boundary - cursor) / qMax<qint64>(1, pulse);
        cursor = boundary;
    }
    return qRound(pulses);
}

qint64 advancePulses(const MusicDocument &music, qint64 start, int pulses)
{
    qint64 result = start;
    for (int count = 0; count < pulses; ++count)
        result += pulseAt(music, result);
    return result;
}

int measureAt(const MusicDocument &music, qint64 tick)
{
    if (music.measures.isEmpty()) return -1;
    auto it = std::upper_bound(music.measures.cbegin(), music.measures.cend(), tick,
        [](qint64 value, const MusicMeasure &measure) { return value < measure.startTick; });
    return qBound(0, int(std::distance(music.measures.cbegin(), it)) - 1,
                  music.measures.size() - 1);
}

double millisecondsAtExtended(const MusicDocument &music, qint64 tick)
{
    if (tick <= music.durationTick) return music.millisecondsAt(tick);
    const double bpm = music.tempos.isEmpty() ? 120.0 : music.tempos.last().bpm;
    return music.millisecondsAt(music.durationTick)
        + (tick - music.durationTick) * 60000.0 / (TicksPerQuarter * qMax(1.0, bpm));
}

QString candidateId(const QString &kind, qint64 tick, int existingSetIndex = -1)
{
    return QStringLiteral("%1:%2:%3").arg(kind).arg(tick).arg(existingSetIndex);
}

QString defaultColor(const QString &kind)
{
    if (kind == QStringLiteral("impact")) return QStringLiteral("#f97316");
    if (kind == QStringLiteral("marker") || kind == QStringLiteral("userMarker"))
        return QStringLiteral("#f59e0b");
    if (kind == QStringLiteral("section")) return QStringLiteral("#14b8a6");
    if (kind == QStringLiteral("tempo") || kind == QStringLiteral("meter"))
        return QStringLiteral("#ec4899");
    if (kind == QStringLiteral("alignment")) return QStringLiteral("#38bdf8");
    if (kind == QStringLiteral("review")) return QStringLiteral("#94a3b8");
    return QStringLiteral("#8b5cf6");
}

void finishCandidate(SetPlanCandidate &candidate, const MusicDocument &music)
{
    const int measureIndex = measureAt(music, candidate.tick);
    if (measureIndex >= 0) {
        const auto &measure = music.measures[measureIndex];
        candidate.measure = measure.displayNumber;
        candidate.beat = qMax(1, int((candidate.tick - measure.startTick)
            / qMax<qint64>(1, measure.pulseTicks)) + 1);
    }
    candidate.timeMs = millisecondsAtExtended(music, candidate.tick);
    if (candidate.color.isEmpty()) candidate.color = defaultColor(candidate.kind);
    candidate.confidence = qBound(0.0, candidate.confidence, 1.0);
    if (candidate.id.isEmpty())
        candidate.id = candidateId(candidate.kind, candidate.tick, candidate.existingSetIndex);
}

double priorityBoost(const SetPlanCandidate &candidate, const QString &priority)
{
    if (priority == QStringLiteral("impacts") && candidate.kind == QStringLiteral("impact")) return 0.22;
    if (priority == QStringLiteral("phrases")
        && (candidate.kind == QStringLiteral("phrase") || candidate.kind == QStringLiteral("section"))) return 0.22;
    if (priority == QStringLiteral("markers")
        && (candidate.kind == QStringLiteral("marker") || candidate.kind == QStringLiteral("userMarker"))) return 0.25;
    if (priority == QStringLiteral("regular") && candidate.kind == QStringLiteral("regular")) return 0.22;
    return 0.0;
}

double preferredLengthScore(int counts, const QVector<int> &preferred)
{
    if (preferred.isEmpty()) return 0.0;
    int distance = std::numeric_limits<int>::max();
    for (int target : preferred) distance = qMin(distance, qAbs(counts - target));
    return 0.24 * std::exp(-double(distance) / 4.0);
}

} // namespace

QJsonObject TimelineMarker::toJson() const
{
    return {{QStringLiteral("id"), id}, {QStringLiteral("tick"), tick},
            {QStringLiteral("name"), name}, {QStringLiteral("type"), type},
            {QStringLiteral("color"), color}, {QStringLiteral("notes"), notes}};
}

TimelineMarker TimelineMarker::fromJson(const QJsonObject &object)
{
    TimelineMarker marker;
    marker.id = object.value(QStringLiteral("id")).toString();
    marker.tick = qMax<qint64>(0, object.value(QStringLiteral("tick")).toVariant().toLongLong());
    marker.name = object.value(QStringLiteral("name")).toString().simplified().left(80);
    marker.type = object.value(QStringLiteral("type")).toString(QStringLiteral("user")).simplified().left(32);
    if (marker.type.isEmpty()) marker.type = QStringLiteral("user");
    const QColor color(object.value(QStringLiteral("color")).toString());
    marker.color = color.isValid() ? color.name() : QStringLiteral("#f59e0b");
    marker.notes = object.value(QStringLiteral("notes")).toString().left(1000);
    return marker;
}

QVariantMap SetPlanCandidate::toVariant() const
{
    return {{QStringLiteral("id"), id}, {QStringLiteral("tick"), tick},
            {QStringLiteral("sourceTick"), sourceTick}, {QStringLiteral("kind"), kind},
            {QStringLiteral("action"), action}, {QStringLiteral("title"), title},
            {QStringLiteral("reason"), reason}, {QStringLiteral("color"), color},
            {QStringLiteral("confidence"), confidence}, {QStringLiteral("timeMs"), timeMs},
            {QStringLiteral("measure"), measure}, {QStringLiteral("beat"), beat},
            {QStringLiteral("countsFromPrevious"), countsFromPrevious},
            {QStringLiteral("existingSetIndex"), existingSetIndex},
            {QStringLiteral("accepted"), accepted}, {QStringLiteral("manual"), manual}};
}

QVector<SetPlanCandidate> SetPlanAnalyzer::analyze(const MusicDocument &music,
                                                    const QVector<TimelineMarker> &markers,
                                                    const QVector<SetPlanSection> &sections,
                                                    const QVector<ExistingSetTiming> &existingSets,
                                                    const SetPlanOptions &options)
{
    QVector<SetPlanCandidate> raw;
    const QString density = options.density == QStringLiteral("sparse")
        || options.density == QStringLiteral("detailed") ? options.density : QStringLiteral("balanced");
    const int phraseMeasures = density == QStringLiteral("sparse") ? 8
        : density == QStringLiteral("detailed") ? 2 : 4;

    for (int index = phraseMeasures; index < music.measures.size(); index += phraseMeasures) {
        const auto &measure = music.measures[index];
        raw.push_back({{}, measure.startTick, -1, QStringLiteral("phrase"), QStringLiteral("add"),
            QStringLiteral("Phrase candidate"),
            QStringLiteral("%1-measure boundary").arg(phraseMeasures), {},
            density == QStringLiteral("detailed") ? 0.48 : 0.56});
    }

    for (int index = 1; index < music.measures.size(); ++index) {
        const auto &previous = music.measures[index - 1];
        const auto &measure = music.measures[index];
        const int change = qAbs(measure.noteCount - previous.noteCount);
        if (change >= qMax(8, previous.noteCount / 2)) {
            raw.push_back({{}, measure.startTick, -1, QStringLiteral("phrase"), QStringLiteral("add"),
                QStringLiteral("Texture change"),
                QStringLiteral("Note activity changes from %1 to %2 at the measure boundary")
                    .arg(previous.noteCount).arg(measure.noteCount), {},
                qMin(0.78, 0.48 + change / 100.0)});
        }
    }

    for (const auto &section : sections) {
        if (section.startTick <= 0) continue;
        raw.push_back({{}, section.startTick, -1, QStringLiteral("section"), QStringLiteral("add"),
            section.name.isEmpty() ? QStringLiteral("Section boundary") : section.name,
            QStringLiteral("Start of the authored %1 section").arg(section.type), {}, 0.82});
    }

    for (const auto &marker : music.markers) {
        if (marker.tick <= 0) continue;
        raw.push_back({{}, marker.tick, -1, QStringLiteral("marker"), QStringLiteral("add"),
            marker.text.isEmpty() ? QStringLiteral("Score marker") : marker.text,
            QStringLiteral("Imported %1 marker").arg(marker.kind.isEmpty()
                ? QStringLiteral("score") : marker.kind), {}, 0.84, 0.0, 0, 0, 0, -1, false, true});
    }

    for (const auto &marker : markers) {
        if (marker.tick <= 0) continue;
        const bool impact = marker.type == QStringLiteral("impact") || marker.type == QStringLiteral("hit");
        raw.push_back({{}, marker.tick, -1, QStringLiteral("userMarker"), QStringLiteral("add"),
            marker.name.isEmpty() ? QStringLiteral("User marker") : marker.name,
            marker.notes.isEmpty() ? QStringLiteral("Authored %1 marker").arg(marker.type)
                                   : marker.notes,
            marker.color, impact ? 1.0 : 0.94, 0.0, 0, 0, 0, -1, false, true});
    }

    for (int index = 1; index < music.tempos.size(); ++index) {
        raw.push_back({{}, music.tempos[index].tick, -1, QStringLiteral("tempo"), QStringLiteral("add"),
            QStringLiteral("Tempo change"),
            QStringLiteral("Tempo changes to %1 BPM").arg(music.tempos[index].bpm, 0, 'f', 0), {}, 0.68});
    }
    for (int index = 1; index < music.meters.size(); ++index) {
        const auto &meter = music.meters[index];
        raw.push_back({{}, meter.tick, -1, QStringLiteral("meter"), QStringLiteral("add"),
            QStringLiteral("Meter change"),
            QStringLiteral("Meter changes to %1/%2").arg(meter.numerator).arg(meter.denominator), {}, 0.72});
    }

    QVector<MusicAttackEvent> attacks = music.attackEvents;
    std::stable_sort(attacks.begin(), attacks.end(), [](const auto &a, const auto &b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        return a.track < b.track;
    });
    int attackIndex = 0;
    while (attackIndex < attacks.size()) {
        const double groupMs = millisecondsAtExtended(music, attacks[attackIndex].tick);
        int end = attackIndex;
        QSet<int> tracks;
        int velocityTotal = 0;
        int percussion = 0;
        while (end < attacks.size()
               && millisecondsAtExtended(music, attacks[end].tick) - groupMs <= 80.0) {
            tracks.insert(attacks[end].track);
            velocityTotal += attacks[end].velocity;
            if (attacks[end].percussion) ++percussion;
            ++end;
        }
        int prior = 0;
        for (int cursor = attackIndex - 1; cursor >= 0; --cursor) {
            const double delta = groupMs - millisecondsAtExtended(music, attacks[cursor].tick);
            if (delta > 700.0) break;
            if (delta >= 120.0) ++prior;
        }
        const int count = end - attackIndex;
        if (tracks.size() >= 2 || count >= 4 || percussion >= 2) {
            const bool afterLowActivity = prior <= qMax(1, count / 3);
            const double velocity = count > 0 ? double(velocityTotal) / (count * 127.0) : 0.0;
            double score = 0.22 + qMin(0.34, tracks.size() * 0.055)
                + qMin(0.18, count * 0.018) + velocity * 0.12
                + (percussion > 0 ? 0.07 : 0.0) + (afterLowActivity ? 0.14 : 0.0);
            QString reason = QStringLiteral("%1 active track%2 / %3 attack%4 within 80 ms")
                .arg(tracks.size()).arg(tracks.size() == 1 ? QString() : QStringLiteral("s"))
                .arg(count).arg(count == 1 ? QString() : QStringLiteral("s"));
            if (afterLowActivity) reason += QStringLiteral(" after reduced activity");
            if (percussion > 0) reason += QStringLiteral("; percussion is present");
            raw.push_back({{}, attacks[attackIndex].tick, -1, QStringLiteral("impact"),
                QStringLiteral("add"), QStringLiteral("Strong ensemble impact"), reason, {}, score});
        }
        attackIndex = end;
    }

    const qint64 durationTick = qMax(music.durationTick,
        existingSets.isEmpty() ? qint64(0) : existingSets.last().tick);
    for (qint64 tick = advancePulses(music, 0, 8); tick < durationTick;
         tick = advancePulses(music, tick, 8)) {
        raw.push_back({{}, tick, -1, QStringLiteral("regular"), QStringLiteral("add"),
            QStringLiteral("Regular count structure"), QStringLiteral("Eight-count grid boundary"), {}, 0.30});
    }

    for (auto &candidate : raw) candidate.tick = quantizedTick(music, candidate.tick);
    std::stable_sort(raw.begin(), raw.end(), [](const auto &a, const auto &b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        if (a.confidence != b.confidence) return a.confidence > b.confidence;
        return a.kind < b.kind;
    });

    QVector<SetPlanCandidate> merged;
    for (auto candidate : std::as_const(raw)) {
        if (candidate.tick <= 0) continue;
        const bool nearbyDuplicate = !merged.isEmpty()
            && pulsesBetween(music, merged.last().tick, candidate.tick) <= 1;
        if (nearbyDuplicate) {
            auto &winner = merged.last();
            const bool candidateWins = (candidate.manual && !winner.manual)
                || (candidate.manual == winner.manual && candidate.confidence > winner.confidence);
            if (candidateWins) {
                const QString secondary = winner.reason;
                const bool anyManual = winner.manual || candidate.manual;
                winner = candidate;
                if (!secondary.isEmpty() && secondary != winner.reason)
                    winner.reason += QStringLiteral("; ") + secondary;
                winner.manual = anyManual;
            } else if (!candidate.reason.isEmpty() && !winner.reason.contains(candidate.reason)) {
                winner.reason += QStringLiteral("; ") + candidate.reason;
            }
            winner.manual = winner.manual || candidate.manual;
            continue;
        }
        merged.push_back(std::move(candidate));
    }

    const int minimumCounts = density == QStringLiteral("sparse") ? 12
        : density == QStringLiteral("detailed") ? 4 : 6;
    const double threshold = density == QStringLiteral("sparse") ? 0.67
        : density == QStringLiteral("detailed") ? 0.34 : 0.49;
    QVector<qint64> anchors;
    for (const auto &set : existingSets) anchors.push_back(set.tick);
    if (anchors.isEmpty()) anchors.push_back(0);

    for (auto &candidate : merged) {
        qint64 previous = 0;
        qint64 next = std::numeric_limits<qint64>::max();
        for (qint64 anchor : std::as_const(anchors)) {
            if (anchor <= candidate.tick) previous = qMax(previous, anchor);
            else { next = qMin(next, anchor); break; }
        }
        const int fromPrevious = pulsesBetween(music, previous, candidate.tick);
        const int toNext = next == std::numeric_limits<qint64>::max()
            ? std::numeric_limits<int>::max() : pulsesBetween(music, candidate.tick, next);
        candidate.countsFromPrevious = fromPrevious;
        const double score = candidate.confidence + priorityBoost(candidate, options.priority)
            + preferredLengthScore(fromPrevious, options.preferredCounts);
        const bool clearOfExisting = fromPrevious >= minimumCounts && toNext >= minimumCounts;
        candidate.accepted = clearOfExisting && (candidate.manual || score >= threshold);
        if (candidate.accepted) {
            anchors.push_back(candidate.tick);
            std::sort(anchors.begin(), anchors.end());
        }
        finishCandidate(candidate, music);
    }

    // The regular grid is supporting evidence, not a list of every legal edit
    // point.  Keeping rejected eight-count boundaries would overwhelm the
    // review surface on long shows even though they are never drawn as ghost
    // sets or applied.  Evidence-backed candidates remain available for
    // individual review whether or not the initial scoring accepts them.
    merged.erase(std::remove_if(merged.begin(), merged.end(), [](const auto &candidate) {
        return candidate.kind == QStringLiteral("regular") && !candidate.accepted;
    }), merged.end());

    QVector<SetPlanCandidate> recommendations;
    for (int setPosition = 1; setPosition < existingSets.size(); ++setPosition) {
        const auto &set = existingSets[setPosition];
        const SetPlanCandidate *nearest = nullptr;
        int nearestCounts = std::numeric_limits<int>::max();
        for (const auto &candidate : std::as_const(merged)) {
            if (candidate.kind == QStringLiteral("regular") || candidate.confidence < 0.62) continue;
            const int distance = pulsesBetween(music, qMin(set.tick, candidate.tick),
                                               qMax(set.tick, candidate.tick));
            if (distance > 4 || distance == 0 || distance >= nearestCounts) continue;
            nearest = &candidate;
            nearestCounts = distance;
        }
        if (nearest) {
            const bool before = set.tick < nearest->tick;
            SetPlanCandidate recommendation;
            recommendation.tick = nearest->tick;
            recommendation.sourceTick = set.tick;
            recommendation.kind = QStringLiteral("alignment");
            recommendation.action = QStringLiteral("align");
            recommendation.title = QStringLiteral("Align Set %1").arg(set.number);
            recommendation.reason = QStringLiteral("Set %1 ends %2 count%3 %4 %5")
                .arg(set.number).arg(nearestCounts).arg(nearestCounts == 1 ? QString() : QStringLiteral("s"))
                .arg(before ? QStringLiteral("before") : QStringLiteral("after"))
                .arg(nearest->title.toLower());
            recommendation.confidence = nearest->confidence;
            recommendation.existingSetIndex = set.index;
            finishCandidate(recommendation, music);
            recommendations.push_back(std::move(recommendation));
        }
        if (set.counts > 0 && (set.counts < 6 || set.counts > 40)) {
            SetPlanCandidate review;
            review.tick = set.tick;
            review.sourceTick = set.tick;
            review.kind = QStringLiteral("review");
            review.action = QStringLiteral("review");
            review.title = QStringLiteral("Review Set %1 transition").arg(set.number);
            review.reason = set.counts < 6
                ? QStringLiteral("The incoming transition is unusually short at %1 counts").arg(set.counts)
                : QStringLiteral("The incoming transition is unusually long at %1 counts").arg(set.counts);
            review.confidence = 0.58;
            review.existingSetIndex = set.index;
            finishCandidate(review, music);
            recommendations.push_back(std::move(review));
        }
    }

    merged += recommendations;
    std::stable_sort(merged.begin(), merged.end(), [](const auto &a, const auto &b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        if (a.action != b.action) return a.action < b.action;
        return a.confidence > b.confidence;
    });
    return merged;
}

} // namespace MarchCraft
