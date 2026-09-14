
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


#include "ProjectAlgorithms.h"

namespace MarchCraft::ProjectAlgorithms {
QVector<int> minimumCostAssignment(const QVector<QVector<double>> &costs)
{
    if (costs.isEmpty() || costs.size() >= std::numeric_limits<int>::max()) return {};
    const int n = static_cast<int>(costs.size());
    for (const auto &row : costs) {
        if (row.size() != n) return {};
        for (double cost : row) if (!std::isfinite(cost)) return {};
    }
    // Deterministic Hungarian algorithm. Potentials are 1-indexed following the
    // classic shortest augmenting-path formulation.
    QVector<double> u(n + 1), v(n + 1);
    QVector<int> p(n + 1), way(n + 1);
    for (int row = 1; row <= n; ++row) {
        p[0] = row; int column0 = 0;
        QVector<double> minimum(n + 1, std::numeric_limits<double>::max());
        QVector<bool> used(n + 1, false);
        do {
            used[column0] = true; const int row0 = p[column0];
            double delta = std::numeric_limits<double>::max(); int column1 = 0;
            for (int column = 1; column <= n; ++column) if (!used[column]) {
                const double current = costs[row0 - 1][column - 1] - u[row0] - v[column];
                if (current < minimum[column]) { minimum[column] = current; way[column] = column0; }
                if (minimum[column] < delta - 1e-9 || (qAbs(minimum[column] - delta) <= 1e-9 && column < column1)) {
                    delta = minimum[column]; column1 = column;
                }
            }
            for (int column = 0; column <= n; ++column) {
                if (used[column]) { u[p[column]] += delta; v[column] -= delta; }
                else minimum[column] -= delta;
            }
            column0 = column1;
        } while (p[column0] != 0);
        do { const int column1 = way[column0]; p[column0] = p[column1]; column0 = column1; }
        while (column0 != 0);
    }
    QVector<int> assignment(n, -1);
    for (int column = 1; column <= n; ++column) if (p[column] > 0) assignment[p[column] - 1] = column - 1;
    return assignment;
}


QString csvCell(QString value)
{
    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"") + value + QStringLiteral("\"");
}

QString instrumentAssetIdFor(const QString &instrument)
{
    const QString value = instrument.toLower();
    if (value.contains(QStringLiteral("trumpet"))) return QStringLiteral("instrument.trumpet");
    if (value.contains(QStringLiteral("tuba")) || value.contains(QStringLiteral("sousaphone")))
        return QStringLiteral("instrument.tuba");
    if (value.contains(QStringLiteral("guard")) || value.contains(QStringLiteral("flag")))
        return QStringLiteral("equipment.guard.flag");
    return QStringLiteral("instrument.generic");
}

double degreesToRadians(double degrees)
{
    return degrees * std::numbers::pi / 180.0;
}

QPointF rotateAround(QPointF point, QPointF center, double degrees)
{
    const double angle = degreesToRadians(degrees), c = std::cos(angle), s = std::sin(angle);
    const QPointF relative = point - center;
    return center + QPointF(relative.x() * c - relative.y() * s,
                            relative.x() * s + relative.y() * c);
}

QVector<QPointF> equalDistancePoints(const QVector<QPointF> &path, int count, bool closed)
{
    QVector<QPointF> result;
    if (path.isEmpty() || count <= 0) return result;
    if (count == 1) { result.push_back(path[path.size() / 2]); return result; }
    QVector<QPointF> work = path;
    if (closed && work.first() != work.last()) work.push_back(work.first());
    QVector<double> cumulative{0.0}; double total = 0.0;
    for (int i = 1; i < work.size(); ++i) {
        total += std::hypot(work[i].x() - work[i - 1].x(), work[i].y() - work[i - 1].y());
        cumulative.push_back(total);
    }
    if (qFuzzyIsNull(total)) return QVector<QPointF>(count, work.first());
    int segment = 1;
    for (int n = 0; n < count; ++n) {
        const double target = total * (closed ? double(n) / count : double(n) / (count - 1));
        while (segment < cumulative.size() - 1 && cumulative[segment] < target) ++segment;
        const double start = cumulative[segment - 1], length = cumulative[segment] - start;
        const double t = qFuzzyIsNull(length) ? 0.0 : (target - start) / length;
        result.push_back(work[segment - 1] + (work[segment] - work[segment - 1]) * t);
    }
    return result;
}

double polylineLength(const QVector<QPointF> &path, bool closed)
{
    if (path.size() < 2) return 0.0;
    double result = 0.0;
    for (int i = 1; i < path.size(); ++i)
        result += std::hypot(path[i].x() - path[i - 1].x(), path[i].y() - path[i - 1].y());
    if (closed && path.first() != path.last())
        result += std::hypot(path.first().x() - path.last().x(), path.first().y() - path.last().y());
    return result;
}

QVector<QPointF> uniformlySampledPath(const QVector<QPointF> &path, bool closed, double spacing)
{
    const double length = polylineLength(path, closed);
    if (path.size() < 2 || length <= 0.0) return path;
    const int count = qBound(2, qCeil(length / qMax(0.05, spacing)) + (closed ? 0 : 1), 2048);
    QVector<QPointF> sampled = equalDistancePoints(path, count, closed);
    if (closed && !sampled.isEmpty()) sampled.push_back(sampled.first());
    return sampled;
}

QVector<QPointF> smoothedStroke(const QVector<QPointF> &path, bool closed, int passes)
{
    if (path.size() < 3 || passes <= 0) return path;
    QVector<QPointF> result = path;
    const bool duplicatedEndpoint = closed && result.first() == result.last();
    if (duplicatedEndpoint) result.removeLast();
    for (int pass = 0; pass < passes; ++pass) {
        QVector<QPointF> next = result;
        for (int i = 0; i < result.size(); ++i) {
            if (!closed && (i == 0 || i == result.size() - 1)) continue;
            const QPointF &previous = result[(i - 1 + result.size()) % result.size()];
            const QPointF &following = result[(i + 1) % result.size()];
            next[i] = previous * 0.2 + result[i] * 0.6 + following * 0.2;
        }
        result = std::move(next);
    }
    if (duplicatedEndpoint && !result.isEmpty()) result.push_back(result.first());
    return result;
}

QVector<QPointF> rectanglePerimeterPoints(const QVector<QPointF> &path, int count)
{
    if (path.size() < 5 || count < 4) return {};
    QVector<QPointF> corners{path[0], path[1], path[2], path[3]};
    QVector<double> lengths(4); double perimeter = 0.0;
    for (int side = 0; side < 4; ++side) {
        lengths[side] = std::hypot(corners[(side + 1) % 4].x() - corners[side].x(),
                                   corners[(side + 1) % 4].y() - corners[side].y());
        perimeter += lengths[side];
    }
    if (perimeter <= 0.0) return {};
    const int extras = count - 4;
    QVector<int> interior(4);
    QVector<double> fractional(4);
    int assigned = 0;
    for (int side = 0; side < 4; ++side) {
        const double exact = extras * lengths[side] / perimeter;
        interior[side] = qFloor(exact);
        fractional[side] = exact - interior[side];
        assigned += interior[side];
    }
    while (assigned < extras) {
        int best = 0;
        for (int side = 1; side < 4; ++side)
            if (fractional[side] > fractional[best]) best = side;
        ++interior[best]; fractional[best] = -1.0; ++assigned;
    }
    QVector<QPair<double, QPointF>> ordered;
    double distance = 0.0;
    for (int side = 0; side < 4; ++side) {
        ordered.push_back({distance, corners[side]});
        const QPointF delta = corners[(side + 1) % 4] - corners[side];
        for (int item = 1; item <= interior[side]; ++item) {
            const double t = double(item) / (interior[side] + 1);
            ordered.push_back({distance + lengths[side] * t, corners[side] + delta * t});
        }
        distance += lengths[side];
    }
    std::sort(ordered.begin(), ordered.end(), [](const auto &left, const auto &right) {
        return left.first < right.first;
    });
    QVector<QPointF> result;
    result.reserve(count);
    for (const auto &entry : ordered) result.push_back(entry.second);
    return result;
}

void fitPathToField(QVector<QPointF> &path, QPointF &anchor,
                    double minX, double maxX, double minY, double maxY)
{
    if (path.isEmpty()) return;
    constexpr double margin = 2.0;
    auto pathBounds = [](const QVector<QPointF> &points) {
        double left = points.first().x(), right = left, top = points.first().y(), bottom = top;
        for (const auto &p : points) { left = qMin(left, p.x()); right = qMax(right, p.x());
            top = qMin(top, p.y()); bottom = qMax(bottom, p.y()); }
        return QRectF(QPointF(left, top), QPointF(right, bottom));
    };
    QRectF bounds = pathBounds(path);
    const double usableWidth = qMax(1.0, maxX - minX - margin * 2.0);
    const double usableDepth = qMax(1.0, maxY - minY - margin * 2.0);
    const double scale = qMin(1.0, qMin(usableWidth / qMax(0.001, bounds.width()),
                                       usableDepth / qMax(0.001, bounds.height())));
    if (scale < 1.0) for (auto &p : path) p = anchor + (p - anchor) * scale;
    bounds = pathBounds(path);
    QPointF shift;
    if (bounds.left() < minX + margin) shift.rx() += minX + margin - bounds.left();
    if (bounds.right() > maxX - margin) shift.rx() -= bounds.right() - (maxX - margin);
    if (bounds.top() < minY + margin) shift.ry() += minY + margin - bounds.top();
    if (bounds.bottom() > maxY - margin) shift.ry() -= bounds.bottom() - (maxY - margin);
    for (auto &p : path) p += shift;
    anchor += shift;
}

QString compactNumber(double value)
{
    return QString::number(value, 'f', std::abs(value - std::round(value)) < 0.01 ? 0 : 2);
}

double pointDistance(QPointF a, QPointF b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

double crossProduct(QPointF a, QPointF b, QPointF c)
{
    return (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
}

bool segmentsCross(QPointF a, QPointF b, QPointF c, QPointF d)
{
    constexpr double epsilon = 1e-8;
    const double abC = crossProduct(a, b, c), abD = crossProduct(a, b, d);
    const double cdA = crossProduct(c, d, a), cdB = crossProduct(c, d, b);
    const bool proper = ((abC > epsilon && abD < -epsilon) || (abC < -epsilon && abD > epsilon))
        && ((cdA > epsilon && cdB < -epsilon) || (cdA < -epsilon && cdB > epsilon));
    if (proper) return true;
    if (qAbs(abC) > epsilon || qAbs(abD) > epsilon || qAbs(cdA) > epsilon || qAbs(cdB) > epsilon)
        return false;
    // Collinear swap/overlap paths still cross operationally even though the
    // classic orientation test reports no proper intersection.
    const bool useX = qAbs(b.x() - a.x()) >= qAbs(b.y() - a.y());
    const double a0 = useX ? qMin(a.x(), b.x()) : qMin(a.y(), b.y());
    const double a1 = useX ? qMax(a.x(), b.x()) : qMax(a.y(), b.y());
    const double b0 = useX ? qMin(c.x(), d.x()) : qMin(c.y(), d.y());
    const double b1 = useX ? qMax(c.x(), d.x()) : qMax(c.y(), d.y());
    return qMin(a1, b1) - qMax(a0, b0) > epsilon;
}

double instrumentClearanceRadius(const MarchCraft::Performer &performer)
{
    const QString value = (performer.instrument + QLatin1Char(' ') + performer.appearance.instrumentAssetId
        + QLatin1Char(' ') + performer.appearance.equipmentAssetId).toLower();
    if (value.contains(QStringLiteral("tuba")) || value.contains(QStringLiteral("sousaphone"))) return 0.95;
    if (value.contains(QStringLiteral("percussion")) || value.contains(QStringLiteral("drum"))) return 1.1;
    if (value.contains(QStringLiteral("trombone"))) return 0.85;
    if (value.contains(QStringLiteral("guard")) || value.contains(QStringLiteral("flag")) || value.contains(QStringLiteral("rifle"))) return 1.0;
    if (value.contains(QStringLiteral("baritone")) || value.contains(QStringLiteral("euphonium"))) return 0.75;
    return 0.5;
}

QSizeF propFootprintSteps(const MarchCraft::PropInstance &prop)
{
    QSizeF meters{1.0, 1.0};
    if (prop.definitionId == QStringLiteral("prop.panel")) meters = {2.4, 0.15};
    else if (prop.definitionId == QStringLiteral("prop.platform")) meters = {2.4, 2.4};
    else if (prop.definitionId == QStringLiteral("prop.podium")) meters = {1.2, 1.2};
    constexpr double metersPerStep = 0.5715;
    return {meters.width() * prop.scale.x() / metersPerStep,
            meters.height() * prop.scale.z() / metersPerStep};
}

QString propDisplayName(const MarchCraft::PropInstance &prop)
{
    QString value = prop.definitionId;
    if (value.startsWith(QStringLiteral("prop."))) value.remove(0, 5);
    if (!value.isEmpty()) value[0] = value[0].toUpper();
    return value.isEmpty() ? QStringLiteral("Prop") : value;
}

QPointF propPositionAt(const MarchCraft::PropInstance &prop, int destinationSet, double progress)
{
    QVector<QPointF> path{prop.position};
    for (const auto &point : prop.motion.controlPoints) path.push_back(point);
    if (path.size() < 2 || prop.motion.endSet <= prop.motion.startSet) return prop.position;
    const double showPosition = qMax(0.0, destinationSet - 1 + qBound(0.0, progress, 1.0));
    const double t = qBound(0.0, (showPosition - prop.motion.startSet)
        / qMax(1, prop.motion.endSet - prop.motion.startSet), 1.0);
    QVector<double> cumulative{0.0}; double total = 0.0;
    for (int index = 1; index < path.size(); ++index) {
        total += pointDistance(path[index - 1], path[index]); cumulative.push_back(total);
    }
    if (total < 1e-6) return path.last();
    const double target = t * total; int segment = 1;
    while (segment < cumulative.size() - 1 && cumulative[segment] < target) ++segment;
    const double length = qMax(1e-9, cumulative[segment] - cumulative[segment - 1]);
    return path[segment - 1] + (path[segment] - path[segment - 1])
        * ((target - cumulative[segment - 1]) / length);
}

double distanceToPropFootprint(QPointF point, QPointF center, double rotation, QSizeF footprint)
{
    const QPointF local = rotateAround(point, center, -rotation) - center;
    const double dx = qMax(0.0, qAbs(local.x()) - footprint.width() / 2.0);
    const double dy = qMax(0.0, qAbs(local.y()) - footprint.height() / 2.0);
    return std::hypot(dx, dy);
}

}
