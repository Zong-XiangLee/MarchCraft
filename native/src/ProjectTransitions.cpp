#include "DrillProject.h"

#include <QSizeF>
#include <QSet>
#include <QUuid>
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
using MarchCraft::AnimationState;

#include "ProjectAlgorithms.h"
#include "ProjectStorage.h"

using namespace MarchCraft::ProjectAlgorithms;
using namespace MarchCraft::ProjectStorage;

namespace {

int performerRowForId(const QVector<Performer> &performers, const QString &id)
{
    for (int row = 0; row < performers.size(); ++row)
        if (performers[row].id == id) return row;
    return -1;
}

double distanceToSegment(QPointF point, QPointF start, QPointF end)
{
    const QPointF segment = end - start;
    const double squaredLength = QPointF::dotProduct(segment, segment);
    if (squaredLength <= 1e-12) return pointDistance(point, start);
    const double t = std::clamp(QPointF::dotProduct(point - start, segment) / squaredLength, 0.0, 1.0);
    return pointDistance(point, start + segment * t);
}

QVariantList pointList(const QVector<QPointF> &points)
{
    QVariantList result;
    result.reserve(points.size());
    for (const auto &point : points) result.push_back(point);
    return result;
}

QVector<QPointF> variantPoints(const QVariant &value)
{
    QVector<QPointF> result;
    for (const auto &item : value.toList()) {
        const QPointF point = item.toPointF();
        if (item.canConvert<QPointF>()) result.push_back(point);
    }
    return result;
}

QPointF defaultCurveControl(QPointF origin, QPointF destination)
{
    const QPointF delta = destination - origin;
    const double length = std::hypot(delta.x(), delta.y());
    const QPointF perpendicular = length > 1e-6
        ? QPointF{-delta.y() / length, delta.x() / length} : QPointF{0.0, -1.0};
    return (origin + destination) / 2.0 + perpendicular * std::clamp(length * 0.22, 3.0, 8.0);
}

}

Placement DrillProject::placementAt(int performerIndex, int setIndex) const
{
    if (performerIndex < 0 || performerIndex >= m_performers.size() || m_sets.isEmpty()) return {};
    setIndex = std::clamp(setIndex, 0, static_cast<int>(m_sets.size()) - 1);
    return m_sets[setIndex].activeVariant().placements.value(m_performers[performerIndex].id);
}

QPointF DrillProject::interpolatedPosition(int performerIndex) const
{
    if (playbackSetIndex() <= 0 || m_playhead >= 1.0) return placementAt(performerIndex, playbackSetIndex()).position;
    return pathPosition(performerIndex, playbackSetIndex(), m_playhead);
}

double DrillProject::interpolatedFacing(int performerIndex) const
{
    const double destination = placementAt(performerIndex, playbackSetIndex()).facing;
    if (playbackSetIndex() <= 0 || m_playhead >= 1.0) return destination;
    const double start = placementAt(performerIndex, playbackSetIndex() - 1).facing;
    // Turn along the shortest arc so a 350-to-10 degree change passes through
    // front field instead of spinning almost a full revolution.
    const double delta = std::fmod(destination - start + 540.0, 360.0) - 180.0;
    double result = std::fmod(start + delta * m_playhead, 360.0);
    if (result < 0.0) result += 360.0;
    return result;
}

AnimationState DrillProject::animationStateAt(int performerIndex) const
{
    AnimationState state;
    const int counts = playbackSetCounts();
    if (counts > 0) {
        const auto &currentPath = transitionPath(performerIndex, playbackSetIndex());
        const double rawCount = m_playhead * counts;
        state.elapsedCounts = qBound(0.0, rawCount - currentPath.startCount(),
                                     currentPath.durationCounts());
        // Preserve left/right alternation across genuinely consecutive moves.
        // A delayed step-off or an early arrival creates a planted break and
        // starts the next marching phrase at count zero.
        if (currentPath.startCount() <= 0.001) {
            for (int set = playbackSetIndex() - 1; set > 0; --set) {
                const auto &path = transitionPath(performerIndex, set);
                if (transitionDistance(performerIndex, set) <= 1e-5
                    || path.startCount() > 0.001
                    || qAbs(path.arrivalCount() - m_sets[set].counts) > 0.001)
                    break;
                state.elapsedCounts += path.durationCounts();
            }
        }
        state.normalizedTime = std::fmod(state.elapsedCounts / 2.0, 1.0);
        if (state.normalizedTime < 0.0) state.normalizedTime += 1.0;
    }
    if (!m_playbackActive || playbackSetIndex() <= 0 || counts <= 0) return state;

    const Placement destination = placementAt(performerIndex, playbackSetIndex());
    const Placement origin = placementAt(performerIndex, playbackSetIndex() - 1);
    if (playbackSetIndex() + 1 >= m_sets.size()) {
        state.closesAtDestination = true;
    } else {
        state.closesAtDestination = transitionDistance(performerIndex, playbackSetIndex() + 1) <= 1e-5;
    }
    const double facingDelta = std::abs(std::fmod(destination.facing - origin.facing + 540.0, 360.0) - 180.0);

    constexpr double speedSampleRadius = 0.001;
    const double beforeProgress = std::max(0.0, m_playhead - speedSampleRadius);
    const double afterProgress = std::min(1.0, m_playhead + speedSampleRadius);
    const QPointF before = pathPosition(performerIndex, playbackSetIndex(), beforeProgress);
    const QPointF current = pathPosition(performerIndex, playbackSetIndex(), m_playhead);
    const QPointF after = pathPosition(performerIndex, playbackSetIndex(), afterProgress);
    const QPointF speedDelta = after - before;
    const double sampleProgress = afterProgress - beforeProgress;
    const double sampleDistance = std::hypot(current.x() - before.x(), current.y() - before.y())
                                + std::hypot(after.x() - current.x(), after.y() - current.y());

    if (sampleProgress <= 0.0 || sampleDistance <= 1e-7) {
        if (facingDelta > 0.5 && std::hypot(destination.position.x() - origin.position.x(),
                                            destination.position.y() - origin.position.y()) <= 1e-5)
            state.locomotion = QStringLiteral("direction_change");
        return state;
    }

    state.travelStepsPerCount = sampleDistance / sampleProgress / counts;
    const double headingSampleRadius = std::min(0.025, std::max(speedSampleRadius, 0.18 / counts));
    const double headingBeforeProgress = std::max(0.0, m_playhead - headingSampleRadius);
    const double headingAfterProgress = std::min(1.0, m_playhead + headingSampleRadius);
    const QPointF headingDelta = pathPosition(performerIndex, playbackSetIndex(), headingAfterProgress)
                               - pathPosition(performerIndex, playbackSetIndex(), headingBeforeProgress);
    const QPointF directionDelta = std::hypot(headingDelta.x(), headingDelta.y()) > 1e-7
            ? headingDelta : speedDelta;
    // Field Y increases away from the audience: front = -Y, Side 2 = +X.
    state.travelDirectionDegrees = std::fmod(std::atan2(directionDelta.x(), -directionDelta.y())
                                             * 180.0 / std::numbers::pi + 360.0, 360.0);
    const double facing = interpolatedFacing(performerIndex);
    const double relative = std::fmod(state.travelDirectionDegrees - facing + 540.0, 360.0) - 180.0;
    const double absoluteRelative = std::abs(relative);
    constexpr double slideHalfWidthDegrees = 7.5;
    if (absoluteRelative < 90.0 - slideHalfWidthDegrees)
        state.locomotion = QStringLiteral("march.forward");
    else if (absoluteRelative > 90.0 + slideHalfWidthDegrees)
        state.locomotion = QStringLiteral("march.backward");
    else if (relative > 0.0)
        state.locomotion = QStringLiteral("slide.right");
    else
        state.locomotion = QStringLiteral("slide.left");
    return state;
}

const AnimationState &DrillProject::cachedAnimationStateAt(int performerIndex) const
{
    if (m_animationStateCache.size() != m_performers.size()) {
        m_animationStateCache.resize(m_performers.size());
        m_animationStateCacheRevisions.fill(0, m_performers.size());
    }
    if (m_animationStateCacheRevisions.at(performerIndex) != m_animationStateRevision) {
        m_animationStateCache[performerIndex] = animationStateAt(performerIndex);
        m_animationStateCacheRevisions[performerIndex] = m_animationStateRevision;
    }
    return m_animationStateCache.at(performerIndex);
}

const MarchCraft::TransitionPath &DrillProject::transitionPath(int performerIndex, int destinationSet) const
{
    const quint64 key = (quint64(quint32(destinationSet)) << 32) | quint32(performerIndex);
    auto found = m_transitionPaths.constFind(key);
    if (found != m_transitionPaths.cend()) return found.value();
    return m_transitionPaths.insert(key, MarchCraft::TransitionPath(
        placementAt(performerIndex, destinationSet - 1).position,
        placementAt(performerIndex, destinationSet),
        destinationSet >= 0 && destinationSet < m_sets.size()
            ? qMax(1, m_sets[destinationSet].counts) : 1)).value();
}

QPointF DrillProject::pathPosition(int performerIndex, int destinationSet, double progress) const
{
    return transitionPath(performerIndex, destinationSet).position(progress);
}

double DrillProject::pathDistance(int performerIndex, int destinationSet) const
{
    if (destinationSet <= 0 || destinationSet >= m_sets.size()) return 0.0;
    return transitionPath(performerIndex, destinationSet).distance();
}

double DrillProject::transitionDistance(int performerIndex, int destinationSet) const
{
    if (destinationSet <= 0 || destinationSet >= m_sets.size()) return 0.0;
    return pathDistance(performerIndex, destinationSet);
}

Placement DrillProject::transitionEditPlacement(int performerIndex) const
{
    const int destination = m_transitionEdit.active ? m_transitionEdit.destinationSet : m_currentSet;
    Placement placement = placementAt(performerIndex, destination);
    if (!m_transitionEdit.active || performerIndex < 0 || performerIndex >= m_performers.size())
        return placement;
    const auto found = m_transitionEdit.previewPlacements.constFind(m_performers[performerIndex].id);
    return found == m_transitionEdit.previewPlacements.cend() ? placement : found.value();
}

int DrillProject::transitionEditActiveRow() const
{
    return m_transitionEdit.active
        ? performerRowForId(m_performers, m_transitionEdit.activePerformerId) : -1;
}

QVector<int> DrillProject::orderedTransitionEditRows(const QString &order, bool reversed) const
{
    QVector<int> rows;
    QSet<QString> included;
    auto appendId = [&](const QString &id) {
        if (!m_transitionEdit.originalPlacements.contains(id) || included.contains(id)) return;
        const int row = performerRowForId(m_performers, id);
        if (row >= 0) { rows.push_back(row); included.insert(id); }
    };

    if (m_transitionEdit.options.value(QStringLiteral("useStoredOrder")).toBool()) {
        for (const auto &value : m_transitionEdit.options.value(QStringLiteral("orderedIds")).toList())
            appendId(value.toString());
        for (const auto &id : m_transitionEdit.performerIds) appendId(id);
        return rows;
    }

    if (order == QStringLiteral("formation") && m_transitionEdit.destinationSet >= 0
        && m_transitionEdit.destinationSet < m_sets.size()) {
        const auto &variant = m_sets[m_transitionEdit.destinationSet].activeVariant();
        for (const auto &shape : variant.shapes)
            for (const auto &id : shape.performerIds) appendId(id);
    }
    for (const auto &id : m_transitionEdit.performerIds) appendId(id);

    if (order == QStringLiteral("roster")) {
        std::sort(rows.begin(), rows.end());
    } else if (order == QStringLiteral("leftToRight") || order == QStringLiteral("rightToLeft")) {
        const bool descending = order == QStringLiteral("rightToLeft");
        std::stable_sort(rows.begin(), rows.end(), [&](int left, int right) {
            const QPointF a = placementAt(left, qMax(0, m_transitionEdit.destinationSet - 1)).position;
            const QPointF b = placementAt(right, qMax(0, m_transitionEdit.destinationSet - 1)).position;
            if (!qFuzzyCompare(a.x() + 1.0, b.x() + 1.0)) return descending ? a.x() > b.x() : a.x() < b.x();
            return a.y() < b.y();
        });
    }
    if (reversed) std::reverse(rows.begin(), rows.end());
    return rows;
}

int minimumRelationshipMembers(const MarchCraft::TransitionRelationship &relationship)
{
    return relationship.type == QStringLiteral("stagger") ? 1 : 2;
}

int generatedRelationshipMembers(const MarchCraft::TransitionRelationship &relationship)
{
    int count = 0;
    for (const auto &id : relationship.performerIds)
        if (!relationship.manualOverrideIds.contains(id)) ++count;
    return count;
}

bool DrillProject::materializeTransitionRelationship(
    int destinationSet, MarchCraft::SetVariant &variant,
    MarchCraft::TransitionRelationship &relationship, QString *warning) const
{
    if (warning) warning->clear();
    if (destinationSet <= 0 || destinationSet >= m_sets.size()) return false;

    auto setWarning = [&](const QString &message) {
        if (!warning || message.isEmpty()) return;
        if (warning->isEmpty()) *warning = message;
        else if (!warning->contains(message)) *warning += QStringLiteral(" ") + message;
    };
    auto rowForId = [&](const QString &id) { return performerRowForId(m_performers, id); };
    relationship.performerIds.erase(
        std::remove_if(relationship.performerIds.begin(), relationship.performerIds.end(),
            [&](const QString &id) { return rowForId(id) < 0 || !variant.placements.contains(id); }),
        relationship.performerIds.end());
    relationship.manualOverrideIds.erase(
        std::remove_if(relationship.manualOverrideIds.begin(), relationship.manualOverrideIds.end(),
            [&](const QString &id) { return !relationship.performerIds.contains(id); }),
        relationship.manualOverrideIds.end());
    if (generatedRelationshipMembers(relationship) < minimumRelationshipMembers(relationship)) {
        setWarning(QStringLiteral("The relationship no longer has enough performers."));
        return false;
    }

    const double counts = qMax(1, m_sets[destinationSet].counts);
    relationship.authoredCounts = counts;
    bool valid = true;

    if (relationship.type == QStringLiteral("follow")) {
        if (!relationship.performerIds.contains(relationship.leaderId)
            || relationship.manualOverrideIds.contains(relationship.leaderId)) {
            setWarning(QStringLiteral("The Follow the Leader leader is missing."));
            return false;
        }
        const int leaderIndex = relationship.performerIds.indexOf(relationship.leaderId);
        std::rotate(relationship.performerIds.begin(),
                    relationship.performerIds.begin() + leaderIndex,
                    relationship.performerIds.end());
        const int leaderRow = rowForId(relationship.leaderId);
        const QPointF leaderOrigin = placementAt(leaderRow, destinationSet - 1).position;
        Placement leader = variant.placements.value(relationship.leaderId);
        if (leader.pathPoints.isEmpty())
            leader.pathPoints = {clampPosition(defaultCurveControl(leaderOrigin, leader.position))};
        for (auto &point : leader.pathPoints) point = clampPosition(point);
        leader.pathType = QStringLiteral("follow");
        leader.pathStartCount = 0.0;
        leader.pathDurationCounts = counts;
        variant.placements.insert(relationship.leaderId, leader);

        MarchCraft::TransitionPath leaderPath(leaderOrigin, leader, counts);
        if (leaderPath.distance() <= 0.01) {
            valid = false;
            setWarning(QStringLiteral("The Follow the Leader route has zero length."));
        }
        const int routeSamples = qBound(96, qCeil(leaderPath.distance() * 4.0), 384);
        QVector<QPointF> route;
        route.reserve(routeSamples + 1);
        for (int sample = 0; sample <= routeSamples; ++sample)
            route.push_back(leaderPath.geometryPosition(double(sample) / routeSamples));
        const double leaderSpeed = qMax(0.001, leaderPath.distance() / counts);
        double spacingDistance = 0.0;
        for (int ordinal = 1; ordinal < relationship.performerIds.size(); ++ordinal) {
            const QString id = relationship.performerIds[ordinal];
            const QString previousId = relationship.performerIds[ordinal - 1];
            const int row = rowForId(id);
            const int previousRow = rowForId(previousId);
            const QPointF origin = placementAt(row, destinationSet - 1).position;
            const QPointF previousOrigin = placementAt(previousRow, destinationSet - 1).position;
            spacingDistance += pointDistance(origin, previousOrigin);

            if (relationship.manualOverrideIds.contains(id)) continue;

            Placement follower = variant.placements.value(id);
            int entry = 0;
            int exit = routeSamples;
            double entryDistance = std::numeric_limits<double>::max();
            double exitDistance = std::numeric_limits<double>::max();
            for (int sample = 0; sample <= routeSamples; ++sample) {
                const double fromOrigin = pointDistance(origin, route[sample]);
                if (fromOrigin < entryDistance) { entryDistance = fromOrigin; entry = sample; }
                const double fromDestination = pointDistance(follower.position, route[sample]);
                if (fromDestination < exitDistance) { exitDistance = fromDestination; exit = sample; }
            }
            if (exit < entry) {
                valid = false;
                setWarning(QStringLiteral("A follower destination occurs before its route entry."));
                entry = 0;
                exit = routeSamples;
            }

            QVector<QPointF> points;
            auto appendDistinct = [&](QPointF point) {
                point = clampPosition(point);
                const QPointF previous = points.isEmpty() ? origin : points.last();
                if (pointDistance(previous, point) > 0.05
                    && pointDistance(follower.position, point) > 0.05)
                    points.push_back(point);
            };
            appendDistinct(route[entry]);
            const int routeStride = qMax(1, (exit - entry) / 32);
            for (int sample = entry + routeStride; sample < exit; sample += routeStride)
                appendDistinct(route[sample]);
            appendDistinct(route[exit]);

            const double requestedStart = spacingDistance / leaderSpeed;
            if (requestedStart >= counts - 0.249) {
                valid = false;
                setWarning(QStringLiteral("Follower spacing requires more counts than this transition provides."));
            }
            follower.pathType = QStringLiteral("follow");
            follower.pathPoints = points;
            follower.pathStartCount = qMin(counts - 0.25, requestedStart);
            follower.pathDurationCounts = qMax(0.25, counts - follower.pathStartCount);
            variant.placements.insert(id, follower);
        }
        return valid;
    }

    if (relationship.type == QStringLiteral("gate")) {
        if (!relationship.performerIds.contains(relationship.pivotId)
            || relationship.manualOverrideIds.contains(relationship.pivotId)) {
            setWarning(QStringLiteral("The gate pivot is missing."));
            return false;
        }
        const int pivotIndex = relationship.performerIds.indexOf(relationship.pivotId);
        std::rotate(relationship.performerIds.begin(),
                    relationship.performerIds.begin() + pivotIndex,
                    relationship.performerIds.end());
        const int pivotRow = rowForId(relationship.pivotId);
        const QPointF sourcePivot = placementAt(pivotRow, destinationSet - 1).position;
        const QPointF destinationPivot = variant.placements.value(relationship.pivotId).position;
        const bool clockwise = relationship.direction != QStringLiteral("counterclockwise");
        bool radiusMismatch = false;

        for (const auto &id : std::as_const(relationship.performerIds)) {
            if (relationship.manualOverrideIds.contains(id)) continue;
            const int row = rowForId(id);
            Placement placement = variant.placements.value(id);
            placement.pathPoints.clear();
            if (id == relationship.pivotId) {
                placement.pathType = QStringLiteral("pivot");
                variant.placements.insert(id, placement);
                continue;
            }

            const QPointF origin = placementAt(row, destinationSet - 1).position;
            const QPointF startVector = origin - sourcePivot;
            const QPointF endVector = placement.position - destinationPivot;
            const double startRadius = pointDistance({}, startVector);
            const double endRadius = pointDistance({}, endVector);
            if (startRadius < 0.01 || endRadius < 0.01) {
                valid = false;
                setWarning(QStringLiteral("A gate member overlaps the pivot."));
            }
            const double radiusTolerance = qMax(0.5, startRadius * 0.05);
            radiusMismatch = radiusMismatch || qAbs(startRadius - endRadius) > radiusTolerance;
            const double startAngle = std::atan2(startVector.y(), startVector.x());
            const double endAngle = std::atan2(endVector.y(), endVector.x());
            double sweep = endAngle - startAngle;
            if (clockwise) while (sweep > 0.0) sweep -= 2.0 * std::numbers::pi;
            else while (sweep < 0.0) sweep += 2.0 * std::numbers::pi;

            placement.pathType = QStringLiteral("gate");
            constexpr int arcSamples = 32;
            for (int sample = 1; sample < arcSamples; ++sample) {
                const double progress = double(sample) / arcSamples;
                const QPointF movingPivot = sourcePivot + (destinationPivot - sourcePivot) * progress;
                const double radius = startRadius + (endRadius - startRadius) * progress;
                const double angle = startAngle + sweep * progress;
                placement.pathPoints.push_back(clampPosition(movingPivot
                    + QPointF(std::cos(angle) * radius, std::sin(angle) * radius)));
            }
            variant.placements.insert(id, placement);
        }
        if (radiusMismatch)
            setWarning(QStringLiteral("Destination radii differ; MarchCraft uses a smooth spiral gate to preserve endpoints."));
        return valid;
    }

    if (relationship.type == QStringLiteral("stagger")) {
        const int groupSize = qMax(1, relationship.groupSize);
        const double interval = qMax(0.0, relationship.intervalCounts);
        const double baseDelay = qMax(0.0, relationship.baseDelayCounts);
        const int lastGroup = (relationship.performerIds.size() - 1) / groupSize;
        const double requestedLatestStart = baseDelay + lastGroup * interval;
        if (requestedLatestStart >= counts - 0.249) {
            valid = false;
            setWarning(QStringLiteral("The requested stagger starts at %1 counts, but only %2 counts are available.")
                .arg(requestedLatestStart, 0, 'f', 2).arg(counts, 0, 'f', 0));
        }
        const double latestStart = qMin(counts - 0.25, requestedLatestStart);
        const double commonDuration = qMax(0.25, counts - latestStart);
        for (int ordinal = 0; ordinal < relationship.performerIds.size(); ++ordinal) {
            const QString id = relationship.performerIds[ordinal];
            Placement placement = variant.placements.value(id);
            if (placement.pathType == QStringLiteral("delayed")) {
                placement.pathType = QStringLiteral("direct");
                placement.pathPoints.clear();
            }
            const double requestedStart = baseDelay + (ordinal / groupSize) * interval;
            placement.pathStartCount = qMin(counts - 0.25, requestedStart);
            placement.pathDurationCounts = relationship.arriveTogether
                ? qMax(0.25, counts - placement.pathStartCount) : commonDuration;
            variant.placements.insert(id, placement);
        }
        return valid;
    }

    setWarning(QStringLiteral("Unknown transition relationship type."));
    return false;
}

void DrillProject::detachFromTransitionRelationships(
    MarchCraft::SetVariant &variant, const QSet<QString> &performerIds)
{
    for (int index = variant.transitionRelationships.size() - 1; index >= 0; --index) {
        auto &relationship = variant.transitionRelationships[index];
        bool changed = false;
        for (int member = relationship.performerIds.size() - 1; member >= 0; --member) {
            if (!performerIds.contains(relationship.performerIds[member])) continue;
            relationship.manualOverrideIds.removeAll(relationship.performerIds[member]);
            relationship.performerIds.removeAt(member);
            changed = true;
        }
        if (!changed) continue;
        const bool lostAnchor = (relationship.type == QStringLiteral("follow")
                                 && performerIds.contains(relationship.leaderId))
            || (relationship.type == QStringLiteral("gate")
                && performerIds.contains(relationship.pivotId));
        if (lostAnchor
            || generatedRelationshipMembers(relationship) < minimumRelationshipMembers(relationship))
            variant.transitionRelationships.removeAt(index);
    }
}

bool DrillProject::reconcileTransitionRelationships(int destinationSet)
{
    bool changed = false;
    m_transitionReconcileWarning.clear();
    const int first = destinationSet > 0 ? destinationSet : 1;
    const int last = destinationSet > 0 ? destinationSet : m_sets.size() - 1;
    for (int set = first; set <= last && set < m_sets.size(); ++set) {
        auto reconcileVariants = [&](QVector<MarchCraft::SetVariant> &variants) {
        for (auto &variant : variants) {
            const QJsonObject before = variant.toJson();
            for (int index = variant.transitionRelationships.size() - 1; index >= 0; --index) {
                auto &relationship = variant.transitionRelationships[index];
                QString warning;
                const bool anchorPresent = relationship.type == QStringLiteral("follow")
                        ? relationship.performerIds.contains(relationship.leaderId)
                            && !relationship.manualOverrideIds.contains(relationship.leaderId)
                        : relationship.type == QStringLiteral("gate")
                            ? relationship.performerIds.contains(relationship.pivotId)
                                && !relationship.manualOverrideIds.contains(relationship.pivotId) : true;
                if (!anchorPresent
                    || generatedRelationshipMembers(relationship) < minimumRelationshipMembers(relationship)) {
                    variant.transitionRelationships.removeAt(index);
                    continue;
                }
                materializeTransitionRelationship(set, variant, relationship, &warning);
                const bool anchorStillPresent = relationship.type == QStringLiteral("follow")
                        ? relationship.performerIds.contains(relationship.leaderId)
                            && !relationship.manualOverrideIds.contains(relationship.leaderId)
                        : relationship.type == QStringLiteral("gate")
                            ? relationship.performerIds.contains(relationship.pivotId)
                                && !relationship.manualOverrideIds.contains(relationship.pivotId) : true;
                if (!anchorStillPresent
                    || generatedRelationshipMembers(relationship) < minimumRelationshipMembers(relationship)) {
                    variant.transitionRelationships.removeAt(index);
                    continue;
                }
                if (!warning.isEmpty() && set == m_currentSet
                    && variant.id == m_sets[set].activeVariantId)
                    m_transitionReconcileWarning = warning;
            }
            changed = changed || before != variant.toJson();
        }
        };
        reconcileVariants(m_sets[set].variants);
        reconcileVariants(m_sets[set].archivedVariants);
    }
    return changed;
}

bool DrillProject::beginTransitionEdit()
{
    if (m_currentSet <= 0 || m_currentSet >= m_sets.size() || selectedCount() == 0) return false;
    if (m_playbackActive) setPlaybackActive(false);
    cancelFormationPreview();
    m_transitionEdit = {};
    m_transitionEdit.active = true;
    m_transitionEdit.destinationSet = m_currentSet;

    QSet<QString> selectedIds;
    for (const auto &performer : m_performers) {
        if (performer.selected) m_transitionEdit.initialSelectedIds.insert(performer.id);
        if (performer.selected && performer.visible && !performer.locked) selectedIds.insert(performer.id);
    }
    for (const auto &performer : m_performers)
        if (selectedIds.contains(performer.id)) m_transitionEdit.performerIds.push_back(performer.id);
    if (m_transitionEdit.performerIds.isEmpty()) { m_transitionEdit = {}; return false; }
    for (const auto &id : m_transitionEdit.performerIds) {
        const int row = performerRowForId(m_performers, id);
        const Placement placement = placementAt(row, m_currentSet);
        m_transitionEdit.originalPlacements.insert(id, placement);
        m_transitionEdit.previewPlacements.insert(id, placement);
    }
    m_transitionEdit.activePerformerId = m_transitionEdit.performerIds.first();
    m_transitionEdit.options = {{QStringLiteral("order"), QStringLiteral("selection")},
        {QStringLiteral("reversed"), false}, {QStringLiteral("leaderRow"), transitionEditActiveRow()},
        {QStringLiteral("pivotRow"), transitionEditActiveRow()},
        {QStringLiteral("direction"), QStringLiteral("clockwise")},
        {QStringLiteral("intervalCounts"), 1.0}, {QStringLiteral("groupSize"), 1},
        {QStringLiteral("baseDelayCounts"), 0.0}, {QStringLiteral("arriveTogether"), true}};

    QSet<QString> selectedSet;
    for (const auto &id : std::as_const(m_transitionEdit.performerIds)) selectedSet.insert(id);
    const auto &variant = m_sets[m_currentSet].activeVariant();
    const MarchCraft::TransitionRelationship *persistedRelationship = nullptr;
    for (const auto &relationship : variant.transitionRelationships) {
        if (relationship.performerIds.size() != selectedSet.size()) continue;
        bool exact = true;
        for (const auto &id : relationship.performerIds) exact = exact && selectedSet.contains(id);
        if (exact) { persistedRelationship = &relationship; break; }
    }

    if (persistedRelationship) {
        const auto &relationship = *persistedRelationship;
        m_transitionEdit.relationshipId = relationship.id;
        m_transitionEdit.type = relationship.type;
        m_transitionEdit.performerIds = relationship.performerIds;
        for (const auto &id : relationship.manualOverrideIds) {
            m_transitionEdit.detachedPerformerIds.insert(id);
            m_transitionEdit.detachedPlacements.insert(id, m_transitionEdit.previewPlacements.value(id));
        }
        m_transitionEdit.activePerformerId = relationship.type == QStringLiteral("follow")
            ? relationship.leaderId : relationship.type == QStringLiteral("gate")
                ? relationship.pivotId : relationship.performerIds.first();
        QVariantList orderedIds;
        for (const auto &id : relationship.performerIds) orderedIds.push_back(id);
        m_transitionEdit.options = {
            {QStringLiteral("order"), relationship.order},
            {QStringLiteral("reversed"), relationship.reversed},
            {QStringLiteral("leaderRow"), performerRowForId(m_performers, relationship.leaderId)},
            {QStringLiteral("pivotRow"), performerRowForId(m_performers, relationship.pivotId)},
            {QStringLiteral("direction"), relationship.direction},
            {QStringLiteral("spacingMode"), relationship.spacingMode},
            {QStringLiteral("intervalCounts"), relationship.intervalCounts},
            {QStringLiteral("groupSize"), relationship.groupSize},
            {QStringLiteral("baseDelayCounts"), relationship.baseDelayCounts},
            {QStringLiteral("arriveTogether"), relationship.arriveTogether},
            {QStringLiteral("orderedIds"), orderedIds},
            {QStringLiteral("useStoredOrder"), true}
        };
        if (relationship.type == QStringLiteral("follow")) {
            const Placement leader = m_transitionEdit.previewPlacements.value(relationship.leaderId);
            m_transitionEdit.options.insert(QStringLiteral("leaderPoints"), pointList(leader.pathPoints));
        }
    } else {
        m_transitionEdit.activePerformerId = m_transitionEdit.performerIds.first();
        const Placement activePlacement = m_transitionEdit.previewPlacements.value(m_transitionEdit.activePerformerId);
        m_transitionEdit.type = activePlacement.pathType == QStringLiteral("delayed")
            ? QStringLiteral("stagger") : activePlacement.pathType;
        if (m_transitionEdit.type == QStringLiteral("pivot")) m_transitionEdit.type = QStringLiteral("gate");

        if (m_transitionEdit.type == QStringLiteral("direct")) {
            const bool hasExplicitTiming = std::any_of(
                m_transitionEdit.originalPlacements.cbegin(), m_transitionEdit.originalPlacements.cend(),
                [](const Placement &placement) {
                    return (placement.pathType == QStringLiteral("direct")
                            || placement.pathType == QStringLiteral("delayed"))
                        && (placement.pathStartCount >= 0.0 || placement.pathDurationCounts >= 0.0);
                });
            if (hasExplicitTiming) m_transitionEdit.type = QStringLiteral("stagger");
        }

        // Deterministically recover the useful portion of legacy generated
        // intent. New projects persist the complete relationship metadata.
        if (m_transitionEdit.type == QStringLiteral("follow")) {
            int leaderRow = transitionEditActiveRow();
            double earliest = std::numeric_limits<double>::max();
            QVariantList orderedIds;
            QVector<QString> ordered = m_transitionEdit.performerIds;
            std::stable_sort(ordered.begin(), ordered.end(), [&](const QString &left, const QString &right) {
                return m_transitionEdit.originalPlacements.value(left).pathStartCount
                    < m_transitionEdit.originalPlacements.value(right).pathStartCount;
            });
            for (const auto &id : ordered) {
                orderedIds.push_back(id);
                const double start = m_transitionEdit.originalPlacements.value(id).pathStartCount;
                if (start < earliest) { earliest = start; leaderRow = performerRowForId(m_performers, id); }
            }
            m_transitionEdit.performerIds = ordered;
            m_transitionEdit.activePerformerId = m_performers[leaderRow].id;
            m_transitionEdit.options.insert(QStringLiteral("leaderRow"), leaderRow);
            m_transitionEdit.options.insert(QStringLiteral("leaderPoints"),
                pointList(m_transitionEdit.originalPlacements.value(m_performers[leaderRow].id).pathPoints));
            m_transitionEdit.options.insert(QStringLiteral("orderedIds"), orderedIds);
            m_transitionEdit.options.insert(QStringLiteral("useStoredOrder"), true);
        } else if (m_transitionEdit.type == QStringLiteral("gate")) {
            for (const auto &id : std::as_const(m_transitionEdit.performerIds)) {
                if (m_transitionEdit.originalPlacements.value(id).pathType != QStringLiteral("pivot")) continue;
                m_transitionEdit.activePerformerId = id;
                m_transitionEdit.options.insert(QStringLiteral("pivotRow"), performerRowForId(m_performers, id));
                break;
            }
        } else if (m_transitionEdit.type == QStringLiteral("stagger")) {
            QVector<QString> ordered = m_transitionEdit.performerIds;
            std::stable_sort(ordered.begin(), ordered.end(), [&](const QString &left, const QString &right) {
                return m_transitionEdit.originalPlacements.value(left).pathStartCount
                    < m_transitionEdit.originalPlacements.value(right).pathStartCount;
            });
            QVariantList orderedIds;
            for (const auto &id : ordered) orderedIds.push_back(id);
            m_transitionEdit.performerIds = ordered;
            m_transitionEdit.activePerformerId = ordered.first();
            const double base = qMax(0.0, m_transitionEdit.originalPlacements.value(ordered.first()).pathStartCount);
            int groupSize = 1;
            while (groupSize < ordered.size()
                   && qFuzzyCompare(m_transitionEdit.originalPlacements.value(ordered[groupSize]).pathStartCount + 1.0,
                                    base + 1.0))
                ++groupSize;
            const double interval = groupSize < ordered.size()
                ? qMax(0.0, m_transitionEdit.originalPlacements.value(ordered[groupSize]).pathStartCount - base)
                : 0.0;
            const double firstArrival = m_transitionEdit.originalPlacements.value(ordered.first()).pathStartCount
                + m_transitionEdit.originalPlacements.value(ordered.first()).pathDurationCounts;
            bool together = true;
            for (const auto &id : ordered) {
                const Placement placement = m_transitionEdit.originalPlacements.value(id);
                together = together && qAbs(placement.pathStartCount + placement.pathDurationCounts
                                             - firstArrival) < 0.001;
            }
            m_transitionEdit.options.insert(QStringLiteral("baseDelayCounts"), base);
            m_transitionEdit.options.insert(QStringLiteral("groupSize"), groupSize);
            m_transitionEdit.options.insert(QStringLiteral("intervalCounts"), interval);
            m_transitionEdit.options.insert(QStringLiteral("arriveTogether"), together);
            m_transitionEdit.options.insert(QStringLiteral("orderedIds"), orderedIds);
            m_transitionEdit.options.insert(QStringLiteral("useStoredOrder"), true);
        }
    }
    validateTransitionEdit();
    emit transitionEditChanged();
    setStatus(QStringLiteral("Transition path preview ready — Apply or Cancel"));
    return true;
}

void DrillProject::rebuildTransitionEditPreview()
{
    if (!m_transitionEdit.active || m_transitionEdit.destinationSet <= 0
        || m_transitionEdit.destinationSet >= m_sets.size()) return;
    m_transitionEdit.previewPlacements = m_transitionEdit.originalPlacements;
    for (auto it = m_transitionEdit.detachedPlacements.cbegin();
         it != m_transitionEdit.detachedPlacements.cend(); ++it)
        m_transitionEdit.previewPlacements.insert(it.key(), it.value());
    m_transitionEdit.valid = true;
    m_transitionEdit.warning.clear();
    const QString type = m_transitionEdit.type;
    const bool reversed = m_transitionEdit.options.value(QStringLiteral("reversed"), false).toBool();
    const QString order = m_transitionEdit.options.value(QStringLiteral("order"), QStringLiteral("formation")).toString();
    QVector<int> rows = orderedTransitionEditRows(order, reversed);
    if (rows.isEmpty()) return;

    if (type == QStringLiteral("direct")) {
        for (int row : rows) {
            auto &placement = m_transitionEdit.previewPlacements[m_performers[row].id];
            placement.pathType = QStringLiteral("direct"); placement.pathPoints.clear();
        }
        return;
    }
    if (type == QStringLiteral("delayed")) {
        for (int row : rows) {
            auto &placement = m_transitionEdit.previewPlacements[m_performers[row].id];
            placement.pathType = QStringLiteral("delayed"); placement.pathPoints.clear();
            placement.pathStartCount = -1.0; placement.pathDurationCounts = -1.0;
        }
        return;
    }
    if (type == QStringLiteral("curved")) {
        for (int row : rows) {
            const QString id = m_performers[row].id;
            auto &placement = m_transitionEdit.previewPlacements[id];
            placement.pathType = QStringLiteral("curved");
            if (placement.pathPoints.isEmpty())
                placement.pathPoints = {clampPosition(defaultCurveControl(
                    placementAt(row, m_transitionEdit.destinationSet - 1).position, placement.position))};
        }
        return;
    }

    if (type == QStringLiteral("follow") || type == QStringLiteral("gate")
        || type == QStringLiteral("pivot") || type == QStringLiteral("stagger")) {
        MarchCraft::TransitionRelationship relationship;
        relationship.id = m_transitionEdit.relationshipId.isEmpty()
            ? relationship.id : m_transitionEdit.relationshipId;
        relationship.type = type == QStringLiteral("pivot") ? QStringLiteral("gate") : type;
        for (int row : rows) {
            const QString id = m_performers[row].id;
            relationship.performerIds.push_back(id);
            if (m_transitionEdit.detachedPerformerIds.contains(id))
                relationship.manualOverrideIds.push_back(id);
        }
        const int leaderRow = m_transitionEdit.options.value(QStringLiteral("leaderRow"),
                                                               rows.value(0, -1)).toInt();
        const int pivotRow = m_transitionEdit.options.value(QStringLiteral("pivotRow"),
                                                             rows.value(0, -1)).toInt();
        if (leaderRow >= 0 && leaderRow < m_performers.size())
            relationship.leaderId = m_performers[leaderRow].id;
        if (pivotRow >= 0 && pivotRow < m_performers.size())
            relationship.pivotId = m_performers[pivotRow].id;
        relationship.order = m_transitionEdit.options.value(
            QStringLiteral("order"), QStringLiteral("selection")).toString();
        relationship.reversed = m_transitionEdit.options.value(QStringLiteral("reversed")).toBool();
        relationship.direction = m_transitionEdit.options.value(
            QStringLiteral("direction"), QStringLiteral("clockwise")).toString();
        relationship.spacingMode = m_transitionEdit.options.value(
            QStringLiteral("spacingMode"), QStringLiteral("sourceDistance")).toString();
        relationship.intervalCounts = qMax(0.0, m_transitionEdit.options.value(
            QStringLiteral("intervalCounts"), 1.0).toDouble());
        relationship.groupSize = qMax(1, m_transitionEdit.options.value(
            QStringLiteral("groupSize"), 1).toInt());
        relationship.baseDelayCounts = qMax(0.0, m_transitionEdit.options.value(
            QStringLiteral("baseDelayCounts"), 0.0).toDouble());
        relationship.arriveTogether = m_transitionEdit.options.value(
            QStringLiteral("arriveTogether"), true).toBool();

        if (relationship.type == QStringLiteral("follow") && !relationship.leaderId.isEmpty()) {
            auto leader = m_transitionEdit.previewPlacements.value(relationship.leaderId);
            const auto leaderPoints = variantPoints(
                m_transitionEdit.options.value(QStringLiteral("leaderPoints")));
            if (m_transitionEdit.options.contains(QStringLiteral("leaderPoints")))
                leader.pathPoints = leaderPoints;
            m_transitionEdit.previewPlacements.insert(relationship.leaderId, leader);
        }

        MarchCraft::SetVariant scratch = m_sets[m_transitionEdit.destinationSet].activeVariant();
        for (auto it = m_transitionEdit.previewPlacements.cbegin();
             it != m_transitionEdit.previewPlacements.cend(); ++it)
            scratch.placements.insert(it.key(), it.value());
        QString warning;
        if (generatedRelationshipMembers(relationship) < minimumRelationshipMembers(relationship)) {
            // A manual follower/member override can dissolve the remaining
            // generated relationship.  Keep every edited route as an
            // independent path instead of blocking Apply or silently
            // reattaching it.
            for (const auto &id : std::as_const(relationship.performerIds)) {
                auto placement = m_transitionEdit.previewPlacements.value(id);
                if (relationship.type == QStringLiteral("gate") && id == relationship.pivotId) {
                    placement.pathType = QStringLiteral("direct");
                    placement.pathPoints.clear();
                } else {
                    placement.pathType = placement.pathPoints.isEmpty()
                        ? QStringLiteral("direct") : QStringLiteral("curved");
                }
                m_transitionEdit.previewPlacements.insert(id, placement);
            }
            m_transitionEdit.valid = true;
            warning = QStringLiteral("Manual overrides leave too few members for the generated relationship; remaining routes will be independent.");
        } else {
            m_transitionEdit.valid = materializeTransitionRelationship(
                m_transitionEdit.destinationSet, scratch, relationship, &warning);
        }
        m_transitionEdit.warning = warning;
        if (generatedRelationshipMembers(relationship) >= minimumRelationshipMembers(relationship))
            for (const auto &id : std::as_const(relationship.performerIds))
                if (!relationship.manualOverrideIds.contains(id))
                    m_transitionEdit.previewPlacements.insert(id, scratch.placements.value(id));

        m_transitionEdit.performerIds = relationship.performerIds;
        QVariantList orderedIds;
        for (const auto &id : relationship.performerIds) orderedIds.push_back(id);
        m_transitionEdit.options.insert(QStringLiteral("orderedIds"), orderedIds);
        m_transitionEdit.options.insert(QStringLiteral("useStoredOrder"), true);
        if (relationship.type == QStringLiteral("follow") && !relationship.leaderId.isEmpty()) {
            m_transitionEdit.activePerformerId = relationship.leaderId;
            const Placement leader = m_transitionEdit.previewPlacements.value(relationship.leaderId);
            m_transitionEdit.options.insert(QStringLiteral("leaderPoints"), pointList(leader.pathPoints));
        } else if (relationship.type == QStringLiteral("gate") && !relationship.pivotId.isEmpty()) {
            m_transitionEdit.activePerformerId = relationship.pivotId;
        }
        return;
    }
}

void DrillProject::setTransitionEditType(const QString &type, const QVariantMap &options)
{
    if (!m_transitionEdit.active && !beginTransitionEdit()) return;
    static const QSet<QString> supported{QStringLiteral("direct"), QStringLiteral("curved"),
        QStringLiteral("follow"), QStringLiteral("gate"), QStringLiteral("pivot"),
        QStringLiteral("stagger"), QStringLiteral("delayed")};
    const QString nextType = supported.contains(type) ? type : QStringLiteral("direct");
    const bool typeChanged = m_transitionEdit.type != nextType;
    m_transitionEdit.type = nextType;
    if (typeChanged) {
        m_transitionEdit.detachedPerformerIds.clear();
        m_transitionEdit.detachedPlacements.clear();
        m_transitionEdit.options.remove(QStringLiteral("orderedIds"));
        m_transitionEdit.options.insert(QStringLiteral("useStoredOrder"), false);
        if (nextType != QStringLiteral("follow"))
            m_transitionEdit.options.remove(QStringLiteral("leaderPoints"));
    }
    for (auto it = options.cbegin(); it != options.cend(); ++it) m_transitionEdit.options.insert(it.key(), it.value());
    if (options.contains(QStringLiteral("order")) || options.contains(QStringLiteral("reversed"))) {
        m_transitionEdit.options.remove(QStringLiteral("orderedIds"));
        m_transitionEdit.options.insert(QStringLiteral("useStoredOrder"), false);
    }
    rebuildTransitionEditPreview();
    emit transitionEditChanged();
}

void DrillProject::updateTransitionEditOptions(const QVariantMap &options)
{
    if (!m_transitionEdit.active) return;
    const bool orderChanged = options.contains(QStringLiteral("order"));
    const bool reversedChanged = options.contains(QStringLiteral("reversed"));
    for (auto it = options.cbegin(); it != options.cend(); ++it) m_transitionEdit.options.insert(it.key(), it.value());
    if (orderChanged || reversedChanged) {
        m_transitionEdit.options.remove(QStringLiteral("orderedIds"));
        m_transitionEdit.options.insert(QStringLiteral("useStoredOrder"), false);
    }
    rebuildTransitionEditPreview();
    emit transitionEditChanged();
}

void DrillProject::validateTransitionEdit()
{
    if (!m_transitionEdit.active) return;
    rebuildTransitionEditPreview();
}

bool DrillProject::applyTransitionEdit()
{
    if (!m_transitionEdit.active || m_transitionEdit.destinationSet != m_currentSet) return false;
    if (!m_transitionEdit.valid) {
        setStatus(m_transitionEdit.warning.isEmpty()
            ? QStringLiteral("This transition configuration is invalid")
            : m_transitionEdit.warning);
        return false;
    }
    const auto before = toJson();
    auto &variant = m_sets[m_currentSet].activeVariant();
    auto &placements = variant.placements;
    QSet<QString> editedIds;
    for (auto it = m_transitionEdit.originalPlacements.cbegin();
         it != m_transitionEdit.originalPlacements.cend(); ++it)
        editedIds.insert(it.key());
    detachFromTransitionRelationships(variant, editedIds);
    for (auto it = m_transitionEdit.previewPlacements.cbegin(); it != m_transitionEdit.previewPlacements.cend(); ++it) {
        auto &destination = placements[it.key()];
        destination.pathType = it.value().pathType;
        destination.pathPoints = it.value().pathPoints;
        destination.pathStartCount = it.value().pathStartCount;
        destination.pathDurationCounts = it.value().pathDurationCounts;
    }

    const QString relationshipType = m_transitionEdit.type == QStringLiteral("pivot")
        ? QStringLiteral("gate") : m_transitionEdit.type;
    if (relationshipType == QStringLiteral("follow") || relationshipType == QStringLiteral("gate")
        || relationshipType == QStringLiteral("stagger")) {
        MarchCraft::TransitionRelationship relationship;
        if (!m_transitionEdit.relationshipId.isEmpty()) relationship.id = m_transitionEdit.relationshipId;
        relationship.type = relationshipType;
        for (const auto &id : std::as_const(m_transitionEdit.performerIds))
            relationship.performerIds.push_back(id);
        for (const auto &id : std::as_const(m_transitionEdit.detachedPerformerIds))
            if (relationship.performerIds.contains(id)) relationship.manualOverrideIds.push_back(id);
        const int leaderRow = m_transitionEdit.options.value(QStringLiteral("leaderRow"), -1).toInt();
        const int pivotRow = m_transitionEdit.options.value(QStringLiteral("pivotRow"), -1).toInt();
        if (leaderRow >= 0 && leaderRow < m_performers.size())
            relationship.leaderId = m_performers[leaderRow].id;
        if (pivotRow >= 0 && pivotRow < m_performers.size())
            relationship.pivotId = m_performers[pivotRow].id;
        relationship.order = m_transitionEdit.options.value(
            QStringLiteral("order"), QStringLiteral("selection")).toString();
        relationship.reversed = m_transitionEdit.options.value(QStringLiteral("reversed")).toBool();
        relationship.direction = m_transitionEdit.options.value(
            QStringLiteral("direction"), QStringLiteral("clockwise")).toString();
        relationship.spacingMode = m_transitionEdit.options.value(
            QStringLiteral("spacingMode"), QStringLiteral("sourceDistance")).toString();
        relationship.intervalCounts = qMax(0.0, m_transitionEdit.options.value(
            QStringLiteral("intervalCounts"), 1.0).toDouble());
        relationship.groupSize = qMax(1, m_transitionEdit.options.value(
            QStringLiteral("groupSize"), 1).toInt());
        relationship.baseDelayCounts = qMax(0.0, m_transitionEdit.options.value(
            QStringLiteral("baseDelayCounts"), 0.0).toDouble());
        relationship.arriveTogether = m_transitionEdit.options.value(
            QStringLiteral("arriveTogether"), true).toBool();
        relationship.authoredCounts = qMax(1, m_sets[m_currentSet].counts);
        if (generatedRelationshipMembers(relationship) >= minimumRelationshipMembers(relationship))
            variant.transitionRelationships.push_back(relationship);
    }
    reconcileTransitionRelationships(m_currentSet);
    m_transitionEdit = {};
    emit transitionEditChanged(); emitAllDataChanged();
    commitSnapshot(before, QStringLiteral("Edit transition paths"));
    return true;
}

void DrillProject::cancelTransitionEdit()
{
    if (!m_transitionEdit.active) return;
    const QSet<QString> selectedIds = m_transitionEdit.initialSelectedIds;
    m_transitionEdit = {};
    for (auto &performer : m_performers)
        performer.selected = selectedIds.contains(performer.id);
    emit selectionChanged();
    emit transitionEditChanged();
    setStatus(QStringLiteral("Transition edit canceled"));
}

bool DrillProject::selectTransitionEditPerformer(int row, bool additive)
{
    if (!m_transitionEdit.active || row < 0 || row >= m_performers.size()
        || !m_performers[row].visible || m_performers[row].locked) return false;
    const QString id = m_performers[row].id;
    if (!additive && m_transitionEdit.originalPlacements.contains(id)) {
        m_transitionEdit.activePerformerId = id;
        emit transitionEditChanged();
        return true;
    }
    if (!additive) {
        setStatus(QStringLiteral("Hold Shift or Ctrl to add this performer to the transition operation"));
        return false;
    } else if (m_transitionEdit.originalPlacements.contains(id)) {
        m_transitionEdit.performerIds.removeAll(id);
        m_transitionEdit.originalPlacements.remove(id); m_transitionEdit.previewPlacements.remove(id);
        m_transitionEdit.detachedPlacements.remove(id); m_transitionEdit.detachedPerformerIds.remove(id);
        m_performers[row].selected = false;
    } else {
        m_transitionEdit.performerIds.push_back(id);
        const Placement placement = placementAt(row, m_transitionEdit.destinationSet);
        m_transitionEdit.originalPlacements.insert(id, placement);
        m_transitionEdit.previewPlacements.insert(id, placement);
        m_performers[row].selected = true;
    }
    if (m_transitionEdit.performerIds.isEmpty()) {
        cancelTransitionEdit(); return false;
    }
    m_transitionEdit.activePerformerId = id;
    if (!m_transitionEdit.originalPlacements.contains(id))
        m_transitionEdit.activePerformerId = m_transitionEdit.performerIds.first();
    const int leaderRow = m_transitionEdit.options.value(QStringLiteral("leaderRow"), -1).toInt();
    if (leaderRow < 0 || leaderRow >= m_performers.size()
        || !m_transitionEdit.originalPlacements.contains(m_performers[leaderRow].id))
        m_transitionEdit.options.insert(QStringLiteral("leaderRow"),
                                         performerRowForId(m_performers, m_transitionEdit.performerIds.first()));
    const int pivotRow = m_transitionEdit.options.value(QStringLiteral("pivotRow"), -1).toInt();
    if (pivotRow < 0 || pivotRow >= m_performers.size()
        || !m_transitionEdit.originalPlacements.contains(m_performers[pivotRow].id))
        m_transitionEdit.options.insert(QStringLiteral("pivotRow"),
                                         performerRowForId(m_performers, m_transitionEdit.performerIds.first()));
    // Membership changes are edits to the current operation, not requests to
    // adopt the newly active performer's previously persisted path type.
    // Rebuild so group routes, pivots, and stagger timing use the new set.
    rebuildTransitionEditPreview();
    emit selectionChanged(); emit transitionEditChanged();
    return true;
}

QPointF DrillProject::transitionEditPosition(int row, double progress) const
{
    if (!m_transitionEdit.active || row < 0 || row >= m_performers.size()
        || m_transitionEdit.destinationSet <= 0
        || m_transitionEdit.destinationSet >= m_sets.size()) return {};
    const Placement placement = transitionEditPlacement(row);
    const MarchCraft::TransitionPath path(
        placementAt(row, m_transitionEdit.destinationSet - 1).position,
        placement, qMax(1, m_sets[m_transitionEdit.destinationSet].counts));
    return path.position(qBound(0.0, progress, 1.0));
}

double DrillProject::transitionEditFacing(int row, double progress) const
{
    if (!m_transitionEdit.active || row < 0 || row >= m_performers.size()
        || m_transitionEdit.destinationSet <= 0
        || m_transitionEdit.destinationSet >= m_sets.size()) return 0.0;
    const double source = placementAt(row, m_transitionEdit.destinationSet - 1).facing;
    const double destination = transitionEditPlacement(row).facing;
    const double delta = std::fmod(destination - source + 540.0, 360.0) - 180.0;
    double result = std::fmod(source + delta * qBound(0.0, progress, 1.0), 360.0);
    if (result < 0.0) result += 360.0;
    return result;
}

bool DrillProject::selectTransitionPathAt(double x, double y, double toleranceSteps, bool additive)
{
    if (!m_transitionEdit.active || m_transitionEdit.destinationSet <= 0) return false;
    const QPointF target{x, y};
    int closestRow = -1; double closestDistance = qMax(0.1, toleranceSteps);
    const double counts = qMax(1, m_sets[m_transitionEdit.destinationSet].counts);
    for (int row = 0; row < m_performers.size(); ++row) {
        if (!m_performers[row].visible || m_performers[row].locked) continue;
        const Placement placement = transitionEditPlacement(row);
        MarchCraft::TransitionPath path(placementAt(row, m_transitionEdit.destinationSet - 1).position,
                                       placement, counts);
        QPointF previous = path.position(0.0);
        for (int sample = 1; sample <= 36; ++sample) {
            const QPointF next = path.position(double(sample) / 36.0);
            const double distance = distanceToSegment(target, previous, next);
            if (distance <= closestDistance) { closestDistance = distance; closestRow = row; }
            previous = next;
        }
    }
    return closestRow >= 0 && selectTransitionEditPerformer(closestRow, additive);
}

QVariantList DrillProject::transitionEditControlPoints() const
{
    QVariantList result;
    const int row = transitionEditActiveRow();
    if (row < 0) return result;
    const Placement placement = transitionEditPlacement(row);
    for (int index = 0; index < placement.pathPoints.size(); ++index) {
        const auto &point = placement.pathPoints[index];
        result.push_back(QVariantMap{{QStringLiteral("row"), row}, {QStringLiteral("index"), index},
            {QStringLiteral("x"), point.x()}, {QStringLiteral("y"), point.y()}});
    }
    return result;
}

QVariantMap DrillProject::transitionEditPathInfo(int row) const
{
    const int destination = m_transitionEdit.active ? m_transitionEdit.destinationSet : m_currentSet;
    if (row < 0 || row >= m_performers.size() || destination <= 0 || destination >= m_sets.size()) return {};
    const Placement placement = transitionEditPlacement(row);
    const double counts = qMax(1, m_sets[destination].counts);
    const MarchCraft::TransitionPath path(placementAt(row, destination - 1).position, placement, counts);
    return {{QStringLiteral("row"), row}, {QStringLiteral("label"), m_performers[row].label},
        {QStringLiteral("pathType"), placement.pathType}, {QStringLiteral("distance"), path.distance()},
        {QStringLiteral("startCount"), path.startCount()}, {QStringLiteral("durationCounts"), path.durationCounts()},
        {QStringLiteral("arrivalCount"), path.arrivalCount()},
        {QStringLiteral("stepsPerCount"), path.distance() / qMax(0.001, path.durationCounts())},
        {QStringLiteral("active"), row == transitionEditActiveRow()},
        {QStringLiteral("detached"), m_transitionEdit.detachedPerformerIds.contains(m_performers[row].id)},
        {QStringLiteral("selected"), m_transitionEdit.originalPlacements.contains(m_performers[row].id)}};
}

QVariantList DrillProject::transitionEditPerformers() const
{
    QVariantList result;
    if (!m_transitionEdit.active) return result;
    for (const auto &id : m_transitionEdit.performerIds) {
        const int row = performerRowForId(m_performers, id);
        if (row >= 0) result.push_back(transitionEditPathInfo(row));
    }
    return result;
}

QVariantMap DrillProject::transitionEditMetrics() const
{
    QVariantMap result;
    if (!m_transitionEdit.active || m_transitionEdit.destinationSet <= 0) return result;
    const int counts = qMax(1, m_sets[m_transitionEdit.destinationSet].counts);
    QVector<MarchCraft::TransitionPath> paths;
    paths.reserve(m_performers.size());
    QSet<int> editedRows;
    double totalDistance = 0.0, longest = 0.0, maximumSteps = 0.0;
    int warnings = 0;
    for (int row = 0; row < m_performers.size(); ++row) {
        const Placement placement = transitionEditPlacement(row);
        paths.push_back(MarchCraft::TransitionPath(
            placementAt(row, m_transitionEdit.destinationSet - 1).position, placement, counts));
        if (m_transitionEdit.originalPlacements.contains(m_performers[row].id)) {
            editedRows.insert(row);
            totalDistance += paths.last().distance(); longest = qMax(longest, paths.last().distance());
            const double steps = paths.last().distance() / qMax(0.001, paths.last().durationCounts());
            maximumSteps = qMax(maximumSteps, steps);
            if (steps > m_capability.maximumStepsPerCount) ++warnings;
        }
    }
    QSet<quint64> collisionPairs;
    const int distanceSamples = qCeil(longest / qMax(0.05, m_capability.collisionClearance * 0.125));
    const int samples = qBound(16, qMax(counts * 8, distanceSamples), 256);
    QVector<QPointF> positions(m_performers.size());
    const double cellSize = qMax(0.25, m_capability.collisionClearance);
    auto cellKey = [](int x, int y) { return (quint64(quint32(x)) << 32) | quint32(y); };
    for (int sample = 0; sample <= samples; ++sample) {
        const double progress = double(sample) / samples;
        for (int row = 0; row < paths.size(); ++row) positions[row] = paths[row].position(progress);
        QHash<quint64, QVector<int>> grid;
        for (int row = 0; row < positions.size(); ++row) {
            const int cellX = qFloor(positions[row].x() / cellSize);
            const int cellY = qFloor(positions[row].y() / cellSize);
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
                for (int other : grid.value(cellKey(cellX + dx, cellY + dy))) {
                    if (!editedRows.contains(row) && !editedRows.contains(other)) continue;
                    if (pointDistance(positions[row], positions[other]) < m_capability.collisionClearance) {
                        const int left = qMin(row, other), right = qMax(row, other);
                        collisionPairs.insert((quint64(quint32(left)) << 32) | quint32(right));
                    }
                }
            grid[cellKey(cellX, cellY)].push_back(row);
        }
    }
    const int activeRow = transitionEditActiveRow();
    const auto active = activeRow >= 0 && activeRow < paths.size() ? paths[activeRow] : MarchCraft::TransitionPath{};
    result = {{QStringLiteral("pathCount"), editedRows.size()},
        {QStringLiteral("transitionCounts"), counts},
        {QStringLiteral("averageDistance"), editedRows.isEmpty() ? 0.0 : totalDistance / editedRows.size()},
        {QStringLiteral("longestDistance"), longest}, {QStringLiteral("maximumStepsPerCount"), maximumSteps},
        {QStringLiteral("warningCount"), warnings}, {QStringLiteral("collisionCount"), collisionPairs.size()},
        {QStringLiteral("activeDistance"), active.distance()}, {QStringLiteral("activeStartCount"), active.startCount()},
        {QStringLiteral("activeArrivalCount"), active.arrivalCount()}};
    return result;
}

void DrillProject::preserveTransitionEditOverride(int performerRow, bool detachRelationship)
{
    if (!m_transitionEdit.active || performerRow < 0 || performerRow >= m_performers.size()) return;
    const QString id = m_performers[performerRow].id;
    auto found = m_transitionEdit.previewPlacements.find(id);
    if (found == m_transitionEdit.previewPlacements.end()) return;
    if (detachRelationship) {
        found->pathType = found->pathPoints.isEmpty()
            ? QStringLiteral("direct") : QStringLiteral("curved");
        m_transitionEdit.detachedPerformerIds.insert(id);
    }
    m_transitionEdit.detachedPlacements.insert(id, found.value());
    m_transitionEdit.activePerformerId = id;
}

void DrillProject::moveTransitionControlPoint(int row, int pointIndex, double x, double y)
{
    if (!m_transitionEdit.active || row < 0 || row >= m_performers.size()) return;
    auto found = m_transitionEdit.previewPlacements.find(m_performers[row].id);
    if (found == m_transitionEdit.previewPlacements.end()
        || pointIndex < 0 || pointIndex >= found->pathPoints.size()) return;
    found->pathPoints[pointIndex] = clampPosition({x, y});
    const int leaderRow = m_transitionEdit.options.value(QStringLiteral("leaderRow"), -1).toInt();
    const int pivotRow = m_transitionEdit.options.value(QStringLiteral("pivotRow"), -1).toInt();
    if (m_transitionEdit.type == QStringLiteral("follow") && row == leaderRow) {
        m_transitionEdit.options.insert(QStringLiteral("leaderPoints"), pointList(found->pathPoints));
        rebuildTransitionEditPreview();
    } else if ((m_transitionEdit.type == QStringLiteral("gate")
                || m_transitionEdit.type == QStringLiteral("pivot")) && row == pivotRow) {
        setStatus(QStringLiteral("The gate pivot follows its source and destination; edit a gate member to make an independent override."));
        return;
    } else if (m_transitionEdit.type == QStringLiteral("follow")
               || m_transitionEdit.type == QStringLiteral("gate")
               || m_transitionEdit.type == QStringLiteral("pivot")) {
        preserveTransitionEditOverride(row, true);
    } else if (m_transitionEdit.type == QStringLiteral("stagger")) {
        preserveTransitionEditOverride(row, false);
    }
    emit transitionEditChanged();
}

void DrillProject::insertTransitionControlPoint(int row, int pointIndex, double x, double y)
{
    if (!m_transitionEdit.active) return;
    if (row < 0) row = transitionEditActiveRow();
    if (row < 0 || row >= m_performers.size()) return;
    auto found = m_transitionEdit.previewPlacements.find(m_performers[row].id);
    if (found == m_transitionEdit.previewPlacements.end()) return;
    const int pivotRow = m_transitionEdit.options.value(QStringLiteral("pivotRow"), -1).toInt();
    if ((m_transitionEdit.type == QStringLiteral("gate")
         || m_transitionEdit.type == QStringLiteral("pivot")) && row == pivotRow) {
        setStatus(QStringLiteral("The gate pivot follows its source and destination; edit a gate member to make an independent override."));
        return;
    }
    if (found->pathType == QStringLiteral("direct") || found->pathType == QStringLiteral("delayed")) {
        found->pathType = QStringLiteral("curved");
        if (m_transitionEdit.type == QStringLiteral("direct")
            || m_transitionEdit.type == QStringLiteral("delayed"))
            m_transitionEdit.type = QStringLiteral("curved");
    }
    pointIndex = qBound(0, pointIndex, found->pathPoints.size());
    found->pathPoints.insert(pointIndex, clampPosition({x, y}));
    const int leaderRow = m_transitionEdit.options.value(QStringLiteral("leaderRow"), -1).toInt();
    if (m_transitionEdit.type == QStringLiteral("follow") && row == leaderRow) {
        m_transitionEdit.options.insert(QStringLiteral("leaderPoints"), pointList(found->pathPoints));
        rebuildTransitionEditPreview();
    } else if (m_transitionEdit.type == QStringLiteral("follow")
               || m_transitionEdit.type == QStringLiteral("gate")
               || m_transitionEdit.type == QStringLiteral("pivot")) {
        preserveTransitionEditOverride(row, true);
    } else if (m_transitionEdit.type == QStringLiteral("stagger")) {
        preserveTransitionEditOverride(row, false);
    }
    emit transitionEditChanged();
}

void DrillProject::addTransitionControlPoint(int row)
{
    if (!m_transitionEdit.active) return;
    if (row < 0) row = transitionEditActiveRow();
    if (row < 0 || row >= m_performers.size()) return;
    const Placement placement = transitionEditPlacement(row);
    const QPointF origin = placementAt(row, m_transitionEdit.destinationSet - 1).position;
    const QPointF point = placement.pathPoints.isEmpty()
        ? defaultCurveControl(origin, placement.position)
        : (placement.pathPoints.last() + placement.position) / 2.0;
    insertTransitionControlPoint(row, placement.pathPoints.size(), point.x(), point.y());
}

void DrillProject::removeTransitionControlPoint(int row, int pointIndex)
{
    if (!m_transitionEdit.active) return;
    if (row < 0) row = transitionEditActiveRow();
    if (row < 0 || row >= m_performers.size()) return;
    auto found = m_transitionEdit.previewPlacements.find(m_performers[row].id);
    if (found == m_transitionEdit.previewPlacements.end()
        || pointIndex < 0 || pointIndex >= found->pathPoints.size()) return;
    const int pivotRow = m_transitionEdit.options.value(QStringLiteral("pivotRow"), -1).toInt();
    if ((m_transitionEdit.type == QStringLiteral("gate")
         || m_transitionEdit.type == QStringLiteral("pivot")) && row == pivotRow) {
        setStatus(QStringLiteral("The gate pivot follows its source and destination; edit a gate member to make an independent override."));
        return;
    }
    found->pathPoints.removeAt(pointIndex);
    if (found->pathType == QStringLiteral("curved") && found->pathPoints.isEmpty())
        found->pathType = QStringLiteral("direct");
    const int leaderRow = m_transitionEdit.options.value(QStringLiteral("leaderRow"), -1).toInt();
    if (m_transitionEdit.type == QStringLiteral("follow") && row == leaderRow) {
        m_transitionEdit.options.insert(QStringLiteral("leaderPoints"), pointList(found->pathPoints));
        rebuildTransitionEditPreview();
    } else if (m_transitionEdit.type == QStringLiteral("follow")
               || m_transitionEdit.type == QStringLiteral("gate")
               || m_transitionEdit.type == QStringLiteral("pivot")) {
        preserveTransitionEditOverride(row, true);
    } else if (m_transitionEdit.type == QStringLiteral("stagger")) {
        preserveTransitionEditOverride(row, false);
    }
    emit transitionEditChanged();
}

void DrillProject::resetTransitionPath(int row)
{
    if (!m_transitionEdit.active) return;
    if (row < 0) row = transitionEditActiveRow();
    if (row < 0 || row >= m_performers.size()) return;
    auto found = m_transitionEdit.previewPlacements.find(m_performers[row].id);
    if (found == m_transitionEdit.previewPlacements.end()) return;
    const int leaderRow = m_transitionEdit.options.value(QStringLiteral("leaderRow"), -1).toInt();
    const int pivotRow = m_transitionEdit.options.value(QStringLiteral("pivotRow"), -1).toInt();
    if (m_transitionEdit.type == QStringLiteral("follow") && row == leaderRow) {
        m_transitionEdit.options.insert(QStringLiteral("leaderPoints"), QVariantList{});
        rebuildTransitionEditPreview();
    } else if ((m_transitionEdit.type == QStringLiteral("gate")
                || m_transitionEdit.type == QStringLiteral("pivot")) && row == pivotRow) {
        rebuildTransitionEditPreview();
    } else {
        found->pathType = QStringLiteral("direct"); found->pathPoints.clear();
        if (m_transitionEdit.type != QStringLiteral("stagger")) {
            found->pathStartCount = -1.0; found->pathDurationCounts = -1.0;
        }
        if (m_transitionEdit.type == QStringLiteral("follow")
            || m_transitionEdit.type == QStringLiteral("gate")
            || m_transitionEdit.type == QStringLiteral("pivot"))
            preserveTransitionEditOverride(row, true);
        else if (m_transitionEdit.type == QStringLiteral("stagger"))
            preserveTransitionEditOverride(row, false);
    }
    emit transitionEditChanged();
}

void DrillProject::mirrorActiveTransitionPath(bool horizontal)
{
    const int row = transitionEditActiveRow();
    if (row < 0) return;
    auto found = m_transitionEdit.previewPlacements.find(m_performers[row].id);
    if (found == m_transitionEdit.previewPlacements.end()) return;
    const QPointF origin = placementAt(row, m_transitionEdit.destinationSet - 1).position;
    const QPointF delta = found->position - origin;
    const double length = pointDistance({}, delta);
    if (length <= 1e-6) return;
    const QPointF tangent = delta / length, normal{-tangent.y(), tangent.x()};
    for (auto &point : found->pathPoints) {
        const QPointF relative = point - origin;
        double along = QPointF::dotProduct(relative, tangent);
        double offset = QPointF::dotProduct(relative, normal);
        if (horizontal) offset = -offset; else along = length - along;
        point = clampPosition(origin + tangent * along + normal * offset);
    }
    const int leaderRow = m_transitionEdit.options.value(QStringLiteral("leaderRow"), -1).toInt();
    const int pivotRow = m_transitionEdit.options.value(QStringLiteral("pivotRow"), -1).toInt();
    if (m_transitionEdit.type == QStringLiteral("follow") && row == leaderRow) {
        m_transitionEdit.options.insert(QStringLiteral("leaderPoints"), pointList(found->pathPoints));
        rebuildTransitionEditPreview();
    } else if ((m_transitionEdit.type == QStringLiteral("gate")
                || m_transitionEdit.type == QStringLiteral("pivot")) && row == pivotRow) {
        setStatus(QStringLiteral("The gate pivot route is determined by its endpoints."));
        rebuildTransitionEditPreview();
    } else if (m_transitionEdit.type == QStringLiteral("follow")
               || m_transitionEdit.type == QStringLiteral("gate")
               || m_transitionEdit.type == QStringLiteral("pivot")) {
        preserveTransitionEditOverride(row, true);
    } else if (m_transitionEdit.type == QStringLiteral("stagger")) {
        preserveTransitionEditOverride(row, false);
    }
    emit transitionEditChanged();
}

void DrillProject::copyActiveTransitionPathToSelection(bool mirrored)
{
    const int sourceRow = transitionEditActiveRow();
    if (sourceRow < 0) return;
    const QString sourceId = m_performers[sourceRow].id;
    const Placement source = transitionEditPlacement(sourceRow);
    const QPointF sourceOrigin = placementAt(sourceRow, m_transitionEdit.destinationSet - 1).position;
    const QPointF sourceDelta = source.position - sourceOrigin;
    const double sourceLength = pointDistance({}, sourceDelta);
    const QPointF sourceTangent = sourceLength > 1e-6 ? sourceDelta / sourceLength : QPointF{1.0, 0.0};
    const QPointF sourceNormal{-sourceTangent.y(), sourceTangent.x()};
    for (const auto &id : m_transitionEdit.performerIds) {
        const int row = performerRowForId(m_performers, id);
        if (row < 0 || row == sourceRow) continue;
        Placement target = m_transitionEdit.previewPlacements.value(id);
        const QPointF targetOrigin = placementAt(row, m_transitionEdit.destinationSet - 1).position;
        const QPointF targetDelta = target.position - targetOrigin;
        const double targetLength = pointDistance({}, targetDelta);
        const QPointF targetTangent = targetLength > 1e-6 ? targetDelta / targetLength : QPointF{1.0, 0.0};
        const QPointF targetNormal{-targetTangent.y(), targetTangent.x()};
        target.pathPoints.clear();
        for (const auto &sourcePoint : source.pathPoints) {
            const QPointF relative = sourcePoint - sourceOrigin;
            const double alongRatio = sourceLength > 1e-6
                ? QPointF::dotProduct(relative, sourceTangent) / sourceLength : 0.0;
            double normalRatio = sourceLength > 1e-6
                ? QPointF::dotProduct(relative, sourceNormal) / sourceLength : 0.0;
            if (mirrored) normalRatio = -normalRatio;
            target.pathPoints.push_back(clampPosition(targetOrigin
                + targetTangent * (alongRatio * targetLength)
                + targetNormal * (normalRatio * targetLength)));
        }
        target.pathType = source.pathType;
        target.pathStartCount = source.pathStartCount;
        target.pathDurationCounts = source.pathDurationCounts;
        m_transitionEdit.previewPlacements.insert(id, target);
        if (m_transitionEdit.type == QStringLiteral("follow")
            || m_transitionEdit.type == QStringLiteral("gate")
            || m_transitionEdit.type == QStringLiteral("pivot"))
            preserveTransitionEditOverride(row, true);
        else if (m_transitionEdit.type == QStringLiteral("stagger"))
            preserveTransitionEditOverride(row, false);
    }
    m_transitionEdit.activePerformerId = sourceId;
    emit transitionEditChanged();
}

double DrillProject::performerTotalDistance(int performerIndex) const
{
    ensureAnalyticsCache();
    return performerIndex >= 0 && performerIndex < m_cachedTotalDistances.size()
        ? m_cachedTotalDistances[performerIndex] : 0.0;
}

bool DrillProject::performerHasWarning(int performerIndex) const
{
    ensureAnalyticsCache();
    return performerIndex >= 0 && performerIndex < m_cachedWarnings.size()
        ? m_cachedWarnings[performerIndex] : false;
}

void DrillProject::ensureAnalyticsCache() const
{
    if (m_analyticsValid) return;
    const int count = m_performers.size();
    m_cachedTotalDistances.fill(0.0, count);
    m_cachedSetDistances.fill(0.0, count);
    m_cachedWarnings.fill(false, count);
    m_cachedEnsembleTotal = 0.0; m_cachedLongestMove = 0.0; m_cachedWarningCount = 0;
    for (int performer = 0; performer < count; ++performer) {
        for (int set = 1; set < m_sets.size(); ++set) {
            const double distance = transitionDistance(performer, set);
            m_cachedTotalDistances[performer] += distance;
            m_cachedLongestMove = qMax(m_cachedLongestMove, distance);
            if (set == m_currentSet) m_cachedSetDistances[performer] = distance;
        }
        m_cachedEnsembleTotal += m_cachedTotalDistances[performer];
        const double plannedSteps = m_currentSet > 0 && m_currentSet < m_sets.size()
            ? transitionPath(performer, m_currentSet).durationCounts()
                * m_sets[m_currentSet].stepMultiplier : 0.0;
        if ((plannedSteps <= 0.0 && m_cachedSetDistances[performer] > 0.01)
            || (plannedSteps > 0.0 && m_cachedSetDistances[performer] / plannedSteps > 1.25))
            m_cachedWarnings[performer] = true;
    }
    constexpr double cellSize = 1.5;
    QHash<qint64, QVector<int>> grid;
    auto keyFor = [](int x, int y) { return (qint64(x) << 32) ^ quint32(y); };
    for (int performer = 0; performer < count; ++performer) {
        const QPointF point = placementAt(performer, m_currentSet).position;
        const int cellX = qFloor(point.x() / cellSize), cellY = qFloor(point.y() / cellSize);
        for (int y = cellY - 1; y <= cellY + 1; ++y) for (int x = cellX - 1; x <= cellX + 1; ++x) {
            for (int other : grid.value(keyFor(x, y))) {
                const QPointF otherPoint = placementAt(other, m_currentSet).position;
                if (std::hypot(point.x() - otherPoint.x(), point.y() - otherPoint.y()) < cellSize)
                    m_cachedWarnings[performer] = m_cachedWarnings[other] = true;
            }
        }
        grid[keyFor(cellX, cellY)].push_back(performer);
    }
    for (bool warning : std::as_const(m_cachedWarnings)) if (warning) ++m_cachedWarningCount;
    m_analyticsValid = true;
}
