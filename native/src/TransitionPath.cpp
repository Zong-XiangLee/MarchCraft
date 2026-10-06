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

    const auto finitePoint = [](QPointF point) {
        return std::isfinite(point.x()) && std::isfinite(point.y());
    };
    if (!finitePoint(origin)) origin = {};
    const QPointF endpoint = finitePoint(destination.position) ? destination.position : origin;
    auto appendPoint = [&](QPointF point) {
        if (!finitePoint(point)) return;
        if (m_points.isEmpty() || std::hypot(point.x() - m_points.last().x(),
                                             point.y() - m_points.last().y()) > 1e-9)
            m_points.push_back(point);
    };

    appendPoint(origin);
    if (destination.pathType == QStringLiteral("curved") && !destination.pathPoints.isEmpty()) {
        QVector<QPointF> controlPolygon{origin};
        for (const auto &point : destination.pathPoints)
            if (finitePoint(point)) controlPolygon.push_back(point);
        controlPolygon.push_back(endpoint);
        const int samples = std::clamp(int(controlPolygon.size() - 1) * 48, 64, 512);
        for (int sample = 1; sample < samples; ++sample)
            appendPoint(bezierPoint(controlPolygon, double(sample) / samples));
    } else if (destination.pathType == QStringLiteral("follow")
               || destination.pathType == QStringLiteral("gate")
               || destination.pathType == QStringLiteral("pivot")) {
        for (const auto &point : destination.pathPoints) appendPoint(point);
    }
    appendPoint(endpoint);
    if (m_points.size() == 1) m_points.push_back(endpoint);
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
    return geometryPosition(progress);
}

QPointF TransitionPath::geometryPosition(double progress) const
{
    if (m_points.isEmpty()) return {};
    progress = std::clamp(progress, 0.0, 1.0);
    if (progress >= 1.0) return m_points.last();
    const double target = progress * distance();
    const auto found = std::lower_bound(m_cumulative.cbegin() + 1, m_cumulative.cend(), target);
    if (found == m_cumulative.cend()) return m_points.last();
    const int segment = int(found - m_cumulative.cbegin());
    const double length = m_cumulative[segment] - m_cumulative[segment - 1];
    const double t = length <= 0.0 ? 0.0 : (target - m_cumulative[segment - 1]) / length;
    return m_points[segment - 1] + (m_points[segment] - m_points[segment - 1]) * t;
}

QPointF TransitionPath::tangent(double progress) const
{
    if (m_points.size() < 2 || distance() <= 1e-9) return {};
    constexpr double radius = 0.0025;
    const double before = std::max(0.0, progress - radius);
    const double after = std::min(1.0, progress + radius);
    QPointF delta = geometryPosition(after) - geometryPosition(before);
    const double length = std::hypot(delta.x(), delta.y());
    if (length <= 1e-9) return {};
    return delta / length;
}

}
