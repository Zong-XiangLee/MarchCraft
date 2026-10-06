#include "CoordinateSheet.h"
#include "ProjectAlgorithms.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace MarchCraft {

using ProjectAlgorithms::compactNumber;

namespace {

QFont sheetFont(double size, bool bold = false)
{
    QFont result(QStringLiteral("Arial"));
    result.setPixelSize(qRound(size));
    result.setBold(bold);
    return result;
}

QString amount(double value)
{
    // Field landmarks are expressed in true field geometry and can land on
    // repeating decimal step values. Performer coordinates are read in the
    // established quarter-step vocabulary, not hundredths of a step.
    return compactNumber(std::round(std::abs(value) * 4.0) / 4.0);
}

double bodyFontSize(const CoordinateSheetOptions &options)
{
    if (options.density == QStringLiteral("compact")) return 7.0;
    if (options.density == QStringLiteral("large")) return 9.5;
    return 8.2;
}

struct Column
{
    QString heading;
    QString CoordinateSheetRow::*member = nullptr;
    double weight = 1.0;
};

QVector<Column> columns(const CoordinateSheetOptions &options)
{
    QVector<Column> result{{QStringLiteral("Set"), &CoordinateSheetRow::set, 0.08}};
    if (options.showSetNames)
        result.push_back({QStringLiteral("Set Name"), &CoordinateSheetRow::setName, 0.17});
    if (options.showMeasures)
        result.push_back({QStringLiteral("Measure"), &CoordinateSheetRow::measure, 0.10});
    result.push_back({QStringLiteral("Counts"), &CoordinateSheetRow::counts, 0.08});
    result.push_back({QStringLiteral("Side-to-Side"), &CoordinateSheetRow::sideToSide, 0.235});
    result.push_back({QStringLiteral("Front-to-Back"), &CoordinateSheetRow::frontToBack, 0.245});
    if (options.showNotes)
        result.push_back({QStringLiteral("Notes"), &CoordinateSheetRow::notes, 0.20});
    return result;
}

void drawMarchCraftMark(QPainter &painter, QPointF origin)
{
    painter.save();
    painter.translate(origin);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    const QPolygonF mark{QPointF(0, 22),  QPointF(0, 0),  QPointF(5, 0),  QPointF(11, 10),
                         QPointF(17, 0),  QPointF(22, 0), QPointF(22, 22), QPointF(17, 22),
                         QPointF(17, 9),  QPointF(11, 18), QPointF(5, 9),  QPointF(5, 22)};
    painter.drawPolygon(mark);
    painter.setPen(Qt::black);
    painter.setFont(sheetFont(6.2, true));
    painter.drawText(QRectF(27, -1, 48, 24), Qt::AlignVCenter, QStringLiteral("MARCH\nCRAFT"));
    painter.restore();
}

} // namespace

QString CoordinateText::combined() const
{
    if (!valid) return QStringLiteral("Coordinate unavailable");
    return sideToSide + QStringLiteral(" | ") + frontToBack;
}

CoordinateText formatCoordinate(QPointF point, const FieldGeometry &geometry, double fieldWidth)
{
    CoordinateText result;

    if (point.x() < 0.0)
        result.sideToSide = QStringLiteral("Side 1 end zone: %1 beyond goal line").arg(amount(point.x()));
    else if (point.x() > fieldWidth)
        result.sideToSide = QStringLiteral("Side 2 end zone: %1 beyond goal line")
                                .arg(amount(point.x() - fieldWidth));
    else {
        const double midpoint = fieldWidth / 2.0;
        const double centered = point.x() - midpoint;
        const int side = centered <= 0.0 ? 1 : 2;
        const double rawYard = 50.0 - std::abs(centered) * 5.0 / 8.0;
        const double yard = std::round(rawYard / 5.0) * 5.0;
        const double yardX = midpoint + (side == 1 ? -1.0 : 1.0) * (50.0 - yard) * 1.6;
        const double offset = std::abs(point.x() - yardX);
        const bool towardCenter = std::abs(point.x() - midpoint) < std::abs(yardX - midpoint);
        const QString coordinate = offset < 0.01
            ? QStringLiteral("On %1").arg(compactNumber(yard))
            : QStringLiteral("%1 %2 %3")
                  .arg(amount(offset),
                       towardCenter ? QStringLiteral("inside") : QStringLiteral("outside"),
                       compactNumber(yard));
        result.sideToSide = yard == 50.0 && offset < 0.01
            ? coordinate : QStringLiteral("Side %1: %2").arg(side).arg(coordinate);
    }

    if (point.y() < 0.0) {
        result.frontToBack = QStringLiteral("%1 in front of front sideline").arg(amount(point.y()));
    } else if (point.y() > geometry.depth) {
        result.frontToBack = QStringLiteral("%1 behind back sideline").arg(amount(point.y() - geometry.depth));
    } else {
        struct Landmark { QString name; double value; };
        const QVector<Landmark> landmarks{{QStringLiteral("front sideline"), 0.0},
                                          {QStringLiteral("front hash"), geometry.frontHash},
                                          {QStringLiteral("back hash"), geometry.backHash},
                                          {QStringLiteral("back sideline"), geometry.depth}};
        Landmark closest = landmarks.first();
        for (const auto &candidate : landmarks)
            if (std::abs(point.y() - candidate.value) < std::abs(point.y() - closest.value))
                closest = candidate;
        const double offset = std::abs(point.y() - closest.value);
        result.frontToBack = offset < 0.01
            ? QStringLiteral("On %1").arg(closest.name)
            : QStringLiteral("%1 %2 %3")
                  .arg(amount(offset),
                       point.y() < closest.value ? QStringLiteral("in front of")
                                                 : QStringLiteral("behind"),
                       closest.name);
    }
    return result;
}

double CoordinateSheetLayout::rowHeight(const CoordinateSheetOptions &options)
{
    double result = options.density == QStringLiteral("compact") ? 20.0
                    : options.density == QStringLiteral("large") ? 31.0 : 25.0;
    if (options.showSetNames) result += options.density == QStringLiteral("compact") ? 1.0 : 2.0;
    if (options.showNotes) result += options.density == QStringLiteral("compact") ? 1.0 : 2.0;
    return result;
}

int CoordinateSheetLayout::capacity(const QSizeF &pageSize, const CoordinateSheetOptions &options)
{
    constexpr double headerHeight = 62.0;
    constexpr double tableHeaderHeight = 22.0;
    constexpr double footerHeight = 20.0;
    const double available = pageSize.height() - 2.0 * options.marginPoints - headerHeight
        - tableHeaderHeight - footerHeight;
    return qMax(1, int(std::floor(available / rowHeight(options))));
}

void CoordinateSheetLayout::draw(QPainter &painter, const QRectF &target, const QSizeF &pageSize,
                                 const CoordinateSheetData &data,
                                 const CoordinateSheetOptions &options, bool capacityExceeded)
{
    painter.save();
    const double scale = qMin(target.width() / pageSize.width(), target.height() / pageSize.height());
    painter.translate(target.center() - QPointF(pageSize.width() * scale / 2.0,
                                                pageSize.height() * scale / 2.0));
    painter.scale(scale, scale);
    painter.fillRect(QRectF(QPointF(), pageSize), Qt::white);

    const QRectF body(options.marginPoints, options.marginPoints,
                      pageSize.width() - 2.0 * options.marginPoints,
                      pageSize.height() - 2.0 * options.marginPoints);
    double logoRight = body.right();
    if (options.marchcraftLogo) {
        drawMarchCraftMark(painter, QPointF(body.right() - 76.0, body.top() + 1.0));
        logoRight -= 82.0;
    }
    if (options.companyLogo && !data.companyLogo.isNull()) {
        QImage logo = data.companyLogo;
        if (options.monochrome)
            logo = logo.convertToFormat(QImage::Format_Grayscale8);
        const QSizeF logoSize = QSizeF(logo.size()).scaled(qMax(42.0, logoRight - body.left()), 26.0,
                                                           Qt::KeepAspectRatio);
        logoRight -= logoSize.width();
        painter.drawImage(QRectF(logoRight, body.top(), logoSize.width(), logoSize.height()), logo);
        logoRight -= 6.0;
    }

    painter.setPen(QColor(QStringLiteral("#111111")));
    painter.setFont(sheetFont(9.0, true));
    const QString show = data.showName.trimmed().isEmpty() ? QStringLiteral("Untitled Show") : data.showName;
    painter.drawText(QRectF(body.left(), body.top(), qMax(80.0, logoRight - body.left()), 14.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(show, Qt::ElideRight,
                                                      qMax(80, qRound(logoRight - body.left()))));

    QStringList identity;
    identity << data.performerLabel.trimmed();
    if (!data.performerName.trimmed().isEmpty()
        && data.performerName.trimmed().compare(data.performerLabel.trimmed(), Qt::CaseInsensitive) != 0)
        identity << data.performerName.trimmed();
    QString role = data.instrument.trimmed();
    if (!data.section.trimmed().isEmpty() && data.section.trimmed() != role)
        role += (role.isEmpty() ? QString{} : QStringLiteral(" / ")) + data.section.trimmed();
    if (!role.isEmpty()) identity << role;
    painter.setFont(sheetFont(13.0, true));
    painter.drawText(QRectF(body.left(), body.top() + 17.0, body.width(), 22.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(identity.join(QStringLiteral(" - ")),
                                                      Qt::ElideRight, qRound(body.width())));
    painter.setFont(sheetFont(8.0, true));
    QString range = data.rehearsalRange;
    if (!data.movement.trimmed().isEmpty())
        range = data.movement.trimmed() + (range.isEmpty() ? QString{} : QStringLiteral(" - ") + range);
    painter.drawText(QRectF(body.left(), body.top() + 41.0, body.width(), 16.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(range, Qt::ElideRight, qRound(body.width())));

    constexpr double headerHeight = 62.0;
    constexpr double tableHeaderHeight = 22.0;
    const double tableTop = body.top() + headerHeight;
    const auto visibleColumns = columns(options);
    double totalWeight = 0.0;
    for (const auto &column : visibleColumns) totalWeight += column.weight;
    QVector<double> widths;
    double used = 0.0;
    for (int i = 0; i < visibleColumns.size(); ++i) {
        const double width = i + 1 == visibleColumns.size()
            ? body.width() - used : body.width() * visibleColumns[i].weight / totalWeight;
        widths << width;
        used += width;
    }

    painter.fillRect(QRectF(body.left(), tableTop, body.width(), tableHeaderHeight),
                     QColor(QStringLiteral("#202020")));
    painter.setPen(Qt::white);
    painter.setFont(sheetFont(qMax(6.5, bodyFontSize(options) - 0.5), true));
    double x = body.left();
    for (int i = 0; i < visibleColumns.size(); ++i) {
        painter.drawText(QRectF(x + 4.0, tableTop, widths[i] - 8.0, tableHeaderHeight),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         painter.fontMetrics().elidedText(visibleColumns[i].heading, Qt::ElideRight,
                                                          qMax(1, qRound(widths[i] - 8.0))));
        x += widths[i];
    }

    const int pageCapacity = capacity(pageSize, options);
    const int rowCount = qMin(pageCapacity, data.rows.size());
    const double height = rowHeight(options);
    painter.setFont(sheetFont(bodyFontSize(options)));
    for (int row = 0; row < rowCount; ++row) {
        const double y = tableTop + tableHeaderHeight + row * height;
        if (row % 2 == 1)
            painter.fillRect(QRectF(body.left(), y, body.width(), height),
                             QColor(QStringLiteral("#f1f1f1")));
        painter.setPen(QColor(QStringLiteral("#161616")));
        x = body.left();
        for (int column = 0; column < visibleColumns.size(); ++column) {
            const QString value = data.rows[row].*(visibleColumns[column].member);
            painter.drawText(QRectF(x + 4.0, y, widths[column] - 8.0, height),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             painter.fontMetrics().elidedText(value, Qt::ElideRight,
                                                              qMax(1, qRound(widths[column] - 8.0))));
            x += widths[column];
            if (column + 1 < visibleColumns.size()) {
                painter.setPen(QColor(QStringLiteral("#b9b9b9")));
                painter.drawLine(QPointF(x, y), QPointF(x, y + height));
                painter.setPen(QColor(QStringLiteral("#161616")));
            }
        }
        painter.setPen(QColor(QStringLiteral("#a9a9a9")));
        painter.drawLine(QPointF(body.left(), y + height), QPointF(body.right(), y + height));
    }

    if (capacityExceeded) {
        const double y = tableTop + tableHeaderHeight + qMax(0, pageCapacity - 1) * height;
        painter.fillRect(QRectF(body.left(), y, body.width(), height), QColor(QStringLiteral("#d8d8d8")));
        painter.setPen(Qt::black);
        painter.setFont(sheetFont(bodyFontSize(options), true));
        painter.drawText(QRectF(body.left() + 6.0, y, body.width() - 12.0, height),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("CAPACITY WARNING: %1 rows selected; %2 fit. Choose Compact or narrow the set range.")
                             .arg(data.rows.size()).arg(pageCapacity));
    }

    const double footerTop = body.bottom() - 16.0;
    painter.setPen(QColor(QStringLiteral("#444444")));
    painter.setFont(sheetFont(6.5));
    QString footer = data.rehearsalRange;
    if (!data.revision.isEmpty())
        footer += (footer.isEmpty() ? QString{} : QStringLiteral(" - ")) + data.revision;
    if (!data.company.trimmed().isEmpty())
        footer = data.company.trimmed() + (footer.isEmpty() ? QString{} : QStringLiteral(" - ") + footer);
    painter.drawText(QRectF(body.left(), footerTop, body.width() - 80.0, 13.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(footer, Qt::ElideRight,
                                                      qMax(1, qRound(body.width() - 80.0))));
    painter.setFont(sheetFont(6.5, true));
    painter.drawText(QRectF(body.right() - 75.0, footerTop, 75.0, 13.0),
                     Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("Sheet 1 of 1"));
    painter.restore();
}

} // namespace MarchCraft
