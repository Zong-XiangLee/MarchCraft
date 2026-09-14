#pragma once

#include "DrillTypes.h"
#include <QSizeF>

namespace MarchCraft::ProjectAlgorithms {
QVector<int> minimumCostAssignment(const QVector<QVector<double>> &costs);
QString csvCell(QString value);
QString instrumentAssetIdFor(const QString &instrument);
double degreesToRadians(double degrees);
QPointF rotateAround(QPointF point, QPointF center, double degrees);
QVector<QPointF> equalDistancePoints(const QVector<QPointF> &path, int count, bool closed);
double polylineLength(const QVector<QPointF> &path, bool closed = false);
QVector<QPointF> uniformlySampledPath(const QVector<QPointF> &path, bool closed, double spacing);
QVector<QPointF> smoothedStroke(const QVector<QPointF> &path, bool closed, int passes);
QVector<QPointF> rectanglePerimeterPoints(const QVector<QPointF> &path, int count);
void fitPathToField(QVector<QPointF> &path, QPointF &anchor,
                    double minX, double maxX, double minY, double maxY);
QString compactNumber(double value);
double pointDistance(QPointF a, QPointF b);
double crossProduct(QPointF a, QPointF b, QPointF c);
bool segmentsCross(QPointF a, QPointF b, QPointF c, QPointF d);
double instrumentClearanceRadius(const MarchCraft::Performer &performer);
QSizeF propFootprintSteps(const MarchCraft::PropInstance &prop);
QString propDisplayName(const MarchCraft::PropInstance &prop);
QPointF propPositionAt(const MarchCraft::PropInstance &prop, int destinationSet, double progress);
double distanceToPropFootprint(QPointF point, QPointF center, double rotation, QSizeF footprint);
}
