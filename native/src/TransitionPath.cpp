#include "TransitionPath.h"

#include <algorithm>
#include <cmath>

namespace MarchCraft {

namespace {

QPointF bezierPoint(const QVector<QPointF> &controlPolygon, double progress)
{
    QVector<QPointF> work = controlPolygon;
    for (int level = work.size() - 1; level > 0; --level)
        for (int index = 0; index < level; ++index)
            work[index] = work[index] * (1.0 - progress) + work[index + 1] * progress;
    return work.isEmpty() ? QPointF{} : work.first();
}

}

TransitionPath::TransitionPath(QPointF origin, const Placement &destination, double transitionCounts)
{
    m_transitionCounts = std::max(0.001, transitionCounts);
    const double legacyStart = destination.pathType == QStringLiteral("delayed")
        ? m_transitionCounts * 0.25 : 0.0;
    m_startCount = destination.pathStartCount >= 0.0
        ? std::clamp(destination.pathStartCount, 0.0, std::max(0.0, m_transitionCounts - 0.001))
        : legacyStart;
    const double availableCounts = std::max(0.001, m_transitionCounts - m_startCount);
    m_durationCounts = destination.pathDurationCounts >= 0.0
        ? std::clamp(destination.pathDurationCounts, 0.001, availableCounts)
        : availableCounts;

    m_points.push_back(origin);
    if (destination.pathType == QStringLiteral("curved") && !destination.pathPoints.isEmpty()) {
        QVector<QPointF> controlPolygon{origin};
        controlPolygon += destination.pathPoints;
        controlPolygon.push_back(destination.position);
        const int samples = std::clamp(int(controlPolygon.size() - 1) * 24, 32, 256);
        for (int sample = 1; sample < samples; ++sample)
            m_points.push_back(bezierPoint(controlPolygon, double(sample) / samples));
    } else if (destination.pathType == QStringLiteral("follow")
               || destination.pathType == QStringLiteral("gate")
               || destination.pathType == QStringLiteral("pivot")) {
        m_points += destination.pathPoints;
    }
    m_points.push_back(destination.position);
    m_cumulative.push_back(0.0);
    for (int i = 1; i < m_points.size(); ++i) {
        const auto delta = m_points[i] - m_points[i - 1];
        m_cumulative.push_back(m_cumulative.last() + std::hypot(delta.x(), delta.y()));
    }
}

QPointF TransitionPath::position(double progress) const
{
    if (m_points.isEmpty()) return {};
    progress = std::clamp(progress, 0.0, 1.0);
    const double elapsedCounts = progress * m_transitionCounts;
    progress = std::clamp((elapsedCounts - m_startCount) / m_durationCounts, 0.0, 1.0);
    if (progress >= 1.0) return m_points.last();
    const double target = progress * distance();
    const auto found = std::lower_bound(m_cumulative.cbegin() + 1, m_cumulative.cend(), target);
    if (found == m_cumulative.cend()) return m_points.last();
    const int segment = int(found - m_cumulative.cbegin());
    const double length = m_cumulative[segment] - m_cumulative[segment - 1];
    const double t = length <= 0.0 ? 0.0 : (target - m_cumulative[segment - 1]) / length;
    return m_points[segment - 1] + (m_points[segment] - m_points[segment - 1]) * t;
}

}
