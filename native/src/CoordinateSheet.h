#pragma once

#include "DrillTypes.h"

#include <QImage>
#include <QSizeF>
#include <QString>
#include <QVector>

class QPainter;

namespace MarchCraft {

struct CoordinateText
{
    QString sideToSide;
    QString frontToBack;
    bool valid = true;

    QString combined() const;
};

// The single authoritative human-readable coordinate formatter used by PDF,
// native printing, preview, and CSV exports.
CoordinateText formatCoordinate(QPointF position, const FieldGeometry &geometry,
                                double fieldWidth = 160.0);

struct CoordinateSheetRow
{
    QString movement;
    QString set;
    QString variant;
    QString setName;
    QString measure;
    QString counts;
    QString sideToSide;
    QString frontToBack;
    QString notes;
};

struct CoordinateSheetData
{
    QString showName;
    QString movement;
    QString rehearsalRange;
    QString performerLabel;
    QString performerName;
    QString instrument;
    QString section;
    QString company;
    QString revision;
    QImage companyLogo;
    QVector<CoordinateSheetRow> rows;
};

struct CoordinateSheetOptions
{
    QString density{QStringLiteral("standard")};
    double marginPoints = 24.0;
    bool showSetNames = true;
    bool showMeasures = false;
    bool showNotes = true;
    bool companyLogo = false;
    bool marchcraftLogo = false;
    bool monochrome = true;
};

class CoordinateSheetLayout
{
public:
    static int capacity(const QSizeF &pageSize, const CoordinateSheetOptions &options);
    static double rowHeight(const CoordinateSheetOptions &options);
    static void draw(QPainter &painter, const QRectF &target, const QSizeF &pageSize,
                     const CoordinateSheetData &data, const CoordinateSheetOptions &options,
                     bool capacityExceeded = false);
};

} // namespace MarchCraft
