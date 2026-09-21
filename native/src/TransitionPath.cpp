#include "TransitionPath.h"

#include <algorithm>
#include <cmath>

namespace MarchCraft {

TransitionPath::TransitionPath(QPointF origin, const Placement &destination, int transitionCounts)
    : m_delayFraction(transitionCounts > 0
          ? std::clamp(double(destination.stepOffCount) / transitionCounts, 0.0, 0.99) : 0.0)
{
    m_points.push_back(origin);
    if (destination.pathType == QStringLiteral("curved") && !destination.pathPoints.isEmpty()) {
        const QPointF control = destination.pathPoints.first();
        for (int sample = 1; sample < 32; ++sample) {
            const double t = sample / 32.0, u = 1.0 - t;
            m_points.push_back(origin * (u * u) + control * (2 * u * t) + destination.position * (t * t));
        }
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
    if (m_delayFraction > 0.0)
        progress = progress <= m_delayFraction ? 0.0 : (progress - m_delayFraction) / (1.0 - m_delayFraction);
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
