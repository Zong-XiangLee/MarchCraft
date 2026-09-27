#pragma once

#include "DrillTypes.h"

namespace MarchCraft {

// Immutable arc-length table shared by rendering and collision analysis.
class TransitionPath
{
public:
    TransitionPath() = default;
    TransitionPath(QPointF origin, const Placement &destination, double transitionCounts = 1.0);
    QPointF position(double progress) const;
    double distance() const { return m_cumulative.isEmpty() ? 0.0 : m_cumulative.last(); }
    double startCount() const { return m_startCount; }
    double durationCounts() const { return m_durationCounts; }
    double arrivalCount() const { return m_startCount + m_durationCounts; }

private:
    QVector<QPointF> m_points;
    QVector<double> m_cumulative;
    double m_transitionCounts = 1.0;
    double m_startCount = 0.0;
    double m_durationCounts = 1.0;
};

}
