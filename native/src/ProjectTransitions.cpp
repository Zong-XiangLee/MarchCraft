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

Placement DrillProject::placementAt(int performerIndex, int setIndex) const
{
    if (performerIndex < 0 || performerIndex >= m_performers.size() || m_sets.isEmpty()) return {};
    setIndex = std::clamp(setIndex, 0, static_cast<int>(m_sets.size()) - 1);
    return m_sets[setIndex].activeVariant().placements.value(m_performers[performerIndex].id);
}

QPointF DrillProject::interpolatedPosition(int performerIndex) const
{
    if (m_currentSet <= 0 || m_playhead >= 1.0) return placementAt(performerIndex, m_currentSet).position;
    return pathPosition(performerIndex, m_currentSet, m_playhead);
}

double DrillProject::interpolatedFacing(int performerIndex) const
{
    const double destination = placementAt(performerIndex, m_currentSet).facing;
    if (m_currentSet <= 0 || m_playhead >= 1.0) return destination;
    const double start = placementAt(performerIndex, m_currentSet - 1).facing;
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
    const int counts = currentSetCounts();
    if (counts > 0) {
        state.elapsedCounts = m_playhead * counts;
        // Preserve left/right alternation and step-off weight across consecutive
        // sets, including odd count moves. A stationary hold starts a new phrase.
        for (int set = m_currentSet - 1; set > 0; --set) {
            if (transitionDistance(performerIndex, set) <= 1e-5) break;
            state.elapsedCounts += m_sets[set].counts;
        }
        state.normalizedTime = std::fmod(state.elapsedCounts / 2.0, 1.0);
        if (state.normalizedTime < 0.0) state.normalizedTime += 1.0;
    }
    if (!m_playbackActive || m_currentSet <= 0 || counts <= 0) return state;

    const Placement destination = placementAt(performerIndex, m_currentSet);
    const Placement origin = placementAt(performerIndex, m_currentSet - 1);
    if (m_currentSet + 1 >= m_sets.size()) {
        state.closesAtDestination = true;
    } else {
        state.closesAtDestination = transitionDistance(performerIndex, m_currentSet + 1) <= 1e-5;
    }
    const double facingDelta = std::abs(std::fmod(destination.facing - origin.facing + 540.0, 360.0) - 180.0);

    constexpr double speedSampleRadius = 0.001;
    const double beforeProgress = std::max(0.0, m_playhead - speedSampleRadius);
    const double afterProgress = std::min(1.0, m_playhead + speedSampleRadius);
    const QPointF before = pathPosition(performerIndex, m_currentSet, beforeProgress);
    const QPointF current = pathPosition(performerIndex, m_currentSet, m_playhead);
    const QPointF after = pathPosition(performerIndex, m_currentSet, afterProgress);
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
    const QPointF headingDelta = pathPosition(performerIndex, m_currentSet, headingAfterProgress)
                               - pathPosition(performerIndex, m_currentSet, headingBeforeProgress);
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
        placementAt(performerIndex, destinationSet))).value();
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
    const double plannedSteps = m_currentSet >= 0 && m_currentSet < m_sets.size()
        ? m_sets[m_currentSet].counts * m_sets[m_currentSet].stepMultiplier : 0.0;
    for (int performer = 0; performer < count; ++performer) {
        for (int set = 1; set < m_sets.size(); ++set) {
            const double distance = transitionDistance(performer, set);
            m_cachedTotalDistances[performer] += distance;
            m_cachedLongestMove = qMax(m_cachedLongestMove, distance);
            if (set == m_currentSet) m_cachedSetDistances[performer] = distance;
        }
        m_cachedEnsembleTotal += m_cachedTotalDistances[performer];
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
