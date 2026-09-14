#pragma once

#include "DrillTypes.h"

namespace MarchCraft {

// Immutable arc-length table shared by rendering and collision analysis.
class TransitionPath
{
public:
    TransitionPath() = default;
    TransitionPath(QPointF origin, const Placement &destination);
    QPointF position(double progress) const;
    double distance() const { return m_cumulative.isEmpty() ? 0.0 : m_cumulative.last(); }

private:
    QVector<QPointF> m_points;
    QVector<double> m_cumulative;
    bool m_delayed = false;
};

}
