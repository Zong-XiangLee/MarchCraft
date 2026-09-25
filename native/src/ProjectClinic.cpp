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

#include "ProjectAlgorithms.h"
#include "ProjectStorage.h"

using namespace MarchCraft::ProjectAlgorithms;
using namespace MarchCraft::ProjectStorage;

QString DrillProject::issueLabelList(const QVector<int> &rows, int limit) const
{
    QStringList labels; for (int row : rows) if (row >= 0 && row < m_performers.size() && labels.size() < limit)
        labels.push_back(m_performers[row].label);
    if (rows.size() > limit) labels.push_back(QStringLiteral("+%1 more").arg(rows.size() - limit));
    return labels.join(QStringLiteral(", "));
}

void DrillProject::invalidateClinic()
{
    m_clinicIssues.clear(); m_dismissedClinicIssues.clear(); m_pendingSuggestion.clear();
    m_highlightedClinicProps.clear(); emit propsChanged();
    emit clinicChanged();
}

QVariantList DrillProject::analyzeTransition(int destinationSet)
{
    destinationSet = destinationSet < 0 ? m_currentSet : destinationSet;
    QVariantList issues;
    if (destinationSet <= 0 || destinationSet >= m_sets.size() || m_performers.isEmpty()) {
        if (destinationSet == m_currentSet) { m_clinicIssues = issues; emit clinicChanged(); }
        return issues;
    }
    const int counts = qMax(1, m_sets[destinationSet].counts);
    auto action = [&](const QString &type, const QString &label) {
        return QVariantMap{{QStringLiteral("id"), QStringLiteral("%1:%2:%3").arg(destinationSet).arg(type, label)},
                           {QStringLiteral("type"), type}, {QStringLiteral("label"), label}};
    };
    auto addIssue = [&](const QString &type, const QString &severity, const QString &title, const QString &detail,
                        const QVector<int> &rows, double measured, double limit, double countPosition, const QVariantList &actions) {
        const QString id = QStringLiteral("%1:%2").arg(destinationSet).arg(type);
        QVariantList ids; for (int row : rows) if (row >= 0 && row < m_performers.size()) ids.push_back(m_performers[row].id);
        issues.push_back(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("type"), type},
            {QStringLiteral("severity"), severity}, {QStringLiteral("title"), title}, {QStringLiteral("detail"), detail},
            {QStringLiteral("setIndex"), destinationSet}, {QStringLiteral("setLabel"), setInfo(destinationSet).value(QStringLiteral("number"))},
            {QStringLiteral("count"), countPosition}, {QStringLiteral("measured"), measured}, {QStringLiteral("limit"), limit},
            {QStringLiteral("performerIds"), ids}, {QStringLiteral("performers"), issueLabelList(rows)},
            {QStringLiteral("affectedCount"), rows.size()}, {QStringLiteral("actions"), actions}});
    };

    QVector<int> strideRows, cautionStrideRows; double maximumStride = 0.0, requiredCounts = 0.0;
    for (int row = 0; row < m_performers.size(); ++row) {
        const double distance = transitionDistance(row, destinationSet);
        const double stride = distance
            / qMax(0.001, transitionPath(row, destinationSet).durationCounts());
        maximumStride = qMax(maximumStride, stride);
        requiredCounts = qMax(requiredCounts, transitionPath(row, destinationSet).startCount()
            + distance / m_capability.maximumStepsPerCount);
        if (stride > m_capability.maximumStepsPerCount) strideRows.push_back(row);
        else if (stride >= m_capability.maximumStepsPerCount * 0.85) cautionStrideRows.push_back(row);
    }
    if (!strideRows.isEmpty()) {
        const int recommended = qCeil(requiredCounts);
        addIssue(QStringLiteral("stride"), QStringLiteral("critical"), QStringLiteral("Move exceeds capability profile"),
            QStringLiteral("%1 need more than %2 steps per count. %3 counts would make the longest move achievable.")
                .arg(issueLabelList(strideRows)).arg(m_capability.maximumStepsPerCount, 0, 'f', 2).arg(recommended),
            strideRows, maximumStride, m_capability.maximumStepsPerCount, 0,
            {action(QStringLiteral("increaseCounts"), QStringLiteral("Preview %1 counts").arg(recommended)),
             action(QStringLiteral("optimize"), QStringLiteral("Reassign destination points"))});
        QVariantMap last = issues.last().toMap(); last.insert(QStringLiteral("recommendedCounts"), recommended); issues.last() = last;
    } else if (!cautionStrideRows.isEmpty()) {
        addIssue(QStringLiteral("strideCaution"), QStringLiteral("caution"), QStringLiteral("Move is near the stride limit"),
            QStringLiteral("%1 are at least 85% of the configured stride limit.").arg(issueLabelList(cautionStrideRows)),
            cautionStrideRows, maximumStride, m_capability.maximumStepsPerCount, 0,
            {action(QStringLiteral("optimize"), QStringLiteral("Try rehearsal-safe reassignment"))});
    }

    const int samples = qBound(8, counts * 4, 128); QVector<int> collisionRows, nearRows, equipmentRows;
    QStringList equipmentPairs;
    double smallestClearance = std::numeric_limits<double>::max(), firstCollisionCount = -1;
    QVector<double> clearanceRadii;
    clearanceRadii.reserve(m_performers.size());
    for (const auto &performer : m_performers) clearanceRadii.push_back(instrumentClearanceRadius(performer));
    const double clinicCellSize = qMax(3.0, m_capability.collisionClearance * 1.25);
    auto clinicCellKey = [](int x, int y) { return (quint64(quint32(x)) << 32) | quint32(y); };
    for (int sample = 0; sample <= samples; ++sample) {
        const double progress = double(sample) / samples;
        QHash<quint64, QVector<int>> grid; QVector<QPointF> samplePositions; samplePositions.reserve(m_performers.size());
        for (int row = 0; row < m_performers.size(); ++row) {
            const QPointF position = pathPosition(row, destinationSet, progress); samplePositions.push_back(position);
            const int cellX = qFloor(position.x() / clinicCellSize), cellY = qFloor(position.y() / clinicCellSize);
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
                for (int other : grid.value(clinicCellKey(cellX + dx, cellY + dy))) {
                    const double clearance = pointDistance(position, samplePositions[other]); smallestClearance = qMin(smallestClearance, clearance);
                    const double equipmentClearance = clearanceRadii[row] + clearanceRadii[other] + 0.25;
                    const double required = qMax(m_capability.collisionClearance, equipmentClearance);
                    // Only surface a near miss when it is genuinely tight. The
                    // old 1.25x buffer made valid pathways look like problems.
                    QVector<int> *bucket = clearance < required ? &collisionRows
                        : clearance < required * 1.10 ? &nearRows : nullptr;
                    if (bucket) { if (!bucket->contains(row)) bucket->push_back(row); if (!bucket->contains(other)) bucket->push_back(other); }
                    if (clearance < required && equipmentClearance > m_capability.collisionClearance + 0.01) {
                        if (!equipmentRows.contains(row)) equipmentRows.push_back(row); if (!equipmentRows.contains(other)) equipmentRows.push_back(other);
                        const QString pair = QStringLiteral("%1 (%2) / %3 (%4)").arg(m_performers[row].label,
                            m_performers[row].instrument, m_performers[other].label, m_performers[other].instrument);
                        if (!equipmentPairs.contains(pair) && equipmentPairs.size() < 4) equipmentPairs.push_back(pair);
                    }
                    if (clearance < required && firstCollisionCount < 0) firstCollisionCount = progress * counts;
                }
            grid[clinicCellKey(cellX, cellY)].push_back(row);
        }
    }
    if (!collisionRows.isEmpty()) addIssue(QStringLiteral("collision"), QStringLiteral("critical"), QStringLiteral("Collision predicted"),
        QStringLiteral("%1 violate the %2-step clearance near count %3.").arg(issueLabelList(collisionRows))
            .arg(m_capability.collisionClearance, 0, 'f', 2).arg(firstCollisionCount, 0, 'f', 1),
        collisionRows, smallestClearance, m_capability.collisionClearance, firstCollisionCount,
        {action(QStringLiteral("optimize"), QStringLiteral("Reassign spots")),
         action(QStringLiteral("reroute"), QStringLiteral("Preview collision detour")),
         action(QStringLiteral("delayed"), QStringLiteral("Stagger entrances")),
         action(QStringLiteral("insertSubset"), QStringLiteral("Preview resolving subset"))});
    else if (!nearRows.isEmpty()) addIssue(QStringLiteral("nearMiss"), QStringLiteral("caution"), QStringLiteral("Tight pathway clearance"),
        QStringLiteral("%1 pass within the caution buffer.").arg(issueLabelList(nearRows)), nearRows,
        smallestClearance, m_capability.collisionClearance * 1.25, 0,
        {action(QStringLiteral("reroute"), QStringLiteral("Preview safer paths"))});
    if (!equipmentRows.isEmpty()) addIssue(QStringLiteral("equipmentCollision"), QStringLiteral("critical"),
        QStringLiteral("Instrument or equipment clearance conflict"),
        QStringLiteral("These pathways need extra room for equipment: %1.").arg(equipmentPairs.join(QStringLiteral("; "))),
        equipmentRows, smallestClearance, m_capability.collisionClearance, firstCollisionCount,
        {action(QStringLiteral("optimize"), QStringLiteral("Reassign with equipment clearance")),
         action(QStringLiteral("reroute"), QStringLiteral("Route instruments apart")),
         action(QStringLiteral("delayed"), QStringLiteral("Stagger the conflict"))});

    QVector<int> propCollisionRows, propNearRows; QSet<QString> propCollisionIds, propNearIds;
    QStringList propCollisionNames; double closestPropClearance = std::numeric_limits<double>::max();
    double firstPropCollisionCount = -1.0; bool propDestinationCollision = false;
    for (int sample = 0; !m_props.isEmpty() && sample <= samples; ++sample) {
        const double progress = double(sample) / samples;
        for (int row = 0; row < m_performers.size(); ++row) {
            const QPointF performerPosition = pathPosition(row, destinationSet, progress);
            for (const auto &prop : m_props) {
                if (prop.assignedMoverIds.contains(m_performers[row].id)) continue;
                const QPointF propPosition = propPositionAt(prop, destinationSet, progress);
                const double edgeClearance = distanceToPropFootprint(performerPosition, propPosition,
                    prop.rotation, propFootprintSteps(prop));
                const double required = clearanceRadii[row] + 0.5;
                closestPropClearance = qMin(closestPropClearance, edgeClearance);
                if (edgeClearance < required) {
                    if (!propCollisionRows.contains(row)) propCollisionRows.push_back(row);
                    propCollisionIds.insert(prop.id); if (!propCollisionNames.contains(propDisplayName(prop))) propCollisionNames.push_back(propDisplayName(prop));
                    if (firstPropCollisionCount < 0) firstPropCollisionCount = progress * counts;
                    if (sample == samples) propDestinationCollision = true;
                } else if (edgeClearance < required * 1.10) {
                    if (!propNearRows.contains(row)) propNearRows.push_back(row);
                    propNearIds.insert(prop.id);
                }
            }
        }
    }
    if (!propCollisionRows.isEmpty()) {
        QVariantList propActions{action(QStringLiteral("reroute"), QStringLiteral("Route around prop")),
            action(QStringLiteral("delayed"), QStringLiteral("Stagger prop crossing"))};
        if (propDestinationCollision)
            propActions.insert(1, action(QStringLiteral("shiftProp"), QStringLiteral("Shift destinations clear of prop")));
        addIssue(QStringLiteral("propCollision"), QStringLiteral("critical"), QStringLiteral("Prop collision predicted"),
            QStringLiteral("%1 intersect the safety footprint of %2 near count %3.").arg(issueLabelList(propCollisionRows),
                propCollisionNames.join(QStringLiteral(", "))).arg(firstPropCollisionCount, 0, 'f', 1),
            propCollisionRows, closestPropClearance, 0.5, firstPropCollisionCount,
            propActions);
        QVariantMap last = issues.last().toMap(); QVariantList ids; for (const auto &id : propCollisionIds) ids.push_back(id);
        last.insert(QStringLiteral("propIds"), ids); issues.last() = last;
    } else if (!propNearRows.isEmpty()) {
        addIssue(QStringLiteral("propNearMiss"), QStringLiteral("caution"), QStringLiteral("Tight prop clearance"),
            QStringLiteral("%1 pass close to a prop safety footprint.").arg(issueLabelList(propNearRows)),
            propNearRows, closestPropClearance, 0.625, 0,
            {action(QStringLiteral("reroute"), QStringLiteral("Add prop clearance"))});
        QVariantMap last = issues.last().toMap(); QVariantList ids; for (const auto &id : propNearIds) ids.push_back(id);
        last.insert(QStringLiteral("propIds"), ids); issues.last() = last;
    }

    // Crossings can be intentional choreography, so they are not Clinic issues.

    QVector<int> directionRows; double largestDirection = 0.0;
    if (destinationSet + 1 < m_sets.size()) for (int row = 0; row < m_performers.size(); ++row) {
        const QPointF incoming = placementAt(row, destinationSet).position - placementAt(row, destinationSet - 1).position;
        const QPointF outgoing = placementAt(row, destinationSet + 1).position - placementAt(row, destinationSet).position;
        const double lengths = pointDistance({}, incoming) * pointDistance({}, outgoing); if (lengths < 0.01) continue;
        const double cosine = qBound(-1.0, QPointF::dotProduct(incoming, outgoing) / lengths, 1.0);
        const double angle = std::acos(cosine) * 180.0 / std::numbers::pi; largestDirection = qMax(largestDirection, angle);
        if (angle > m_capability.directionChangeDegrees + 15.0) directionRows.push_back(row);
    }
    if (!directionRows.isEmpty()) addIssue(QStringLiteral("direction"), QStringLiteral("caution"), QStringLiteral("Abrupt direction change"),
        QStringLiteral("%1 reverse or redirect more sharply than the profile allows.").arg(issueLabelList(directionRows)),
        directionRows, largestDirection, m_capability.directionChangeDegrees, counts,
        {action(QStringLiteral("insertSubset"), QStringLiteral("Preview directional subset")),
         action(QStringLiteral("reroute"), QStringLiteral("Round the approach")),
         action(QStringLiteral("gate"), QStringLiteral("Try gate / pivot"))});

    QVector<int> complexPathRows; double sharpestPathTurn = 0.0, firstPathTurnCount = -1.0;
    const int directionSamples = qBound(12, counts * 4, 96);
    for (int row = 0; row < m_performers.size(); ++row) {
        QPointF previous = pathPosition(row, destinationSet, 0.0), previousDirection;
        bool hasDirection = false;
        for (int sample = 1; sample <= directionSamples; ++sample) {
            const QPointF current = pathPosition(row, destinationSet, double(sample) / directionSamples);
            const QPointF direction = current - previous;
            previous = current;
            const double length = pointDistance({}, direction);
            if (length < 0.01) continue;
            if (hasDirection) {
                const double previousLength = pointDistance({}, previousDirection);
                const double cosine = qBound(-1.0, QPointF::dotProduct(previousDirection, direction)
                    / (previousLength * length), 1.0);
                const double angle = std::acos(cosine) * 180.0 / std::numbers::pi;
                sharpestPathTurn = qMax(sharpestPathTurn, angle);
                if (angle > m_capability.directionChangeDegrees) {
                    if (!complexPathRows.contains(row)) complexPathRows.push_back(row);
                    if (firstPathTurnCount < 0.0) firstPathTurnCount = double(sample) / directionSamples * counts;
                }
            }
            previousDirection = direction; hasDirection = true;
        }
    }
    if (!complexPathRows.isEmpty()) addIssue(QStringLiteral("complexPath"), QStringLiteral("caution"),
        QStringLiteral("Authored path turns too sharply"),
        QStringLiteral("%1 change direction by up to %2° inside the transition.")
            .arg(issueLabelList(complexPathRows)).arg(sharpestPathTurn, 0, 'f', 0),
        complexPathRows, sharpestPathTurn, m_capability.directionChangeDegrees, firstPathTurnCount,
        {action(QStringLiteral("simplifyPath"), QStringLiteral("Simplify control points")),
         action(QStringLiteral("reroute"), QStringLiteral("Round the route"))});

    QVector<int> spacingRows; double worstSpacingVariation = 0.0;
    const auto &destinationVariant = m_sets[destinationSet].activeVariant();
    for (const auto &shape : destinationVariant.shapes) {
        if (shape.performerIds.size() < 3) continue;
        QVector<int> rows; QVector<double> intervals;
        for (const auto &id : shape.performerIds) {
            for (int row = 0; row < m_performers.size(); ++row)
                if (m_performers[row].id == id) { rows.push_back(row); break; }
        }
        const int segmentCount = shape.closed ? rows.size() : rows.size() - 1;
        for (int index = 0; index < segmentCount; ++index)
            intervals.push_back(pointDistance(placementAt(rows[index], destinationSet).position,
                placementAt(rows[(index + 1) % rows.size()], destinationSet).position));
        if (intervals.isEmpty()) continue;
        const double mean = std::accumulate(intervals.begin(), intervals.end(), 0.0) / intervals.size();
        double variance = 0.0; for (double interval : intervals) variance += (interval - mean) * (interval - mean);
        const double coefficient = mean > 0.01 ? std::sqrt(variance / intervals.size()) / mean : 1.0;
        if (coefficient >= 0.25) {
            worstSpacingVariation = qMax(worstSpacingVariation, coefficient);
            for (int row : rows) if (!spacingRows.contains(row)) spacingRows.push_back(row);
        }
    }
    if (!spacingRows.isEmpty()) addIssue(QStringLiteral("spacing"), QStringLiteral("caution"),
        QStringLiteral("Attached form has uneven intervals"),
        QStringLiteral("%1 have interval variation of up to %2%. Reflowing preserves the guide while restoring equal spacing.")
            .arg(issueLabelList(spacingRows)).arg(worstSpacingVariation * 100.0, 0, 'f', 0),
        spacingRows, worstSpacingVariation, 0.25, counts,
        {action(QStringLiteral("reflow"), QStringLiteral("Reflow evenly along shape"))});

    QVector<int> boundaryRows; for (int row = 0; row < m_performers.size(); ++row) {
        const auto placement = placementAt(row, destinationSet); const QPointF p = placement.position;
        if (p.x() < canvasMinX() - .25 || p.x() > canvasMaxX() + .25 ||
            p.y() < canvasMinY() - .25 || p.y() > canvasMaxY() + .25) boundaryRows.push_back(row);
    }
    if (!boundaryRows.isEmpty()) addIssue(QStringLiteral("boundary"), QStringLiteral("caution"), QStringLiteral("Formation extends beyond workspace"),
        QStringLiteral("%1 extend beyond the editable field and need to be brought back inside.").arg(issueLabelList(boundaryRows)), boundaryRows, 0, 0, counts,
        {action(QStringLiteral("shiftBoundary"), QStringLiteral("Shift inside apron"))});
    std::stable_sort(issues.begin(), issues.end(), [](const QVariant &left, const QVariant &right) {
        const auto rank=[](const QString&s){return s==QStringLiteral("critical")?0:s==QStringLiteral("caution")?1:2;};
        return rank(left.toMap().value(QStringLiteral("severity")).toString()) < rank(right.toMap().value(QStringLiteral("severity")).toString());
    });
    issues.removeIf([this](const QVariant &issue) {
        return m_dismissedClinicIssues.contains(issue.toMap().value(QStringLiteral("id")).toString());
    });
    if (destinationSet == m_currentSet) { m_clinicIssues = issues; emit clinicChanged(); }
    return issues;
}

QVariantList DrillProject::scanShow()
{
    QVariantList all; for (int set = 1; set < m_sets.size(); ++set) for (const auto &issue : analyzeTransition(set)) all.push_back(issue);
    m_clinicIssues = all; emit clinicChanged(); setStatus(QStringLiteral("Drill Clinic scanned %1 transitions and found %2 issues").arg(qMax(0, m_sets.size()-1)).arg(all.size()));
    return all;
}

QVariantMap DrillProject::clinicIssue(int index) const
{
    return index >= 0 && index < m_clinicIssues.size() ? m_clinicIssues[index].toMap() : QVariantMap{};
}

void DrillProject::dismissIssue(const QString &issueId)
{
    if (issueId.isEmpty()) return; m_dismissedClinicIssues.insert(issueId);
    m_clinicIssues.erase(std::remove_if(m_clinicIssues.begin(), m_clinicIssues.end(), [&](const QVariant &issue) {
        return issue.toMap().value(QStringLiteral("id")).toString() == issueId; }), m_clinicIssues.end()); emit clinicChanged();
}

bool DrillProject::selectClinicIssue(const QString &issueId)
{
    for (const auto &value : m_clinicIssues) {
        const auto issue = value.toMap();
        if (issue.value(QStringLiteral("id")).toString() != issueId) continue;
        const QSet<QString> ids = [&] {
            QSet<QString> result;
            for (const auto &id : issue.value(QStringLiteral("performerIds")).toList()) result.insert(id.toString());
            return result;
        }();
        m_highlightedClinicProps.clear();
        for (const auto &id : issue.value(QStringLiteral("propIds")).toList()) m_highlightedClinicProps.insert(id.toString());
        for (int row = 0; row < m_performers.size(); ++row)
            m_performers[row].selected = ids.contains(m_performers[row].id)
                && m_performers[row].visible && !m_performers[row].locked;
        if (issue.value(QStringLiteral("setIndex")).toInt() >= 0)
            setCurrentSetIndex(issue.value(QStringLiteral("setIndex")).toInt());
        emit selectionChanged(); emit propsChanged(); emitAllDataChanged();
        setStatus(QStringLiteral("Highlighted %1 affected performers").arg(ids.size()));
        return true;
    }
    return false;
}

bool DrillProject::previewSuggestion(const QString &suggestionId)
{
    QVariantMap chosenIssue, chosenAction;
    for (const auto &issueValue : m_clinicIssues) {
        const auto issue = issueValue.toMap(); for (const auto &actionValue : issue.value(QStringLiteral("actions")).toList()) {
            const auto actionMap = actionValue.toMap(); if (actionMap.value(QStringLiteral("id")).toString() == suggestionId) { chosenIssue = issue; chosenAction = actionMap; break; }
        }
        if (!chosenAction.isEmpty()) break;
    }
    if (chosenAction.isEmpty()) return false;
    m_pendingSuggestion = chosenIssue; m_pendingSuggestion.insert(QStringLiteral("suggestionId"), suggestionId);
    m_pendingSuggestion.insert(QStringLiteral("actionType"), chosenAction.value(QStringLiteral("type")));
    const QString type = chosenAction.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("optimize")) {
        const int destination = chosenIssue.value(QStringLiteral("setIndex")).toInt();
        if (destination <= 0 || destination >= m_sets.size()) return false;
        setCurrentSetIndex(destination);
        QSet<QString> affected;
        for (const auto &id : chosenIssue.value(QStringLiteral("performerIds")).toList()) affected.insert(id.toString());
        QVector<int> rows; QVector<QPointF> targets;
        for (int row = 0; row < m_performers.size(); ++row) if (affected.contains(m_performers[row].id)) {
            rows.push_back(row); targets.push_back(placementAt(row, destination).position);
        }
        if (rows.size() >= 2) {
            QVector<QVector<double>> costs(rows.size(), QVector<double>(rows.size()));
            for (int source = 0; source < rows.size(); ++source) for (int target = 0; target < rows.size(); ++target) {
                const double distance = pointDistance(placementAt(rows[source], destination - 1).position, targets[target]);
                costs[source][target] = distance * distance + source * 1e-9 + target * 1e-12;
            }
            const auto assignment = minimumCostAssignment(costs);
            m_formationPreview = {}; m_formationPreview.active = true;
            m_formationPreview.mode = QStringLiteral("rehearsalSafe");
            m_formationPreviewSourceSet = destination - 1;
            for (int source = 0; source < rows.size(); ++source) {
                const QString id = m_performers[rows[source]].id;
                m_formationPreview.sourcePlacements.insert(id, placementAt(rows[source], destination - 1).position);
                m_formationPreview.placements.insert(id, targets[assignment[source]]);
            }
            m_formationPreview.metrics = assignmentMetrics(rows, targets, assignment);
            emit formationPreviewChanged();
        }
    } else if (type == QStringLiteral("shiftProp")) {
        const int destination = chosenIssue.value(QStringLiteral("setIndex")).toInt();
        if (destination <= 0 || destination >= m_sets.size()) return false;
        setCurrentSetIndex(destination); QSet<QString> affected, propIds;
        for (const auto &id : chosenIssue.value(QStringLiteral("performerIds")).toList()) affected.insert(id.toString());
        for (const auto &id : chosenIssue.value(QStringLiteral("propIds")).toList()) propIds.insert(id.toString());
        QVector<int> rows; QVector<QPointF> targets; QVector<int> identity;
        m_formationPreview = {}; m_formationPreview.active = true; m_formationPreview.mode = QStringLiteral("propClearance");
        m_formationPreviewSourceSet = destination - 1;
        for (int row = 0; row < m_performers.size(); ++row) if (affected.contains(m_performers[row].id)) {
            QPointF target = placementAt(row, destination).position;
            const QPointF source = placementAt(row, destination - 1).position;
            for (const auto &prop : m_props) if (propIds.contains(prop.id)) {
                const QPointF center = propPositionAt(prop, destination, 1.0); const QSizeF footprint = propFootprintSteps(prop);
                QPointF local = rotateAround(target, center, -prop.rotation) - center;
                const double clearance = instrumentClearanceRadius(m_performers[row]) + 0.75;
                const double limitX = footprint.width()/2.0 + clearance, limitY = footprint.height()/2.0 + clearance;
                if (qAbs(local.x()) < limitX && qAbs(local.y()) < limitY) {
                    const QVector<QPointF> candidates{{-limitX,qBound(-limitY,local.y(),limitY)},
                        {limitX,qBound(-limitY,local.y(),limitY)},{qBound(-limitX,local.x(),limitX),-limitY},
                        {qBound(-limitX,local.x(),limitX),limitY}};
                    double best = std::numeric_limits<double>::max(); QPointF bestPoint;
                    for (const auto &candidateLocal : candidates) {
                        const QPointF candidate = rotateAround(center + candidateLocal, center, prop.rotation);
                        const double score = pointDistance(source,candidate) + pointDistance(target,candidate)*0.2;
                        if(score<best){best=score;bestPoint=candidate;}
                    }
                    target = clampPosition(bestPoint);
                }
            }
            rows.push_back(row); targets.push_back(target); identity.push_back(identity.size());
            const QString id = m_performers[row].id;
            m_formationPreview.sourcePlacements.insert(id, placementAt(row, destination - 1).position);
            m_formationPreview.placements.insert(id, target);
        }
        m_formationPreview.metrics = assignmentMetrics(rows, targets, identity); emit formationPreviewChanged();
    }
    setStatus(QStringLiteral("Preview ready: %1").arg(chosenAction.value(QStringLiteral("label")).toString())); emit clinicChanged(); return true;
}

bool DrillProject::acceptSuggestion(const QString &suggestionId)
{
    if (m_formationPreview.active) return commitFormationPreview();
    if (m_pendingSuggestion.value(QStringLiteral("suggestionId")).toString() != suggestionId && !previewSuggestion(suggestionId)) return false;
    const QString type = m_pendingSuggestion.value(QStringLiteral("actionType")).toString();
    const int destination = m_pendingSuggestion.value(QStringLiteral("setIndex"), m_currentSet).toInt();
    if (destination <= 0 || destination >= m_sets.size()) return false;
    const auto before = toJson(); QVariantList ids = m_pendingSuggestion.value(QStringLiteral("performerIds")).toList(); QSet<QString> affected;
    for (const auto &id : ids) affected.insert(id.toString());
    if (type == QStringLiteral("increaseCounts")) {
        const int recommended = qMax(m_sets[destination].counts + 1, m_pendingSuggestion.value(QStringLiteral("recommendedCounts")).toInt());
        const qint64 oldTick = m_sets[destination].startTick;
        const qint64 newTick = advancePulses(m_sets[destination - 1].startTick, recommended); const qint64 delta = newTick - oldTick;
        for (int set = destination; set < m_sets.size(); ++set) m_sets[set].startTick += delta; recalculateCounts(); emit timingChanged(); emit setsChanged();
        for (const auto &performer : m_performers) if (affected.contains(performer.id)) {
            auto &placement = m_sets[destination].activeVariant().placements[performer.id];
            if (placement.pathDurationCounts >= 0.0) {
                const double startCount = qMax(0.0, placement.pathStartCount);
                placement.pathDurationCounts = qMax(placement.pathDurationCounts, recommended - startCount);
            }
        }
    } else if (type == QStringLiteral("reroute")) {
        const bool propIssue = m_pendingSuggestion.value(QStringLiteral("type")).toString().startsWith(QStringLiteral("prop"));
        QSet<QString> propIds; for (const auto &id : m_pendingSuggestion.value(QStringLiteral("propIds")).toList()) propIds.insert(id.toString());
        int ordinal = 0; for (const auto &performer : m_performers) if (affected.contains(performer.id)) {
            auto &placement = m_sets[destination].activeVariant().placements[performer.id];
            const QPointF source = m_sets[destination - 1].activeVariant().placements.value(performer.id).position;
            QPointF waypoint; double best = std::numeric_limits<double>::max();
            if (propIssue) for (const auto &prop : m_props) if (propIds.contains(prop.id)) {
                const QPointF center = propPositionAt(prop, destination, 0.5); const QSizeF footprint = propFootprintSteps(prop);
                const double margin = instrumentClearanceRadius(performer) + 1.0;
                const QVector<QPointF> localCandidates{{-footprint.width()/2.0-margin,0},{footprint.width()/2.0+margin,0},
                    {0,-footprint.height()/2.0-margin},{0,footprint.height()/2.0+margin}};
                for (const auto &local : localCandidates) {
                    const QPointF candidate = rotateAround(center + local, center, prop.rotation);
                    const double length = pointDistance(source,candidate)+pointDistance(candidate,placement.position);
                    if(length<best){best=length;waypoint=candidate;}
                }
            }
            if (best == std::numeric_limits<double>::max()) {
                const QPointF delta = placement.position - source; const double length = qMax(0.01, pointDistance({}, delta));
                const QPointF perpendicular{-delta.y()/length, delta.x()/length};
                waypoint = (source + placement.position)/2.0 + perpendicular * ((ordinal++ % 2 ? -1 : 1) * m_capability.collisionClearance * 1.5);
            }
            placement.pathType = propIssue ? QStringLiteral("follow") : QStringLiteral("curved");
            placement.pathPoints = {clampPosition(waypoint)};
            placement.pathStartCount = -1.0; placement.pathDurationCounts = -1.0;
        }
    } else if (type == QStringLiteral("delayed")) {
        for (const auto &performer : m_performers) if (affected.contains(performer.id)) {
            auto &placement = m_sets[destination].activeVariant().placements[performer.id];
            placement.pathType = QStringLiteral("delayed"); placement.pathPoints.clear();
            placement.pathStartCount = -1.0; placement.pathDurationCounts = -1.0;
        }
    } else if (type == QStringLiteral("follow") || type == QStringLiteral("gate")) {
        int ordinal = 0;
        for (const auto &performer : m_performers) if (affected.contains(performer.id)) {
            auto &placement = m_sets[destination].activeVariant().placements[performer.id];
            const QPointF source = m_sets[destination - 1].activeVariant().placements.value(performer.id).position;
            placement.pathType = type;
            placement.pathPoints = {type == QStringLiteral("gate")
                ? QPointF(source.x(), placement.position.y())
                : source + (placement.position - source) * (0.35 + 0.3 * (ordinal++ % 3) / 2.0)};
            placement.pathStartCount = -1.0; placement.pathDurationCounts = -1.0;
        }
    } else if (type == QStringLiteral("reflow")) {
        auto &variant = m_sets[destination].activeVariant();
        for (auto &shape : variant.shapes) {
            bool intersects = false; for (const auto &id : shape.performerIds) if (affected.contains(id)) { intersects = true; break; }
            if (!intersects || shape.performerIds.size() < 2 || shape.points.size() < 2) continue;
            const auto targets = equalDistancePoints(shape.points, shape.performerIds.size(), shape.closed);
            if (targets.size() != shape.performerIds.size()) continue;
            for (int index = 0; index < shape.performerIds.size(); ++index)
                variant.placements[shape.performerIds[index]].position = targets[index];
        }
    } else if (type == QStringLiteral("insertSubset")) {
        DrillSet subset; subset.number = m_sets[destination - 1].number + QStringLiteral("A"); subset.subset = true;
        subset.activeVariant().name = QStringLiteral("Clinic resolving subset");
        const qint64 a = m_sets[destination - 1].startTick, b = m_sets[destination].startTick; subset.startTick = a + (b-a)/2;
        for (const auto &performer : m_performers) {
            Placement placement = m_sets[destination].activeVariant().placements.value(performer.id);
            const QPointF source = m_sets[destination - 1].activeVariant().placements.value(performer.id).position;
            placement.position = (source + placement.position) / 2.0; subset.activeVariant().placements.insert(performer.id, placement);
        }
        m_sets.insert(destination, subset); m_currentSet = destination; recalculateCounts(); emit setsChanged(); emit currentSetChanged();
    } else if (type == QStringLiteral("shiftBoundary")) {
        for (auto &performer : m_performers) if (affected.contains(performer.id)) {
            auto &p = m_sets[destination].activeVariant().placements[performer.id].position;
            p.setX(qBound(canvasMinX()+2.0,p.x(),canvasMaxX()-2.0)); p.setY(qBound(canvasMinY()+2.0,p.y(),canvasMaxY()-2.0));
        }
    } else if (type == QStringLiteral("simplifyPath")) {
        for (auto &performer : m_performers) if (affected.contains(performer.id)) {
            auto &placement = m_sets[destination].activeVariant().placements[performer.id];
            if (placement.pathPoints.size()>1) placement.pathPoints = {placement.pathPoints[placement.pathPoints.size()/2]};
        }
    } else return false;
    m_pendingSuggestion.clear(); emitAllDataChanged(); commitSnapshot(before, QStringLiteral("Apply Drill Clinic suggestion"));
    analyzeTransition(m_currentSet); return true;
}

QVariantList DrillProject::suggestNextSet()
{
    QVariantList candidates; if (selectedCount() < 2) return candidates;
    struct SuggestionSpec { const char *type; const char *label; const char *detail; const char *tag; const char *intent; };
    // These are deliberately framed as creative starting points, not commands.
    // A copilot should explain the trade-off and let the author choose.
    const QVector<SuggestionSpec> suggestions{
        {"line", "Reset line", "A clean, readable reset with the smallest form complexity.", "REHEARSAL", "clarity"},
        {"arc", "Forward arc", "An open curve that keeps the front readable while adding flow.", "FLOW", "direction"},
        {"block", "Anchor block", "A compact, evenly spaced grid for a stable visual statement.", "STABLE", "spacing"},
        {"circle", "Orbit circle", "A balanced closed form with equal visual weight in every direction.", "FEATURE", "symmetry"},
        {"ellipse", "Runway ellipse", "A stretched circle that carries energy across the field.", "MOTION", "travel"},
        {"rectangle", "Frame rectangle", "A crisp perimeter that creates clear staging lanes.", "PICTURE", "structure"},
        {"triangle", "Point triangle", "A focused, directional form that creates a natural emphasis point.", "ACCENT", "focus"},
        {"diamond", "Turn diamond", "A centered angular form that reads cleanly from the stands.", "ACCENT", "focus"},
        {"polygon", "Soft polygon", "A rounded geometric form for a more gradual transition.", "FLOW", "softness"},
        {"star", "Feature star", "A decorative picture for a musical peak or reveal.", "FEATURE", "impact"},
        {"spiral", "Reveal spiral", "An expanding path for a featured transition with visible build.", "FEATURE", "build"}
    };
    for (const auto &suggestion : suggestions) {
        const QString type = QString::fromLatin1(suggestion.type);
        auto defaults = formationDefaults(type, QStringLiteral("selection"));
        defaults.insert(QStringLiteral("insertAsNextSet"), true);
        const auto estimate = formationEstimate(type, defaults);
        const double score = estimate.value(QStringLiteral("estimatedSpacing")).toDouble() * 10.0
            - estimate.value(QStringLiteral("currentAverageMove")).toDouble();
        candidates.push_back(QVariantMap{{QStringLiteral("id"),QStringLiteral("next:%1").arg(type)},
            {QStringLiteral("type"),type},{QStringLiteral("label"),QString::fromLatin1(suggestion.label)},
            {QStringLiteral("detail"),QString::fromLatin1(suggestion.detail)},
            {QStringLiteral("tag"),QString::fromLatin1(suggestion.tag)},
            {QStringLiteral("intent"),QString::fromLatin1(suggestion.intent)},
            {QStringLiteral("score"),score},{QStringLiteral("options"),defaults}});
    }
    std::stable_sort(candidates.begin(),candidates.end(),[](const QVariant&a,const QVariant&b){return a.toMap().value(QStringLiteral("score")).toDouble()>b.toMap().value(QStringLiteral("score")).toDouble();});
    return candidates;
}
