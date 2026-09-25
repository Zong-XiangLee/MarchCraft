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
        state.elapsedCounts = m_playhead * counts;
        // Preserve left/right alternation and step-off weight across consecutive
        // sets, including odd count moves. A stationary hold starts a new phrase.
        for (int set = playbackSetIndex() - 1; set > 0; --set) {
            if (transitionDistance(performerIndex, set) <= 1e-5) break;
            state.elapsedCounts += m_sets[set].counts;
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
    if (destination.pathType == QStringLiteral("follow")) {
        state.locomotion = QStringLiteral("march.forward");
        return state;
    }

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
    const Placement activePlacement = m_transitionEdit.previewPlacements.value(m_transitionEdit.activePerformerId);
    m_transitionEdit.type = activePlacement.pathType == QStringLiteral("delayed")
        ? QStringLiteral("stagger") : activePlacement.pathType;
    if (m_transitionEdit.type == QStringLiteral("pivot")) m_transitionEdit.type = QStringLiteral("gate");
    m_transitionEdit.options = {{QStringLiteral("order"), QStringLiteral("selection")},
        {QStringLiteral("reversed"), false}, {QStringLiteral("leaderRow"), transitionEditActiveRow()},
        {QStringLiteral("pivotRow"), transitionEditActiveRow()},
        {QStringLiteral("direction"), QStringLiteral("clockwise")},
        {QStringLiteral("intervalCounts"), 1.0}, {QStringLiteral("groupSize"), 1},
        {QStringLiteral("baseDelayCounts"), 0.0}, {QStringLiteral("arriveTogether"), true}};
    emit transitionEditChanged();
    setStatus(QStringLiteral("Transition path preview ready — Apply or Cancel"));
    return true;
}

void DrillProject::rebuildTransitionEditPreview()
{
    if (!m_transitionEdit.active || m_transitionEdit.destinationSet <= 0
        || m_transitionEdit.destinationSet >= m_sets.size()) return;
    m_transitionEdit.previewPlacements = m_transitionEdit.originalPlacements;
    const double counts = qMax(1, m_sets[m_transitionEdit.destinationSet].counts);
    const QString type = m_transitionEdit.type;
    const bool reversed = m_transitionEdit.options.value(QStringLiteral("reversed"), false).toBool();
    const QString order = m_transitionEdit.options.value(QStringLiteral("order"), QStringLiteral("formation")).toString();
    QVector<int> rows = orderedTransitionEditRows(order, reversed);
    if (rows.isEmpty()) return;

    if (type == QStringLiteral("direct")) {
        for (int row : rows) {
            auto &placement = m_transitionEdit.previewPlacements[m_performers[row].id];
            placement.pathType = QStringLiteral("direct"); placement.pathPoints.clear();
            placement.pathStartCount = -1.0; placement.pathDurationCounts = -1.0;
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
            placement.pathStartCount = -1.0; placement.pathDurationCounts = -1.0;
        }
        return;
    }
    if (type == QStringLiteral("follow")) {
        int leaderRow = m_transitionEdit.options.value(QStringLiteral("leaderRow"), rows.first()).toInt();
        auto leaderPosition = std::find(rows.begin(), rows.end(), leaderRow);
        if (leaderPosition == rows.end()) { leaderRow = rows.first(); leaderPosition = rows.begin(); }
        std::rotate(rows.begin(), leaderPosition, rows.end());
        m_transitionEdit.options.insert(QStringLiteral("leaderRow"), leaderRow);
        m_transitionEdit.activePerformerId = m_performers[leaderRow].id;

        const QString leaderId = m_performers[leaderRow].id;
        Placement leader = m_transitionEdit.previewPlacements.value(leaderId);
        QVector<QPointF> leaderPoints = variantPoints(m_transitionEdit.options.value(QStringLiteral("leaderPoints")));
        if (leaderPoints.isEmpty() && leader.pathType != QStringLiteral("direct")
            && leader.pathType != QStringLiteral("delayed")) leaderPoints = leader.pathPoints;
        const QPointF leaderOrigin = placementAt(leaderRow, m_transitionEdit.destinationSet - 1).position;
        if (leaderPoints.isEmpty()) leaderPoints = {clampPosition(defaultCurveControl(leaderOrigin, leader.position))};
        for (auto &point : leaderPoints) point = clampPosition(point);
        m_transitionEdit.options.insert(QStringLiteral("leaderPoints"), pointList(leaderPoints));
        leader.pathType = QStringLiteral("follow"); leader.pathPoints = leaderPoints;
        leader.pathStartCount = 0.0; leader.pathDurationCounts = counts;
        m_transitionEdit.previewPlacements.insert(leaderId, leader);

        MarchCraft::TransitionPath leaderPath(leaderOrigin, leader, counts);
        QVector<QPointF> route;
        constexpr int routeSamples = 64;
        route.reserve(routeSamples + 1);
        for (int sample = 0; sample <= routeSamples; ++sample)
            route.push_back(leaderPath.position(double(sample) / routeSamples));
        const double leaderSpeed = qMax(0.25, leaderPath.distance() / counts);
        double spacingDistance = 0.0;
        for (int ordinal = 1; ordinal < rows.size(); ++ordinal) {
            const int row = rows[ordinal];
            const int previousRow = rows[ordinal - 1];
            const QPointF origin = placementAt(row, m_transitionEdit.destinationSet - 1).position;
            const QPointF previousOrigin = placementAt(previousRow, m_transitionEdit.destinationSet - 1).position;
            spacingDistance += pointDistance(origin, previousOrigin);
            Placement follower = m_transitionEdit.previewPlacements.value(m_performers[row].id);
            int entry = 0, exit = routeSamples;
            double entryDistance = std::numeric_limits<double>::max();
            double exitDistance = std::numeric_limits<double>::max();
            for (int sample = 0; sample <= routeSamples; ++sample) {
                const double fromOrigin = pointDistance(origin, route[sample]);
                if (fromOrigin < entryDistance) { entryDistance = fromOrigin; entry = sample; }
                const double fromDestination = pointDistance(follower.position, route[sample]);
                if (fromDestination < exitDistance) { exitDistance = fromDestination; exit = sample; }
            }
            if (exit < entry) { entry = 0; exit = routeSamples; }
            QVector<QPointF> points;
            auto appendDistinct = [&](QPointF point) {
                point = clampPosition(point);
                if ((points.isEmpty() ? pointDistance(origin, point) : pointDistance(points.last(), point)) > 0.05
                    && pointDistance(follower.position, point) > 0.05) points.push_back(point);
            };
            appendDistinct(route[entry]);
            for (int sample = entry + 4; sample < exit; sample += 4) appendDistinct(route[sample]);
            appendDistinct(route[exit]);
            follower.pathType = QStringLiteral("follow"); follower.pathPoints = points;
            follower.pathStartCount = qMin(counts - 0.25, spacingDistance / leaderSpeed);
            follower.pathDurationCounts = qMax(0.25, counts - follower.pathStartCount);
            m_transitionEdit.previewPlacements.insert(m_performers[row].id, follower);
        }
        return;
    }
    if (type == QStringLiteral("gate") || type == QStringLiteral("pivot")) {
        int pivotRow = m_transitionEdit.options.value(QStringLiteral("pivotRow"), rows.first()).toInt();
        if (!rows.contains(pivotRow)) pivotRow = rows.first();
        m_transitionEdit.options.insert(QStringLiteral("pivotRow"), pivotRow);
        m_transitionEdit.activePerformerId = m_performers[pivotRow].id;
        const QPointF pivot = placementAt(pivotRow, m_transitionEdit.destinationSet - 1).position;
        const bool clockwise = m_transitionEdit.options.value(QStringLiteral("direction"),
            QStringLiteral("clockwise")).toString() != QStringLiteral("counterclockwise");
        for (int row : rows) {
            const QString id = m_performers[row].id;
            Placement placement = m_transitionEdit.previewPlacements.value(id);
            const QPointF origin = placementAt(row, m_transitionEdit.destinationSet - 1).position;
            const QPointF startVector = origin - pivot, endVector = placement.position - pivot;
            const double startRadius = pointDistance({}, startVector), endRadius = pointDistance({}, endVector);
            placement.pathPoints.clear(); placement.pathStartCount = -1.0; placement.pathDurationCounts = -1.0;
            if (row == pivotRow || (startRadius < 0.01 && endRadius < 0.01)) {
                placement.pathType = QStringLiteral("pivot");
            } else {
                placement.pathType = QStringLiteral("gate");
                const double startAngle = std::atan2(startVector.y(), startVector.x());
                const double endAngle = std::atan2(endVector.y(), endVector.x());
                double sweep = endAngle - startAngle;
                if (clockwise) while (sweep > 0.0) sweep -= 2.0 * std::numbers::pi;
                else while (sweep < 0.0) sweep += 2.0 * std::numbers::pi;
                constexpr int arcSamples = 12;
                for (int sample = 1; sample < arcSamples; ++sample) {
                    const double progress = double(sample) / arcSamples;
                    const double radius = startRadius + (endRadius - startRadius) * progress;
                    const double angle = startAngle + sweep * progress;
                    placement.pathPoints.push_back(clampPosition(pivot
                        + QPointF(std::cos(angle) * radius, std::sin(angle) * radius)));
                }
            }
            m_transitionEdit.previewPlacements.insert(id, placement);
        }
        return;
    }
    if (type == QStringLiteral("stagger")) {
        const int groupSize = qMax(1, m_transitionEdit.options.value(QStringLiteral("groupSize"), 1).toInt());
        const double interval = qMax(0.0, m_transitionEdit.options.value(QStringLiteral("intervalCounts"), 1.0).toDouble());
        const double baseDelay = qMax(0.0, m_transitionEdit.options.value(QStringLiteral("baseDelayCounts"), 0.0).toDouble());
        const bool arriveTogether = m_transitionEdit.options.value(QStringLiteral("arriveTogether"), true).toBool();
        const int lastGroup = rows.isEmpty() ? 0 : (rows.size() - 1) / groupSize;
        const double maximumStart = qMin(counts - 0.25, baseDelay + lastGroup * interval);
        const double commonDuration = qMax(0.25, counts - maximumStart);
        for (int ordinal = 0; ordinal < rows.size(); ++ordinal) {
            const int row = rows[ordinal];
            Placement placement = m_transitionEdit.previewPlacements.value(m_performers[row].id);
            if (placement.pathType == QStringLiteral("delayed")) {
                placement.pathType = QStringLiteral("direct"); placement.pathPoints.clear();
            }
            placement.pathStartCount = qMin(counts - 0.25, baseDelay + (ordinal / groupSize) * interval);
            placement.pathDurationCounts = arriveTogether
                ? qMax(0.25, counts - placement.pathStartCount) : commonDuration;
            m_transitionEdit.previewPlacements.insert(m_performers[row].id, placement);
        }
    }
}

void DrillProject::setTransitionEditType(const QString &type, const QVariantMap &options)
{
    if (!m_transitionEdit.active && !beginTransitionEdit()) return;
    static const QSet<QString> supported{QStringLiteral("direct"), QStringLiteral("curved"),
        QStringLiteral("follow"), QStringLiteral("gate"), QStringLiteral("pivot"),
        QStringLiteral("stagger"), QStringLiteral("delayed")};
    m_transitionEdit.type = supported.contains(type) ? type : QStringLiteral("direct");
    for (auto it = options.cbegin(); it != options.cend(); ++it) m_transitionEdit.options.insert(it.key(), it.value());
    rebuildTransitionEditPreview();
    emit transitionEditChanged();
}

void DrillProject::updateTransitionEditOptions(const QVariantMap &options)
{
    if (!m_transitionEdit.active) return;
    for (auto it = options.cbegin(); it != options.cend(); ++it) m_transitionEdit.options.insert(it.key(), it.value());
    rebuildTransitionEditPreview();
    emit transitionEditChanged();
}

bool DrillProject::applyTransitionEdit()
{
    if (!m_transitionEdit.active || m_transitionEdit.destinationSet != m_currentSet) return false;
    const auto before = toJson();
    auto &placements = m_sets[m_currentSet].activeVariant().placements;
    for (auto it = m_transitionEdit.previewPlacements.cbegin(); it != m_transitionEdit.previewPlacements.cend(); ++it) {
        auto &destination = placements[it.key()];
        destination.pathType = it.value().pathType;
        destination.pathPoints = it.value().pathPoints;
        destination.pathStartCount = it.value().pathStartCount;
        destination.pathDurationCounts = it.value().pathDurationCounts;
    }
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
        m_transitionEdit.performerIds = {id};
        m_transitionEdit.originalPlacements.clear(); m_transitionEdit.previewPlacements.clear();
        const Placement placement = placementAt(row, m_transitionEdit.destinationSet);
        m_transitionEdit.originalPlacements.insert(id, placement);
        m_transitionEdit.previewPlacements.insert(id, placement);
        for (auto &performer : m_performers) performer.selected = performer.id == id;
    } else if (m_transitionEdit.originalPlacements.contains(id)) {
        m_transitionEdit.performerIds.removeAll(id);
        m_transitionEdit.originalPlacements.remove(id); m_transitionEdit.previewPlacements.remove(id);
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
    const Placement active = m_transitionEdit.previewPlacements.value(m_transitionEdit.activePerformerId);
    m_transitionEdit.type = active.pathType == QStringLiteral("delayed") ? QStringLiteral("stagger") : active.pathType;
    emit selectionChanged(); emit transitionEditChanged();
    return true;
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
    const int samples = qBound(8, counts * 4, 64);
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
        {QStringLiteral("averageDistance"), editedRows.isEmpty() ? 0.0 : totalDistance / editedRows.size()},
        {QStringLiteral("longestDistance"), longest}, {QStringLiteral("maximumStepsPerCount"), maximumSteps},
        {QStringLiteral("warningCount"), warnings}, {QStringLiteral("collisionCount"), collisionPairs.size()},
        {QStringLiteral("activeDistance"), active.distance()}, {QStringLiteral("activeStartCount"), active.startCount()},
        {QStringLiteral("activeArrivalCount"), active.arrivalCount()}};
    return result;
}

void DrillProject::moveTransitionControlPoint(int row, int pointIndex, double x, double y)
{
    if (!m_transitionEdit.active || row < 0 || row >= m_performers.size()) return;
    auto found = m_transitionEdit.previewPlacements.find(m_performers[row].id);
    if (found == m_transitionEdit.previewPlacements.end()
        || pointIndex < 0 || pointIndex >= found->pathPoints.size()) return;
    found->pathPoints[pointIndex] = clampPosition({x, y});
    const int leaderRow = m_transitionEdit.options.value(QStringLiteral("leaderRow"), -1).toInt();
    if (m_transitionEdit.type == QStringLiteral("follow") && row == leaderRow) {
        m_transitionEdit.options.insert(QStringLiteral("leaderPoints"), pointList(found->pathPoints));
        rebuildTransitionEditPreview();
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
    if (found->pathType == QStringLiteral("direct") || found->pathType == QStringLiteral("delayed")) {
        found->pathType = QStringLiteral("curved");
        m_transitionEdit.type = QStringLiteral("curved");
    }
    pointIndex = qBound(0, pointIndex, found->pathPoints.size());
    found->pathPoints.insert(pointIndex, clampPosition({x, y}));
    const int leaderRow = m_transitionEdit.options.value(QStringLiteral("leaderRow"), -1).toInt();
    if (m_transitionEdit.type == QStringLiteral("follow") && row == leaderRow) {
        m_transitionEdit.options.insert(QStringLiteral("leaderPoints"), pointList(found->pathPoints));
        rebuildTransitionEditPreview();
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
    found->pathPoints.removeAt(pointIndex);
    if (found->pathType == QStringLiteral("curved") && found->pathPoints.isEmpty())
        found->pathType = QStringLiteral("direct");
    const int leaderRow = m_transitionEdit.options.value(QStringLiteral("leaderRow"), -1).toInt();
    if (m_transitionEdit.type == QStringLiteral("follow") && row == leaderRow) {
        m_transitionEdit.options.insert(QStringLiteral("leaderPoints"), pointList(found->pathPoints));
        rebuildTransitionEditPreview();
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
    found->pathType = QStringLiteral("direct"); found->pathPoints.clear();
    found->pathStartCount = -1.0; found->pathDurationCounts = -1.0;
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
    if (m_transitionEdit.type == QStringLiteral("follow") && row == leaderRow)
        m_transitionEdit.options.insert(QStringLiteral("leaderPoints"), pointList(found->pathPoints));
    emit transitionEditChanged();
}

void DrillProject::copyActiveTransitionPathToSelection(bool mirrored)
{
    const int sourceRow = transitionEditActiveRow();
    if (sourceRow < 0) return;
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
    }
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
