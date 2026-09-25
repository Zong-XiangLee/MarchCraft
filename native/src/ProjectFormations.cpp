#include "DrillProject.h"

#include <QFutureWatcher>
#include <QPolygonF>
#include <QSizeF>
#include <QSet>
#include <QUuid>
#include <QtConcurrent>
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

#include "ProjectAlgorithms.h"
#include "ProjectStorage.h"

using namespace MarchCraft::ProjectAlgorithms;
using namespace MarchCraft::ProjectStorage;

QVariantMap DrillProject::assignmentMetrics(const QVector<int> &rows, const QVector<QPointF> &targets,
                                             const QVector<int> &assignment) const
{
    if (rows.isEmpty() || rows.size() != assignment.size()) return {};
    QVector<QPointF> sources, destinations; sources.reserve(rows.size()); destinations.reserve(rows.size());
    const int sourceSet = m_formationPreviewSourceSet >= 0 ? m_formationPreviewSourceSet
        : (m_currentSet > 0 ? m_currentSet - 1 : m_currentSet);
    double total = 0.0, maximum = 0.0;
    for (int index = 0; index < rows.size(); ++index) {
        const QPointF source = placementAt(rows[index], sourceSet).position;
        const QPointF destination = targets.value(assignment[index]);
        sources.push_back(source); destinations.push_back(destination);
        const double distance = pointDistance(source, destination); total += distance; maximum = qMax(maximum, distance);
    }
    int crossings = 0;
    for (int a = 0; a < rows.size(); ++a) for (int b = a + 1; b < rows.size(); ++b)
        if (segmentsCross(sources[a], destinations[a], sources[b], destinations[b])) ++crossings;
    double minimumSpacing = std::numeric_limits<double>::max(), nearestTotal = 0.0;
    for (int a = 0; a < destinations.size(); ++a) {
        double nearest = std::numeric_limits<double>::max();
        for (int b = 0; b < destinations.size(); ++b) if (a != b) nearest = qMin(nearest, pointDistance(destinations[a], destinations[b]));
        if (destinations.size() == 1) nearest = 0.0;
        nearestTotal += nearest; minimumSpacing = qMin(minimumSpacing, nearest);
    }
    const int counts = m_formationPreviewInsertsNext ? 8
        : (m_currentSet > 0 ? qMax(1, m_sets[m_currentSet].counts) : 1);
    const int samples = qBound(8, counts * 2, 96); int collisions = 0;
    for (int sample = 0; sample <= samples; ++sample) {
        const double progress = double(sample) / samples;
        for (int a = 0; a < sources.size(); ++a) for (int b = a + 1; b < sources.size(); ++b) {
            const QPointF pa = sources[a] + (destinations[a] - sources[a]) * progress;
            const QPointF pb = sources[b] + (destinations[b] - sources[b]) * progress;
            if (pointDistance(pa, pb) < m_capability.collisionClearance) ++collisions;
        }
    }
    int changed = 0; for (int index = 0; index < assignment.size(); ++index) if (assignment[index] != index) ++changed;
    return {{QStringLiteral("averageMove"), total / rows.size()}, {QStringLiteral("maximumMove"), maximum},
            {QStringLiteral("maximumStepsPerCount"), maximum / counts}, {QStringLiteral("totalMove"), total},
            {QStringLiteral("crossings"), crossings}, {QStringLiteral("predictedCollisions"), collisions},
            {QStringLiteral("minimumSpacing"), minimumSpacing == std::numeric_limits<double>::max() ? 0.0 : minimumSpacing},
            {QStringLiteral("averageSpacing"), nearestTotal / rows.size()}, {QStringLiteral("changedAssignments"), changed},
            {QStringLiteral("performerCount"), rows.size()}};
}

QVector<int> DrillProject::assignedTargetIndices(const QVector<int> &rows, const QVector<QPointF> &targets,
                                                  bool closed, const QString &requestedMode) const
{
    const int count = rows.size(); QVector<int> roster(count); std::iota(roster.begin(), roster.end(), 0);
    if (count < 2 || targets.size() != count) return roster;
    // There is no incoming transition to optimize for the opening formation.
    // Keeping its stable selection order also makes large opening shapes cheap.
    if (m_currentSet == 0 && !m_formationPreviewInsertsNext) return roster;
    const auto mode = MarchCraft::assignmentModeFromName(requestedMode);
    const int sourceSet = m_formationPreviewSourceSet >= 0 ? m_formationPreviewSourceSet
        : (m_currentSet > 0 ? m_currentSet - 1 : m_currentSet);
    QVector<QPointF> sources; for (int row : rows) sources.push_back(placementAt(row, sourceSet).position);
    auto hungarianForPower = [&](double power, bool feature) {
        QVector<QVector<double>> costs(count, QVector<double>(count));
        const double reachable = qMax(1, m_currentSet > 0 ? m_sets[m_currentSet].counts : 8)
            * m_capability.maximumStepsPerCount;
        for (int i = 0; i < count; ++i) for (int j = 0; j < count; ++j) {
            const double distance = pointDistance(sources[i], targets[j]);
            double cost = std::pow(distance, power);
            if (feature) cost = distance <= reachable ? -cost : 1e9 + distance * 1000.0;
            costs[i][j] = cost + i * 1e-9 + j * 1e-12;
        }
        return minimumCostAssignment(costs);
    };
    auto evenEffort = [&] {
        QVector<QVector<double>> distances(count, QVector<double>(count));
        QVector<double> thresholds; thresholds.reserve(count * count);
        for (int source = 0; source < count; ++source) for (int target = 0; target < count; ++target) {
            distances[source][target] = pointDistance(sources[source], targets[target]);
            thresholds.push_back(distances[source][target]);
        }
        std::sort(thresholds.begin(), thresholds.end());
        thresholds.erase(std::unique(thresholds.begin(), thresholds.end()), thresholds.end());
        auto feasible = [&](double threshold) {
            QVector<int> owner(count, -1);
            std::function<bool(int,QVector<bool>&)> visit = [&](int source, QVector<bool> &seen) {
                for (int target = 0; target < count; ++target)
                    if (!seen[target] && distances[source][target] <= threshold + 1e-9) {
                        seen[target] = true;
                        if (owner[target] < 0 || visit(owner[target], seen)) { owner[target] = source; return true; }
                    }
                return false;
            };
            for (int source = 0; source < count; ++source) {
                QVector<bool> seen(count, false); if (!visit(source, seen)) return false;
            }
            return true;
        };
        int low = 0, high = qMax(0, thresholds.size() - 1);
        while (low < high) { const int middle = (low + high) / 2; if (feasible(thresholds[middle])) high = middle; else low = middle + 1; }
        const double bottleneck = thresholds.value(low);
        QVector<QVector<double>> costs(count, QVector<double>(count));
        for (int source = 0; source < count; ++source) for (int target = 0; target < count; ++target) {
            const double distance = distances[source][target];
            costs[source][target] = distance <= bottleneck + 1e-9
                ? distance * distance * 1000.0 + distance
                : 1e12 + distance;
        }
        return minimumCostAssignment(costs);
    };
    auto preserveOrder = [&] {
        QVector<int> sourceOrder = roster, targetOrder = roster, result(count, -1);
        QPointF sourceCenter, targetCenter; for (const auto &p : sources) sourceCenter += p; for (const auto &p : targets) targetCenter += p;
        sourceCenter /= count; targetCenter /= count;
        if (closed) {
            auto angleSource = [&](int i) { return std::atan2(sources[i].y() - sourceCenter.y(), sources[i].x() - sourceCenter.x()); };
            auto angleTarget = [&](int i) { return std::atan2(targets[i].y() - targetCenter.y(), targets[i].x() - targetCenter.x()); };
            std::stable_sort(sourceOrder.begin(), sourceOrder.end(), [&](int a, int b){ return angleSource(a) < angleSource(b); });
            std::stable_sort(targetOrder.begin(), targetOrder.end(), [&](int a, int b){ return angleTarget(a) < angleTarget(b); });
            double best = std::numeric_limits<double>::max(); int bestOffset = 0; bool reverse = false;
            for (int reversed = 0; reversed < 2; ++reversed) for (int offset = 0; offset < count; ++offset) {
                double score = 0.0; for (int n = 0; n < count; ++n) {
                    const int targetAt = reversed ? (offset - n + count * 2) % count : (offset + n) % count;
                    score += pointDistance(sources[sourceOrder[n]], targets[targetOrder[targetAt]]);
                }
                if (score < best) { best = score; bestOffset = offset; reverse = reversed; }
            }
            for (int n = 0; n < count; ++n) result[sourceOrder[n]] = targetOrder[reverse ? (bestOffset - n + count * 2) % count : (bestOffset + n) % count];
        } else {
            QPointF axis = targets.last() - targets.first();
            if (pointDistance({}, axis) < 0.001) axis = QPointF(1, 0);
            auto projection = [&](QPointF p, QPointF center){ return (p.x()-center.x())*axis.x() + (p.y()-center.y())*axis.y(); };
            std::stable_sort(sourceOrder.begin(), sourceOrder.end(), [&](int a,int b){return projection(sources[a],sourceCenter)<projection(sources[b],sourceCenter);});
            std::stable_sort(targetOrder.begin(), targetOrder.end(), [&](int a,int b){return projection(targets[a],targetCenter)<projection(targets[b],targetCenter);});
            double forward = 0, reverse = 0; for (int n=0;n<count;++n){forward+=pointDistance(sources[sourceOrder[n]],targets[targetOrder[n]]);reverse+=pointDistance(sources[sourceOrder[n]],targets[targetOrder[count-1-n]]);}
            for(int n=0;n<count;++n)result[sourceOrder[n]]=targetOrder[forward<=reverse?n:count-1-n];
        }
        return result;
    };
    if (mode == MarchCraft::FormationAssignmentMode::RosterOrder) return roster;
    if (mode == MarchCraft::FormationAssignmentMode::ShortestTotal) return hungarianForPower(1.0, false);
    if (mode == MarchCraft::FormationAssignmentMode::EvenEffort) return evenEffort();
    if (mode == MarchCraft::FormationAssignmentMode::FeatureMove) return hungarianForPower(1.0, true);
    if (mode == MarchCraft::FormationAssignmentMode::PreserveOrder) return preserveOrder();
    // Safe mode needs one globally optimal solution plus two inexpensive
    // structural alternatives. Even-effort remains available explicitly, but
    // running a second cubic solve here needlessly doubles preview latency.
    const QVector<QVector<int>> candidates{hungarianForPower(1.0, false), preserveOrder(), roster};
    double bestScore = std::numeric_limits<double>::max(); QVector<int> best = candidates.first();
    for (int candidateIndex = 0; candidateIndex < candidates.size(); ++candidateIndex) {
        const auto metrics = assignmentMetrics(rows, targets, candidates[candidateIndex]);
        const double over = qMax(0.0, metrics.value(QStringLiteral("maximumStepsPerCount")).toDouble() - m_capability.maximumStepsPerCount);
        const double score = metrics.value(QStringLiteral("predictedCollisions")).toInt() * 1e7
            + over * over * 1e6 + metrics.value(QStringLiteral("crossings")).toInt() * 1e4
            + metrics.value(QStringLiteral("maximumMove")).toDouble() * 100.0
            + metrics.value(QStringLiteral("totalMove")).toDouble()
            + (candidateIndex == 1 ? -0.01 : 0.0);
        if (score < bestScore) { bestScore = score; best = candidates[candidateIndex]; }
    }
    return best;
}

QVariantList DrillProject::formationPreviewPoints() const
{
    QVariantList result;
    for (const auto &performer : m_performers) if (m_formationPreview.placements.contains(performer.id)) {
        const QPointF from = m_formationPreview.sourcePlacements.value(performer.id);
        const QPointF to = m_formationPreview.placements.value(performer.id);
        result.push_back(QVariantMap{{QStringLiteral("id"), performer.id}, {QStringLiteral("label"), performer.label},
            {QStringLiteral("fromX"), from.x()}, {QStringLiteral("fromY"), from.y()},
            {QStringLiteral("x"), to.x()}, {QStringLiteral("y"), to.y()}});
    }
    return result;
}

QVariantMap DrillProject::formationDefaults(const QString &type, const QString &placementMode) const
{
    QVector<int> selected;
    for (int i = 0; i < m_performers.size(); ++i) if (m_performers[i].selected) selected.push_back(i);
    const int count = qMax(1, static_cast<int>(selected.size()));
    QPointF selectionCenter{fieldWidthSteps() / 2.0, fieldDepthSteps() / 2.0};
    if (!selected.isEmpty()) {
        QPointF first = placementAt(selected.first(), m_currentSet).position;
        double left = first.x(), right = first.x(), top = first.y(), bottom = first.y();
        for (int row : selected) { const QPointF p = placementAt(row, m_currentSet).position;
            left = qMin(left, p.x()); right = qMax(right, p.x()); top = qMin(top, p.y()); bottom = qMax(bottom, p.y()); }
        selectionCenter = {(left + right) / 2.0, (top + bottom) / 2.0};
    }
    const QString normalized = type.trimmed().toLower();
    double width = qMax(24.0, count * 4.0 / std::numbers::pi);
    double height = width;
    if (normalized == QStringLiteral("line")) { width = qMax(12.0, (count - 1) * 4.0); height = 0.0; }
    else if (normalized == QStringLiteral("ellipse")) { width *= 1.35; height *= 0.72; }
    else if (normalized == QStringLiteral("rectangle") || normalized == QStringLiteral("block")) {
        width = qMax(12.0, std::ceil(std::sqrt(count)) * 4.0);
        height = qMax(8.0, std::ceil(double(count) / qMax(1.0, std::ceil(std::sqrt(count)))) * 4.0);
    }
    QPointF center = selectionCenter;
    const QString mode = placementMode.isEmpty() ? m_shapePlacementMode : placementMode;
    if (mode == QStringLiteral("fieldCenter")) center = {fieldWidthSteps() / 2.0, fieldDepthSteps() / 2.0};
    else if (mode == QStringLiteral("openSpace")) {
        double bestScore = -1e12;
        const double footprint = qMax(width, height) / 2.0;
        for (double y = 4.0; y <= fieldDepthSteps() - 4.0; y += 4.0) {
            for (double x = 4.0; x <= fieldWidthSteps() - 4.0; x += 4.0) {
                const QPointF candidate{x, y}; double clearance = 100.0;
                for (int row = 0; row < m_performers.size(); ++row) {
                    if (m_performers[row].selected || !m_performers[row].visible) continue;
                    const QPointF occupied = placementAt(row, m_currentSet).position;
                    clearance = qMin(clearance, std::hypot(candidate.x() - occupied.x(), candidate.y() - occupied.y()) - footprint);
                }
                const double edge = qMin(qMin(x, fieldWidthSteps() - x), qMin(y, fieldDepthSteps() - y)) - footprint;
                const double displacement = std::hypot(x - selectionCenter.x(), y - selectionCenter.y());
                const double score = clearance * 10.0 + edge * 0.5 - displacement * 0.15;
                if (score > bestScore) { bestScore = score; center = candidate; }
            }
        }
    }
    return {{QStringLiteral("centerX"), center.x()}, {QStringLiteral("centerY"), center.y()},
            {QStringLiteral("width"), width}, {QStringLiteral("height"), height},
            {QStringLiteral("radius"), width / 2.0}, {QStringLiteral("spacing"), 4.0},
            {QStringLiteral("rotation"), 0.0}, {QStringLiteral("startAngle"), 0.0},
            {QStringLiteral("sweepAngle"), normalized == QStringLiteral("arc") ? 180.0 : 360.0},
            {QStringLiteral("sides"), 6}, {QStringLiteral("points"), 5},
            {QStringLiteral("turns"), 1.5}, {QStringLiteral("spiralStyle"), QStringLiteral("drill")}, {QStringLiteral("innerRadius"), 2.0},
            {QStringLiteral("outerRadius"), width / 2.0},
            {QStringLiteral("rows"), qMax(1, qRound(std::sqrt(count)))},
            {QStringLiteral("placementMode"), mode}};
}

QVariantMap DrillProject::formationEstimate(const QString &type, const QVariantMap &options) const
{
    const int count=qMax(1,selectedCount());QVariantMap values=formationDefaults(type,options.value(QStringLiteral("placementMode")).toString());
    for(auto it=options.cbegin();it!=options.cend();++it)values.insert(it.key(),it.value());
    const QString kind=type.trimmed().toLower();const double width=qMax(0.0,values.value(QStringLiteral("width")).toDouble());
    const double height=qMax(0.0,values.value(QStringLiteral("height")).toDouble());const double radius=qMax(0.0,values.value(QStringLiteral("radius"),width/2.0).toDouble());
    bool closed=false;double length=width;
    if(kind==QStringLiteral("circle")){closed=true;length=2.0*std::numbers::pi*radius;}
    else if(kind==QStringLiteral("arc")){const double sweep=qAbs(values.value(QStringLiteral("sweepAngle"),180).toDouble());closed=sweep>=359.999;length=2.0*std::numbers::pi*radius*sweep/360.0;}
    else if(kind==QStringLiteral("ellipse")){closed=true;const double a=width/2.0,b=height/2.0;length=std::numbers::pi*(3*(a+b)-std::sqrt(qMax(0.0,(3*a+b)*(a+3*b))));}
    else if(kind==QStringLiteral("rectangle")){closed=true;length=2*(width+height);}
    else if(kind==QStringLiteral("triangle")){closed=true;length=3*width*std::sqrt(3.0)/2.0;}
    else if(kind==QStringLiteral("diamond")||kind==QStringLiteral("polygon")||kind==QStringLiteral("star")){closed=true;length=qMax(width*2.5,count*4.0);}
    else if(kind==QStringLiteral("spiral")){const double turns=values.value(QStringLiteral("turns"),1.5).toDouble();const double outer=values.value(QStringLiteral("outerRadius"),width/2.0).toDouble();length=qMax(width,turns*std::numbers::pi*outer);}
    else if(kind==QStringLiteral("block")){length=qMax(0,count-1)*values.value(QStringLiteral("spacing"),4).toDouble();}
    const double spacing=count<=1?0.0:length/(closed?count:qMax(1,count-1));
    const auto metrics=selectionMetrics();
    return {{QStringLiteral("estimatedSpacing"),spacing},{QStringLiteral("estimatedPathLength"),length},
            {QStringLiteral("currentAverageMove"),metrics.value(QStringLiteral("averageMove"))},
            {QStringLiteral("currentAverageSpacing"),metrics.value(QStringLiteral("averageSpacing"))}};
}

QVariantMap DrillProject::formationGeometry(const QString &type, const QVariantMap &options) const
{
    QVector<int> selected;
    for (int i = 0; i < m_performers.size(); ++i) if (m_performers[i].selected) selected.push_back(i);
    if (selected.isEmpty() || m_currentSet < 0) return {};
    const QString kind = type.trimmed().toLower();
    QVariantMap values = formationDefaults(kind, options.value(QStringLiteral("placementMode")).toString());
    for (auto it = options.cbegin(); it != options.cend(); ++it) values.insert(it.key(), it.value());
    QPointF center{values.value(QStringLiteral("centerX")).toDouble(), values.value(QStringLiteral("centerY")).toDouble()};
    const double width = qMax(2.0, values.value(QStringLiteral("width")).toDouble());
    const double height = qMax(2.0, values.value(QStringLiteral("height")).toDouble());
    const double radius = qMax(1.0, values.value(QStringLiteral("radius"), width / 2.0).toDouble());
    const double rotation = values.value(QStringLiteral("rotation")).toDouble();
    QVector<QPointF> path, placements; bool closed = false;
    auto addRegularPolygon = [&](int sides, double outer, double inner = -1.0) {
        const int vertices = inner > 0.0 ? sides * 2 : sides;
        for (int i = 0; i <= vertices; ++i) {
            const double r = inner > 0.0 && i % 2 ? inner : outer;
            const double phase = kind == QStringLiteral("triangle") ? std::numbers::pi / 2.0 : -std::numbers::pi / 2.0;
            const double angle = phase + 2.0 * std::numbers::pi * i / vertices;
            path.push_back(rotateAround(center + QPointF(std::cos(angle) * r, std::sin(angle) * r), center, rotation));
        }
        closed = true;
    };
    if (kind == QStringLiteral("line")) {
        path = {rotateAround(center + QPointF(-width / 2.0, 0), center, rotation),
                rotateAround(center + QPointF(width / 2.0, 0), center, rotation)};
    } else if (kind == QStringLiteral("circle") || kind == QStringLiteral("ellipse")) {
        const double rx = kind == QStringLiteral("circle") ? radius : width / 2.0;
        const double ry = kind == QStringLiteral("circle") ? radius : height / 2.0;
        for (int i = 0; i <= 256; ++i) {
            const double angle = 2.0 * std::numbers::pi * i / 256.0;
            path.push_back(rotateAround(center + QPointF(std::cos(angle) * rx, std::sin(angle) * ry), center, rotation));
        }
        closed = true;
    } else if (kind == QStringLiteral("arc")) {
        const double start = values.value(QStringLiteral("startAngle")).toDouble();
        const double sweep = values.value(QStringLiteral("sweepAngle"), 180.0).toDouble();
        closed = std::abs(sweep) >= 359.999;
        for (int i = 0; i <= 256; ++i) {
            const double angle = degreesToRadians(start + sweep * i / 256.0);
            path.push_back(center + QPointF(std::cos(angle) * radius, std::sin(angle) * radius));
        }
    } else if (kind == QStringLiteral("rectangle")) {
        path = {{center.x() - width / 2, center.y() - height / 2}, {center.x() + width / 2, center.y() - height / 2},
                {center.x() + width / 2, center.y() + height / 2}, {center.x() - width / 2, center.y() + height / 2},
                {center.x() - width / 2, center.y() - height / 2}};
        for (auto &p : path) p = rotateAround(p, center, rotation); closed = true;
    } else if (kind == QStringLiteral("triangle")) addRegularPolygon(3, width / 2.0);
    else if (kind == QStringLiteral("diamond")) addRegularPolygon(4, width / 2.0);
    else if (kind == QStringLiteral("polygon")) addRegularPolygon(qBound(3, values.value(QStringLiteral("sides"), 6).toInt(), 12), width / 2.0);
    else if (kind == QStringLiteral("star")) {
        const int points = qBound(3, values.value(QStringLiteral("points"), 5).toInt(), 12);
        addRegularPolygon(points, width / 2.0, width / 4.0);
    } else if (kind == QStringLiteral("spiral")) {
        const double requestedTurns = qBound(0.5, values.value(QStringLiteral("turns"), 1.5).toDouble(), 30.0);
        const QString spiralStyle = values.value(QStringLiteral("spiralStyle"), QStringLiteral("drill")).toString();
        double outer = qMax(2.0, values.value(QStringLiteral("outerRadius"), width / 2.0).toDouble());
        // Preserve at least roughly four steps of radial separation per turn for
        // galaxy arms. The formation fitter then scales the entire curve once,
        // instead of letting later loops collapse into a flame-like core.
        double turns = requestedTurns;
        if (spiralStyle == QStringLiteral("galaxy")) {
            // At a fixed radius, each additional loop consumes radial clearance.
            // Cap only the rendered loop count, rather than allowing dense loops to
            // visually collapse into a flame. Two steps is the practical minimum.
            turns = qMin(requestedTurns, qMax(0.5, (outer - qMax(1.5, outer * 0.16)) / 2.0));
            outer = qMax(outer, turns * 5.5);
        }
        const double phi = (1.0 + std::sqrt(5.0)) / 2.0;
        const double inner = spiralStyle == QStringLiteral("golden") ? outer / std::pow(phi, turns * 4.0)
            : spiralStyle == QStringLiteral("galaxy") ? qMax(1.5, outer * 0.16) : qMax(0.35, outer * 0.055);
        const double direction = values.value(QStringLiteral("clockwise"), false).toBool() ? -1.0 : 1.0;
        const int samples = qBound(64, qCeil(turns * qMax(outer, 8.0) * 1.5), 384);
        for (int i = 0; i <= samples; ++i) {
            const double t = double(i) / samples;
            double r = inner + (outer - inner) * t;
            double angle = degreesToRadians(rotation) + direction * turns * 2.0 * std::numbers::pi * t;
            if (spiralStyle == QStringLiteral("golden")) {
                r = inner * std::pow(phi, turns * 4.0 * t);
            } else if (spiralStyle == QStringLiteral("galaxy")) {
                // An evenly spaced Archimedean arm with a broad core. Unlike the
                // former eased-radius curve, additional loops never bunch together.
                r = inner + (outer - inner) * t;
                angle += direction * 0.18 * t * t;
            }
            path.push_back(center + QPointF(std::cos(angle) * r, std::sin(angle) * r));
        }
        if (spiralStyle == QStringLiteral("galaxy")) {
            // Galaxy formations are two opposed arms, not a disguised single-arm
            // drill spiral. Performer order alternates arms so each remains balanced.
            placements.reserve(selected.size());
            const int armCount = 2;
            const int perArm = qMax(1, qCeil(double(selected.size()) / armCount));
            for (int performer = 0; performer < selected.size(); ++performer) {
                const int arm = performer % armCount;
                const int indexOnArm = performer / armCount;
                const double t = perArm <= 1 ? 0.5 : double(indexOnArm) / (perArm - 1);
                const double r = inner + (outer - inner) * t;
                const double angle = degreesToRadians(rotation) + direction * turns * 2.0 * std::numbers::pi * t
                    + arm * std::numbers::pi + direction * 0.18 * t * t;
                placements.push_back(center + QPointF(std::cos(angle) * r, std::sin(angle) * r));
            }
        }
    } else if (kind == QStringLiteral("block") || kind == QStringLiteral("block grid")) {
        const int rows = qBound(1, values.value(QStringLiteral("rows"), 1).toInt(), selected.size());
        const int columns = qMax(1, qCeil(double(selected.size()) / rows));
        const double spacing = qMax(1.0, values.value(QStringLiteral("spacing"), 4.0).toDouble());
        for (int n = 0; n < selected.size(); ++n) {
            const int row = n / columns, columnInRow = n % columns;
            const int column = row % 2 ? columns - 1 - columnInRow : columnInRow;
            placements.push_back(rotateAround(center + QPointF((column - (columns - 1) / 2.0) * spacing,
                                                                 (row - (rows - 1) / 2.0) * spacing), center, rotation));
        }
        path = placements;
    } else return {};
    fitPathToField(path, center, canvasMinX(), canvasMaxX(), canvasMinY(), canvasMaxY());
    if (placements.isEmpty()) {
        placements = kind == QStringLiteral("rectangle")
            ? rectanglePerimeterPoints(path, selected.size()) : QVector<QPointF>{};
        if (placements.isEmpty()) placements = equalDistancePoints(path, selected.size(), closed);
    }
    else {
        QPointF ignored = values.contains(QStringLiteral("centerX")) ? QPointF(values.value(QStringLiteral("centerX")).toDouble(), values.value(QStringLiteral("centerY")).toDouble()) : center;
        fitPathToField(placements, ignored, canvasMinX(), canvasMaxX(), canvasMinY(), canvasMaxY()); path = placements; center = ignored;
    }
    if (placements.size() != selected.size()) return {};
    QVariantList pathValues, destinationValues;
    for (const auto &point : path) pathValues.push_back(point);
    for (const auto &point : placements) destinationValues.push_back(point);
    return {{QStringLiteral("path"), pathValues}, {QStringLiteral("placements"), destinationValues},
            {QStringLiteral("center"), center}, {QStringLiteral("closed"), closed},
            {QStringLiteral("options"), values}};
}

void DrillProject::createFormation(const QString &type, const QVariantMap &options)
{
    if (!m_generatingFormationPreview) {
        const QString mode = options.value(QStringLiteral("assignmentMode"), QStringLiteral("rehearsalSafe")).toString();
        previewFormation(type, options, mode); commitFormationPreview(); return;
    }
    const auto geometry = formationGeometry(type, options);
    if (geometry.isEmpty()) return;
    QVector<int> selected;
    for (int i = 0; i < m_performers.size(); ++i) if (m_performers[i].selected) selected.push_back(i);
    const QString kind = type.trimmed().toLower();
    QVariantMap values = geometry.value(QStringLiteral("options")).toMap();
    const QPointF center = geometry.value(QStringLiteral("center")).toPointF();
    const bool closed = geometry.value(QStringLiteral("closed")).toBool();
    const double width = qMax(2.0, values.value(QStringLiteral("width")).toDouble());
    const double height = qMax(2.0, values.value(QStringLiteral("height")).toDouble());
    const double rotation = values.value(QStringLiteral("rotation")).toDouble();
    QVector<QPointF> path, placements;
    for (const auto &point : geometry.value(QStringLiteral("path")).toList()) path.push_back(point.toPointF());
    for (const auto &point : geometry.value(QStringLiteral("placements")).toList()) placements.push_back(point.toPointF());
    const QString assignmentMode = values.value(QStringLiteral("assignmentMode"), QStringLiteral("rehearsalSafe")).toString();
    const QVector<int> assignment = assignedTargetIndices(selected, placements, closed, assignmentMode);
    MarchCraft::FormationShape shape; shape.type = kind; shape.points = path; shape.anchor = center;
    shape.width = width; shape.height = height; shape.rotation = rotation; shape.closed = closed;
    values.insert(QStringLiteral("assignmentMode"), MarchCraft::assignmentModeName(MarchCraft::assignmentModeFromName(assignmentMode)));
    shape.parameters = QJsonObject::fromVariantMap(values);
    QVector<QString> orderedIds(selected.size());
    for (int n = 0; n < selected.size(); ++n) {
        const QString id = m_performers[selected[n]].id; orderedIds[assignment[n]] = id;
        m_formationPreview.placements.insert(id, placements[assignment[n]]);
        const int sourceSet = m_currentSet > 0 ? m_currentSet - 1 : m_currentSet;
        m_formationPreview.sourcePlacements.insert(id, placementAt(selected[n], sourceSet).position);
    }
    shape.performerIds = orderedIds;
    m_formationPreview.active = true; m_formationPreview.type = kind;
    m_formationPreview.mode = MarchCraft::assignmentModeName(MarchCraft::assignmentModeFromName(assignmentMode));
    m_formationPreview.options = values; m_formationPreview.shape = std::move(shape);
    m_formationPreview.metrics = assignmentMetrics(selected, placements, assignment);
    emit formationPreviewChanged();
}

QVariantMap DrillProject::previewFormation(const QString &type, const QVariantMap &options, const QString &assignmentMode)
{
    cancelFormationPreview(); QVariantMap values = options;
    values.insert(QStringLiteral("assignmentMode"), assignmentMode);
    m_formationPreviewInsertsNext = options.value(QStringLiteral("insertAsNextSet"), false).toBool();
    m_formationPreviewSourceSet = m_formationPreviewInsertsNext
        ? m_currentSet : (m_currentSet > 0 ? m_currentSet - 1 : m_currentSet);
    m_generatingFormationPreview = true; createFormation(type, values); m_generatingFormationPreview = false;
    if (m_formationPreview.active)
        setStatus(QStringLiteral("Previewing %1 assignment (%2 performers)").arg(m_formationPreview.mode).arg(m_formationPreview.placements.size()));
    return m_formationPreview.metrics;
}

void DrillProject::requestFormationPreview(const QString &type, const QVariantMap &options,
                                           const QString &assignmentMode)
{
    cancelFormationPreview();
    const quint64 generation = ++m_formationPreviewGeneration;
    m_formationPreviewBusy = true; emit formationPreviewChanged();
    setStatus(QStringLiteral("Optimizing %1 assignment...").arg(assignmentMode));
    const QJsonObject snapshot = toJson();
    const int currentSet = m_currentSet;
    QStringList selectedIds;
    for (const auto &performer : m_performers) if (performer.selected) selectedIds.push_back(performer.id);
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher, generation] {
        const QVariantMap result = watcher->result(); watcher->deleteLater();
        if (generation != m_formationPreviewGeneration) return;
        m_formationPreviewBusy = false;
        m_formationPreview = {};
        m_formationPreview.active = result.value(QStringLiteral("active")).toBool();
        m_formationPreview.type = result.value(QStringLiteral("type")).toString();
        m_formationPreview.mode = result.value(QStringLiteral("mode")).toString();
        m_formationPreview.options = result.value(QStringLiteral("options")).toMap();
        m_formationPreview.metrics = result.value(QStringLiteral("metrics")).toMap();
        m_formationPreview.shape = MarchCraft::FormationShape::fromJson(
            QJsonObject::fromVariantMap(result.value(QStringLiteral("shape")).toMap()));
        for (const auto &value : result.value(QStringLiteral("points")).toList()) {
            const auto point = value.toMap(); const QString id = point.value(QStringLiteral("id")).toString();
            m_formationPreview.sourcePlacements.insert(id, QPointF(point.value(QStringLiteral("fromX")).toDouble(), point.value(QStringLiteral("fromY")).toDouble()));
            m_formationPreview.placements.insert(id, QPointF(point.value(QStringLiteral("x")).toDouble(), point.value(QStringLiteral("y")).toDouble()));
        }
        m_formationPreviewSourceSet = result.value(QStringLiteral("sourceSet"), -1).toInt();
        m_formationPreviewInsertsNext = m_formationPreview.options.value(QStringLiteral("insertAsNextSet"), false).toBool();
        emit formationPreviewChanged();
        setStatus(m_formationPreview.active
            ? QStringLiteral("Preview ready: %1 assignment").arg(m_formationPreview.mode)
            : QStringLiteral("Unable to create formation preview"));
    });
    watcher->setFuture(QtConcurrent::run([snapshot, currentSet, selectedIds, type, options, assignmentMode] {
        DrillProject clone(true, nullptr); clone.restoreJson(snapshot); clone.m_currentSet = currentSet;
        const QSet<QString> selected(selectedIds.begin(), selectedIds.end());
        for (auto &performer : clone.m_performers) performer.selected = selected.contains(performer.id);
        clone.previewFormation(type, options, assignmentMode);
        QVariantList points;
        for (auto it = clone.m_formationPreview.placements.cbegin(); it != clone.m_formationPreview.placements.cend(); ++it) {
            const QPointF source = clone.m_formationPreview.sourcePlacements.value(it.key());
            points.push_back(QVariantMap{{QStringLiteral("id"), it.key()},
                {QStringLiteral("fromX"), source.x()}, {QStringLiteral("fromY"), source.y()},
                {QStringLiteral("x"), it.value().x()}, {QStringLiteral("y"), it.value().y()}});
        }
        return QVariantMap{{QStringLiteral("active"), clone.m_formationPreview.active},
            {QStringLiteral("type"), clone.m_formationPreview.type}, {QStringLiteral("mode"), clone.m_formationPreview.mode},
            {QStringLiteral("options"), clone.m_formationPreview.options}, {QStringLiteral("metrics"), clone.m_formationPreview.metrics},
            {QStringLiteral("shape"), clone.m_formationPreview.shape.toJson().toVariantMap()},
            {QStringLiteral("sourceSet"), clone.m_formationPreviewSourceSet}, {QStringLiteral("points"), points}};
    }));
}

void DrillProject::requestFreehandPreview(const QVariantList &points, const QString &movementMode,
                                          bool createGroup, const QString &recognitionMode)
{
    cancelFormationPreview();
    const quint64 generation = ++m_formationPreviewGeneration;
    m_formationPreviewBusy = true; emit formationPreviewChanged();
    setStatus(QStringLiteral("Optimizing freehand assignment..."));
    const QJsonObject snapshot = toJson(); const int currentSet = m_currentSet;
    QStringList selectedIds; for (const auto &performer : m_performers) if (performer.selected) selectedIds.push_back(performer.id);
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, generation] {
        const QVariantMap result = watcher->result(); watcher->deleteLater();
        if (generation != m_formationPreviewGeneration) return;
        m_formationPreviewBusy = false; m_formationPreview = {};
        m_formationPreview.active = result.value(QStringLiteral("active")).toBool();
        m_formationPreview.type = result.value(QStringLiteral("type")).toString();
        m_formationPreview.mode = result.value(QStringLiteral("mode")).toString();
        m_formationPreview.options = result.value(QStringLiteral("options")).toMap();
        m_formationPreview.metrics = result.value(QStringLiteral("metrics")).toMap();
        m_formationPreview.shape = MarchCraft::FormationShape::fromJson(QJsonObject::fromVariantMap(result.value(QStringLiteral("shape")).toMap()));
        for (const auto &value : result.value(QStringLiteral("points")).toList()) {
            const auto point = value.toMap(); const QString id = point.value(QStringLiteral("id")).toString();
            m_formationPreview.sourcePlacements.insert(id, QPointF(point.value(QStringLiteral("fromX")).toDouble(), point.value(QStringLiteral("fromY")).toDouble()));
            m_formationPreview.placements.insert(id, QPointF(point.value(QStringLiteral("x")).toDouble(), point.value(QStringLiteral("y")).toDouble()));
        }
        m_formationPreviewSourceSet = result.value(QStringLiteral("sourceSet"), -1).toInt();
        emit formationPreviewChanged(); setStatus(QStringLiteral("Freehand preview ready"));
    });
    watcher->setFuture(QtConcurrent::run([snapshot, currentSet, selectedIds, points, movementMode, createGroup, recognitionMode] {
        DrillProject clone(true, nullptr); clone.restoreJson(snapshot); clone.m_currentSet = currentSet;
        const QSet<QString> selected(selectedIds.begin(), selectedIds.end());
        for (auto &performer : clone.m_performers) performer.selected = selected.contains(performer.id);
        const int sourceSet = currentSet > 0 ? currentSet - 1 : currentSet;
        QHash<QString,QPointF> sources; for (int row = 0; row < clone.m_performers.size(); ++row)
            if (clone.m_performers[row].selected) sources.insert(clone.m_performers[row].id, clone.placementAt(row, sourceSet).position);
        clone.createFreehandFormation(points, movementMode, createGroup, recognitionMode);
        if (clone.m_sets[currentSet].activeVariant().shapes.isEmpty()) return QVariantMap{};
        const auto shape = clone.m_sets[currentSet].activeVariant().shapes.last(); QVariantList proposed;
        for (int row = 0; row < clone.m_performers.size(); ++row) if (clone.m_performers[row].selected) {
            const QString id = clone.m_performers[row].id; const QPointF target = clone.placementAt(row, currentSet).position;
            proposed.push_back(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("fromX"), sources.value(id).x()},
                {QStringLiteral("fromY"), sources.value(id).y()}, {QStringLiteral("x"), target.x()}, {QStringLiteral("y"), target.y()}});
        }
        QVariantMap metrics = clone.selectionMetrics();
        metrics.insert(QStringLiteral("maximumStepsPerCount"), metrics.value(QStringLiteral("maximumMove")).toDouble()
            / qMax(1, currentSet > 0 ? clone.m_sets[currentSet].counts : 1));
        metrics.insert(QStringLiteral("crossings"), 0); metrics.insert(QStringLiteral("predictedCollisions"), metrics.value(QStringLiteral("collisionCount")));
        return QVariantMap{{QStringLiteral("active"), true}, {QStringLiteral("type"), shape.type},
            {QStringLiteral("mode"), MarchCraft::assignmentModeName(MarchCraft::assignmentModeFromName(movementMode))},
            {QStringLiteral("options"), QVariantMap{{QStringLiteral("createGroup"), createGroup}}},
            {QStringLiteral("metrics"), metrics}, {QStringLiteral("shape"), shape.toJson().toVariantMap()},
            {QStringLiteral("sourceSet"), sourceSet}, {QStringLiteral("points"), proposed}};
    }));
}

bool DrillProject::commitFormationPreview()
{
    if (!m_formationPreview.active || m_currentSet < 0) return false;
    const auto before = toJson();
    if (m_formationPreview.options.value(QStringLiteral("insertAsNextSet"), false).toBool()) {
        DrillSet next;
        next.number = QString::number(m_currentSet + 2);
        next.counts = 8;
        next.startTick = advancePulses(m_sets[m_currentSet].startTick, next.counts);
        next.activeVariant().name = QStringLiteral("Active");
        for (int row = 0; row < m_performers.size(); ++row)
            next.activeVariant().placements.insert(m_performers[row].id, placementAt(row, m_currentSet));
        const qint64 insertedDuration = next.startTick - m_sets[m_currentSet].startTick;
        for (int set = m_currentSet + 1; set < m_sets.size(); ++set)
            m_sets[set].startTick += insertedDuration;
        m_sets.insert(m_currentSet + 1, next);
        ++m_currentSet;
        emit setsChanged(); emit currentSetChanged();
    }
    auto &variant = m_sets[m_currentSet].activeVariant();
    MarchCraft::FormationShape shape = m_formationPreview.shape;
    QSet<QString> selectedIds; for (auto it = m_formationPreview.placements.cbegin(); it != m_formationPreview.placements.cend(); ++it) selectedIds.insert(it.key());
    QSet<QString> replacedGroupIds;
    if (!shape.type.isEmpty()) {
        variant.shapes.erase(std::remove_if(variant.shapes.begin(), variant.shapes.end(), [&](const auto &oldShape) {
            bool overlaps = false; for (const auto &id : oldShape.performerIds) if (selectedIds.contains(id)) { overlaps = true; break; }
            if (overlaps && !oldShape.groupId.isEmpty()) replacedGroupIds.insert(oldShape.groupId); return overlaps;
        }), variant.shapes.end());
        variant.groups.erase(std::remove_if(variant.groups.begin(), variant.groups.end(), [&](const auto &group) {
            return replacedGroupIds.contains(group.id); }), variant.groups.end());
    }
    for (auto it = m_formationPreview.placements.cbegin(); it != m_formationPreview.placements.cend(); ++it)
        variant.placements[it.key()].position = it.value();
    if (m_formationPreview.options.value(QStringLiteral("createGroup"), false).toBool() && shape.performerIds.size() >= 2) {
        for (auto &group : variant.groups)
            group.performerIds.erase(std::remove_if(group.performerIds.begin(), group.performerIds.end(), [&](const QString &id) { return selectedIds.contains(id); }), group.performerIds.end());
        variant.groups.erase(std::remove_if(variant.groups.begin(), variant.groups.end(), [](const auto &group) { return group.performerIds.size() < 2; }), variant.groups.end());
        MarchCraft::PerformerGroup group; group.performerIds = shape.performerIds;
        group.name = QStringLiteral("%1 formation").arg(shape.type); shape.groupId = group.id; variant.groups.push_back(group);
    }
    if (!shape.type.isEmpty()) variant.shapes.push_back(shape);
    const QString kind = shape.type.isEmpty() ? QStringLiteral("optimized") : shape.type, mode = m_formationPreview.mode;
    m_formationPreview = {}; m_formationPreviewSourceSet = -1; m_formationPreviewInsertsNext = false;
    emit formationPreviewChanged(); emit shapesChanged(); emitAllDataChanged();
    commitSnapshot(before, QStringLiteral("Create %1 formation (%2)").arg(kind, mode));
    analyzeTransition(m_currentSet); return true;
}

void DrillProject::cancelFormationPreview()
{
    ++m_formationPreviewGeneration;
    const bool changed = m_formationPreview.active || !m_formationPreview.placements.isEmpty() || m_formationPreviewBusy;
    m_formationPreview = {}; m_formationPreviewSourceSet = -1; m_formationPreviewInsertsNext = false;
    m_formationPreviewBusy = false;
    if (changed) emit formationPreviewChanged();
}

void DrillProject::createFreehandFormation(const QVariantList &values, const QString &movementMode,
                                           bool createGroup, const QString &recognitionMode)
{
    if(m_currentSet<0||selectedCount()<1||values.size()<2)return;
    QVector<QPointF> path;path.reserve(values.size());
    for(const auto&value:values){const QPointF point=clampPosition(value.toPointF());if(path.isEmpty()||std::hypot(point.x()-path.last().x(),point.y()-path.last().y())>=0.05)path.push_back(point);}
    if(path.size()<2)return;
    double total=polylineLength(path);
    // A long, complex stroke must not become a closed loop merely because its
    // endpoints happen to be several steps apart. Use a local-sized closure
    // tolerance, then normalize the input so mouse event frequency cannot
    // change recognition or smoothing.
    const double closureTolerance=qMin(2.0,qMax(0.65,total*0.025));
    bool closed=path.size()>=4&&std::hypot(path.first().x()-path.last().x(),path.first().y()-path.last().y())<=closureTolerance;
    path=uniformlySampledPath(path,closed,0.22);
    total=polylineLength(path,closed);
    QString recognized=QStringLiteral("freehand");
    if(recognitionMode==QStringLiteral("auto")||recognitionMode==QStringLiteral("straighten")){
        const double direct=std::hypot(path.first().x()-path.last().x(),path.first().y()-path.last().y());
        if(!closed&&total>0&&direct/total>0.965){path={path.first(),path.last()};recognized=QStringLiteral("line");}
        else if(closed&&recognitionMode==QStringLiteral("auto")){
            QPointF center;const int uniqueCount=path.first()==path.last()?path.size()-1:path.size();for(int i=0;i<uniqueCount;++i)center+=path[i];center/=uniqueCount;double mean=0.0;for(int i=0;i<uniqueCount;++i)mean+=std::hypot(path[i].x()-center.x(),path[i].y()-center.y());mean/=uniqueCount;double variance=0.0;for(int i=0;i<uniqueCount;++i){const double r=std::hypot(path[i].x()-center.x(),path[i].y()-center.y());variance+=(r-mean)*(r-mean);}variance/=uniqueCount;
            if(mean>1&&std::sqrt(variance)/mean<0.18){path.clear();for(int i=0;i<=128;++i){const double a=2*std::numbers::pi*i/128.0;path.push_back(center+QPointF(std::cos(a)*mean,std::sin(a)*mean));}recognized=QStringLiteral("circle");}
        }
    }
    if(recognized==QStringLiteral("freehand")&&recognitionMode!=QStringLiteral("preserve")){
        path=smoothedStroke(path,closed,recognitionMode==QStringLiteral("smooth")?3:2);
        path=uniformlySampledPath(path,closed,0.18);
    }
    QVector<int> selected;for(int i=0;i<m_performers.size();++i)if(m_performers[i].selected)selected.push_back(i);
    QVector<QPointF> targets=equalDistancePoints(path,selected.size(),closed);if(targets.size()!=selected.size())return;
    if(selected.size()>1){double minimum=1e9;for(int i=0;i<targets.size();++i)for(int j=i+1;j<targets.size();++j)minimum=qMin(minimum,std::hypot(targets[i].x()-targets[j].x(),targets[i].y()-targets[j].y()));if(minimum<1.5&&minimum>0){QPointF center;for(const auto&p:path)center+=p;center/=path.size();const double scale=qMin(4.0,1.5/minimum);for(auto&p:path)p=center+(p-center)*scale;fitPathToField(path,center,canvasMinX(),canvasMaxX(),canvasMinY(),canvasMaxY());targets=equalDistancePoints(path,selected.size(),closed);}}
    // Avoid unselected performers by translating the complete drawing to the
    // clearest nearby location. This retains the hand-drawn geometry.
    auto clearanceAt=[&](const QPointF &offset){double clearance=1000;for(const auto&t:targets){const QPointF candidate=t+offset;if(candidate.x()<canvasMinX()||candidate.x()>canvasMaxX()||candidate.y()<canvasMinY()||candidate.y()>canvasMaxY())return -1.0;for(int row=0;row<m_performers.size();++row)if(!m_performers[row].selected&&m_performers[row].visible){const QPointF p=placementAt(row,m_currentSet).position;clearance=qMin(clearance,std::hypot(candidate.x()-p.x(),candidate.y()-p.y()));}}return clearance;};
    QPointF bestOffset;const double originalClearance=clearanceAt({});double bestScore=originalClearance;
    // Keep accurate strokes exactly where they were drawn. Only search for a
    // nearby translation when the original destinations actually collide.
    if(originalClearance>=0.0&&originalClearance<1.5)for(double dy=-6;dy<=6;dy+=1)for(double dx=-6;dx<=6;dx+=1){const QPointF offset{dx,dy};const double clearance=clearanceAt(offset);if(clearance<0)continue;const double score=qMin(clearance,6.0)-0.35*std::hypot(dx,dy);if(score>bestScore+0.05){bestScore=score;bestOffset=offset;}}
    for(auto&p:path)p+=bestOffset;for(auto&p:targets)p+=bestOffset;
    const QVector<int> assignment = assignedTargetIndices(selected, targets, closed, movementMode);
    const auto before=toJson();auto&variant=m_sets[m_currentSet].activeVariant();QSet<QString> selectedIds;for(int row:selected)selectedIds.insert(m_performers[row].id);QSet<QString> replacedGroups;
    variant.shapes.erase(std::remove_if(variant.shapes.begin(),variant.shapes.end(),[&](const auto&shape){bool overlap=false;for(const auto&id:shape.performerIds)if(selectedIds.contains(id)){overlap=true;break;}if(overlap&&!shape.groupId.isEmpty())replacedGroups.insert(shape.groupId);return overlap;}),variant.shapes.end());
    variant.groups.erase(std::remove_if(variant.groups.begin(),variant.groups.end(),[&](const auto&group){return replacedGroups.contains(group.id);}),variant.groups.end());
    MarchCraft::FormationShape shape;shape.type=recognized;shape.points=path;shape.closed=closed;
    QPointF anchor;for(const auto&p:path)anchor+=p;anchor/=path.size();shape.anchor=anchor;const QRectF bounds=QPolygonF(path).boundingRect();shape.width=bounds.width();shape.height=bounds.height();shape.parameters={{QStringLiteral("source"),QStringLiteral("freehand")},{QStringLiteral("assignmentMode"),MarchCraft::assignmentModeName(MarchCraft::assignmentModeFromName(movementMode))},{QStringLiteral("recognitionMode"),recognitionMode}};
    QVector<QString> orderedIds(selected.size());
    for(int i=0;i<selected.size();++i){const QString id=m_performers[selected[i]].id;variant.placements[id].position=targets[assignment[i]];orderedIds[assignment[i]]=id;}
    shape.performerIds=orderedIds;
    if(createGroup&&shape.performerIds.size()>=2){MarchCraft::PerformerGroup group;group.name=QStringLiteral("Freehand formation");group.performerIds=shape.performerIds;shape.groupId=group.id;variant.groups.push_back(group);}
    variant.shapes.push_back(shape);emit shapesChanged();emitAllDataChanged();commitSnapshot(before,QStringLiteral("Create freehand formation"));setStatus(QStringLiteral("Created %1 freehand formation").arg(recognized));
}

void DrillProject::distributeLine(double x1, double y1, double x2, double y2)
{
    const QPointF a{x1, y1}, b{x2, y2}, center = (a + b) / 2.0;
    createFormation(QStringLiteral("line"), {{QStringLiteral("centerX"), center.x()},
        {QStringLiteral("centerY"), center.y()}, {QStringLiteral("width"), std::hypot(x2 - x1, y2 - y1)},
        {QStringLiteral("height"), 2.0}, {QStringLiteral("rotation"), std::atan2(y2 - y1, x2 - x1) * 180.0 / std::numbers::pi}});
}

void DrillProject::distributeArc(double cx, double cy, double radius,
                                 double startDegrees, double endDegrees)
{
    const double sweep = endDegrees - startDegrees;
    createFormation(std::abs(sweep) >= 359.999 ? QStringLiteral("circle") : QStringLiteral("arc"),
                    {{QStringLiteral("centerX"), cx}, {QStringLiteral("centerY"), cy},
                     {QStringLiteral("radius"), radius}, {QStringLiteral("width"), radius * 2.0},
                     {QStringLiteral("height"), radius * 2.0}, {QStringLiteral("startAngle"), startDegrees},
                     {QStringLiteral("sweepAngle"), sweep}});
}

void DrillProject::distributeRectangle(double x, double y, double width, double height)
{
    createFormation(QStringLiteral("rectangle"), {{QStringLiteral("centerX"), x + width / 2.0},
        {QStringLiteral("centerY"), y + height / 2.0}, {QStringLiteral("width"), std::abs(width)},
        {QStringLiteral("height"), std::abs(height)}});
}

void DrillProject::mirrorSelected(bool horizontal)
{
    if (selectedCount() == 0 || m_currentSet < 0) return;
    const auto before = toJson();
    for (const auto &person : m_performers) if (person.selected) {
        auto &placement = m_sets[m_currentSet].activeVariant().placements[person.id];
        if (horizontal) placement.position.setX(fieldWidthSteps() - placement.position.x());
        else placement.position.setY(fieldDepthSteps() - placement.position.y());
    }
    emitAllDataChanged(); commitSnapshot(before, QStringLiteral("Mirror performers"));
}

void DrillProject::snapSelected(double grid)
{
    if (grid <= 0.0 || m_currentSet < 0) return;
    const auto before = toJson();
    for (const auto &person : m_performers) if (person.selected) {
        auto &point = m_sets[m_currentSet].activeVariant().placements[person.id].position;
        point = clampPosition({std::round(point.x() / grid) * grid, std::round(point.y() / grid) * grid});
    }
    emitAllDataChanged(); commitSnapshot(before, QStringLiteral("Snap performers"));
}

void DrillProject::faceSelected(double degrees)
{
    if (m_currentSet < 0) return;
    const auto before = toJson();
    for (const auto &person : m_performers) if (person.selected)
        m_sets[m_currentSet].activeVariant().placements[person.id].facing =
            std::fmod(degrees + 360.0, 360.0);
    emitAllDataChanged(); commitSnapshot(before, QStringLiteral("Change facing"));
}

void DrillProject::setSelectedTransitionPath(const QString &type, const QVariantList &controlPoints)
{
    if (!beginTransitionEdit()) return;
    setTransitionEditType(type);
    if (!controlPoints.isEmpty()) {
        QVector<QPointF> points;
        for (const auto &value : controlPoints)
            if (value.canConvert<QPointF>()) points.push_back(clampPosition(value.toPointF()));
        for (const auto &id : m_transitionEdit.performerIds) {
            auto &placement = m_transitionEdit.previewPlacements[id];
            placement.pathType = m_transitionEdit.type;
            placement.pathPoints = points;
        }
    }
    applyTransitionEdit();
}

QVariantList DrillProject::transitionPathSamples(int performerRow, int samples) const
{
    QVariantList result;
    if (performerRow < 0 || performerRow >= m_performers.size()) return result;
    samples = qBound(2, samples, 128);
    const int destination = m_transitionEdit.active ? m_transitionEdit.destinationSet : playbackSetIndex();
    if (destination <= 0 || destination >= m_sets.size()) return result;
    const Placement placement = m_transitionEdit.active
        ? transitionEditPlacement(performerRow) : placementAt(performerRow, destination);
    const MarchCraft::TransitionPath path(placementAt(performerRow, destination - 1).position,
                                         placement, qMax(1, m_sets[destination].counts));
    for (int i = 0; i <= samples; ++i) result.push_back(path.position(double(i) / samples));
    return result;
}

QVariantMap DrillProject::shapeInfo(int index) const
{
    if (m_currentSet < 0 || m_currentSet >= m_sets.size()) return {};
    const auto &shapes = m_sets[m_currentSet].activeVariant().shapes;
    if (index < 0 || index >= shapes.size()) return {};
    QVariantList points;
    for (const auto &point : shapes[index].points) points.push_back(point);
    return {{QStringLiteral("id"), shapes[index].id}, {QStringLiteral("type"), shapes[index].type},
            {QStringLiteral("points"), points}, {QStringLiteral("memberCount"), shapes[index].performerIds.size()},
            {QStringLiteral("endpointLock"), shapes[index].endpointLock},
            {QStringLiteral("reversed"), shapes[index].reversed},
            {QStringLiteral("anchor"), shapes[index].anchor}, {QStringLiteral("width"), shapes[index].width},
            {QStringLiteral("height"), shapes[index].height}, {QStringLiteral("rotation"), shapes[index].rotation},
            {QStringLiteral("closed"), shapes[index].closed},
            {QStringLiteral("parameters"), shapes[index].parameters.toVariantMap()}};
}

void DrillProject::removeShape(int index, bool bakePlacements)
{
    if (m_currentSet < 0 || m_currentSet >= m_sets.size()) return;
    auto &shapes = m_sets[m_currentSet].activeVariant().shapes;
    if (index < 0 || index >= shapes.size()) return;
    const auto before = toJson();
    if (!bakePlacements)
        for (const auto &id : shapes[index].performerIds)
            for (int row = 0; row < m_performers.size(); ++row)
                if (m_performers[row].id == id)
                    m_sets[m_currentSet].activeVariant().placements[id].position =
                        placementAt(row, qMax(0, m_currentSet - 1)).position;
    shapes.removeAt(index); emit shapesChanged(); emitAllDataChanged();
    commitSnapshot(before, bakePlacements ? QStringLiteral("Bake formation shape") : QStringLiteral("Remove formation shape"));
}

void DrillProject::copyShapeToAdjacent(int index, int direction)
{
    if (m_currentSet < 0 || m_currentSet >= m_sets.size() || direction == 0) return;
    const int target = m_currentSet + (direction < 0 ? -1 : 1);
    if (target < 0 || target >= m_sets.size()) return;
    const auto &sourceShapes = m_sets[m_currentSet].activeVariant().shapes;
    if (index < 0 || index >= sourceShapes.size()) return;
    const auto before = toJson(); auto copy = sourceShapes[index];
    copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_sets[target].activeVariant().shapes.push_back(copy);
    for (const auto &id : copy.performerIds)
        m_sets[target].activeVariant().placements[id] = m_sets[m_currentSet].activeVariant().placements.value(id);
    emit shapesChanged(); emitAllDataChanged(); commitSnapshot(before, QStringLiteral("Copy shape to adjacent set"));
}
