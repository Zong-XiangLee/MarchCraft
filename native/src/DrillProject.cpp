#include "DrillProject.h"

#include <QDir>
#include <QDateTime>
#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QPageSize>
#include <QPdfWriter>
#include <QPolygonF>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QSizeF>
#include <QSet>
#include <QTextStream>
#include <QUrl>
#include <QXmlStreamReader>
#include <QtConcurrent>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <numeric>
#include <utility>

using MarchCraft::DrillSet;
using MarchCraft::AnimationState;
using MarchCraft::Performer;
using MarchCraft::Placement;

class ProjectStateCommand final : public QUndoCommand
{
public:
    ProjectStateCommand(DrillProject *project, QJsonObject before, QJsonObject after,
                        const QString &text)
        : QUndoCommand(text), m_project(project), m_before(std::move(before)),
          m_after(std::move(after))
    {
    }

    void undo() override { m_project->restoreJson(m_before); }

    void redo() override
    {
        if (m_firstRedo) {
            m_firstRedo = false;
            return;
        }
        m_project->restoreJson(m_after);
    }

private:
    DrillProject *m_project;
    QJsonObject m_before;
    QJsonObject m_after;
    bool m_firstRedo = true;
};

namespace {

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

double polylineLength(const QVector<QPointF> &path, bool closed = false)
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

bool writeSqliteProject(const QString &path, const QJsonObject &document, QString *error)
{
    const QString connection = QStringLiteral("marchcraft-write-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool ok = false;
    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        db.setDatabaseName(path);
        if (!db.open()) { if (error) *error = db.lastError().text(); }
        else if (!db.transaction()) { if (error) *error = db.lastError().text(); }
        else {
            QSqlQuery query(db);
            ok = query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS project_document (id INTEGER PRIMARY KEY CHECK(id=1), schema_version INTEGER NOT NULL, json BLOB NOT NULL, updated_utc TEXT NOT NULL)"));
            if (ok) {
                query.prepare(QStringLiteral("INSERT OR REPLACE INTO project_document(id,schema_version,json,updated_utc) VALUES(1,9,?,datetime('now'))"));
                query.addBindValue(QJsonDocument(document).toJson(QJsonDocument::Compact));
                ok = query.exec();
            }
            if (ok) ok = db.commit(); else db.rollback();
            if (!ok && error) *error = query.lastError().text();
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connection);
    return ok;
}

bool readSqliteProject(const QString &path, QJsonObject *document, QString *error)
{
    const QString connection = QStringLiteral("marchcraft-read-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool ok = false;
    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY")); db.setDatabaseName(path);
        if (!db.open()) { if (error) *error = db.lastError().text(); }
        else {
            QSqlQuery query(QStringLiteral("SELECT json FROM project_document WHERE id=1"), db);
            if (query.next()) {
                const auto parsed = QJsonDocument::fromJson(query.value(0).toByteArray());
                if (parsed.isObject()) { *document = parsed.object(); ok = true; }
            }
            if (!ok && error) *error = query.lastError().text().isEmpty()
                ? QStringLiteral("Project database has no readable document") : query.lastError().text();
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connection);
    return ok;
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

} // namespace

DrillProject::DrillProject(QObject *parent)
    : QAbstractListModel(parent)
{
    QSettings settings;
    m_shapePlacementMode = settings.value(QStringLiteral("formation/placementMode"), QStringLiteral("selection")).toString();
    m_showShapeGuides = settings.value(QStringLiteral("view/showShapeGuides"), false).toBool();
    m_showTransitionPaths = settings.value(QStringLiteral("view/showTransitionPaths"), true).toBool();
    m_performerMarkerStyle = settings.value(QStringLiteral("view/performerMarkerStyle"), QStringLiteral("colored")).toString();
    m_performerMarkerSize = qBound(8, settings.value(QStringLiteral("view/performerMarkerSize"), 14).toInt(), 28);
    m_markerGeometry = settings.value(QStringLiteral("view/markerGeometry"), QStringLiteral("circle")).toString();
    m_markerFillColor = settings.value(QStringLiteral("view/markerFillColor"), QStringLiteral("section")).toString();
    m_markerOutlineColor = settings.value(QStringLiteral("view/markerOutlineColor"), QStringLiteral("#e7f5ed")).toString();
    m_markerOutlineWidth = qBound(0, settings.value(QStringLiteral("view/markerOutlineWidth"), 1).toInt(), 5);
    m_markerLabelMode = settings.value(QStringLiteral("view/markerLabelMode"), QStringLiteral("adaptive")).toString();
    m_markerLabelFontSize = qBound(7, settings.value(QStringLiteral("view/markerLabelFontSize"), 10).toInt(), 32);
    m_markerLabelColor = settings.value(QStringLiteral("view/markerLabelColor"), QStringLiteral("#f0f7f3")).toString();
    m_markerFacingVisible = settings.value(QStringLiteral("view/markerFacingVisible"), true).toBool();
    m_markerFacingColor = settings.value(QStringLiteral("view/markerFacingColor"), QStringLiteral("#f8fafc")).toString();
    m_markerWarningColor = settings.value(QStringLiteral("view/markerWarningColor"), QStringLiteral("#fb7185")).toString();
    m_showFieldGrid = settings.value(QStringLiteral("view/showFieldGrid"), false).toBool();
    m_fieldGridInterval = settings.value(QStringLiteral("view/fieldGridInterval"), 1.0).toDouble();
    m_fieldGridColor = settings.value(QStringLiteral("view/fieldGridColor"), QStringLiteral("#7dd3fc")).toString();
    m_fieldGridOpacity = qBound(0.02, settings.value(QStringLiteral("view/fieldGridOpacity"), 0.18).toDouble(), 0.8);
    m_measurementUnit = settings.value(QStringLiteral("view/measurementUnit"), QStringLiteral("steps")).toString();
    m_audioDecoder = new QAudioDecoder(this);
    m_midiWatcher = new QFutureWatcher<MarchCraft::MidiImportResult>(this);
    connect(m_midiWatcher, &QFutureWatcher<MarchCraft::MidiImportResult>::finished, this, [this] {
        const auto result = m_midiWatcher->result();
        if (!result.ok) setStatus(result.error);
        else applyMusicDocument(result.document, QStringLiteral("Import MIDI"));
        emit musicChanged();
    });
    connect(m_audioDecoder, &QAudioDecoder::bufferReady, this, [this] {
        const QAudioBuffer buffer = m_audioDecoder->read();
        if (!buffer.isValid() || buffer.frameCount() <= 0) return;
        const auto format = buffer.format();
        double peak = 0.0;
        const qsizetype samples = buffer.sampleCount();
        if (format.sampleFormat() == QAudioFormat::Float) {
            const float *values = buffer.constData<float>();
            for (qsizetype i = 0; i < samples; ++i) peak = qMax(peak, qAbs(double(values[i])));
        } else if (format.sampleFormat() == QAudioFormat::Int16) {
            const qint16 *values = buffer.constData<qint16>();
            for (qsizetype i = 0; i < samples; ++i) peak = qMax(peak, qAbs(double(values[i])) / 32768.0);
        } else if (format.sampleFormat() == QAudioFormat::Int32) {
            const qint32 *values = buffer.constData<qint32>();
            for (qsizetype i = 0; i < samples; ++i) peak = qMax(peak, qAbs(double(values[i])) / 2147483648.0);
        } else if (format.sampleFormat() == QAudioFormat::UInt8) {
            const quint8 *values = buffer.constData<quint8>();
            for (qsizetype i = 0; i < samples; ++i) peak = qMax(peak, qAbs(double(values[i]) - 128.0) / 128.0);
        }
        m_waveformPeaks.push_back(qBound(0.0, peak, 1.0));
        emit waveformChanged();
    });
    connect(m_audioDecoder, &QAudioDecoder::finished, this, &DrillProject::waveformChanged);
    m_autosaveTimer.setSingleShot(true);
    m_autosaveTimer.setInterval(800);
    connect(&m_autosaveTimer, &QTimer::timeout, this, &DrillProject::autosave);
    connect(&m_undo, &QUndoStack::canUndoChanged, this, &DrillProject::historyChanged);
    connect(&m_undo, &QUndoStack::canRedoChanged, this, &DrillProject::historyChanged);
    loadDemo();
    m_undo.clear();
    m_dirty = false;
}

void DrillProject::setShapePlacementMode(const QString &mode)
{
    static const QSet<QString> valid{QStringLiteral("selection"), QStringLiteral("openSpace"), QStringLiteral("fieldCenter")};
    const QString next = valid.contains(mode) ? mode : QStringLiteral("selection");
    if (next == m_shapePlacementMode) return;
    m_shapePlacementMode = next; QSettings().setValue(QStringLiteral("formation/placementMode"), next);
    emit editorSettingsChanged();
}

void DrillProject::setShowShapeGuides(bool value)
{
    if (value == m_showShapeGuides) return;
    m_showShapeGuides = value; QSettings().setValue(QStringLiteral("view/showShapeGuides"), value);
    emit editorSettingsChanged(); emit shapesChanged();
}

void DrillProject::setShowTransitionPaths(bool value)
{
    if (value == m_showTransitionPaths) return;
    m_showTransitionPaths = value; QSettings().setValue(QStringLiteral("view/showTransitionPaths"), value);
    emit editorSettingsChanged();
}

void DrillProject::setPerformerMarkerStyle(const QString &value)
{
    static const QSet<QString> valid{QStringLiteral("colored"), QStringLiteral("compact"), QStringLiteral("black")};
    const QString next = valid.contains(value) ? value : QStringLiteral("colored");
    if (next == m_performerMarkerStyle) return;
    m_performerMarkerStyle = next;
    QSettings().setValue(QStringLiteral("view/performerMarkerStyle"), next);
    emit editorSettingsChanged();
}

void DrillProject::setPerformerMarkerSize(int value)
{
    value = qBound(8, value, 28);
    if (value == m_performerMarkerSize) return;
    m_performerMarkerSize = value;
    QSettings().setValue(QStringLiteral("view/performerMarkerSize"), value);
    emit editorSettingsChanged();
}

#define EDITOR_STRING_SETTER(Method, Member, Key) \
void DrillProject::Method(const QString &value) { if (value == Member) return; Member = value; QSettings().setValue(QStringLiteral(Key), value); emit editorSettingsChanged(); }
EDITOR_STRING_SETTER(setMarkerFillColor, m_markerFillColor, "view/markerFillColor")
EDITOR_STRING_SETTER(setMarkerOutlineColor, m_markerOutlineColor, "view/markerOutlineColor")
EDITOR_STRING_SETTER(setMarkerLabelMode, m_markerLabelMode, "view/markerLabelMode")
EDITOR_STRING_SETTER(setMarkerLabelColor, m_markerLabelColor, "view/markerLabelColor")
EDITOR_STRING_SETTER(setMarkerFacingColor, m_markerFacingColor, "view/markerFacingColor")
EDITOR_STRING_SETTER(setMarkerWarningColor, m_markerWarningColor, "view/markerWarningColor")
EDITOR_STRING_SETTER(setFieldGridColor, m_fieldGridColor, "view/fieldGridColor")
#undef EDITOR_STRING_SETTER

void DrillProject::setMarkerGeometry(const QString &value)
{
    static const QSet<QString> valid{QStringLiteral("dot"), QStringLiteral("circle"),
        QStringLiteral("square"), QStringLiteral("diamond")};
    const QString next = valid.contains(value) ? value : QStringLiteral("circle");
    if (next == m_markerGeometry) return;
    m_markerGeometry = next;
    QSettings().setValue(QStringLiteral("view/markerGeometry"), next);
    emit editorSettingsChanged();
}

void DrillProject::setMarkerOutlineWidth(int value) { value = qBound(0, value, 5); if (value == m_markerOutlineWidth) return; m_markerOutlineWidth = value; QSettings().setValue(QStringLiteral("view/markerOutlineWidth"), value); emit editorSettingsChanged(); }
void DrillProject::setMarkerLabelFontSize(int value) { value = qBound(7, value, 32); if (value == m_markerLabelFontSize) return; m_markerLabelFontSize = value; QSettings().setValue(QStringLiteral("view/markerLabelFontSize"), value); emit editorSettingsChanged(); }
void DrillProject::setMarkerFacingVisible(bool value) { if (value == m_markerFacingVisible) return; m_markerFacingVisible = value; QSettings().setValue(QStringLiteral("view/markerFacingVisible"), value); emit editorSettingsChanged(); }
void DrillProject::setShowFieldGrid(bool value) { if (value == m_showFieldGrid) return; m_showFieldGrid = value; QSettings().setValue(QStringLiteral("view/showFieldGrid"), value); emit editorSettingsChanged(); }
void DrillProject::setFieldGridInterval(double value) { static const QVector<double> valid{0.25,0.5,1.0,2.0,4.0}; double next=1.0,best=99; for(double v:valid) if(qAbs(v-value)<best){best=qAbs(v-value);next=v;} if(qFuzzyCompare(next,m_fieldGridInterval))return; m_fieldGridInterval=next; QSettings().setValue(QStringLiteral("view/fieldGridInterval"),next); emit editorSettingsChanged(); }
void DrillProject::setFieldGridOpacity(double value) { value=qBound(0.02,value,0.8); if(qFuzzyCompare(value,m_fieldGridOpacity))return; m_fieldGridOpacity=value; QSettings().setValue(QStringLiteral("view/fieldGridOpacity"),value); emit editorSettingsChanged(); }
void DrillProject::setMeasurementUnit(const QString &value) { const QString next=value==QStringLiteral("yards")?value:QStringLiteral("steps");if(next==m_measurementUnit)return;m_measurementUnit=next;QSettings().setValue(QStringLiteral("view/measurementUnit"),next);emit editorSettingsChanged();emit statisticsChanged(); }

void DrillProject::setCapabilityProfile(const QString &value)
{
    static const QSet<QString> valid{QStringLiteral("beginner"), QStringLiteral("intermediate"),
        QStringLiteral("advanced"), QStringLiteral("custom")};
    const QString next = valid.contains(value) ? value : QStringLiteral("intermediate");
    if (next == m_capability.name && next == QStringLiteral("custom")) return;
    const auto before = toJson();
    m_capability = MarchCraft::CapabilityProfile::preset(next);
    invalidateClinic(); emit clinicChanged(); commitSnapshot(before, QStringLiteral("Change ensemble capability profile"));
}

void DrillProject::setMaximumStepsPerCount(double value)
{
    value = qBound(0.25, value, 4.0); if (qFuzzyCompare(value, m_capability.maximumStepsPerCount)) return;
    const auto before = toJson(); m_capability.name = QStringLiteral("custom"); m_capability.maximumStepsPerCount = value;
    invalidateClinic(); emit clinicChanged(); commitSnapshot(before, QStringLiteral("Change stride limit"));
}

void DrillProject::setCollisionClearance(double value)
{
    value = qBound(0.25, value, 8.0); if (qFuzzyCompare(value, m_capability.collisionClearance)) return;
    const auto before = toJson(); m_capability.name = QStringLiteral("custom"); m_capability.collisionClearance = value;
    invalidateClinic(); emit clinicChanged(); commitSnapshot(before, QStringLiteral("Change collision clearance"));
}

void DrillProject::setDirectionChangeDegrees(double value)
{
    value = qBound(15.0, value, 180.0); if (qFuzzyCompare(value, m_capability.directionChangeDegrees)) return;
    const auto before = toJson(); m_capability.name = QStringLiteral("custom"); m_capability.directionChangeDegrees = value;
    invalidateClinic(); emit clinicChanged(); commitSnapshot(before, QStringLiteral("Change direction limit"));
}

void DrillProject::setVenuePreset(const QString &value)
{
    static const QSet<QString> valid{QStringLiteral("venue.rehearsal"), QStringLiteral("venue.high_school"),
                                     QStringLiteral("venue.bowl"), QStringLiteral("venue.gym"),
                                     QStringLiteral("venue.arena")};
    const QString next = valid.contains(value) ? value : QStringLiteral("venue.rehearsal");
    if (next == m_venue.venueId) return;
    const auto before = toJson(); m_venue.venueId = next; emit sceneChanged(); commitSnapshot(before, QStringLiteral("Change venue"));
}

void DrillProject::setLightingPreset(const QString &value)
{
    static const QSet<QString> valid{QStringLiteral("lighting.daylight"), QStringLiteral("lighting.overcast"),
                                     QStringLiteral("lighting.sunset"), QStringLiteral("lighting.night"),
                                     QStringLiteral("lighting.indoor")};
    const QString next = valid.contains(value) ? value : QStringLiteral("lighting.daylight");
    if (next == m_venue.lightingId) return;
    const auto before = toJson(); m_venue.lightingId = next; emit sceneChanged(); commitSnapshot(before, QStringLiteral("Change lighting"));
}

void DrillProject::setGraphicsProfile(const QString &value)
{
    static const QSet<QString> valid{QStringLiteral("automatic"), QStringLiteral("performance"),
                                     QStringLiteral("balanced"), QStringLiteral("presentation")};
    const QString next = valid.contains(value) ? value : QStringLiteral("automatic");
    if (next == m_venue.graphicsProfile) return;
    const auto before = toJson(); m_venue.graphicsProfile = next; emit sceneChanged(); commitSnapshot(before, QStringLiteral("Change graphics quality"));
}

void DrillProject::setVenuePrimaryColor(const QString &value)
{
    const QColor next(value); if (!next.isValid() || next == m_venue.primaryColor) return;
    const auto before = toJson(); m_venue.primaryColor = next; emit sceneChanged(); commitSnapshot(before, QStringLiteral("Change venue color"));
}

void DrillProject::setVenueSecondaryColor(const QString &value)
{
    const QColor next(value); if (!next.isValid() || next == m_venue.secondaryColor) return;
    const auto before = toJson(); m_venue.secondaryColor = next; emit sceneChanged(); commitSnapshot(before, QStringLiteral("Change venue color"));
}

void DrillProject::setTurfColor(const QString &value)
{
    const QColor next(value); if (!next.isValid() || next == m_venue.turfColor) return;
    const auto before = toJson(); m_venue.turfColor = next; emit sceneChanged(); commitSnapshot(before, QStringLiteral("Change surface color"));
}

void DrillProject::setScoreboardText(const QString &value)
{
    const QString next = value.simplified().left(40); if (next == m_venue.scoreboardText) return;
    const auto before = toJson(); m_venue.scoreboardText = next; emit sceneChanged(); commitSnapshot(before, QStringLiteral("Change scoreboard"));
}

void DrillProject::setCrowdDensity(double value)
{
    value = qBound(0.0, value, 1.0); if (qFuzzyCompare(value, m_venue.crowdDensity)) return;
    const auto before = toJson(); m_venue.crowdDensity = value; emit sceneChanged(); commitSnapshot(before, QStringLiteral("Change crowd density"));
}

void DrillProject::setDebug3D(bool value)
{
    if (value == m_venue.debugOverlay) return;
    m_venue.debugOverlay = value; emit sceneChanged();
}

QVariantList DrillProject::props() const
{
    QVariantList result;
    for (const auto &prop : m_props) {
        const auto world = MarchCraft::FieldTransform::drillToWorld(prop.position, fieldDepthSteps());
        const QSizeF footprint = propFootprintSteps(prop);
        result.push_back(QVariantMap{{QStringLiteral("id"), prop.id},
            {QStringLiteral("definitionId"), prop.definitionId}, {QStringLiteral("fieldX"), prop.position.x()},
            {QStringLiteral("fieldY"), prop.position.y()}, {QStringLiteral("worldX"), world.x()},
            {QStringLiteral("worldZ"), world.z()}, {QStringLiteral("rotation"), prop.rotation},
            {QStringLiteral("scaleX"), prop.scale.x()}, {QStringLiteral("scaleY"), prop.scale.y()},
            {QStringLiteral("scaleZ"), prop.scale.z()}, {QStringLiteral("appearanceVariant"), prop.appearanceVariant},
            {QStringLiteral("moverCount"), prop.assignedMoverIds.size()}, {QStringLiteral("label"), propDisplayName(prop)},
            {QStringLiteral("widthSteps"), footprint.width()}, {QStringLiteral("depthSteps"), footprint.height()},
            {QStringLiteral("highlighted"), m_highlightedClinicProps.contains(prop.id)}});
    }
    return result;
}

QString DrillProject::formatDistance(double steps, int precision) const
{
    const bool yards=m_measurementUnit==QStringLiteral("yards");
    const double value=yards?steps/1.6:steps;
    return QStringLiteral("%1 %2").arg(QString::number(value,'f',qBound(0,precision,3)),yards?QStringLiteral("yd"):QStringLiteral("st"));
}

int DrillProject::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_performers.size();
}

QVariant DrillProject::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_performers.size())
        return {};
    const auto &performer = m_performers.at(index.row());
    const auto placement = placementAt(index.row(), m_currentSet);
    const auto from = placementAt(index.row(), qMax(0, m_currentSet - 1));
    const auto displayed = m_playbackActive ? interpolatedPosition(index.row()) : placement.position;
    switch (role) {
    case IdRole: return performer.id;
    case LabelRole: return performer.label;
    case NameRole: return performer.name;
    case SectionRole: return performer.section;
    case InstrumentRole: return performer.instrument;
    case SymbolRole: return performer.symbol;
    case ColorRole: return performer.color;
    case NotesRole: return performer.notes;
    case XRole: return displayed.x();
    case YRole: return displayed.y();
    case FromXRole: return from.position.x();
    case FromYRole: return from.position.y();
    case FacingRole: return m_playbackActive ? interpolatedFacing(index.row()) : placement.facing;
    case SelectedRole: return performer.selected;
    case SetDistanceRole:
        ensureAnalyticsCache(); return m_cachedSetDistances.value(index.row());
    case TravelHeadingRole: return cachedAnimationStateAt(index.row()).travelDirectionDegrees;
    case TravelStepsPerCountRole: return cachedAnimationStateAt(index.row()).travelStepsPerCount;
    case LocomotionModeRole: return cachedAnimationStateAt(index.row()).locomotion;
    case GaitPhaseRole: return cachedAnimationStateAt(index.row()).normalizedTime;
    case TravelPathTypeRole: return m_currentSet > 0 ? placement.pathType : QStringLiteral("direct");
    case ClosingTransitionRole: return cachedAnimationStateAt(index.row()).closesAtDestination;
    case TotalDistanceRole: return performerTotalDistance(index.row());
    case WarningRole: return performerHasWarning(index.row());
    case VisibleRole: return performer.visible;
    case LockedRole: return performer.locked;
    case BodyRigRole: return performer.appearance.bodyRigId;
    case UniformRole: return performer.appearance.uniformId;
    case SkinPaletteRole: return performer.appearance.skinPaletteId;
    case InstrumentAssetRole: return performer.appearance.instrumentAssetId;
    case EquipmentAssetRole: return performer.appearance.equipmentAssetId;
    case PerformerHeightRole: return performer.appearance.heightMeters;
    case PerformerRoleRole: return performer.appearance.roleId;
    default: return {};
    }
}

bool DrillProject::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_performers.size())
        return false;
    const auto before = toJson();
    auto &performer = m_performers[index.row()];
    switch (role) {
    case LabelRole: performer.label = value.toString(); break;
    case NameRole: performer.name = value.toString(); break;
    case SectionRole: performer.section = value.toString(); break;
    case InstrumentRole: performer.instrument = value.toString(); break;
    case SymbolRole: performer.symbol = value.toString().left(2); break;
    case ColorRole: performer.color = value.value<QColor>(); break;
    case NotesRole: performer.notes = value.toString(); break;
    case VisibleRole: performer.visible = value.toBool(); break;
    case LockedRole: performer.locked = value.toBool(); break;
    case BodyRigRole: performer.appearance.bodyRigId = value.toString(); break;
    case UniformRole: performer.appearance.uniformId = value.toString(); break;
    case SkinPaletteRole: performer.appearance.skinPaletteId = value.toString(); break;
    case InstrumentAssetRole: performer.appearance.instrumentAssetId = value.toString(); break;
    case EquipmentAssetRole: performer.appearance.equipmentAssetId = value.toString(); break;
    case PerformerHeightRole: performer.appearance.heightMeters = qBound(1.1, value.toDouble(), 2.25); break;
    case PerformerRoleRole: performer.appearance.roleId = value.toString(); break;
    default: return false;
    }
    emit dataChanged(index, index, {role});
    commitSnapshot(before, QStringLiteral("Edit performer"));
    return true;
}

Qt::ItemFlags DrillProject::flags(const QModelIndex &index) const
{
    return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

QHash<int, QByteArray> DrillProject::roleNames() const
{
    return {{IdRole, "performerId"}, {LabelRole, "label"}, {NameRole, "performerName"},
            {SectionRole, "section"}, {InstrumentRole, "instrument"}, {SymbolRole, "symbol"},
            {ColorRole, "performerColor"}, {NotesRole, "notes"}, {XRole, "fieldX"},
            {YRole, "fieldY"}, {FromXRole, "fromX"}, {FromYRole, "fromY"},
            {FacingRole, "facing"}, {SelectedRole, "isSelected"},
            {SetDistanceRole, "setDistance"}, {TravelHeadingRole, "travelHeading"},
            {TravelStepsPerCountRole, "travelStepsPerCount"},
            {LocomotionModeRole, "locomotionMode"}, {GaitPhaseRole, "gaitPhase"},
            {TravelPathTypeRole, "travelPathType"},
            {ClosingTransitionRole, "closingTransition"},
            {TotalDistanceRole, "totalDistance"},
            {WarningRole, "hasWarning"}, {VisibleRole, "performerVisible"},
            {LockedRole, "performerLocked"}, {BodyRigRole, "bodyRigId"},
            {UniformRole, "uniformId"}, {SkinPaletteRole, "skinPaletteId"},
            {InstrumentAssetRole, "instrumentAssetId"}, {EquipmentAssetRole, "equipmentAssetId"},
            {PerformerHeightRole, "performerHeightMeters"}, {PerformerRoleRole, "performerRole"}};
}

void DrillProject::setShowName(const QString &value)
{
    if (value == m_showName || value.trimmed().isEmpty())
        return;
    const auto before = toJson();
    m_showName = value.trimmed();
    emit projectChanged();
    commitSnapshot(before, QStringLiteral("Rename show"));
}

void DrillProject::setFieldPreset(const QString &value)
{
    if (value == m_fieldPreset)
        return;
    const auto before = toJson();
    m_fieldPreset = MarchCraft::fieldGeometry(value).id;
    auto clampSets = [this](QVector<DrillSet> &sets) {
        for (auto &set : sets) {
            for (auto &variant : set.variants)
                for (auto it = variant.placements.begin(); it != variant.placements.end(); ++it)
                    it->position = clampPosition(it->position);
            for (auto &variant : set.archivedVariants)
                for (auto it = variant.placements.begin(); it != variant.placements.end(); ++it)
                    it->position = clampPosition(it->position);
        }
    };
    clampSets(m_sets);
    clampSets(m_archivedSets);
    emit projectChanged();
    emitAllDataChanged();
    commitSnapshot(before, QStringLiteral("Change field"));
}

double DrillProject::fieldDepthSteps() const
{
    return MarchCraft::fieldGeometry(m_fieldPreset).depth;
}

double DrillProject::fieldInsertStep(int column) const
{
    if (column < 0 || column >= fieldInsertCount())
        return -1.0;
    return (column / 4) * 8.0 + (column % 4 + 1) * 1.6;
}

double DrillProject::frontHashSteps() const
{
    return MarchCraft::fieldGeometry(m_fieldPreset).frontHash;
}

double DrillProject::backHashSteps() const
{
    return MarchCraft::fieldGeometry(m_fieldPreset).backHash;
}

void DrillProject::setCurrentSetIndex(int value)
{
    if (m_sets.isEmpty())
        value = -1;
    else
        value = std::clamp(value, 0, static_cast<int>(m_sets.size()) - 1);
    if (m_currentSet == value)
        return;
    m_currentSet = value;
    m_analyticsValid = false;
    m_playhead = 0.0;
    emit currentSetChanged();
    emit playheadChanged();
    emitAllDataChanged();
    cancelFormationPreview();
    m_clinicIssues.clear(); analyzeTransition(m_currentSet);
}

void DrillProject::selectSetRange(int index, bool extend)
{
    if (m_sets.isEmpty()) return;
    index = qBound(0, index, m_sets.size() - 1);
    if (extend) m_selectedSetEnd = index;
    else m_selectedSetStart = m_selectedSetEnd = index;
    setCurrentSetIndex(index);
    emit setRangeChanged();
}

void DrillProject::setPlaybackSource(const QString &value)
{
    const QString next = value == QStringLiteral("rehearsal") || value == QStringLiteral("mute")
        ? value : QStringLiteral("midi");
    if (m_playbackSource == next) return;
    m_playbackSource = next; emit transportSettingsChanged(); markDirty(QStringLiteral("Playback source changed")); emit projectChanged();
}

void DrillProject::setMidiMasterVolume(double value)
{
    value = qBound(0.0, value, 1.0); if (qFuzzyCompare(m_midiMasterVolume, value)) return;
    m_midiMasterVolume = value; emit transportSettingsChanged(); markDirty(QStringLiteral("MIDI volume changed")); emit projectChanged();
}

void DrillProject::setLoopEnabled(bool value)
{
    if (m_loopEnabled == value) return; m_loopEnabled = value;
    emit transportSettingsChanged(); markDirty(QStringLiteral("Loop setting changed")); emit projectChanged();
}

QString DrillProject::currentSetName() const
{
    return m_currentSet >= 0 && m_currentSet < m_sets.size()
        ? m_sets[m_currentSet].activeVariant().name : QString();
}

int DrillProject::currentVariantCount() const
{
    return m_currentSet >= 0 && m_currentSet < m_sets.size()
        ? m_sets[m_currentSet].variants.size() : 0;
}

int DrillProject::currentShapeCount() const
{
    return m_currentSet >= 0 && m_currentSet < m_sets.size()
        ? m_sets[m_currentSet].activeVariant().shapes.size() : 0;
}

int DrillProject::currentVariantIndex() const
{
    return m_currentSet >= 0 && m_currentSet < m_sets.size()
        ? m_sets[m_currentSet].activeVariantIndex() : -1;
}

int DrillProject::currentArchivedVariantCount() const
{
    return m_currentSet >= 0 && m_currentSet < m_sets.size()
        ? m_sets[m_currentSet].archivedVariants.size() : 0;
}

int DrillProject::currentSetCounts() const
{
    if (m_currentSet == 0) return m_openingBehavior == QStringLiteral("hold") ? m_openingCounts : 0;
    return m_currentSet >= 0 && m_currentSet < m_sets.size() ? m_sets[m_currentSet].counts : 0;
}

void DrillProject::setPlayhead(double value)
{
    value = std::clamp(value, 0.0, 1.0);
    if (qFuzzyCompare(value, m_playhead))
        return;
    m_playhead = value;
    emit playheadChanged();
    emitAllDataChanged();
}

void DrillProject::setPlaybackActive(bool value)
{
    if (m_playbackActive == value)
        return;
    m_playbackActive = value;
    emit playbackActiveChanged();
    emitAllDataChanged();
}

void DrillProject::setAudioSource(const QString &value)
{
    const QString path = localPath(value);
    if (path == m_audioSource)
        return;
    const auto before = toJson();
    m_audioSource = path;
    startWaveformDecode();
    emit projectChanged();
    emit musicChanged();
    commitSnapshot(before, QStringLiteral("Change audio"));
}

void DrillProject::attachAudio(const QString &urlOrPath)
{
    setAudioSource(urlOrPath);
    if (!m_audioSource.isEmpty()) setPlaybackSource(QStringLiteral("rehearsal"));
}

void DrillProject::startWaveformDecode()
{
    m_audioDecoder->stop(); m_waveformPeaks.clear(); emit waveformChanged();
    if (m_audioSource.isEmpty() || !QFileInfo::exists(m_audioSource)) return;
    m_audioDecoder->setSource(QUrl::fromLocalFile(m_audioSource)); m_audioDecoder->start();
}

double DrillProject::waveformPeak(int index) const
{
    return index >= 0 && index < m_waveformPeaks.size() ? m_waveformPeaks[index] : 0.0;
}

void DrillProject::addAudioAnchor(double audioMs, qint64 musicTick)
{
    if (!m_music.loaded()) return;
    const auto before = toJson();
    MarchCraft::AudioAnchor anchor{qMax(0.0, audioMs), qBound<qint64>(0, musicTick, m_music.durationTick)};
    auto it = std::lower_bound(m_music.audioAnchors.begin(), m_music.audioAnchors.end(), anchor.audioMs,
        [](const MarchCraft::AudioAnchor &existing, double value) { return existing.audioMs < value; });
    if (it != m_music.audioAnchors.end() && qAbs(it->audioMs - anchor.audioMs) < 1.0) *it = anchor;
    else m_music.audioAnchors.insert(it, anchor);
    emit musicChanged(); commitSnapshot(before, QStringLiteral("Add audio anchor"));
}

void DrillProject::clearAudioAnchors()
{
    if (m_music.audioAnchors.isEmpty()) return;
    const auto before = toJson(); m_music.audioAnchors.clear(); emit musicChanged();
    commitSnapshot(before, QStringLiteral("Clear audio anchors"));
}

qint64 DrillProject::musicTickForAudioMs(double audioMs) const
{
    const double adjusted = audioMs - m_audioOffsetMs;
    const auto &anchors = m_music.audioAnchors;
    if (anchors.size() < 2) {
        if (anchors.size() == 1)
            return m_music.tickAtMilliseconds(qMax(0.0, m_music.millisecondsAt(anchors.first().musicTick)
                + adjusted - anchors.first().audioMs));
        return m_music.tickAtMilliseconds(qMax(0.0, adjusted));
    }
    int right = 1;
    while (right < anchors.size() && anchors[right].audioMs < adjusted) ++right;
    right = qBound(1, right, anchors.size() - 1); const auto &a = anchors[right - 1]; const auto &b = anchors[right];
    const double ratio = qFuzzyCompare(a.audioMs, b.audioMs) ? 0.0 : (adjusted - a.audioMs) / (b.audioMs - a.audioMs);
    return qBound<qint64>(0, qRound64(a.musicTick + ratio * (b.musicTick - a.musicTick)), m_music.durationTick);
}

double DrillProject::audioMsForMusicTick(qint64 tick) const
{
    tick = qBound<qint64>(0, tick, m_music.durationTick); const auto &anchors = m_music.audioAnchors;
    if (anchors.size() < 2) {
        if (anchors.size() == 1)
            return m_audioOffsetMs + anchors.first().audioMs
                + m_music.millisecondsAt(tick) - m_music.millisecondsAt(anchors.first().musicTick);
        return m_audioOffsetMs + m_music.millisecondsAt(tick);
    }
    int right = 1; while (right < anchors.size() && anchors[right].musicTick < tick) ++right;
    right = qBound(1, right, anchors.size() - 1); const auto &a=anchors[right-1]; const auto &b=anchors[right];
    const double ratio = a.musicTick == b.musicTick ? 0.0 : double(tick-a.musicTick)/double(b.musicTick-a.musicTick);
    return m_audioOffsetMs + a.audioMs + ratio * (b.audioMs-a.audioMs);
}

void DrillProject::setBpm(double value)
{
    value = std::clamp(value, 20.0, 400.0);
    if (qFuzzyCompare(value, m_bpm))
        return;
    const auto before = toJson();
    m_bpm = value;
    if (m_tempoRegions.size() == 1 && m_tempoRegions.first().startTick == 0) {
        m_tempoRegions.first().startBpm = value;
        m_tempoRegions.first().endBpm = value;
    }
    emit projectChanged();
    emit timingChanged();
    commitSnapshot(before, QStringLiteral("Change tempo"));
}

bool DrillProject::midiImporting() const
{
    return m_midiWatcher && m_midiWatcher->isRunning();
}

void DrillProject::setAudioOffsetMs(double value)
{
    value = qBound(-3600000.0, value, 3600000.0);
    if (qFuzzyCompare(value, m_audioOffsetMs)) return;
    const auto before = toJson(); m_audioOffsetMs = value;
    emit musicChanged(); commitSnapshot(before, QStringLiteral("Adjust audio offset"));
}

bool DrillProject::importMidi(const QString &urlOrPath)
{
    const auto result = MarchCraft::parseMidiFile(localPath(urlOrPath));
    if (!result.ok) { setStatus(result.error); return false; }
    applyMusicDocument(result.document, QStringLiteral("Import MIDI"));
    return true;
}

void DrillProject::importMidiAsync(const QString &urlOrPath)
{
    if (m_midiWatcher->isRunning()) { setStatus(QStringLiteral("A MIDI import is already running")); return; }
    const QString path = localPath(urlOrPath);
    setStatus(QStringLiteral("Reading MIDI…")); emit musicChanged();
    m_midiWatcher->setFuture(QtConcurrent::run([path] { return MarchCraft::parseMidiFile(path); }));
}

void DrillProject::applyMusicDocument(MarchCraft::MusicDocument document, const QString &undoText)
{
    const auto before = toJson();
    m_music = std::move(document);
    if (m_music.sourceType == QStringLiteral("midi")) setPlaybackSource(QStringLiteral("midi"));
    m_musicSelectionStart = m_music.measures.isEmpty() ? -1 : 0;
    m_musicSelectionEnd = m_musicSelectionStart;
    rebuildTimingFromMusic();
    emit projectChanged(); emit timingChanged(); emit musicChanged();
    commitSnapshot(before, undoText);
    setStatus(QStringLiteral("Imported %1 measures, %2 tracks, %3 tempo events")
        .arg(m_music.measures.size()).arg(m_music.tracks.size()).arg(m_music.tempos.size()));
}

void DrillProject::rebuildTimingFromMusic()
{
    if (!m_music.loaded()) return;
    m_meterRegions.clear();
    for (int i = 0; i < m_music.meters.size(); ++i) {
        const auto &event = m_music.meters[i];
        const qint64 end = i + 1 < m_music.meters.size() ? m_music.meters[i + 1].tick
                                                        : std::numeric_limits<qint64>::max();
        m_meterRegions.push_back({event.tick, end, event.numerator, event.denominator,
                                  event.pulseTicks, event.grouping});
    }
    m_tempoRegions.clear();
    for (int i = 0; i < m_music.tempos.size(); ++i) {
        const auto &event = m_music.tempos[i];
        const qint64 end = i + 1 < m_music.tempos.size() ? m_music.tempos[i + 1].tick
                                                        : std::numeric_limits<qint64>::max();
        m_tempoRegions.push_back({event.tick, end, event.bpm, event.bpm, QStringLiteral("MIDI tempo")});
    }
    if (!m_music.tempos.isEmpty()) m_bpm = m_music.tempos.first().bpm;
}

QVariantMap DrillProject::musicMeasureInfo(int index) const
{
    if (index < 0 || index >= m_music.measures.size()) return {};
    const auto &measure = m_music.measures[index];
    double bpm = m_bpm;
    for (const auto &tempo : m_music.tempos) { if (tempo.tick > measure.startTick) break; bpm = tempo.bpm; }
    int setIndex = -1;
    for (int i = 0; i < m_sets.size(); ++i)
        if (m_sets[i].startTick >= measure.startTick && m_sets[i].startTick <= measure.endTick) { setIndex = i; break; }
    const int rangeA=m_sets.isEmpty()?0:qBound(0,qMin(m_selectedSetStart,m_selectedSetEnd),m_sets.size()-1);
    const int rangeB=m_sets.isEmpty()?0:qBound(0,qMax(m_selectedSetStart,m_selectedSetEnd),m_sets.size()-1);
    const qint64 rangeStart = m_sets.isEmpty() ? -1 : m_sets[rangeA].startTick;
    const qint64 rangeEnd = m_sets.isEmpty() ? -1 : m_sets[rangeB].startTick;
    return {{QStringLiteral("index"), index}, {QStringLiteral("number"), measure.displayNumber},
            {QStringLiteral("startTick"), measure.startTick}, {QStringLiteral("endTick"), measure.endTick},
            {QStringLiteral("numerator"), measure.numerator}, {QStringLiteral("denominator"), measure.denominator},
            {QStringLiteral("counts"), measure.counts}, {QStringLiteral("noteCount"), measure.noteCount},
            {QStringLiteral("density"), qMin(1.0, measure.noteCount / 160.0)},
            {QStringLiteral("tempo"), bpm}, {QStringLiteral("partial"), measure.partial},
            {QStringLiteral("selected"), index >= qMin(m_musicSelectionStart, m_musicSelectionEnd)
                && index <= qMax(m_musicSelectionStart, m_musicSelectionEnd)},
            {QStringLiteral("setIndex"), setIndex},
            {QStringLiteral("inSetRange"), rangeEnd > rangeStart && measure.endTick > rangeStart && measure.startTick < rangeEnd}};
}

QVariantMap DrillProject::musicTrackInfo(int index) const
{
    if (index < 0 || index >= m_music.tracks.size()) return {};
    const auto &track = m_music.tracks[index];
    return {{QStringLiteral("index"), track.index}, {QStringLiteral("name"), track.name},
            {QStringLiteral("noteCount"), track.noteCount}, {QStringLiteral("channel"), track.channel},
            {QStringLiteral("program"), track.program}, {QStringLiteral("selected"), track.selected},
            {QStringLiteral("muted"), track.muted}, {QStringLiteral("solo"), track.solo},
            {QStringLiteral("volume"), track.volume}};
}

void DrillProject::setMusicTrackSelected(int index, bool selected)
{
    if (index < 0 || index >= m_music.tracks.size() || m_music.tracks[index].selected == selected) return;
    const auto before = toJson(); m_music.tracks[index].selected = selected;
    emit musicChanged(); commitSnapshot(before, QStringLiteral("Choose score tracks"));
}

void DrillProject::setMusicTrackMuted(int index, bool muted)
{
    if (index < 0 || index >= m_music.tracks.size() || m_music.tracks[index].muted == muted) return;
    const auto before=toJson(); m_music.tracks[index].muted=muted; emit musicChanged();
    commitSnapshot(before, QStringLiteral("Mute MIDI track"));
}

void DrillProject::setMusicTrackSolo(int index, bool solo)
{
    if (index < 0 || index >= m_music.tracks.size() || m_music.tracks[index].solo == solo) return;
    const auto before=toJson(); m_music.tracks[index].solo=solo; emit musicChanged();
    commitSnapshot(before, QStringLiteral("Solo MIDI track"));
}

void DrillProject::setMusicTrackVolume(int index, double volume)
{
    if (index < 0 || index >= m_music.tracks.size()) return; volume=qBound(0.0,volume,1.0);
    if (qFuzzyCompare(m_music.tracks[index].volume,volume)) return;
    const auto before=toJson(); m_music.tracks[index].volume=volume; emit musicChanged();
    commitSnapshot(before, QStringLiteral("Change MIDI track volume"));
}

void DrillProject::setMusicSelection(int startMeasure, int endMeasure)
{
    if (m_music.measures.isEmpty()) return;
    startMeasure = qBound(0, startMeasure, m_music.measures.size() - 1);
    endMeasure = qBound(0, endMeasure, m_music.measures.size() - 1);
    if (startMeasure == m_musicSelectionStart && endMeasure == m_musicSelectionEnd) return;
    m_musicSelectionStart = startMeasure; m_musicSelectionEnd = endMeasure; emit musicChanged();
}

QVariantList DrillProject::previewSetGeneration(int startMeasure, int endMeasure,
                                                 const QString &mode, int subdivision,
                                                 double stepMultiplier) const
{
    QVariantList result;
    if (m_music.measures.isEmpty()) return result;
    startMeasure = qBound(0, qMin(startMeasure, endMeasure), m_music.measures.size() - 1);
    endMeasure = qBound(startMeasure, qMax(startMeasure, endMeasure), m_music.measures.size() - 1);
    subdivision = qBound(1, subdivision, 256); stepMultiplier = qBound(0.0, stepMultiplier, 2.0);
    const qint64 selectionStart = m_music.measures[startMeasure].startTick;
    const qint64 selectionEnd = m_music.measures[endMeasure].endTick;
    QVector<qint64> boundaries{selectionStart};
    if (mode == QStringLiteral("oneMove")) boundaries.push_back(selectionEnd);
    else {
        qint64 cursor = selectionStart;
        while (cursor < selectionEnd) {
            const qint64 next = qMin(selectionEnd, advancePulses(cursor, subdivision));
            if (next <= cursor) break;
            boundaries.push_back(next); cursor = next;
        }
        if (boundaries.last() != selectionEnd) boundaries.push_back(selectionEnd);
    }
    for (int i = 1; i < boundaries.size(); ++i) {
        const int first = musicMeasureAtTick(boundaries[i - 1]);
        const int last = musicMeasureAtTick(qMax<qint64>(boundaries[i - 1], boundaries[i] - 1));
        const int counts = qMax(1, pulsesBetween(boundaries[i - 1], boundaries[i]));
        result.push_back(QVariantMap{{QStringLiteral("startTick"), boundaries[i - 1]},
            {QStringLiteral("endTick"), boundaries[i]}, {QStringLiteral("startMeasure"), first + m_music.firstMeasureNumber},
            {QStringLiteral("endMeasure"), last + m_music.firstMeasureNumber}, {QStringLiteral("counts"), counts},
            {QStringLiteral("durationMs"), millisecondsBetween(boundaries[i - 1], boundaries[i])},
            {QStringLiteral("stepMultiplier"), stepMultiplier}, {QStringLiteral("steps"), counts * stepMultiplier}});
    }
    return result;
}

bool DrillProject::commitSetGeneration(int startMeasure, int endMeasure, const QString &mode,
                                        int subdivision, double stepMultiplier)
{
    return commitSetGenerationPlan(previewSetGeneration(startMeasure, endMeasure, mode, subdivision, stepMultiplier));
}

bool DrillProject::commitSetGenerationPlan(const QVariantList &segments)
{
    if (segments.isEmpty()) return false;
    qint64 previousEnd = -1;
    for (const auto &value : segments) {
        const auto segment = value.toMap();
        const qint64 start = segment.value(QStringLiteral("startTick")).toLongLong();
        const qint64 end = segment.value(QStringLiteral("endTick")).toLongLong();
        if (end <= start || (previousEnd >= 0 && start != previousEnd)) {
            setStatus(QStringLiteral("Set-generation segments must be contiguous and chronological")); return false;
        }
        previousEnd = end;
    }
    const auto before = toJson();
    auto insertMarker = [this](qint64 tick, double multiplier, const QString &measureText) {
        for (int i = 0; i < m_sets.size(); ++i) if (m_sets[i].startTick == tick) return i;
        int insertAt = 0; while (insertAt < m_sets.size() && m_sets[insertAt].startTick < tick) ++insertAt;
        const int source = qBound(0, insertAt - 1, m_sets.size() - 1);
        DrillSet set;
        set.startTick = tick; set.stepMultiplier = multiplier; set.measure = measureText;
        set.activeVariant().name = QStringLiteral("Music set %1").arg(insertAt + 1);
        if (!m_sets.isEmpty()) set.activeVariant().placements = m_sets[source].activeVariant().placements;
        m_sets.insert(insertAt, set); return insertAt;
    };
    const auto first = segments.first().toMap();
    insertMarker(first.value(QStringLiteral("startTick")).toLongLong(), 0.0,
                 QString::number(first.value(QStringLiteral("startMeasure")).toInt()));
    int lastIndex = 0;
    for (const auto &value : segments) {
        const auto segment = value.toMap();
        const double multiplier = qBound(0.0, segment.value(QStringLiteral("stepMultiplier"), 1.0).toDouble(), 2.0);
        lastIndex = insertMarker(segment.value(QStringLiteral("endTick")).toLongLong(), multiplier,
            QStringLiteral("%1-%2").arg(segment.value(QStringLiteral("startMeasure")).toInt())
                                      .arg(segment.value(QStringLiteral("endMeasure")).toInt()));
    }
    for (int i = 0; i < m_sets.size(); ++i) m_sets[i].number = QString::number(i + 1);
    if (!m_sets.isEmpty()) m_sets.first().stepMultiplier = 0.0;
    m_currentSet = qBound(0, lastIndex, m_sets.size() - 1); recalculateCounts();
    emit setsChanged(); emit currentSetChanged(); emitAllDataChanged();
    commitSnapshot(before, QStringLiteral("Create sets from music")); return true;
}

QVariantList DrillProject::previewSetMapping() const
{
    QVariantList result;
    for (int i = 0; i < m_sets.size(); ++i) {
        const int measure = musicMeasureAtTick(m_sets[i].startTick);
        result.push_back(QVariantMap{{QStringLiteral("setIndex"), i}, {QStringLiteral("setName"), m_sets[i].activeVariant().name},
            {QStringLiteral("measureIndex"), measure}, {QStringLiteral("measureNumber"), measure >= 0 ? measure + m_music.firstMeasureNumber : 0},
            {QStringLiteral("startTick"), m_sets[i].startTick}});
    }
    return result;
}

bool DrillProject::applySetMapping(const QVariantList &measureIndices)
{
    if (measureIndices.size() != m_sets.size() || m_music.measures.isEmpty()) return false;
    QVector<int> indices; indices.reserve(measureIndices.size());
    for (const auto &value : measureIndices) indices.push_back(qBound(0, value.toInt(), m_music.measures.size() - 1));
    if (!std::is_sorted(indices.cbegin(), indices.cend())
        || std::adjacent_find(indices.cbegin(), indices.cend()) != indices.cend()) {
        setStatus(QStringLiteral("Each set must map to a later measure")); return false;
    }
    const auto before = toJson();
    for (int i = 0; i < m_sets.size(); ++i) {
        m_sets[i].startTick = m_music.measures[indices[i]].startTick;
        m_sets[i].measure = QString::number(m_music.measures[indices[i]].displayNumber);
    }
    recalculateCounts(); emit setsChanged(); emit timingChanged(); emit musicChanged();
    commitSnapshot(before, QStringLiteral("Map sets to music")); return true;
}

void DrillProject::setSetStepMultiplier(int setIndex, double multiplier)
{
    if (setIndex < 0 || setIndex >= m_sets.size()) return;
    multiplier = qBound(0.0, multiplier, 2.0);
    if (qFuzzyCompare(m_sets[setIndex].stepMultiplier, multiplier)) return;
    const auto before = toJson(); m_sets[setIndex].stepMultiplier = multiplier;
    m_analyticsValid = false; emit setsChanged(); emit statisticsChanged();
    commitSnapshot(before, QStringLiteral("Change marching step mode"));
}

int DrillProject::musicMeasureAtTick(qint64 tick) const
{
    if (m_music.measures.isEmpty()) return -1;
    auto it = std::upper_bound(m_music.measures.cbegin(), m_music.measures.cend(), tick,
        [](qint64 value, const MarchCraft::MusicMeasure &measure) { return value < measure.startTick; });
    return qBound(0, int(std::distance(m_music.measures.cbegin(), it)) - 1, m_music.measures.size() - 1);
}

QVariantMap DrillProject::meterRegionInfo(int index) const
{
    if (index < 0 || index >= m_meterRegions.size()) return {};
    const auto &r = m_meterRegions[index];
    return {{QStringLiteral("startTick"), r.startTick}, {QStringLiteral("endTick"), r.endTick},
            {QStringLiteral("numerator"), r.numerator}, {QStringLiteral("denominator"), r.denominator},
            {QStringLiteral("pulseTicks"), r.pulseTicks}, {QStringLiteral("grouping"), r.grouping}};
}

QVariantMap DrillProject::tempoRegionInfo(int index) const
{
    if (index < 0 || index >= m_tempoRegions.size()) return {};
    const auto &r = m_tempoRegions[index];
    return {{QStringLiteral("startTick"), r.startTick}, {QStringLiteral("endTick"), r.endTick},
            {QStringLiteral("startBpm"), r.startBpm}, {QStringLiteral("endBpm"), r.endBpm},
            {QStringLiteral("name"), r.name}};
}

void DrillProject::setMeterRegion(qint64 startTick, qint64 endTick, int numerator,
                                  int denominator, qint64 pulseTicks, const QString &grouping)
{
    startTick = qMax<qint64>(0, startTick); endTick = qMax(startTick + 1, endTick);
    const auto before = toJson();
    MarchCraft::MeterRegion region{startTick, endTick, qMax(1, numerator), qMax(1, denominator),
                                   qMax<qint64>(1, pulseTicks), grouping};
    QVector<MarchCraft::MeterRegion> next;
    for (auto existing : m_meterRegions) {
        if (existing.endTick <= startTick || existing.startTick >= endTick) next.push_back(existing);
        else {
            if (existing.startTick < startTick) { existing.endTick = startTick; next.push_back(existing); }
            if (existing.endTick > endTick) { existing.startTick = endTick; next.push_back(existing); }
        }
    }
    next.push_back(region);
    std::sort(next.begin(), next.end(), [](const auto &a, const auto &b){ return a.startTick < b.startTick; });
    m_meterRegions = std::move(next);
    recalculateCounts();
    emit timingChanged(); emit projectChanged();
    commitSnapshot(before, QStringLiteral("Edit meter region"));
}

void DrillProject::setTempoRegion(qint64 startTick, qint64 endTick, double startBpm,
                                  double endBpm, const QString &name)
{
    startTick = qMax<qint64>(0, startTick); endTick = qMax(startTick + 1, endTick);
    const auto before = toJson();
    MarchCraft::TempoRegion region{startTick, endTick, qBound(20.0, startBpm, 400.0),
        qBound(20.0, endBpm, 400.0), name.isEmpty() ? QStringLiteral("Tempo") : name};
    QVector<MarchCraft::TempoRegion> next;
    for (auto existing : m_tempoRegions) {
        if (existing.endTick <= startTick || existing.startTick >= endTick) next.push_back(existing);
        else {
            if (existing.startTick < startTick) { existing.endTick = startTick; next.push_back(existing); }
            if (existing.endTick > endTick) { existing.startTick = endTick; next.push_back(existing); }
        }
    }
    next.push_back(region);
    std::sort(next.begin(), next.end(), [](const auto &a, const auto &b){ return a.startTick < b.startTick; });
    m_tempoRegions = std::move(next);
    emit timingChanged(); emit projectChanged();
    commitSnapshot(before, QStringLiteral("Edit tempo region"));
}

void DrillProject::removeMeterRegion(int index)
{
    if (index < 0 || index >= m_meterRegions.size() || m_meterRegions.size() == 1) return;
    const auto before = toJson(); m_meterRegions.removeAt(index); recalculateCounts();
    emit timingChanged(); commitSnapshot(before, QStringLiteral("Remove meter region"));
}

void DrillProject::removeTempoRegion(int index)
{
    if (index < 0 || index >= m_tempoRegions.size() || m_tempoRegions.size() == 1) return;
    const auto before = toJson(); m_tempoRegions.removeAt(index);
    emit timingChanged(); commitSnapshot(before, QStringLiteral("Remove tempo region"));
}

int DrillProject::pulsesBetween(qint64 startTick, qint64 endTick) const
{
    if (endTick <= startTick) return 0;
    double pulses = 0.0; qint64 cursor = startTick;
    while (cursor < endTick) {
        const MarchCraft::MeterRegion *chosen = nullptr;
        for (const auto &r : m_meterRegions)
            if (r.startTick <= cursor && cursor < r.endTick) chosen = &r;
        const qint64 boundary = chosen ? qMin(endTick, chosen->endTick) : endTick;
        const qint64 pulse = chosen ? chosen->pulseTicks : MarchCraft::TicksPerQuarter;
        pulses += double(boundary - cursor) / double(qMax<qint64>(1, pulse)); cursor = boundary;
    }
    return qRound(pulses);
}

qint64 DrillProject::advancePulses(qint64 startTick, int pulses) const
{
    qint64 tick = startTick;
    for (int i = 0; i < pulses; ++i) {
        qint64 pulse = MarchCraft::TicksPerQuarter;
        for (const auto &r : m_meterRegions)
            if (r.startTick <= tick && tick < r.endTick) { pulse = r.pulseTicks; break; }
        tick += pulse;
    }
    return tick;
}

void DrillProject::recalculateCounts()
{
    if (m_sets.isEmpty()) return;
    m_sets[0].stepMultiplier = 0.0;
    for (int i = 1; i < m_sets.size(); ++i)
        m_sets[i].counts = pulsesBetween(m_sets[i - 1].startTick, m_sets[i].startTick);
    emit currentSetChanged(); emit setsChanged();
}

void DrillProject::setCurrentSetCounts(int counts)
{
    if (m_currentSet <= 0 || m_currentSet >= m_sets.size()) return;
    counts = qBound(1, counts, 2048); const auto before = toJson();
    m_sets[m_currentSet].startTick = advancePulses(m_sets[m_currentSet - 1].startTick, counts);
    for (int i = m_currentSet + 1; i < m_sets.size(); ++i)
        if (m_sets[i].startTick <= m_sets[i - 1].startTick)
            m_sets[i].startTick = advancePulses(m_sets[i - 1].startTick, qMax(1, m_sets[i].counts));
    recalculateCounts(); emit timingChanged();
    commitSnapshot(before, QStringLiteral("Move musical set marker"));
}

double DrillProject::openingDurationMs() const
{
    if (m_openingBehavior != QStringLiteral("hold") || m_openingCounts <= 0) return 0.0;
    return millisecondsBetween(0, advancePulses(0, m_openingCounts));
}

void DrillProject::setOpeningBehavior(const QString &behavior, int counts)
{
    const QString next = behavior == QStringLiteral("hold") ? QStringLiteral("hold") : QStringLiteral("move");
    counts = qBound(1, counts, 256);
    if (next == m_openingBehavior && counts == m_openingCounts) return;
    const auto before = toJson(); m_openingBehavior = next; m_openingCounts = counts;
    emit setsChanged(); emit currentSetChanged(); emit timingChanged();
    commitSnapshot(before, QStringLiteral("Change opening set behavior"));
}

double DrillProject::millisecondsBetween(qint64 startTick, qint64 endTick) const
{
    if (endTick <= startTick) return 0.0;
    double ms = 0.0; qint64 cursor = startTick;
    while (cursor < endTick) {
        const MarchCraft::TempoRegion *chosen = nullptr;
        for (const auto &r : m_tempoRegions)
            if (r.startTick <= cursor && cursor < r.endTick) chosen = &r;
        const qint64 boundary = chosen ? qMin(endTick, chosen->endTick) : endTick;
        if (!chosen) ms += (boundary - cursor) * 60000.0 / (MarchCraft::TicksPerQuarter * m_bpm);
        else {
            const double span = qMax<qint64>(1, chosen->endTick - chosen->startTick);
            const auto bpmAt = [&](qint64 t) { return chosen->startBpm + (chosen->endBpm - chosen->startBpm)
                * double(t - chosen->startTick) / span; };
            const double b0 = bpmAt(cursor), b1 = bpmAt(boundary);
            if (qAbs(b1 - b0) < 0.000001)
                ms += (boundary - cursor) * 60000.0 / (MarchCraft::TicksPerQuarter * b0);
            else
                ms += 60000.0 * (boundary - cursor) / MarchCraft::TicksPerQuarter
                    * std::log(b1 / b0) / (b1 - b0);
        }
        cursor = boundary;
    }
    return ms;
}

double DrillProject::transitionDurationMs(int destinationSet) const
{
    if (destinationSet <= 0 || destinationSet >= m_sets.size()) return 0.0;
    return millisecondsBetween(m_sets[destinationSet - 1].startTick, m_sets[destinationSet].startTick);
}

double DrillProject::showDurationMs() const
{
    double total = openingDurationMs();
    for (int set = 1; set < m_sets.size(); ++set) total += transitionDurationMs(set);
    return total;
}

bool DrillProject::setShowTimeMs(double milliseconds)
{
    if (m_sets.isEmpty()) return true;
    milliseconds = qMax(0.0, milliseconds);
    double cursor = openingDurationMs();
    if (milliseconds < cursor) {
        if (m_currentSet != 0) setCurrentSetIndex(0);
        setPlaybackActive(true); setPlayhead(1.0); return false;
    }
    if (m_sets.size() < 2) return milliseconds >= cursor;
    for (int destination = 1; destination < m_sets.size(); ++destination) {
        const double duration = qMax(1.0, transitionDurationMs(destination));
        if (milliseconds < cursor + duration || destination == m_sets.size() - 1) {
            if (m_currentSet != destination) setCurrentSetIndex(destination);
            setPlaybackActive(true);
            setPlayhead(qBound(0.0, (milliseconds - cursor) / duration, 1.0));
            return milliseconds >= showDurationMs();
        }
        cursor += duration;
    }
    return true;
}

bool DrillProject::setShowAudioTimeMs(double audioMilliseconds)
{
    if (!m_music.loaded()) return setShowTimeMs(openingDurationMs() + audioMilliseconds - m_audioOffsetMs);
    const qint64 tick = musicTickForAudioMs(audioMilliseconds);
    const qint64 firstTick = m_sets.isEmpty() ? 0 : m_sets.first().startTick;
    return setShowTimeMs(openingDurationMs() + qMax(0.0, m_music.millisecondsAt(tick) - m_music.millisecondsAt(firstTick)));
}

QString DrillProject::effectiveTempoText(int destinationSet) const
{
    if (destinationSet <= 0 || destinationSet >= m_sets.size()) return QString::number(m_bpm, 'f', 0) + QStringLiteral(" BPM");
    const qint64 a = m_sets[destinationSet - 1].startTick, b = m_sets[destinationSet].startTick;
    for (const auto &r : m_tempoRegions) if (r.startTick < b && r.endTick > a)
        return qFuzzyCompare(r.startBpm, r.endBpm) ? QString::number(r.startBpm, 'f', 0) + QStringLiteral(" BPM")
            : QStringLiteral("%1–%2 BPM").arg(r.startBpm, 0, 'f', 0).arg(r.endBpm, 0, 'f', 0);
    return QString::number(m_bpm, 'f', 0) + QStringLiteral(" BPM");
}

int DrillProject::selectedCount() const
{
    return static_cast<int>(std::count_if(m_performers.cbegin(), m_performers.cend(),
                                          [](const Performer &p) { return p.selected; }));
}

int DrillProject::selectedGroupedCount() const
{
    if(m_currentSet<0)return 0; QSet<QString> grouped;
    for(const auto&group:m_sets[m_currentSet].activeVariant().groups)for(const auto&id:group.performerIds)grouped.insert(id);
    int count=0;for(const auto&p:m_performers)if(p.selected&&grouped.contains(p.id))++count;return count;
}

bool DrillProject::selectionIsExactGroup() const
{
    if(m_currentSet<0)return false; QSet<QString> selected;for(const auto&p:m_performers)if(p.selected)selected.insert(p.id);
    for(const auto&group:m_sets[m_currentSet].activeVariant().groups){if(group.performerIds.size()!=selected.size())continue;bool same=true;for(const auto&id:group.performerIds)same=same&&selected.contains(id);if(same)return true;}return false;
}

double DrillProject::averageDistance() const
{
    return m_performers.isEmpty() ? 0.0 : totalDistance() / m_performers.size();
}

double DrillProject::totalDistance() const
{
    ensureAnalyticsCache(); return m_cachedEnsembleTotal;
}

double DrillProject::longestDistance() const
{
    ensureAnalyticsCache(); return m_cachedLongestMove;
}

int DrillProject::warningCount() const
{
    ensureAnalyticsCache(); return m_cachedWarningCount;
}

QVariantMap DrillProject::selectionMetrics() const
{
    QVector<int> rows; for(int i=0;i<m_performers.size();++i)if(m_performers[i].selected)rows.push_back(i);
    if(rows.isEmpty())return {{QStringLiteral("count"),0}};
    double nearestTotal=0.0,minSpacing=std::numeric_limits<double>::max(),maxNearest=0.0,moveTotal=0.0,maxMove=0.0;
    int collisions=0;
    for(int row:rows){const QPointF p=placementAt(row,m_currentSet).position;double nearest=std::numeric_limits<double>::max();
        for(int other:rows)if(other!=row){const QPointF q=placementAt(other,m_currentSet).position;nearest=qMin(nearest,std::hypot(p.x()-q.x(),p.y()-q.y()));}
        if(rows.size()==1)nearest=0.0;nearestTotal+=nearest;minSpacing=qMin(minSpacing,nearest);maxNearest=qMax(maxNearest,nearest);if(nearest>0&&nearest<1.5)++collisions;
        const double move=transitionDistance(row,m_currentSet);moveTotal+=move;maxMove=qMax(maxMove,move);
    }
    const auto bounds=selectedBounds(); const int shapeIndex=selectedShapeIndex();
    QString shapeType; if(shapeIndex>=0)shapeType=m_sets[m_currentSet].activeVariant().shapes[shapeIndex].type;
    return {{QStringLiteral("count"),rows.size()},{QStringLiteral("shapeIndex"),shapeIndex},{QStringLiteral("shapeType"),shapeType},
            {QStringLiteral("averageSpacing"),nearestTotal/rows.size()},{QStringLiteral("minimumSpacing"),minSpacing==std::numeric_limits<double>::max()?0.0:minSpacing},
            {QStringLiteral("maximumNearestSpacing"),maxNearest},{QStringLiteral("averageMove"),moveTotal/rows.size()},
            {QStringLiteral("maximumMove"),maxMove},{QStringLiteral("collisionCount"),collisions},
            {QStringLiteral("width"),bounds.value(QStringLiteral("right")).toDouble()-bounds.value(QStringLiteral("left")).toDouble()},
            {QStringLiteral("height"),bounds.value(QStringLiteral("bottom")).toDouble()-bounds.value(QStringLiteral("top")).toDouble()}};
}

void DrillProject::newProject()
{
    m_analyticsValid = false;
    m_openingBehavior = QStringLiteral("move"); m_openingCounts = 8;
    beginResetModel();
    m_performers.clear();
    m_sets = {DrillSet{}};
    m_archivedSets.clear();
    m_sets[0].number = QStringLiteral("1");
    m_sets[0].activeVariant().name = QStringLiteral("Set 1");
    m_sets[0].counts = 0;
    m_sets[0].stepMultiplier = 0.0;
    m_sets[0].startTick = 0;
    m_meterRegions = {MarchCraft::MeterRegion{}};
    m_tempoRegions = {MarchCraft::TempoRegion{0, std::numeric_limits<qint64>::max(), m_bpm, m_bpm, QStringLiteral("Tempo")}};
    m_showName = QStringLiteral("Untitled Show");
    m_fieldPreset = QStringLiteral("hs");
    m_venue = MarchCraft::VenueConfiguration{};
    m_props.clear();
    m_audioSource.clear();
    m_music.clear();
    m_musicSelectionStart = m_musicSelectionEnd = -1;
    m_audioOffsetMs = 0.0; m_waveformPeaks.clear();
    m_projectPath.clear();
    m_currentSet = 0;
    m_selectedSetStart = m_selectedSetEnd = 0;
    m_playbackSource = QStringLiteral("midi"); m_midiMasterVolume = 0.75; m_loopEnabled = false;
    m_playhead = 0.0;
    m_playbackActive = false;
    endResetModel();
    emit performerCountChanged();
    m_undo.clear();
    markDirty(QStringLiteral("New show"));
    emit projectChanged();
    emit setsChanged();
    emit currentSetChanged();
    emit timingChanged();
    emit sceneChanged();
    emit propsChanged();
    emit musicChanged(); emit waveformChanged(); emit setRangeChanged(); emit transportSettingsChanged();
}

void DrillProject::loadDemo()
{
    m_openingBehavior = QStringLiteral("move"); m_openingCounts = 8;
    if (QFile::exists(QStringLiteral(":/samples/coordinates.json"))) {
        importCoordinateJson(QStringLiteral(":/samples/coordinates.json"));
        m_projectPath.clear();
        m_undo.clear();
        m_dirty = false;
        emit dirtyChanged();
        emit projectChanged();
        setStatus(QStringLiteral("Rancho Bernardo 2025 test show loaded"));
        return;
    }

    beginResetModel();
    m_performers.clear();
    m_sets.clear();
    m_archivedSets.clear();
    const QStringList instruments = {QStringLiteral("Trumpet"), QStringLiteral("Mellophone"),
                                     QStringLiteral("Trombone"), QStringLiteral("Baritone"),
                                     QStringLiteral("Tuba"), QStringLiteral("Clarinet"),
                                     QStringLiteral("Alto Sax"), QStringLiteral("Guard")};
    for (int i = 0; i < 24; ++i) {
        Performer performer;
        performer.section = i < 16 ? QStringLiteral("Winds") : QStringLiteral("Guard");
        performer.instrument = instruments.at(i % instruments.size());
        performer.appearance.instrumentAssetId = instrumentAssetIdFor(performer.instrument);
        performer.label = QStringLiteral("%1%2").arg(performer.instrument.left(1).toUpper())
                              .arg(i + 1, 2, 10, QLatin1Char('0'));
        performer.name = QStringLiteral("Performer %1").arg(i + 1);
        performer.symbol = performer.instrument.left(1).toUpper();
        performer.color = sectionColor(performer.section);
        m_performers.push_back(performer);
    }
    for (int setIndex = 0; setIndex < 4; ++setIndex) {
        DrillSet set;
        set.activeVariant().name = QStringLiteral("Set %1").arg(setIndex + 1);
        set.measure = QStringLiteral("%1-%2").arg(setIndex * 4 + 1).arg(setIndex * 4 + 4);
        set.counts = setIndex == 0 ? 0 : 16;
        set.startTick = setIndex * 16 * MarchCraft::TicksPerQuarter;
        for (int i = 0; i < m_performers.size(); ++i) {
            const double angle = 2.0 * std::numbers::pi * i / m_performers.size();
            Placement placement;
            if (setIndex == 0)
                placement.position = {24.0 + (i % 12) * 10.0, 26.0 + (i / 12) * 12.0};
            else if (setIndex == 1)
                placement.position = {80.0 + std::cos(angle) * 45.0, 42.0 + std::sin(angle) * 24.0};
            else if (setIndex == 2)
                placement.position = {32.0 + (i % 8) * 13.5, 18.0 + (i / 8) * 20.0};
            else
                placement.position = {80.0 + std::cos(angle) * (18.0 + i * 1.1),
                                      42.0 + std::sin(angle) * (10.0 + i * 0.55)};
            placement.facing = 0.0;
            set.activeVariant().placements.insert(m_performers[i].id, placement);
        }
        m_sets.push_back(set);
    }
    m_showName = QStringLiteral("MarchCraft Demo");
    m_fieldPreset = QStringLiteral("hs");
    m_currentSet = 0;
    m_playhead = 0.0;
    m_meterRegions = {MarchCraft::MeterRegion{}};
    m_tempoRegions = {MarchCraft::TempoRegion{0, std::numeric_limits<qint64>::max(), m_bpm, m_bpm, QStringLiteral("Tempo")}};
    endResetModel();
    emit performerCountChanged();
    m_undo.clear();
    markDirty(QStringLiteral("Demo loaded"));
    emit projectChanged();
    emit setsChanged();
    emit currentSetChanged();
    emit timingChanged();
}

bool DrillProject::importCoordinateJson(const QString &urlOrPath)
{
    QFile file(localPath(urlOrPath));
    if (!file.open(QIODevice::ReadOnly)) {
        setStatus(QStringLiteral("Could not open coordinate JSON"));
        return false;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        setStatus(QStringLiteral("Invalid coordinate JSON: %1").arg(error.errorString()));
        return false;
    }
    const auto root = document.object();
    const auto people = root.value(QStringLiteral("performers")).toArray();
    if (people.isEmpty()) {
        setStatus(QStringLiteral("Coordinate file contains no performers"));
        return false;
    }

    beginResetModel();
    // Coordinate sheets begin at a zero-count first set. A written hold is a
    // following set at the same coordinate, with that row's count value.
    m_openingBehavior = QStringLiteral("move");
    m_openingCounts = 8;
    m_performers.clear();
    m_sets.clear();
    m_archivedSets.clear();
    QHash<QString, int> setIndices;
    const auto geometry = MarchCraft::fieldGeometry(m_fieldPreset);
    for (const auto &personValue : people) {
        const auto personObject = personValue.toObject();
        Performer performer;
        performer.label = personObject.value(QStringLiteral("label")).toString();
        performer.instrument = personObject.value(QStringLiteral("instrument")).toString();
        performer.section = performer.instrument;
        performer.appearance.instrumentAssetId = instrumentAssetIdFor(performer.instrument);
        if (performer.appearance.instrumentAssetId == QStringLiteral("equipment.guard.flag"))
            performer.appearance.roleId = QStringLiteral("guard");
        performer.symbol = personObject.value(QStringLiteral("symbol")).toString(performer.label.left(1));
        performer.color = sectionColor(performer.section);
        m_performers.push_back(performer);
        for (const auto &setValue : personObject.value(QStringLiteral("sets")).toArray()) {
            const auto sourceSet = setValue.toObject();
            const QString key = QStringLiteral("%1:%2")
                                    .arg(sourceSet.value(QStringLiteral("part")).toInt())
                                    .arg(sourceSet.value(QStringLiteral("set")).toString());
            int destination = setIndices.value(key, -1);
            if (destination < 0) {
                DrillSet set;
                set.activeVariant().name = sourceSet.value(QStringLiteral("set")).toString();
                set.measure = sourceSet.value(QStringLiteral("measure")).toString();
                set.counts = qMax(1, sourceSet.value(QStringLiteral("counts")).toInt(8));
                destination = m_sets.size();
                setIndices.insert(key, destination);
                m_sets.push_back(set);
            }
            const auto lateral = sourceSet.value(QStringLiteral("lateral")).toObject();
            const auto vertical = sourceSet.value(QStringLiteral("vertical")).toObject();
            const int side = lateral.value(QStringLiteral("side")).toInt(1);
            const double yardLine = lateral.value(QStringLiteral("yardLine")).toDouble(50.0);
            double centeredX = (50.0 - yardLine) * 1.6 * (side == 1 ? -1.0 : 1.0);
            const QString lateralRelation = lateral.value(QStringLiteral("relation")).toString();
            if (lateralRelation != QStringLiteral("on")) {
                const double outward = lateralRelation == QStringLiteral("outside") ? -1.0 : 1.0;
                centeredX += lateral.value(QStringLiteral("offset")).toDouble()
                             * outward * (side == 1 ? 1.0 : -1.0);
            }
            const QString landmark = vertical.value(QStringLiteral("landmark")).toString();
            double y = 0.0;
            if (landmark == QStringLiteral("front hash")) y = geometry.frontHash;
            else if (landmark == QStringLiteral("back hash")) y = geometry.backHash;
            else if (landmark == QStringLiteral("back sideline")) y = geometry.depth;
            const QString verticalRelation = vertical.value(QStringLiteral("relation")).toString();
            const double offset = vertical.value(QStringLiteral("offset")).toDouble();
            if (verticalRelation == QStringLiteral("in front of")) y -= offset;
            else if (verticalRelation == QStringLiteral("behind")) y += offset;
            m_sets[destination].activeVariant().placements.insert(
                performer.id, Placement{{80.0 + centeredX, y}, 0.0});
        }
    }
    m_showName = root.value(QStringLiteral("show")).toString(QStringLiteral("Imported Show"));
    m_meterRegions = {MarchCraft::MeterRegion{}};
    m_tempoRegions = {MarchCraft::TempoRegion{0, std::numeric_limits<qint64>::max(), m_bpm, m_bpm, QStringLiteral("Tempo")}};
    qint64 tick = 0;
    for (int i = 0; i < m_sets.size(); ++i) {
        m_sets[i].number = QString::number(i + 1);
        m_sets[i].startTick = tick;
        if (i == 0) { m_sets[i].counts = 0; m_sets[i].stepMultiplier = 0.0; }
        if (i + 1 < m_sets.size()) tick = advancePulses(tick, qMax(1, m_sets[i + 1].counts));
    }
    m_currentSet = 0;
    m_playhead = 0.0;
    ensurePlacements();
    endResetModel();
    emit performerCountChanged();
    m_undo.clear();
    markDirty(QStringLiteral("Imported %1 performers and %2 sets")
                  .arg(m_performers.size()).arg(m_sets.size()));
    emit projectChanged();
    emit setsChanged();
    emit currentSetChanged();
    emit timingChanged();
    return true;
}

bool DrillProject::saveProject(const QString &urlOrPath)
{
    QString path = localPath(urlOrPath);
    if (path.isEmpty())
        path = m_projectPath;
    if (path.isEmpty()) {
        setStatus(QStringLiteral("Choose a project filename"));
        return false;
    }
    if (!path.endsWith(QStringLiteral(".marchcraft"), Qt::CaseInsensitive))
        path += QStringLiteral(".marchcraft");
    if (QFile::exists(path)) {
        const QString backup = path + QStringLiteral(".backup-")
            + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-hhmmss"));
        QFile::copy(path, backup);
    }
    QString databaseError;
    if (!writeSqliteProject(path, toJson(), &databaseError)) {
        setStatus(QStringLiteral("Could not save project database: %1").arg(databaseError));
        return false;
    }
    m_projectPath = path;
    m_dirty = false;
    emit dirtyChanged();
    emit projectChanged();
    setStatus(QStringLiteral("Saved %1").arg(QFileInfo(path).fileName()));
    return true;
}

bool DrillProject::loadProject(const QString &urlOrPath)
{
    const QString path = localPath(urlOrPath);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setStatus(QStringLiteral("Could not open project"));
        return false;
    }
    const QByteArray signature = file.peek(16); file.close();
    QJsonObject root; bool legacyJson = !signature.startsWith("SQLite format 3");
    if (legacyJson) {
        if (!file.open(QIODevice::ReadOnly)) return false;
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (!document.isObject() || parseError.error != QJsonParseError::NoError) {
            setStatus(QStringLiteral("Invalid project: %1").arg(parseError.errorString())); return false;
        }
        root = document.object();
    } else {
        QString databaseError;
        if (!readSqliteProject(path, &root, &databaseError)) {
            setStatus(QStringLiteral("Invalid project database: %1").arg(databaseError)); return false;
        }
    }
    if (!restoreJson(root, false))
        return false;
    m_projectPath = legacyJson ? QString{} : path;
    m_dirty = false;
    m_undo.clear();
    emit dirtyChanged();
    emit projectChanged();
    setStatus(legacyJson ? QStringLiteral("Imported legacy project; save to create a .marchcraft database")
                         : QStringLiteral("Opened %1").arg(QFileInfo(path).fileName()));
    return true;
}

bool DrillProject::exportCsv(const QString &urlOrPath) const
{
    QSaveFile file(localPath(urlOrPath));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream stream(&file);
    stream << QStringLiteral("Performer,Name,Instrument,Section,Set,Measure,Counts,Coordinate,Move steps,Total steps\n");
    for (int p = 0; p < m_performers.size(); ++p) {
        for (int s = 0; s < m_sets.size(); ++s) {
            const auto &person = m_performers[p];
            const auto &set = m_sets[s];
            stream << csvCell(person.label) << QLatin1Char(',') << csvCell(person.name) << QLatin1Char(',')
                   << csvCell(person.instrument) << QLatin1Char(',') << csvCell(person.section) << QLatin1Char(',')
                   << csvCell(set.activeVariant().name) << QLatin1Char(',') << csvCell(set.measure) << QLatin1Char(',')
                   << set.counts << QLatin1Char(',') << csvCell(coordinateFor(p, s)) << QLatin1Char(',')
                   << QString::number(transitionDistance(p, s), 'f', 2) << QLatin1Char(',')
                   << QString::number(performerTotalDistance(p), 'f', 2) << QLatin1Char('\n');
        }
    }
    return file.commit();
}

bool DrillProject::exportCoordinatePdf(const QString &urlOrPath) const
{
    const QString path = localPath(urlOrPath);
    QPdfWriter writer(path);
    writer.setTitle(m_showName + QStringLiteral(" Coordinate Sheets"));
    writer.setCreator(QStringLiteral("MarchCraft"));
    writer.setPageSize(QPageSize(QPageSize::Letter));
    writer.setPageOrientation(QPageLayout::Landscape);
    writer.setResolution(144);
    QPainter painter(&writer);
    if (!painter.isActive())
        return false;
    const QRect page = writer.pageLayout().paintRectPixels(writer.resolution());
    const int left = page.left() + 34;
    const int right = page.right() - 34;
    const int tableWidth = right - left;
    const int rowHeight = 25;
    const int tableTop = page.top() + 126;
    const int footerSpace = 42;
    const int rowsPerPage = qMax(1, (page.bottom() - footerSpace - tableTop - rowHeight) / rowHeight);
    const int pagesPerPerformer = qMax(1, static_cast<int>(std::ceil(m_sets.size() / static_cast<double>(rowsPerPage))));
    const int totalPages = pagesPerPerformer * m_performers.size();
    int outputPage = 0;

    QFont heading = painter.font();
    heading.setBold(true);
    heading.setPointSize(15);
    QFont performerFont = painter.font();
    performerFont.setBold(true);
    performerFont.setPointSize(10);
    QFont body = painter.font();
    body.setPointSize(7);
    QFont tableHeader = body;
    tableHeader.setBold(true);

    const QVector<double> columnFractions = {0.055, 0.085, 0.060, 0.300, 0.275, 0.095, 0.130};
    QStringList headers = {QStringLiteral("Set"), QStringLiteral("Measure"), QStringLiteral("Counts"),
                           QStringLiteral("Side-to-side"), QStringLiteral("Front-to-back"),
                           QStringLiteral("Move"), QStringLiteral("Step/count")};

    for (int p = 0; p < m_performers.size(); ++p) {
        const auto &person = m_performers[p];
        for (int performerPage = 0; performerPage < pagesPerPerformer; ++performerPage) {
            if (outputPage > 0)
                writer.newPage();
            ++outputPage;

            painter.fillRect(page, Qt::white);
            painter.setPen(QColor(QStringLiteral("#111827")));
            painter.setFont(heading);
            painter.drawText(left, page.top() + 34, QStringLiteral("MARCHCRAFT COORDINATE SHEET"));
            painter.setFont(performerFont);
            painter.drawText(left, page.top() + 64,
                             QStringLiteral("Performer: %1    Instrument: %2    Section: %3")
                                 .arg(person.label, person.instrument, person.section));
            painter.drawText(left, page.top() + 90,
                             QStringLiteral("Name: %1").arg(person.name.isEmpty() ? QStringLiteral("-") : person.name));
            painter.drawText(QRect(left, page.top() + 48, tableWidth, 28), Qt::AlignRight | Qt::AlignVCenter,
                             m_showName);
            painter.setFont(body);
            painter.drawText(QRect(left, page.top() + 78, tableWidth, 24), Qt::AlignRight | Qt::AlignVCenter,
                             QStringLiteral("Performer page %1 of %2").arg(performerPage + 1).arg(pagesPerPerformer));

            int x = left;
            painter.fillRect(QRect(left, tableTop, tableWidth, rowHeight), QColor(QStringLiteral("#17212b")));
            painter.setPen(Qt::white);
            painter.setFont(tableHeader);
            for (int c = 0; c < headers.size(); ++c) {
                const int width = c == headers.size() - 1
                    ? right - x : qRound(tableWidth * columnFractions[c]);
                painter.drawText(QRect(x + 5, tableTop, width - 10, rowHeight),
                                 Qt::AlignLeft | Qt::AlignVCenter, headers[c]);
                x += width;
            }

            painter.setFont(body);
            const int firstSet = performerPage * rowsPerPage;
            const int lastSet = qMin(firstSet + rowsPerPage, static_cast<int>(m_sets.size()));
            for (int s = firstSet; s < lastSet; ++s) {
                const int row = s - firstSet;
                const int y = tableTop + rowHeight * (row + 1);
                if (row % 2 == 1)
                    painter.fillRect(QRect(left, y, tableWidth, rowHeight), QColor(QStringLiteral("#eef2f4")));
                painter.setPen(QColor(QStringLiteral("#111827")));
                const auto &set = m_sets[s];
                const QString combined = coordinateFor(p, s);
                QString lateral = combined.section(QStringLiteral(" · "), 0, 0);
                const QString vertical = combined.section(QStringLiteral(" · "), 1, 1);
                const double move = transitionDistance(p, s);
                const double plannedSteps = set.counts * set.stepMultiplier;
                const double stepPerCount = plannedSteps > 0.0 ? move / plannedSteps
                                                               : (move > 0.0 ? std::numeric_limits<double>::infinity() : 0.0);
                const QStringList values = {set.activeVariant().name, set.measure, QString::number(set.counts), lateral,
                                            vertical, QStringLiteral("%1 st").arg(move, 0, 'f', 1),
                                            QString::number(stepPerCount, 'f', 2)};
                x = left;
                for (int c = 0; c < values.size(); ++c) {
                    const int width = c == values.size() - 1
                        ? right - x : qRound(tableWidth * columnFractions[c]);
                    const QString text = painter.fontMetrics().elidedText(values[c], Qt::ElideRight, width - 10);
                    painter.drawText(QRect(x + 5, y, width - 10, rowHeight),
                                     Qt::AlignLeft | Qt::AlignVCenter, text);
                    painter.setPen(QColor(QStringLiteral("#d1d5db")));
                    painter.drawLine(x + width, y, x + width, y + rowHeight);
                    painter.setPen(QColor(QStringLiteral("#111827")));
                    x += width;
                }
                painter.setPen(QColor(QStringLiteral("#d1d5db")));
                painter.drawLine(left, y + rowHeight, right, y + rowHeight);
            }

            painter.setPen(QColor(QStringLiteral("#64748b")));
            painter.setFont(body);
            painter.drawText(left, page.bottom() - 16,
                             QStringLiteral("Total distance: %1 steps / %2 yards")
                                 .arg(performerTotalDistance(p), 0, 'f', 1)
                                 .arg(performerTotalDistance(p) * 5.0 / 8.0, 0, 'f', 1));
            painter.drawText(QRect(left, page.bottom() - 31, tableWidth, 24),
                             Qt::AlignRight | Qt::AlignVCenter,
                             QStringLiteral("Page %1 of %2").arg(outputPage).arg(totalPages));
        }
    }
    painter.end();
    return true;
}

bool DrillProject::importMusicXml(const QString &urlOrPath)
{
    QFile file(localPath(urlOrPath));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStatus(QStringLiteral("Could not open MusicXML"));
        return false;
    }
    struct ImportedMeasure { QString number; qint64 tick = 0; qint64 duration = 0; };
    QXmlStreamReader xml(&file);
    QVector<ImportedMeasure> measures;
    QVector<MarchCraft::MeterRegion> meters;
    QVector<MarchCraft::TempoRegion> tempos;
    int divisions = 1, beats = 4, beatType = 4, unsupported = 0;
    qint64 scoreTick = 0, cursor = 0, maximum = 0;
    QString measureNumber;
    bool inMeasure = false;
    auto sourceToTicks = [&divisions](int duration) {
        return qRound64(double(duration) * MarchCraft::TicksPerQuarter / qMax(1, divisions));
    };
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1StringView("measure")) {
            inMeasure = true; cursor = 0; maximum = 0;
            measureNumber = xml.attributes().value(QLatin1StringView("number")).toString();
        } else if (xml.isEndElement() && xml.name() == QLatin1StringView("measure")) {
            const qint64 nominal = qRound64(double(beats) * 4.0 / beatType * MarchCraft::TicksPerQuarter);
            const qint64 duration = maximum > 0 ? maximum : nominal;
            measures.push_back({measureNumber, scoreTick, duration});
            scoreTick += duration; inMeasure = false;
        } else if (!xml.isStartElement()) {
            continue;
        } else if (xml.name() == QLatin1StringView("divisions")) {
            divisions = qMax(1, xml.readElementText().toInt());
        } else if (xml.name() == QLatin1StringView("beats")) {
            beats = qMax(1, xml.readElementText().toInt());
        } else if (xml.name() == QLatin1StringView("beat-type")) {
            beatType = qMax(1, xml.readElementText().toInt());
            MarchCraft::MeterRegion r;
            r.startTick = scoreTick; r.numerator = beats; r.denominator = beatType;
            r.pulseTicks = (beats == 6 && beatType == 8) ? MarchCraft::TicksPerQuarter * 3 / 2
                                                        : MarchCraft::TicksPerQuarter * 4 / beatType;
            r.grouping = (beats == 6 && beatType == 8) ? QStringLiteral("3+3") : QString::number(beats);
            if (!meters.isEmpty()) meters.last().endTick = scoreTick;
            meters.push_back(r);
        } else if (xml.name() == QLatin1StringView("note")) {
            int rawDuration = 0; bool chord = false, grace = false;
            while (!(xml.isEndElement() && xml.name() == QLatin1StringView("note")) && !xml.atEnd()) {
                xml.readNext();
                if (!xml.isStartElement()) continue;
                if (xml.name() == QLatin1StringView("duration")) rawDuration = xml.readElementText().toInt();
                else if (xml.name() == QLatin1StringView("chord")) chord = true;
                else if (xml.name() == QLatin1StringView("grace")) grace = true;
            }
            if (!chord && !grace) cursor += sourceToTicks(rawDuration);
            maximum = qMax(maximum, cursor);
        } else if (xml.name() == QLatin1StringView("backup") || xml.name() == QLatin1StringView("forward")) {
            const bool backward = xml.name() == QLatin1StringView("backup"); int rawDuration = 0;
            while (!(xml.isEndElement() && (xml.name() == QLatin1StringView("backup") || xml.name() == QLatin1StringView("forward"))) && !xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement() && xml.name() == QLatin1StringView("duration")) rawDuration = xml.readElementText().toInt();
            }
            cursor += (backward ? -1 : 1) * sourceToTicks(rawDuration); cursor = qMax<qint64>(0, cursor);
            maximum = qMax(maximum, cursor);
        } else if (xml.name() == QLatin1StringView("sound")) {
            bool ok = false; const double tempo = xml.attributes().value(QLatin1StringView("tempo")).toDouble(&ok);
            if (ok) {
                if (!tempos.isEmpty()) tempos.last().endTick = scoreTick + cursor;
                tempos.push_back({scoreTick + cursor, std::numeric_limits<qint64>::max(), tempo, tempo,
                                  QStringLiteral("Imported tempo")});
            }
        } else if (xml.name() == QLatin1StringView("repeat")) {
            ++unsupported;
        }
    }
    if (xml.hasError() || measures.isEmpty()) {
        setStatus(QStringLiteral("MusicXML did not contain readable measures"));
        return false;
    }
    if (meters.isEmpty()) meters = {MarchCraft::MeterRegion{}};
    if (tempos.isEmpty()) tempos = {MarchCraft::TempoRegion{0, std::numeric_limits<qint64>::max(), m_bpm, m_bpm, QStringLiteral("Tempo")}};
    MarchCraft::MusicDocument document;
    document.sourceType = QStringLiteral("musicxml"); document.sourcePath = localPath(urlOrPath);
    QFile hashFile(document.sourcePath);
    if (hashFile.open(QIODevice::ReadOnly))
        document.sourceHash = QString::fromLatin1(QCryptographicHash::hash(hashFile.readAll(), QCryptographicHash::Sha256).toHex());
    document.durationTick = scoreTick;
    document.tracks.push_back({0, QStringLiteral("Score"), 0, -1, -1, true});
    for (const auto &meter : std::as_const(meters))
        document.meters.push_back({meter.startTick, meter.numerator, meter.denominator, meter.pulseTicks, meter.grouping});
    for (const auto &tempo : std::as_const(tempos))
        document.tempos.push_back({tempo.startTick, tempo.startBpm});
    for (int i = 0; i < measures.size(); ++i) {
        const auto &source = measures[i];
        MarchCraft::MusicMeasure measure; measure.index = i;
        bool numberOk = false; measure.displayNumber = source.number.toInt(&numberOk); if (!numberOk) measure.displayNumber = i + 1;
        measure.startTick = source.tick; measure.endTick = source.tick + source.duration;
        const auto meter = std::find_if(document.meters.crbegin(), document.meters.crend(),
            [&](const MarchCraft::MusicMeterEvent &event) { return event.tick <= source.tick; });
        if (meter != document.meters.crend()) {
            measure.numerator = meter->numerator; measure.denominator = meter->denominator; measure.pulseTicks = meter->pulseTicks;
        }
        measure.counts = qMax(1, qRound(double(source.duration) / qMax<qint64>(1, measure.pulseTicks)));
        const qint64 nominal = qRound64(double(MarchCraft::TicksPerQuarter) * 4.0 * measure.numerator / measure.denominator);
        measure.partial = source.duration != nominal; document.measures.push_back(measure);
    }
    document.firstMeasureNumber = document.measures.first().displayNumber;
    document.durationMs = document.millisecondsAt(document.durationTick);
    if (unsupported > 0) document.diagnostics.push_back(
        QStringLiteral("Written-order timing imported; %1 repeat instruction(s) require review").arg(unsupported));
    applyMusicDocument(std::move(document), QStringLiteral("Import MusicXML"));
    return true;
}

void DrillProject::addPerformer(const QString &label, const QString &instrument,
                                const QString &section, double x, double y)
{
    const auto before = toJson();
    Performer person;
    person.label = label.trimmed().isEmpty() ? QStringLiteral("P%1").arg(m_performers.size() + 1) : label.trimmed();
    person.name = person.label;
    person.instrument = instrument.trimmed().isEmpty() ? QStringLiteral("Unassigned") : instrument.trimmed();
    person.section = section.trimmed().isEmpty() ? QStringLiteral("Unassigned") : section.trimmed();
    person.symbol = person.instrument.left(1).toUpper();
    person.appearance.instrumentAssetId = instrumentAssetIdFor(person.instrument);
    person.color = sectionColor(person.section);
    const int row = m_performers.size();
    beginInsertRows({}, row, row);
    m_performers.push_back(person);
    const Placement placement{clampPosition({x, y}), 0.0};
    auto addToSets = [&person, &placement](QVector<DrillSet> &sets) {
        for (auto &set : sets) {
            for (auto &variant : set.variants) variant.placements.insert(person.id, placement);
            for (auto &variant : set.archivedVariants) variant.placements.insert(person.id, placement);
        }
    };
    addToSets(m_sets);
    addToSets(m_archivedSets);
    endInsertRows();
    emit performerCountChanged();
    commitSnapshot(before, QStringLiteral("Add performer"));
}

void DrillProject::batchAddPerformers(const QString &prefix, int count,
                                      const QString &instrument, const QString &section)
{
    count = std::clamp(count, 1, 500);
    const auto before = toJson();
    const int first = m_performers.size();
    beginInsertRows({}, first, first + count - 1);
    for (int i = 0; i < count; ++i) {
        Performer person;
        person.label = QStringLiteral("%1%2").arg(prefix.isEmpty() ? QStringLiteral("P") : prefix.toUpper())
                           .arg(i + 1, 2, 10, QLatin1Char('0'));
        person.name = person.label;
        person.instrument = instrument.isEmpty() ? QStringLiteral("Unassigned") : instrument;
        person.section = section.isEmpty() ? QStringLiteral("Unassigned") : section;
        person.symbol = person.instrument.left(1).toUpper();
        person.appearance.instrumentAssetId = instrumentAssetIdFor(person.instrument);
        person.color = sectionColor(person.section);
        m_performers.push_back(person);
        const QPointF position = clampPosition({24.0 + ((first + i) % 12) * 10.0,
                                                20.0 + ((first + i) / 12) * 8.0});
        auto addToSets = [&person, &position](QVector<DrillSet> &sets) {
            for (auto &set : sets) {
                for (auto &variant : set.variants)
                    variant.placements.insert(person.id, Placement{position, 0.0});
                for (auto &variant : set.archivedVariants)
                    variant.placements.insert(person.id, Placement{position, 0.0});
            }
        };
        addToSets(m_sets);
        addToSets(m_archivedSets);
    }
    endInsertRows();
    emit performerCountChanged();
    commitSnapshot(before, QStringLiteral("Batch add performers"));
}

void DrillProject::removeSelectedPerformers()
{
    if (selectedCount() == 0)
        return;
    const auto before = toJson();
    beginResetModel();
    QSet<QString> removed;
    for (const auto &person : m_performers)
        if (person.selected) removed.insert(person.id);
    m_performers.erase(std::remove_if(m_performers.begin(), m_performers.end(),
                                     [](const Performer &p) { return p.selected; }), m_performers.end());
    auto removeFromSets = [&removed](QVector<DrillSet> &sets) {
        for (auto &set : sets) {
            auto removeFromVariant = [&](MarchCraft::SetVariant &variant) {
                for (const auto &id : removed) variant.placements.remove(id);
                for (auto &group : variant.groups)
                    group.performerIds.erase(std::remove_if(group.performerIds.begin(), group.performerIds.end(),
                        [&](const QString &id) { return removed.contains(id); }), group.performerIds.end());
                variant.groups.erase(std::remove_if(variant.groups.begin(), variant.groups.end(),
                    [](const auto &group) { return group.performerIds.size() < 2; }), variant.groups.end());
                for (auto &shape : variant.shapes)
                    shape.performerIds.erase(std::remove_if(shape.performerIds.begin(), shape.performerIds.end(),
                        [&](const QString &id) { return removed.contains(id); }), shape.performerIds.end());
                variant.shapes.erase(std::remove_if(variant.shapes.begin(), variant.shapes.end(),
                    [](const auto &shape) { return shape.performerIds.size() < 2; }), variant.shapes.end());
            };
            for (auto &variant : set.variants) removeFromVariant(variant);
            for (auto &variant : set.archivedVariants) removeFromVariant(variant);
        }
    };
    removeFromSets(m_sets);
    removeFromSets(m_archivedSets);
    endResetModel();
    emit performerCountChanged();
    emit selectionChanged();
    commitSnapshot(before, QStringLiteral("Remove performers"));
}

void DrillProject::updatePerformer(int row, const QString &label, const QString &name,
                                   const QString &instrument, const QString &section,
                                   const QString &notes)
{
    if (row < 0 || row >= m_performers.size()) return;
    const auto before = toJson();
    auto &person = m_performers[row];
    person.label = label.trimmed();
    person.name = name.trimmed();
    person.instrument = instrument.trimmed();
    person.section = section.trimmed();
    person.notes = notes;
    person.symbol = person.instrument.left(1).toUpper();
    person.appearance.instrumentAssetId = instrumentAssetIdFor(person.instrument);
    person.color = sectionColor(person.section);
    emit dataChanged(index(row, 0), index(row, 0));
    commitSnapshot(before, QStringLiteral("Edit performer"));
}

void DrillProject::setPerformerColor(int row, const QString &color)
{
    if (row < 0 || row >= m_performers.size()) return;
    const QColor parsed(color);
    if (!parsed.isValid()) return;
    const auto before = toJson();
    m_performers[row].color = parsed;
    emit dataChanged(index(row, 0), index(row, 0), {ColorRole});
    commitSnapshot(before, QStringLiteral("Change uniform color"));
}

void DrillProject::setPerformerAppearance(int row, const QString &bodyRigId,
                                          const QString &uniformId, const QString &skinPaletteId,
                                          const QString &instrumentAssetId, double heightMeters)
{
    if (row < 0 || row >= m_performers.size()) return;
    const auto before = toJson();
    auto &appearance = m_performers[row].appearance;
    if (!bodyRigId.trimmed().isEmpty()) appearance.bodyRigId = bodyRigId.trimmed();
    if (!uniformId.trimmed().isEmpty()) appearance.uniformId = uniformId.trimmed();
    if (!skinPaletteId.trimmed().isEmpty()) appearance.skinPaletteId = skinPaletteId.trimmed();
    if (!instrumentAssetId.trimmed().isEmpty()) appearance.instrumentAssetId = instrumentAssetId.trimmed();
    appearance.heightMeters = qBound(1.1, heightMeters, 2.25);
    emit dataChanged(index(row, 0), index(row, 0), {BodyRigRole, UniformRole, SkinPaletteRole,
                                                    InstrumentAssetRole, PerformerHeightRole});
    commitSnapshot(before, QStringLiteral("Change performer appearance"));
}

QString DrillProject::addProp(const QString &definitionId, double x, double y)
{
    const auto before = toJson();
    MarchCraft::PropInstance prop;
    prop.definitionId = definitionId.startsWith(QStringLiteral("prop."))
        ? definitionId : QStringLiteral("prop.box");
    prop.position = clampPosition({x, y});
    const QString id = prop.id;
    m_props.push_back(std::move(prop));
    emit propsChanged();
    commitSnapshot(before, QStringLiteral("Add prop"));
    return id;
}

void DrillProject::updateProp(const QString &id, double x, double y, double rotation,
                              double scaleX, double scaleY, double scaleZ)
{
    auto it = std::find_if(m_props.begin(), m_props.end(), [&](const auto &prop) { return prop.id == id; });
    if (it == m_props.end()) return;
    const auto before = toJson();
    it->position = clampPosition({x, y});
    it->rotation = std::fmod(rotation + 360.0, 360.0);
    it->scale = {static_cast<float>(qBound(0.1, scaleX, 10.0)),
                 static_cast<float>(qBound(0.1, scaleY, 10.0)),
                 static_cast<float>(qBound(0.1, scaleZ, 10.0))};
    emit propsChanged();
    commitSnapshot(before, QStringLiteral("Edit prop"));
}

void DrillProject::removeProp(const QString &id)
{
    const auto it = std::find_if(m_props.cbegin(), m_props.cend(), [&](const auto &prop) { return prop.id == id; });
    if (it == m_props.cend()) return;
    const auto before = toJson();
    m_props.erase(it);
    emit propsChanged();
    commitSnapshot(before, QStringLiteral("Remove prop"));
}

void DrillProject::updateSelectedPerformers(const QString &instrument, const QString &section,
                                            const QString &color, double facing,
                                            int visibleMode, int lockedMode)
{
    if (selectedCount() == 0) return;
    const QColor parsed(color); const auto before = toJson();
    for (auto &person : m_performers) if (person.selected) {
        if (!instrument.trimmed().isEmpty()) { person.instrument = instrument.trimmed(); person.symbol = person.instrument.left(1).toUpper(); person.appearance.instrumentAssetId = instrumentAssetIdFor(person.instrument); }
        if (!section.trimmed().isEmpty()) person.section = section.trimmed();
        if (parsed.isValid()) person.color = parsed;
        if (visibleMode >= 0) person.visible = visibleMode != 0;
        if (lockedMode >= 0) person.locked = lockedMode != 0;
        if (!std::isnan(facing) && m_currentSet >= 0)
            m_sets[m_currentSet].activeVariant().placements[person.id].facing = std::fmod(facing + 360.0, 360.0);
        if (!person.visible || person.locked) person.selected = false;
    }
    emitAllDataChanged(); emit selectionChanged();
    commitSnapshot(before, QStringLiteral("Bulk edit performers"));
}

void DrillProject::autoLabel(const QString &prefix)
{
    const auto before = toJson();
    const QString actualPrefix = prefix.trimmed().isEmpty() ? QStringLiteral("P") : prefix.trimmed().toUpper();
    for (int i = 0; i < m_performers.size(); ++i)
        if (m_performers[i].selected || selectedCount() == 0)
            m_performers[i].label = QStringLiteral("%1%2").arg(actualPrefix).arg(i + 1, 2, 10, QLatin1Char('0'));
    emitAllDataChanged();
    commitSnapshot(before, QStringLiteral("Auto-label performers"));
}

void DrillProject::addSet(const QString &name, int counts, bool subset)
{
    const auto before = toJson();
    DrillSet set;
    set.number = QString::number(m_sets.size() + 1);
    set.activeVariant().name = name.trimmed().isEmpty()
        ? QStringLiteral("Set %1").arg(m_sets.size() + 1) : name.trimmed();
    set.counts = qMax(1, counts);
    set.startTick = m_sets.isEmpty() ? 0 : advancePulses(m_sets[qMax(0, m_currentSet)].startTick, set.counts);
    set.subset = subset;
    if (!m_sets.isEmpty())
        set.activeVariant().placements = m_sets[qMax(0, m_currentSet)].activeVariant().placements;
    m_sets.insert(m_currentSet + 1, set);
    m_currentSet++;
    emit setsChanged();
    emit currentSetChanged();
    emitAllDataChanged();
    commitSnapshot(before, QStringLiteral("Add set"));
}

void DrillProject::batchAddSets(int numberOfSets, int counts)
{
    numberOfSets = std::clamp(numberOfSets, 1, 100);
    counts = std::clamp(counts, 1, 256);
    const auto before = toJson();
    int insertAt = m_currentSet + 1;
    QHash<QString, Placement> placements;
    if (!m_sets.isEmpty())
        placements = m_sets[qMax(0, m_currentSet)].activeVariant().placements;
    for (int i = 0; i < numberOfSets; ++i) {
        DrillSet set;
        set.number = QString::number(m_sets.size() + 1);
        set.activeVariant().name = QStringLiteral("Set %1").arg(m_sets.size() + 1);
        set.counts = counts;
        const qint64 base = (i == 0 ? m_sets[qMax(0, m_currentSet)].startTick
                                    : m_sets[insertAt + i - 1].startTick);
        set.startTick = advancePulses(base, counts);
        set.activeVariant().placements = placements;
        m_sets.insert(insertAt + i, set);
    }
    m_currentSet = insertAt;
    emit setsChanged(); emit currentSetChanged(); emitAllDataChanged();
    commitSnapshot(before, QStringLiteral("Batch add sets"));
}

void DrillProject::duplicateCurrentSet()
{
    if (m_currentSet < 0 || m_currentSet >= m_sets.size()) return;
    const auto before = toJson();
    DrillSet copy = m_sets[m_currentSet];
    copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    for (auto &variant : copy.variants)
        variant.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    copy.activeVariantId = copy.variants.first().id;
    copy.activeVariant().name += QStringLiteral(" Copy");
    m_sets.insert(m_currentSet + 1, copy);
    ++m_currentSet;
    emit setsChanged(); emit currentSetChanged(); emitAllDataChanged();
    commitSnapshot(before, QStringLiteral("Duplicate set"));
}

void DrillProject::duplicateSetAt(int index)
{
    if(index<0||index>=m_sets.size())return; const auto before=toJson(); DrillSet copy=m_sets[index];
    copy.id=QUuid::createUuid().toString(QUuid::WithoutBraces); copy.archivedOrder=-1;
    auto renewVariant=[](MarchCraft::SetVariant &variant){variant.id=QUuid::createUuid().toString(QUuid::WithoutBraces);QHash<QString,QString> groupIds;for(auto&group:variant.groups){const QString old=group.id;group.id=QUuid::createUuid().toString(QUuid::WithoutBraces);groupIds.insert(old,group.id);}for(auto&shape:variant.shapes){shape.id=QUuid::createUuid().toString(QUuid::WithoutBraces);if(groupIds.contains(shape.groupId))shape.groupId=groupIds.value(shape.groupId);}};
    for(auto&variant:copy.variants)renewVariant(variant);for(auto&variant:copy.archivedVariants)renewVariant(variant);
    copy.activeVariantId=copy.variants.value(qMax(0,m_sets[index].activeVariantIndex())).id;
    const int insert=index+1; const int counts=qMax(1,copy.counts); copy.startTick=advancePulses(m_sets[index].startTick,counts);
    const qint64 shift=copy.startTick-m_sets[index].startTick; for(int i=insert;i<m_sets.size();++i)m_sets[i].startTick+=shift;
    m_sets.insert(insert,copy);m_currentSet=insert;recalculateCounts();emit setsChanged();emit currentSetChanged();emitAllDataChanged();commitSnapshot(before,QStringLiteral("Copy set"));
}

void DrillProject::insertSetAt(int index)
{
    index=qBound(0,index,m_sets.size());const auto before=toJson();DrillSet set;set.number=QString::number(index+1);set.activeVariant().name=QStringLiteral("New set");set.counts=index==0?0:8;
    if(!m_sets.isEmpty()){const int source=qBound(0,index-1,m_sets.size()-1);set.activeVariant().placements=m_sets[source].activeVariant().placements;set.startTick=index==0?0:advancePulses(m_sets[source].startTick,8);const qint64 shift=index==0?advancePulses(0,8):set.startTick-m_sets[source].startTick;for(int i=index;i<m_sets.size();++i)m_sets[i].startTick+=shift;}
    m_sets.insert(index,set);m_currentSet=index;recalculateCounts();emit setsChanged();emit currentSetChanged();emitAllDataChanged();commitSnapshot(before,QStringLiteral("Insert set"));
}

void DrillProject::archiveSetAt(int index){if(index<0||index>=m_sets.size()||m_sets.size()<=1)return;setCurrentSetIndex(index);archiveCurrentSet();}

void DrillProject::moveSet(int from,int to)
{
    if (from < 0 || from >= m_sets.size() || to < 0 || to >= m_sets.size() || from == to) return;
    const auto before = toJson();
    auto remapIndex = [from, to](int index) {
        if (index == from) return to;
        if (from < to && index > from && index <= to) return index - 1;
        if (to < from && index >= to && index < from) return index + 1;
        return index;
    };
    QHash<QString,int> retainedCounts;
    for (int index = 0; index < m_sets.size(); ++index) retainedCounts.insert(m_sets[index].id,
        index == 0 && m_sets[index].counts <= 0
            ? qMax(1, m_openingBehavior == QStringLiteral("hold") ? m_openingCounts
                : (m_sets.size() > 1 ? m_sets[1].counts : 8))
            : qMax(1, m_sets[index].counts));
    DrillSet moved = m_sets.takeAt(from); m_sets.insert(to, moved);
    for (auto &prop : m_props) {
        const int mappedStart = remapIndex(prop.motion.startSet), mappedEnd = remapIndex(prop.motion.endSet);
        prop.motion.startSet = qMin(mappedStart, mappedEnd); prop.motion.endSet = qMax(mappedStart, mappedEnd);
    }
    qint64 tick = 0;
    for (int index = 0; index < m_sets.size(); ++index) {
        m_sets[index].startTick = tick;
        m_sets[index].counts = retainedCounts.value(m_sets[index].id, 8);
        if (index + 1 < m_sets.size()) tick = advancePulses(tick, retainedCounts.value(m_sets[index + 1].id, 8));
    }
    m_currentSet = to; m_selectedSetStart = m_selectedSetEnd = to;
    recalculateCounts(); emit propsChanged(); emit timingChanged(); emit setsChanged(); emit setRangeChanged();
    emit currentSetChanged(); emitAllDataChanged();
    setStatus(QStringLiteral("Moved %1 to timeline position %2; transitions and timing were rebuilt")
        .arg(moved.number.isEmpty() ? moved.activeVariant().name : moved.number).arg(to + 1));
    commitSnapshot(before, QStringLiteral("Reorder sets"));
}

bool DrillProject::setLabelsNeedRenumbering() const
{
    int full = 0, subset = 0;
    for (const auto &set : m_sets) {
        QString expected;
        if (!set.subset) {
            ++full;
            subset = 0;
            expected = QString::number(full);
        } else {
            ++subset;
            expected = QString::number(qMax(1, full)) + QChar('A' + qMin(25, subset - 1));
        }
        if (set.number.trimmed() != expected) return true;
    }
    return false;
}

void DrillProject::renumberSets()
{
    const auto before=toJson();int full=0,subset=0;for(auto&set:m_sets){const QString oldNumber=set.number;if(!set.subset){++full;subset=0;set.number=QString::number(full);}else{++subset;set.number=QString::number(qMax(1,full))+QChar('A'+qMin(25,subset-1));}const QString oldDefault=QStringLiteral("Set %1").arg(oldNumber);if(set.activeVariant().name==oldDefault||set.activeVariant().name==QStringLiteral("New set")||QRegularExpression(QStringLiteral("^Set \\d+[A-Z]?$"),QRegularExpression::CaseInsensitiveOption).match(set.activeVariant().name).hasMatch())set.activeVariant().name=QStringLiteral("Set %1").arg(set.number);}emit setsChanged();commitSnapshot(before,QStringLiteral("Renumber sets"));
}

void DrillProject::removeCurrentSet()
{
    archiveCurrentSet();
}

void DrillProject::createVariant(const QString &name, const QString &caption)
{
    if (m_currentSet < 0 || m_currentSet >= m_sets.size()) return;
    const auto before = toJson();
    auto &set = m_sets[m_currentSet];
    const auto &source = set.activeVariant();
    MarchCraft::SetVariant variant;
    variant.label = QString(QChar(char16_t(u'A' + static_cast<char16_t>(set.variants.size()))));
    variant.name = name.trimmed().isEmpty()
        ? source.name + QStringLiteral(" ") + variant.label : name.trimmed();
    variant.caption = caption.trimmed();
    variant.placements = source.placements;
    set.variants.push_back(variant);
    set.activeVariantId = variant.id;
    emit setsChanged(); emit currentSetChanged(); emitAllDataChanged();
    commitSnapshot(before, QStringLiteral("Create set variant"));
}

void DrillProject::activateVariant(int index)
{
    if (m_currentSet < 0 || m_currentSet >= m_sets.size()) return;
    auto &set = m_sets[m_currentSet];
    if (index < 0 || index >= set.variants.size() || index == set.activeVariantIndex()) return;
    const auto before = toJson();
    set.activeVariantId = set.variants[index].id;
    emit setsChanged(); emit currentSetChanged(); emitAllDataChanged();
    commitSnapshot(before, QStringLiteral("Activate set variant"));
}

void DrillProject::archiveCurrentVariant()
{
    if (m_currentSet < 0 || m_currentSet >= m_sets.size()) return;
    auto &set = m_sets[m_currentSet];
    if (set.variants.size() <= 1) {
        archiveCurrentSet();
        return;
    }
    const auto before = toJson();
    const int index = set.activeVariantIndex();
    set.archivedVariants.push_back(set.variants.takeAt(index));
    set.activeVariantId = set.variants[qMin(index, static_cast<int>(set.variants.size()) - 1)].id;
    emit setsChanged(); emit currentSetChanged(); emitAllDataChanged();
    commitSnapshot(before, QStringLiteral("Archive set variant"));
}

void DrillProject::archiveCurrentSet()
{
    if (m_sets.size() <= 1 || m_currentSet < 0 || m_currentSet >= m_sets.size()) {
        setStatus(QStringLiteral("A show must keep at least one active set"));
        return;
    }
    const auto before = toJson();
    DrillSet archived = m_sets.takeAt(m_currentSet);
    archived.archivedOrder = m_currentSet;
    m_archivedSets.push_back(std::move(archived));
    m_currentSet = qMin(m_currentSet, static_cast<int>(m_sets.size()) - 1);
    if (!m_sets.isEmpty() && m_sets[0].startTick != 0) {
        const qint64 offset = m_sets[0].startTick;
        for (auto &set : m_sets) set.startTick -= offset;
    }
    recalculateCounts();
    emit setsChanged(); emit currentSetChanged(); emitAllDataChanged();
    commitSnapshot(before, QStringLiteral("Archive set"));
}

void DrillProject::restoreArchivedSet(int index)
{
    if (index < 0 || index >= m_archivedSets.size()) return;
    const auto before = toJson();
    DrillSet restored = m_archivedSets.takeAt(index);
    const int insertAt = std::clamp(restored.archivedOrder, 0, static_cast<int>(m_sets.size()));
    restored.archivedOrder = -1;
    m_sets.insert(insertAt, std::move(restored));
    m_currentSet = insertAt;
    if (insertAt == 0) {
        const qint64 shift = advancePulses(0, qMax(1, m_sets[0].counts));
        m_sets[0].startTick = 0;
        for (int i = 1; i < m_sets.size(); ++i) m_sets[i].startTick += shift;
    }
    recalculateCounts();
    emit setsChanged(); emit currentSetChanged(); emitAllDataChanged();
    commitSnapshot(before, QStringLiteral("Restore archived set"));
}

void DrillProject::purgeArchivedSet(int index)
{
    if (index < 0 || index >= m_archivedSets.size()) return;
    const auto before = toJson();
    m_archivedSets.removeAt(index);
    emit setsChanged();
    commitSnapshot(before, QStringLiteral("Permanently delete archived set"));
}

void DrillProject::restoreArchivedVariant(int index)
{
    if (m_currentSet < 0 || m_currentSet >= m_sets.size()) return;
    auto &set = m_sets[m_currentSet];
    if (index < 0 || index >= set.archivedVariants.size()) return;
    const auto before = toJson();
    set.variants.push_back(set.archivedVariants.takeAt(index));
    set.activeVariantId = set.variants.last().id;
    emit setsChanged(); emit currentSetChanged(); emitAllDataChanged();
    commitSnapshot(before, QStringLiteral("Restore archived variant"));
}

void DrillProject::purgeArchivedVariant(int index)
{
    if (m_currentSet < 0 || m_currentSet >= m_sets.size()) return;
    auto &set = m_sets[m_currentSet];
    if (index < 0 || index >= set.archivedVariants.size()) return;
    const auto before = toJson();
    set.archivedVariants.removeAt(index);
    emit setsChanged();
    commitSnapshot(before, QStringLiteral("Permanently delete archived variant"));
}

void DrillProject::updateCurrentSet(const QString &number, const QString &name,
                                    const QString &caption, const QString &measure,
                                    int counts, bool subset)
{
    if (m_currentSet < 0 || m_currentSet >= m_sets.size()) return;
    const auto before = toJson();
    auto &set = m_sets[m_currentSet];
    set.number = number.trimmed().isEmpty() ? QString::number(m_currentSet + 1) : number.trimmed();
    const QString requestedName = name.trimmed();
    set.activeVariant().name = requestedName.isEmpty()
        || QRegularExpression(QStringLiteral("^Set \\d+[A-Z]?$"), QRegularExpression::CaseInsensitiveOption).match(requestedName).hasMatch()
        ? QStringLiteral("Set %1").arg(set.number) : requestedName;
    set.activeVariant().caption = caption.trimmed();
    set.measure = measure.trimmed();
    if (m_currentSet == 0) {
        set.startTick = 0;
        set.stepMultiplier = 0.0;
    } else {
        set.counts = qMax(1, counts);
        set.startTick = advancePulses(m_sets[m_currentSet - 1].startTick, set.counts);
    }
    set.subset = subset;
    recalculateCounts();
    emit timingChanged();
    emit setsChanged(); emit currentSetChanged();
    commitSnapshot(before, QStringLiteral("Edit set"));
}

void DrillProject::selectPerformer(int row, bool additive)
{
    selectPerformerMode(row, additive ? 1 : 0);
}

void DrillProject::selectPerformerMode(int row, int mode)
{
    if (row < 0 || row >= m_performers.size()) return;
    if (!m_performers[row].visible || m_performers[row].locked) return;
    if (mode == 0)
        for (auto &person : m_performers) person.selected = false;
    if (mode == 0) {
        const auto &groups = m_sets[m_currentSet].activeVariant().groups;
        for (const auto &group : groups) {
            if (!group.performerIds.contains(m_performers[row].id)) continue;
            QSet<QString> members;
            for (const auto &id : group.performerIds) members.insert(id);
            for (auto &performer : m_performers)
                if (members.contains(performer.id) && performer.visible && !performer.locked)
                    performer.selected = true;
            emitAllDataChanged(); emit selectionChanged(); return;
        }
    }
    if (mode == 1)
        m_performers[row].selected = !m_performers[row].selected;
    else
        m_performers[row].selected = true;
    emitAllDataChanged();
    emit selectionChanged();
}

void DrillProject::selectPerformerRange(int anchorRow, int row, bool additive)
{
    if (m_performers.isEmpty()) return;
    anchorRow = std::clamp(anchorRow, 0, static_cast<int>(m_performers.size()) - 1);
    row = std::clamp(row, 0, static_cast<int>(m_performers.size()) - 1);
    if (!additive)
        for (auto &person : m_performers) person.selected = false;
    const int first = qMin(anchorRow, row);
    const int last = qMax(anchorRow, row);
    for (int i = first; i <= last; ++i)
        if (m_performers[i].visible && !m_performers[i].locked)
            m_performers[i].selected = true;
    emitAllDataChanged();
    emit selectionChanged();
}

void DrillProject::selectInRect(double x1, double y1, double x2, double y2, bool additive)
{
    if (!additive)
        for (auto &person : m_performers) person.selected = false;
    const QRectF bounds(QPointF(qMin(x1, x2), qMin(y1, y2)),
                        QPointF(qMax(x1, x2), qMax(y1, y2)));
    for (int row = 0; row < m_performers.size(); ++row) {
        if (m_performers[row].visible && !m_performers[row].locked
            && bounds.contains(placementAt(row, m_currentSet).position))
            m_performers[row].selected = true;
    }
    emitAllDataChanged();
    emit selectionChanged();
}

void DrillProject::selectInPolygon(const QVariantList &points, bool additive)
{
    if (points.size() < 3) return;
    QPolygonF polygon;
    polygon.reserve(points.size());
    for (const auto &point : points)
        polygon.push_back(point.toPointF());
    if (!additive)
        for (auto &person : m_performers) person.selected = false;
    for (int row = 0; row < m_performers.size(); ++row) {
        if (m_performers[row].visible && !m_performers[row].locked
            && polygon.containsPoint(placementAt(row, m_currentSet).position, Qt::OddEvenFill))
            m_performers[row].selected = true;
    }
    emitAllDataChanged();
    emit selectionChanged();
}

void DrillProject::selectAll()
{
    for (auto &person : m_performers) person.selected = person.visible && !person.locked;
    emitAllDataChanged(); emit selectionChanged();
}

void DrillProject::clearSelection()
{
    for (auto &person : m_performers) person.selected = false;
    emitAllDataChanged(); emit selectionChanged();
}

QVariantMap DrillProject::performerGroupInfo(int row) const
{
    if (row < 0 || row >= m_performers.size() || m_currentSet < 0) return {};
    const auto &groups = m_sets[m_currentSet].activeVariant().groups;
    for (int index = 0; index < groups.size(); ++index) {
        const auto &group = groups[index];
        if (group.performerIds.contains(m_performers[row].id))
            return {{QStringLiteral("id"), group.id}, {QStringLiteral("name"), group.name},
                    {QStringLiteral("size"), group.performerIds.size()}, {QStringLiteral("index"), index}};
    }
    return {};
}

void DrillProject::selectGroupForPerformer(int row, bool additive)
{
    const QVariantMap info = performerGroupInfo(row);
    if (info.isEmpty()) return;
    if (!additive) for (auto &performer : m_performers) performer.selected = false;
    const QString groupId = info.value(QStringLiteral("id")).toString();
    for (const auto &group : m_sets[m_currentSet].activeVariant().groups) {
        if (group.id != groupId) continue;
        QSet<QString> members;
        for (const auto &id : group.performerIds) members.insert(id);
        for (auto &performer : m_performers)
            if (members.contains(performer.id) && performer.visible && !performer.locked)
                performer.selected = true;
        break;
    }
    emitAllDataChanged(); emit selectionChanged();
}

void DrillProject::groupSelected(const QString &name)
{
    if (m_currentSet < 0) return;
    QVector<QString> selected;
    for (const auto &performer : m_performers) if (performer.selected) selected.push_back(performer.id);
    if (selected.size() < 2) { setStatus(QStringLiteral("Select at least two performers to group")); return; }
    const auto before = toJson();
    auto &variant = m_sets[m_currentSet].activeVariant();
    QSet<QString> selectedIds;
    for (const auto &id : selected) selectedIds.insert(id);
    for (auto &group : variant.groups)
        group.performerIds.erase(std::remove_if(group.performerIds.begin(), group.performerIds.end(),
            [&](const QString &id) { return selectedIds.contains(id); }), group.performerIds.end());
    variant.groups.erase(std::remove_if(variant.groups.begin(), variant.groups.end(),
        [](const auto &group) { return group.performerIds.size() < 2; }), variant.groups.end());
    MarchCraft::PerformerGroup group; group.name = name.trimmed(); group.performerIds = selected;
    variant.groups.push_back(group);
    setStatus(QStringLiteral("Grouped %1 performers").arg(selected.size()));
    commitSnapshot(before, QStringLiteral("Group performers"));
}

void DrillProject::removeSelectedFromGroup()
{
    if (m_currentSet < 0) return;
    QSet<QString> selected;
    for (const auto &performer : m_performers) if (performer.selected) selected.insert(performer.id);
    if (selected.isEmpty()) return;
    const auto before = toJson(); auto &groups = m_sets[m_currentSet].activeVariant().groups;
    for (auto &group : groups)
        group.performerIds.erase(std::remove_if(group.performerIds.begin(), group.performerIds.end(),
            [&](const QString &id) { return selected.contains(id); }), group.performerIds.end());
    groups.erase(std::remove_if(groups.begin(), groups.end(),
        [](const auto &group) { return group.performerIds.size() < 2; }), groups.end());
    commitSnapshot(before, QStringLiteral("Remove performers from group"));
}

void DrillProject::ungroupSelected()
{
    if (m_currentSet < 0) return;
    QSet<QString> selected;
    for (const auto &performer : m_performers) if (performer.selected) selected.insert(performer.id);
    if (selected.isEmpty()) return;
    const auto before = toJson(); auto &groups = m_sets[m_currentSet].activeVariant().groups;
    groups.erase(std::remove_if(groups.begin(), groups.end(), [&](const auto &group) {
        for (const auto &id : group.performerIds) if (selected.contains(id)) return true;
        return false;
    }), groups.end());
    commitSnapshot(before, QStringLiteral("Ungroup performers"));
}

void DrillProject::beginMove(int row, bool additive)
{
    if (row < 0 || row >= m_performers.size()) return;
    if (!m_performers[row].visible || m_performers[row].locked) return;
    if (!m_performers[row].selected)
        selectPerformer(row, additive);
    m_moveBefore = toJson();
    m_moveStarts.clear();
    m_shapePointStarts.clear();
    m_shapeAnchorStarts.clear();
    m_shapeRotationStarts.clear();
    m_shapeSizeStarts.clear();
    if (m_currentSet < 0) return;
    for (const auto &person : m_performers)
        if (person.selected)
            m_moveStarts.insert(person.id, m_sets[m_currentSet].activeVariant().placements.value(person.id).position);
    for (const auto &shape : m_sets[m_currentSet].activeVariant().shapes) {
        bool intersects = false;
        for (const auto &id : shape.performerIds) if (m_moveStarts.contains(id)) { intersects = true; break; }
        if (intersects) { m_shapePointStarts.insert(shape.id, shape.points); m_shapeAnchorStarts.insert(shape.id, shape.anchor); m_shapeRotationStarts.insert(shape.id, shape.rotation); m_shapeSizeStarts.insert(shape.id,QSizeF(shape.width,shape.height)); }
    }
}

void DrillProject::previewMove(double dx, double dy, bool lockX, bool lockY)
{
    if (m_currentSet < 0 || m_moveStarts.isEmpty()) return;
    if (lockX) dy = 0.0;
    if (lockY) dx = 0.0;
    double minX = canvasMaxX(), maxX = canvasMinX(), minY = canvasMaxY(), maxY = canvasMinY();
    for (const auto &point : std::as_const(m_moveStarts)) {
        minX = qMin(minX, point.x()); maxX = qMax(maxX, point.x());
        minY = qMin(minY, point.y()); maxY = qMax(maxY, point.y());
    }
    dx = qBound(canvasMinX() - minX, dx, canvasMaxX() - maxX);
    dy = qBound(canvasMinY() - minY, dy, canvasMaxY() - maxY);
    for (auto it = m_moveStarts.cbegin(); it != m_moveStarts.cend(); ++it)
        m_sets[m_currentSet].activeVariant().placements[it.key()].position =
            it.value() + QPointF(dx, dy);
    for (auto &shape : m_sets[m_currentSet].activeVariant().shapes) {
        if (!m_shapePointStarts.contains(shape.id)) continue;
        bool allMoved = !shape.performerIds.isEmpty();
        for (const auto &id : shape.performerIds) if (!m_moveStarts.contains(id)) { allMoved = false; break; }
        if (!allMoved) continue;
        shape.points = m_shapePointStarts.value(shape.id);
        for (auto &point : shape.points) point += QPointF(dx, dy);
        shape.anchor = m_shapeAnchorStarts.value(shape.id) + QPointF(dx, dy);
    }
    emit shapesChanged();
    if (!m_performers.isEmpty())
        emit dataChanged(index(0), index(m_performers.size() - 1), {XRole, YRole});
}

void DrillProject::endMove()
{
    if (m_moveBefore.isEmpty()) return;
    if (m_currentSet >= 0) {
        auto &shapes = m_sets[m_currentSet].activeVariant().shapes;
        for (int i = shapes.size() - 1; i >= 0; --i) {
            auto &shape = shapes[i];
            if (!m_shapePointStarts.contains(shape.id)) continue;
            bool allMoved = !shape.performerIds.isEmpty(), anyMoved = false;
            for (const auto &id : shape.performerIds) {
                anyMoved = anyMoved || m_moveStarts.contains(id);
                allMoved = allMoved && m_moveStarts.contains(id);
            }
            if (!anyMoved || allMoved) continue;
            shape.performerIds.erase(std::remove_if(shape.performerIds.begin(), shape.performerIds.end(),
                [this](const QString &id) { return m_moveStarts.contains(id); }), shape.performerIds.end());
            if (shape.performerIds.size() < 2) { shapes.removeAt(i); continue; }
            shape.type = QStringLiteral("detached"); shape.closed = false; shape.points.clear();
            for (const auto &id : shape.performerIds)
                shape.points.push_back(m_sets[m_currentSet].activeVariant().placements.value(id).position);
        }
    }
    commitSnapshot(m_moveBefore, QStringLiteral("Move performers"));
    m_moveBefore = {};
    m_moveStarts.clear();
    m_shapePointStarts.clear(); m_shapeAnchorStarts.clear(); m_shapeRotationStarts.clear(); m_shapeSizeStarts.clear(); emit shapesChanged();
}

QVariantMap DrillProject::selectedBounds() const
{
    bool found = false; double left=0,right=0,top=0,bottom=0;
    for (int row=0; row<m_performers.size(); ++row) if (m_performers[row].selected) {
        const QPointF p=placementAt(row,m_currentSet).position;
        if(!found){left=right=p.x();top=bottom=p.y();found=true;}
        else { left=qMin(left,p.x()); right=qMax(right,p.x()); top=qMin(top,p.y()); bottom=qMax(bottom,p.y()); }
    }
    if(!found) return {};
    return {{QStringLiteral("left"),left},{QStringLiteral("right"),right},{QStringLiteral("top"),top},{QStringLiteral("bottom"),bottom},
            {QStringLiteral("centerX"),(left+right)/2.0},{QStringLiteral("centerY"),(top+bottom)/2.0}};
}

int DrillProject::selectedShapeIndex() const
{
    if(m_currentSet<0)return -1;QSet<QString> selected;for(const auto&p:m_performers)if(p.selected)selected.insert(p.id);
    if(selected.size()<2)return -1;const auto&shapes=m_sets[m_currentSet].activeVariant().shapes;
    for(int i=0;i<shapes.size();++i){if(shapes[i].performerIds.size()!=selected.size())continue;bool exact=true;for(const auto&id:shapes[i].performerIds)exact=exact&&selected.contains(id);if(exact)return i;}return -1;
}

void DrillProject::beginScale()
{
    const int shapeIndex=selectedShapeIndex();if(shapeIndex<0)return;const auto&shape=m_sets[m_currentSet].activeVariant().shapes[shapeIndex];
    int row=0;while(row<m_performers.size()&&!m_performers[row].selected)++row;beginMove(row,false);m_rotatePivot=shape.anchor;
}

void DrillProject::previewScale(double factor)
{
    if(m_moveStarts.isEmpty()||selectedShapeIndex()<0)return;factor=qBound(0.08,factor,12.0);
    QHash<QString,QPointF> scaled;double left=1e9,right=-1e9,top=1e9,bottom=-1e9;
    for(auto it=m_moveStarts.cbegin();it!=m_moveStarts.cend();++it){const QPointF p=m_rotatePivot+(it.value()-m_rotatePivot)*factor;scaled.insert(it.key(),p);left=qMin(left,p.x());right=qMax(right,p.x());top=qMin(top,p.y());bottom=qMax(bottom,p.y());}
    QPointF shift;if(left<canvasMinX())shift.rx()+=canvasMinX()-left;if(right>canvasMaxX())shift.rx()-=right-canvasMaxX();if(top<canvasMinY())shift.ry()+=canvasMinY()-top;if(bottom>canvasMaxY())shift.ry()-=bottom-canvasMaxY();
    for(auto it=scaled.cbegin();it!=scaled.cend();++it)m_sets[m_currentSet].activeVariant().placements[it.key()].position=it.value()+shift;
    for(auto&shape:m_sets[m_currentSet].activeVariant().shapes){if(!m_shapePointStarts.contains(shape.id))continue;bool all=!shape.performerIds.isEmpty();for(const auto&id:shape.performerIds)all=all&&m_moveStarts.contains(id);if(!all)continue;shape.points=m_shapePointStarts.value(shape.id);for(auto&p:shape.points)p=m_rotatePivot+(p-m_rotatePivot)*factor+shift;shape.anchor=m_rotatePivot+shift;const QSizeF size=m_shapeSizeStarts.value(shape.id);shape.width=size.width()*factor;shape.height=size.height()*factor;}
    emit shapesChanged();if(!m_performers.isEmpty())emit dataChanged(index(0),index(m_performers.size()-1),{XRole,YRole});
}

void DrillProject::endScale()
{
    if(m_moveBefore.isEmpty())return;commitSnapshot(m_moveBefore,QStringLiteral("Resize formation"));m_moveBefore={};m_moveStarts.clear();m_shapePointStarts.clear();m_shapeAnchorStarts.clear();m_shapeRotationStarts.clear();m_shapeSizeStarts.clear();emit shapesChanged();
}

void DrillProject::beginRotate()
{
    const auto bounds=selectedBounds(); if(bounds.isEmpty() || selectedCount()<2) return;
    int row=0; while(row<m_performers.size() && !m_performers[row].selected) ++row;
    beginMove(row,false);
    m_rotatePivot={bounds.value(QStringLiteral("centerX")).toDouble(),bounds.value(QStringLiteral("centerY")).toDouble()};
}

void DrillProject::previewRotate(double degrees)
{
    if(m_moveStarts.isEmpty()) return;
    QHash<QString,QPointF> rotated; double left=1e9,right=-1e9,top=1e9,bottom=-1e9;
    for(auto it=m_moveStarts.cbegin();it!=m_moveStarts.cend();++it){ const QPointF p=rotateAround(it.value(),m_rotatePivot,degrees); rotated.insert(it.key(),p); left=qMin(left,p.x());right=qMax(right,p.x());top=qMin(top,p.y());bottom=qMax(bottom,p.y()); }
    QPointF shift; if(left<canvasMinX())shift.rx()+=canvasMinX()-left; if(right>canvasMaxX())shift.rx()-=right-canvasMaxX(); if(top<canvasMinY())shift.ry()+=canvasMinY()-top; if(bottom>canvasMaxY())shift.ry()-=bottom-canvasMaxY();
    for(auto it=rotated.cbegin();it!=rotated.cend();++it) m_sets[m_currentSet].activeVariant().placements[it.key()].position=it.value()+shift;
    for(auto &shape:m_sets[m_currentSet].activeVariant().shapes){ if(!m_shapePointStarts.contains(shape.id))continue; bool all=!shape.performerIds.isEmpty(); for(const auto&id:shape.performerIds)all=all&&m_moveStarts.contains(id); if(!all)continue; shape.points=m_shapePointStarts.value(shape.id); for(auto&p:shape.points)p=rotateAround(p,m_rotatePivot,degrees)+shift; shape.anchor=rotateAround(m_shapeAnchorStarts.value(shape.id),m_rotatePivot,degrees)+shift; shape.rotation=m_shapeRotationStarts.value(shape.id)+degrees; }
    emit shapesChanged(); if(!m_performers.isEmpty())emit dataChanged(index(0),index(m_performers.size()-1),{XRole,YRole});
}

void DrillProject::endRotate()
{
    if(m_moveBefore.isEmpty())return; commitSnapshot(m_moveBefore,QStringLiteral("Rotate formation")); m_moveBefore={};m_moveStarts.clear();m_shapePointStarts.clear();m_shapeAnchorStarts.clear();m_shapeRotationStarts.clear();m_shapeSizeStarts.clear();emit shapesChanged();
}

void DrillProject::nudgeSelected(double dx, double dy)
{
    if (m_currentSet < 0 || selectedCount() == 0) return;
    int row = 0; while (row < m_performers.size() && !m_performers[row].selected) ++row;
    beginMove(row, false); previewMove(dx, dy); endMove();
}

QVector<int> DrillProject::minimumCostAssignment(const QVector<QVector<double>> &costs) const
{
    const int n = costs.size();
    if (n == 0) return {};
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

void DrillProject::createFormation(const QString &type, const QVariantMap &options)
{
    if (!m_generatingFormationPreview) {
        const QString mode = options.value(QStringLiteral("assignmentMode"), QStringLiteral("rehearsalSafe")).toString();
        previewFormation(type, options, mode); commitFormationPreview(); return;
    }
    QVector<int> selected;
    for (int i = 0; i < m_performers.size(); ++i) if (m_performers[i].selected) selected.push_back(i);
    if (selected.isEmpty() || m_currentSet < 0) return;
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
    } else return;
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
    if (placements.size() != selected.size()) return;
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
        DrillProject clone; clone.m_backgroundWorkerClone = true; clone.restoreJson(snapshot); clone.m_currentSet = currentSet;
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
        DrillProject clone; clone.m_backgroundWorkerClone = true; clone.restoreJson(snapshot); clone.m_currentSet = currentSet;
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
    if (m_currentSet <= 0 || selectedCount() == 0) return;
    static const QSet<QString> supported{QStringLiteral("direct"), QStringLiteral("curved"),
        QStringLiteral("follow"), QStringLiteral("gate"), QStringLiteral("pivot"), QStringLiteral("delayed")};
    const QString actual = supported.contains(type) ? type : QStringLiteral("direct");
    QVector<QPointF> points;
    for (const auto &value : controlPoints) {
        const QPointF point = value.toPointF();
        if (!point.isNull() || value.canConvert<QPointF>()) points.push_back(clampPosition(point));
    }
    const auto before = toJson();
    for (const auto &person : m_performers) if (person.selected) {
        auto &placement = m_sets[m_currentSet].activeVariant().placements[person.id];
        placement.pathType = actual; placement.pathPoints = points;
        if (actual == QStringLiteral("curved") && placement.pathPoints.isEmpty()) {
            const QPointF from = m_sets[m_currentSet - 1].activeVariant().placements.value(person.id).position;
            const QPointF mid = (from + placement.position) / 2.0;
            placement.pathPoints.push_back(clampPosition(mid + QPointF(0, -8)));
        }
    }
    emitAllDataChanged(); commitSnapshot(before, QStringLiteral("Edit transition path"));
}

QVariantList DrillProject::transitionPathSamples(int performerRow, int samples) const
{
    QVariantList result;
    if (performerRow < 0 || performerRow >= m_performers.size() || m_currentSet <= 0) return result;
    samples = qBound(2, samples, 128);
    for (int i = 0; i <= samples; ++i)
        result.push_back(pathPosition(performerRow, m_currentSet, double(i) / samples));
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
        if (m_dismissedClinicIssues.contains(id)) return;
        QVariantList ids; for (int row : rows) if (row >= 0 && row < m_performers.size()) ids.push_back(m_performers[row].id);
        issues.push_back(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("type"), type},
            {QStringLiteral("severity"), severity}, {QStringLiteral("title"), title}, {QStringLiteral("detail"), detail},
            {QStringLiteral("setIndex"), destinationSet}, {QStringLiteral("setLabel"), setInfo(destinationSet).value(QStringLiteral("number"))},
            {QStringLiteral("count"), countPosition}, {QStringLiteral("measured"), measured}, {QStringLiteral("limit"), limit},
            {QStringLiteral("performerIds"), ids}, {QStringLiteral("performers"), issueLabelList(rows)},
            {QStringLiteral("affectedCount"), rows.size()}, {QStringLiteral("actions"), actions}});
    };

    QVector<int> strideRows, cautionStrideRows; double maximumStride = 0.0;
    for (int row = 0; row < m_performers.size(); ++row) {
        const double stride = transitionDistance(row, destinationSet) / counts; maximumStride = qMax(maximumStride, stride);
        if (stride > m_capability.maximumStepsPerCount) strideRows.push_back(row);
        else if (stride >= m_capability.maximumStepsPerCount * 0.85) cautionStrideRows.push_back(row);
    }
    if (!strideRows.isEmpty()) {
        const int recommended = qCeil(maximumStride * counts / m_capability.maximumStepsPerCount);
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
                    const double equipmentClearance = instrumentClearanceRadius(m_performers[row])
                        + instrumentClearanceRadius(m_performers[other]) + 0.25;
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
    for (int sample = 0; sample <= samples; ++sample) {
        const double progress = double(sample) / samples;
        for (int row = 0; row < m_performers.size(); ++row) {
            const QPointF performerPosition = pathPosition(row, destinationSet, progress);
            for (const auto &prop : m_props) {
                if (prop.assignedMoverIds.contains(m_performers[row].id)) continue;
                const QPointF propPosition = propPositionAt(prop, destinationSet, progress);
                const double edgeClearance = distanceToPropFootprint(performerPosition, propPosition,
                    prop.rotation, propFootprintSteps(prop));
                const double required = instrumentClearanceRadius(m_performers[row]) + 0.5;
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
        }
    } else if (type == QStringLiteral("delayed")) {
        for (const auto &performer : m_performers) if (affected.contains(performer.id)) {
            auto &placement = m_sets[destination].activeVariant().placements[performer.id];
            placement.pathType = QStringLiteral("delayed"); placement.pathPoints.clear();
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

QVariantMap DrillProject::setInfo(int index) const
{
    if (index < 0 || index >= m_sets.size()) return {};
    const auto &set = m_sets[index];
    const int endingMeasureIndex = musicMeasureAtTick(qMax<qint64>(0, set.startTick - (index > 0 ? 1 : 0)));
    const auto endingMeasure = endingMeasureIndex >= 0 ? m_music.measures.value(endingMeasureIndex) : MarchCraft::MusicMeasure{};
    const int beat = endingMeasureIndex >= 0
        ? qMax(1, int((set.startTick - endingMeasure.startTick) / qMax<qint64>(1, endingMeasure.pulseTicks)) + 1) : 0;
    const double duration = index > 0 ? transitionDurationMs(index) : 0.0;
    return {{QStringLiteral("number"), set.number.isEmpty() ? QString::number(index + 1) : set.number},
            {QStringLiteral("name"), set.activeVariant().name},
            {QStringLiteral("caption"), set.activeVariant().caption},
            {QStringLiteral("measure"), set.measure},
            {QStringLiteral("counts"), index == 0 ? (m_openingBehavior == QStringLiteral("hold") ? m_openingCounts : 0) : set.counts},
            {QStringLiteral("opening"), index == 0}, {QStringLiteral("openingBehavior"), index == 0 ? m_openingBehavior : QString{}},
            {QStringLiteral("startTick"), set.startTick},
            {QStringLiteral("endingMeasure"), endingMeasureIndex >= 0 ? endingMeasure.displayNumber : 0},
            {QStringLiteral("endingBeat"), beat}, {QStringLiteral("durationMs"), duration},
            {QStringLiteral("stepMultiplier"), set.stepMultiplier},
            {QStringLiteral("steps"), set.counts * set.stepMultiplier},
            {QStringLiteral("tempo"), effectiveTempoText(index)}, {QStringLiteral("subset"), set.subset},
            {QStringLiteral("variantLabel"), set.activeVariant().label},
            {QStringLiteral("variantCount"), set.variants.size()},
            {QStringLiteral("archivedVariantCount"), set.archivedVariants.size()}};
}

QVariantMap DrillProject::variantInfo(int index) const
{
    if (m_currentSet < 0 || m_currentSet >= m_sets.size()) return {};
    const auto &set = m_sets[m_currentSet];
    if (index < 0 || index >= set.variants.size()) return {};
    const auto &variant = set.variants[index];
    return {{QStringLiteral("label"), variant.label},
            {QStringLiteral("name"), variant.name},
            {QStringLiteral("caption"), variant.caption},
            {QStringLiteral("active"), index == set.activeVariantIndex()}};
}

QVariantMap DrillProject::archivedSetInfo(int index) const
{
    if (index < 0 || index >= m_archivedSets.size()) return {};
    const auto &set = m_archivedSets[index];
    return {{QStringLiteral("number"), set.number},
            {QStringLiteral("name"), set.activeVariant().name},
            {QStringLiteral("caption"), set.activeVariant().caption},
            {QStringLiteral("measure"), set.measure},
            {QStringLiteral("variantCount"), set.variants.size()}};
}

QVariantMap DrillProject::archivedVariantInfo(int index) const
{
    if (m_currentSet < 0 || m_currentSet >= m_sets.size()) return {};
    const auto &set = m_sets[m_currentSet];
    if (index < 0 || index >= set.archivedVariants.size()) return {};
    const auto &variant = set.archivedVariants[index];
    return {{QStringLiteral("label"), variant.label},
            {QStringLiteral("name"), variant.name},
            {QStringLiteral("caption"), variant.caption}};
}

QVariantMap DrillProject::performerInfo(int row) const
{
    if (row < 0 || row >= m_performers.size()) return {};
    const auto &person = m_performers[row];
    const double incoming = m_currentSet > 0 ? transitionDistance(row, m_currentSet) : 0.0;
    const double outgoing = m_currentSet + 1 < m_sets.size() ? transitionDistance(row, m_currentSet + 1) : 0.0;
    const int incomingCounts = m_currentSet > 0 ? qMax(1, m_sets[m_currentSet].counts) : 1;
    double directionChange = 0.0;
    if (m_currentSet > 0 && m_currentSet + 1 < m_sets.size()) {
        const QPointF incomingVector = placementAt(row, m_currentSet).position - placementAt(row, m_currentSet - 1).position;
        const QPointF outgoingVector = placementAt(row, m_currentSet + 1).position - placementAt(row, m_currentSet).position;
        const double lengths = pointDistance({}, incomingVector) * pointDistance({}, outgoingVector);
        if (lengths > 0.001)
            directionChange = std::acos(qBound(-1.0, QPointF::dotProduct(incomingVector, outgoingVector) / lengths, 1.0))
                * 180.0 / std::numbers::pi;
    }
    const double stepsPerCount = incoming / incomingCounts;
    const QString warning = stepsPerCount > m_capability.maximumStepsPerCount ? QStringLiteral("critical")
        : stepsPerCount >= m_capability.maximumStepsPerCount * 0.85 ? QStringLiteral("caution") : QStringLiteral("none");
    return {{QStringLiteral("label"), person.label}, {QStringLiteral("name"), person.name},
            {QStringLiteral("instrument"), person.instrument}, {QStringLiteral("section"), person.section},
            {QStringLiteral("notes"), person.notes}, {QStringLiteral("coordinate"), coordinateFor(row)},
            {QStringLiteral("color"), person.color.name(QColor::HexRgb)},
            {QStringLiteral("facing"), placementAt(row, m_currentSet).facing},
            {QStringLiteral("visible"), person.visible}, {QStringLiteral("locked"), person.locked},
            {QStringLiteral("bodyRigId"), person.appearance.bodyRigId},
            {QStringLiteral("uniformId"), person.appearance.uniformId},
            {QStringLiteral("skinPaletteId"), person.appearance.skinPaletteId},
            {QStringLiteral("instrumentAssetId"), person.appearance.instrumentAssetId},
            {QStringLiteral("equipmentAssetId"), person.appearance.equipmentAssetId},
            {QStringLiteral("heightMeters"), person.appearance.heightMeters},
            {QStringLiteral("roleId"), person.appearance.roleId},
            {QStringLiteral("incomingDistance"), incoming}, {QStringLiteral("outgoingDistance"), outgoing},
            {QStringLiteral("stepsPerCount"), stepsPerCount}, {QStringLiteral("directionChange"), directionChange},
            {QStringLiteral("pathType"), m_currentSet > 0 ? placementAt(row, m_currentSet).pathType : QStringLiteral("direct")},
            {QStringLiteral("warning"), warning},
            {QStringLiteral("distance"), performerTotalDistance(row)}};
}

QString DrillProject::coordinateFor(int row, int setIndex) const
{
    if (row < 0 || row >= m_performers.size()) return {};
    if (setIndex < 0) setIndex = m_currentSet;
    const auto point = placementAt(row, setIndex).position;
    if (point.y() >= 0.0 && point.y() <= fieldDepthSteps()) {
        if (point.x() < 0.0 && point.x() >= canvasMinX())
            return QStringLiteral("Side 1 end zone · %1 steps beyond the goal line")
                .arg(compactNumber(-point.x()));
        if (point.x() > fieldWidthSteps() && point.x() <= canvasMaxX())
            return QStringLiteral("Side 2 end zone · %1 steps beyond the goal line")
                .arg(compactNumber(point.x() - fieldWidthSteps()));
    }
    QStringList apronParts;
    if (point.x() < 0.0)
        apronParts << QStringLiteral("%1 steps outside Side 1 goal line").arg(compactNumber(-point.x()));
    else if (point.x() > fieldWidthSteps())
        apronParts << QStringLiteral("%1 steps outside Side 2 goal line").arg(compactNumber(point.x() - fieldWidthSteps()));
    if (point.y() < 0.0)
        apronParts << QStringLiteral("%1 steps in front of front sideline").arg(compactNumber(-point.y()));
    else if (point.y() > fieldDepthSteps())
        apronParts << QStringLiteral("%1 steps behind back sideline").arg(compactNumber(point.y() - fieldDepthSteps()));
    if (!apronParts.isEmpty()) return QStringLiteral("Staging apron: %1").arg(apronParts.join(QStringLiteral(" · ")));
    const double centered = point.x() - 80.0;
    const int side = centered <= 0.0 ? 1 : 2;
    const double rawYard = 50.0 - std::abs(centered) * 5.0 / 8.0;
    const double yard = std::round(rawYard / 5.0) * 5.0;
    const double yardX = 80.0 + (side == 1 ? -1.0 : 1.0) * (50.0 - yard) * 1.6;
    const double offset = std::abs(point.x() - yardX);
    const bool towardCenter = std::abs(point.x() - 80.0) < std::abs(yardX - 80.0);
    QString lateral = offset < 0.01
        ? QStringLiteral("On %1 yard line").arg(compactNumber(yard))
        : QStringLiteral("%1 steps %2 %3 yard line")
              .arg(compactNumber(offset), towardCenter ? QStringLiteral("inside") : QStringLiteral("outside"),
                   compactNumber(yard));
    // Audience-facing coordinate convention: Side 1 is screen-left, Side 2 is
    // screen-right; the front sideline/hash are the audience-side landmarks.
    // Moving toward smaller y is "in front of" a landmark, moving toward larger
    // y is "behind" it. Therefore "in front of back hash" means toward the
    // audience from the far hash, while "behind front hash" means away from it.
    struct Landmark { QString name; double value; };
    const auto geometry = MarchCraft::fieldGeometry(m_fieldPreset);
    const QVector<Landmark> landmarks = {{QStringLiteral("front sideline"), 0.0},
                                         {QStringLiteral("front hash"), geometry.frontHash},
                                         {QStringLiteral("back hash"), geometry.backHash},
                                         {QStringLiteral("back sideline"), geometry.depth}};
    auto closest = landmarks.first();
    for (const auto &candidate : landmarks)
        if (std::abs(point.y() - candidate.value) < std::abs(point.y() - closest.value)) closest = candidate;
    const double verticalOffset = std::abs(point.y() - closest.value);
    const QString vertical = verticalOffset < 0.01
        ? QStringLiteral("On %1").arg(closest.name)
        : QStringLiteral("%1 steps %2 %3")
              .arg(compactNumber(verticalOffset),
                   point.y() < closest.value ? QStringLiteral("in front of") : QStringLiteral("behind"),
                   closest.name);
    return QStringLiteral("Side %1: %2 · %3").arg(side).arg(lateral, vertical);
}

void DrillProject::undo() { m_undo.undo(); }
void DrillProject::redo() { m_undo.redo(); }

QJsonObject DrillProject::toJson() const
{
    QJsonArray performers;
    for (const auto &person : m_performers) performers.push_back(person.toJson());
    QJsonArray sets;
    for (const auto &set : m_sets) sets.push_back(set.toJson());
    QJsonArray archivedSets;
    for (const auto &set : m_archivedSets) archivedSets.push_back(set.toJson());
    QJsonArray meters;
    for (const auto &region : m_meterRegions) meters.push_back(region.toJson());
    QJsonArray tempos;
    for (const auto &region : m_tempoRegions) tempos.push_back(region.toJson());
    QJsonArray props;
    for (const auto &prop : m_props) props.push_back(prop.toJson());
    return {{QStringLiteral("format"), QStringLiteral("marchcraft")},
            {QStringLiteral("version"), 9},
            {QStringLiteral("showName"), m_showName},
            {QStringLiteral("fieldPreset"), m_fieldPreset},
            {QStringLiteral("audioSource"), m_audioSource},
            {QStringLiteral("audioOffsetMs"), m_audioOffsetMs},
            {QStringLiteral("music"), m_music.toJson()},
            {QStringLiteral("bpm"), m_bpm},
            {QStringLiteral("currentSet"), m_currentSet},
            {QStringLiteral("selectedSetStart"), m_selectedSetStart},
            {QStringLiteral("selectedSetEnd"), m_selectedSetEnd},
            {QStringLiteral("playbackSource"), m_playbackSource},
            {QStringLiteral("midiMasterVolume"), m_midiMasterVolume},
            {QStringLiteral("loopEnabled"), m_loopEnabled},
            {QStringLiteral("openingBehavior"), m_openingBehavior},
            {QStringLiteral("openingCounts"), m_openingCounts},
            {QStringLiteral("capabilityProfile"), m_capability.toJson()},
            {QStringLiteral("performers"), performers},
            {QStringLiteral("sets"), sets},
            {QStringLiteral("archivedSets"), archivedSets},
            {QStringLiteral("meterRegions"), meters},
            {QStringLiteral("tempoRegions"), tempos},
            {QStringLiteral("venue"), m_venue.toJson()},
            {QStringLiteral("props"), props}};
}

bool DrillProject::restoreJson(const QJsonObject &object, bool preservePath)
{
    if (object.value(QStringLiteral("format")).toString() != QStringLiteral("marchcraft")
        || object.value(QStringLiteral("version")).toInt() > 9) {
        setStatus(QStringLiteral("Unsupported MarchCraft project format"));
        return false;
    }
    beginResetModel();
    QVector<Performer> performers;
    QVector<DrillSet> sets;
    QVector<DrillSet> archivedSets;
    QVector<MarchCraft::MeterRegion> meterRegions;
    QVector<MarchCraft::TempoRegion> tempoRegions;
    QVector<MarchCraft::PropInstance> props;
    for (const auto &value : object.value(QStringLiteral("performers")).toArray())
        performers.push_back(Performer::fromJson(value.toObject()));
    for (const auto &value : object.value(QStringLiteral("sets")).toArray())
        sets.push_back(DrillSet::fromJson(value.toObject()));
    for (const auto &value : object.value(QStringLiteral("archivedSets")).toArray())
        archivedSets.push_back(DrillSet::fromJson(value.toObject()));
    for (const auto &value : object.value(QStringLiteral("meterRegions")).toArray())
        meterRegions.push_back(MarchCraft::MeterRegion::fromJson(value.toObject()));
    for (const auto &value : object.value(QStringLiteral("tempoRegions")).toArray())
        tempoRegions.push_back(MarchCraft::TempoRegion::fromJson(value.toObject()));
    for (const auto &value : object.value(QStringLiteral("props")).toArray())
        props.push_back(MarchCraft::PropInstance::fromJson(value.toObject()));
    for (int i = 0; i < sets.size(); ++i)
        if (sets[i].number.isEmpty()) sets[i].number = QString::number(i + 1);
    if (sets.isEmpty()) {
        DrillSet first;
        first.activeVariant().name = QStringLiteral("Set 1");
        sets.push_back(first);
    }
    m_performers = std::move(performers);
    m_sets = std::move(sets);
    m_archivedSets = std::move(archivedSets);
    m_showName = object.value(QStringLiteral("showName")).toString(QStringLiteral("Untitled Show"));
    m_fieldPreset = MarchCraft::fieldGeometry(object.value(QStringLiteral("fieldPreset")).toString()).id;
    m_audioSource = object.value(QStringLiteral("audioSource")).toString();
    m_audioOffsetMs = object.value(QStringLiteral("audioOffsetMs")).toDouble();
    m_music = MarchCraft::MusicDocument::fromJson(object.value(QStringLiteral("music")).toObject());
    if (m_music.sourceType == QStringLiteral("midi") && QFileInfo::exists(m_music.sourcePath)) {
        const auto reparsed = MarchCraft::parseMidiFile(m_music.sourcePath);
        if (reparsed.ok) m_music.playbackEvents = reparsed.document.playbackEvents;
    }
    m_venue = MarchCraft::VenueConfiguration::fromJson(object.value(QStringLiteral("venue")).toObject());
    m_props = std::move(props);
    m_bpm = object.value(QStringLiteral("bpm")).toDouble(120.0);
    if (meterRegions.isEmpty()) meterRegions = {MarchCraft::MeterRegion{}};
    if (tempoRegions.isEmpty()) tempoRegions = {MarchCraft::TempoRegion{0, std::numeric_limits<qint64>::max(), m_bpm, m_bpm, QStringLiteral("Tempo")}};
    m_meterRegions = std::move(meterRegions);
    m_tempoRegions = std::move(tempoRegions);
    if (m_music.loaded()) rebuildTimingFromMusic();
    if (object.value(QStringLiteral("version")).toInt() < 3) {
        qint64 tick = 0;
        for (int i = 0; i < m_sets.size(); ++i) {
            m_sets[i].startTick = tick;
            if (i + 1 < m_sets.size()) tick = advancePulses(tick, qMax(1, m_sets[i + 1].counts));
        }
    }
    recalculateCounts();
    m_currentSet = std::clamp(object.value(QStringLiteral("currentSet")).toInt(),
                              0, static_cast<int>(m_sets.size()) - 1);
    m_selectedSetStart = qBound(0, object.value(QStringLiteral("selectedSetStart")).toInt(m_currentSet), m_sets.size()-1);
    m_selectedSetEnd = qBound(0, object.value(QStringLiteral("selectedSetEnd")).toInt(m_selectedSetStart), m_sets.size()-1);
    m_playbackSource = object.value(QStringLiteral("playbackSource")).toString(
        m_audioSource.isEmpty() ? QStringLiteral("midi") : QStringLiteral("rehearsal"));
    m_midiMasterVolume = qBound(0.0, object.value(QStringLiteral("midiMasterVolume")).toDouble(0.75), 1.0);
    m_loopEnabled = object.value(QStringLiteral("loopEnabled")).toBool();
    if (object.value(QStringLiteral("version")).toInt() >= 9) {
        m_openingBehavior = object.value(QStringLiteral("openingBehavior")).toString(QStringLiteral("hold")) == QStringLiteral("hold")
            ? QStringLiteral("hold") : QStringLiteral("move");
        m_openingCounts = qBound(1, object.value(QStringLiteral("openingCounts")).toInt(8), 256);
    } else {
        m_openingBehavior = QStringLiteral("move"); m_openingCounts = 8;
    }
    m_capability = MarchCraft::CapabilityProfile::fromJson(object.value(QStringLiteral("capabilityProfile")).toObject());
    m_playhead = 0.0;
    m_playbackActive = false;
    m_analyticsValid = false;
    ensurePlacements();
    endResetModel();
    emit performerCountChanged();
    if (!preservePath) m_projectPath.clear();
    markDirty();
    emit projectChanged(); emit setsChanged(); emit currentSetChanged(); emit selectionChanged();
    emit sceneChanged(); emit propsChanged();
    emit playbackActiveChanged();
    emit timingChanged();
    emit musicChanged();
    emit setRangeChanged(); emit transportSettingsChanged();
    if (!m_backgroundWorkerClone) { invalidateClinic(); analyzeTransition(m_currentSet); startWaveformDecode(); }
    return true;
}

void DrillProject::commitSnapshot(const QJsonObject &before, const QString &text)
{
    const auto after = toJson();
    if (QJsonDocument(before).toJson(QJsonDocument::Compact)
        == QJsonDocument(after).toJson(QJsonDocument::Compact))
        return;
    m_analyticsValid = false;
    m_undo.push(new ProjectStateCommand(this, before, after, text));
    markDirty(text);
    if (!m_backgroundWorkerClone) { invalidateClinic(); analyzeTransition(m_currentSet); }
    emit statisticsChanged();
}

void DrillProject::markDirty(const QString &message)
{
    if (!m_dirty) {
        m_dirty = true;
        emit dirtyChanged();
    }
    if (!message.isEmpty()) setStatus(message);
    m_autosaveTimer.start();
}

void DrillProject::setStatus(const QString &message)
{
    if (message == m_statusMessage) return;
    m_statusMessage = message;
    emit statusMessageChanged();
}

void DrillProject::autosave()
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(root);
    QString error;
    if (writeSqliteProject(root + QStringLiteral("/recovery.marchcraft"), toJson(), &error)) {
        setStatus(QStringLiteral("Recovery copy updated"));
    }
}

void DrillProject::emitAllDataChanged()
{
    ++m_animationStateRevision;
    if (m_animationStateRevision == 0) {
        m_animationStateRevision = 1;
        m_animationStateCacheRevisions.fill(0);
    }
    if (!m_performers.isEmpty())
        emit dataChanged(index(0), index(m_performers.size() - 1));
    emit statisticsChanged();
}

QString DrillProject::localPath(const QString &urlOrPath) const
{
    const QUrl url(urlOrPath);
    return url.isLocalFile() ? url.toLocalFile() : urlOrPath;
}

Placement DrillProject::placementAt(int performerIndex, int setIndex) const
{
    if (performerIndex < 0 || performerIndex >= m_performers.size() || m_sets.isEmpty()) return {};
    setIndex = std::clamp(setIndex, 0, static_cast<int>(m_sets.size()) - 1);
    return m_sets[setIndex].activeVariant().placements.value(m_performers[performerIndex].id);
}

QPointF DrillProject::interpolatedPosition(int performerIndex) const
{
    if (m_currentSet <= 0 || m_playhead >= 1.0) return placementAt(performerIndex, m_currentSet).position;
    return pathPosition(performerIndex, m_currentSet, m_playhead);
}

double DrillProject::interpolatedFacing(int performerIndex) const
{
    const double destination = placementAt(performerIndex, m_currentSet).facing;
    if (m_currentSet <= 0 || m_playhead >= 1.0) return destination;
    const double start = placementAt(performerIndex, m_currentSet - 1).facing;
    // Turn along the shortest arc so a 350-to-10 degree change passes through
    // front field instead of spinning almost a full revolution.
    const double delta = std::fmod(destination - start + 540.0, 360.0) - 180.0;
    double result = std::fmod(start + delta * m_playhead, 360.0);
    if (result < 0.0) result += 360.0;
    return result;
}

AnimationState DrillProject::animationStateAt(int performerIndex) const
{
    AnimationState state;
    const int counts = currentSetCounts();
    if (counts > 0) {
        state.normalizedTime = std::fmod(m_playhead * counts / 2.0, 1.0);
        if (state.normalizedTime < 0.0)
            state.normalizedTime += 1.0;
    }
    if (!m_playbackActive || m_currentSet <= 0 || counts <= 0)
        return state;

    const Placement destination = placementAt(performerIndex, m_currentSet);
    const Placement origin = placementAt(performerIndex, m_currentSet - 1);
    if (m_currentSet + 1 >= m_sets.size()) {
        state.closesAtDestination = true;
    } else {
        const Placement next = placementAt(performerIndex, m_currentSet + 1);
        state.closesAtDestination = std::hypot(next.position.x() - destination.position.x(),
                                                next.position.y() - destination.position.y()) <= 1e-5;
    }
    const double facingDelta = std::abs(std::fmod(destination.facing - origin.facing + 540.0, 360.0) - 180.0);

    constexpr double speedSampleRadius = 0.001;
    const double beforeProgress = std::max(0.0, m_playhead - speedSampleRadius);
    const double afterProgress = std::min(1.0, m_playhead + speedSampleRadius);
    const QPointF before = pathPosition(performerIndex, m_currentSet, beforeProgress);
    const QPointF current = pathPosition(performerIndex, m_currentSet, m_playhead);
    const QPointF after = pathPosition(performerIndex, m_currentSet, afterProgress);
    const QPointF speedDelta = after - before;
    const double sampleProgress = afterProgress - beforeProgress;
    // Sum the one-sided distances so a sharp polyline corner does not
    // momentarily shorten the stride to the diagonal chord length.
    const double sampleDistance = std::hypot(current.x() - before.x(), current.y() - before.y())
                                + std::hypot(after.x() - current.x(), after.y() - current.y());

    // A counted hold remains at attention. A facing-only transition receives a
    // planted direction-change pose, while the delayed portion of a move is idle.
    if (sampleProgress <= 0.0 || sampleDistance <= 1e-7) {
        if (facingDelta > 0.5 && std::hypot(destination.position.x() - origin.position.x(),
                                            destination.position.y() - origin.position.y()) <= 1e-5)
            state.locomotion = QStringLiteral("direction_change");
        return state;
    }

    state.travelStepsPerCount = sampleDistance / sampleProgress / counts;

    // Average the tangent over a small, count-relative window. This remains
    // deterministic at every playhead position while easing sharp follow,
    // gate, and pivot corners over roughly one third of a count instead of
    // snapping the legs between locomotion families in a single frame.
    const double headingSampleRadius = std::min(0.025, std::max(speedSampleRadius, 0.18 / counts));
    const double headingBeforeProgress = std::max(0.0, m_playhead - headingSampleRadius);
    const double headingAfterProgress = std::min(1.0, m_playhead + headingSampleRadius);
    const QPointF headingDelta = pathPosition(performerIndex, m_currentSet, headingAfterProgress)
                               - pathPosition(performerIndex, m_currentSet, headingBeforeProgress);
    const QPointF directionDelta = std::hypot(headingDelta.x(), headingDelta.y()) > 1e-7
            ? headingDelta : speedDelta;
    state.travelDirectionDegrees = std::fmod(std::atan2(directionDelta.x(), directionDelta.y())
                                             * 180.0 / std::numbers::pi + 360.0, 360.0);
    // Follow-the-leader is a path-facing technique: the entire body follows
    // the tangent instead of keeping an authored front while the lower body
    // slides underneath it.
    if (destination.pathType == QStringLiteral("follow")) {
        state.locomotion = QStringLiteral("march.forward");
        return state;
    }

    const double facing = interpolatedFacing(performerIndex);
    const double relative = std::fmod(state.travelDirectionDegrees - facing + 540.0, 360.0) - 180.0;
    const double absoluteRelative = std::abs(relative);
    // Slides are reserved for travel that is genuinely perpendicular to the
    // authored facing. Rear diagonals already have a clear backward component
    // and must use forefoot technique instead of inheriting a forward slide.
    constexpr double slideHalfWidthDegrees = 7.5;
    if (absoluteRelative < 90.0 - slideHalfWidthDegrees)
        state.locomotion = QStringLiteral("march.forward");
    else if (absoluteRelative > 90.0 + slideHalfWidthDegrees)
        state.locomotion = QStringLiteral("march.backward");
    else if (relative > 0.0)
        state.locomotion = QStringLiteral("slide.right");
    else
        state.locomotion = QStringLiteral("slide.left");
    return state;
}

const AnimationState &DrillProject::cachedAnimationStateAt(int performerIndex) const
{
    if (m_animationStateCache.size() != m_performers.size()) {
        m_animationStateCache.resize(m_performers.size());
        m_animationStateCacheRevisions.fill(0, m_performers.size());
    }
    if (m_animationStateCacheRevisions.at(performerIndex) != m_animationStateRevision) {
        m_animationStateCache[performerIndex] = animationStateAt(performerIndex);
        m_animationStateCacheRevisions[performerIndex] = m_animationStateRevision;
    }
    return m_animationStateCache.at(performerIndex);
}

QPointF DrillProject::pathPosition(int performerIndex, int destinationSet, double progress) const
{
    progress = std::clamp(progress, 0.0, 1.0);
    const QPointF from = placementAt(performerIndex, destinationSet - 1).position;
    const Placement destination = placementAt(performerIndex, destinationSet);
    const QPointF to = destination.position;
    if (destination.pathType == QStringLiteral("delayed"))
        progress = progress < 0.25 ? 0.0 : (progress - 0.25) / 0.75;
    if (destination.pathType == QStringLiteral("curved") && !destination.pathPoints.isEmpty()) {
        const QPointF control = destination.pathPoints.first();
        QVector<QPointF> curve; QVector<double> cumulative{0.0};
        curve.reserve(33); double total = 0.0;
        for (int sample = 0; sample <= 32; ++sample) {
            const double t = sample / 32.0, u = 1.0 - t;
            const QPointF point = from * (u * u) + control * (2.0 * u * t) + to * (t * t);
            if (!curve.isEmpty()) total += std::hypot(point.x() - curve.last().x(), point.y() - curve.last().y());
            curve.push_back(point); if (sample) cumulative.push_back(total);
        }
        const double target = progress * total; int segment = 1;
        while (segment < cumulative.size() - 1 && cumulative[segment] < target) ++segment;
        const double length = cumulative[segment] - cumulative[segment - 1];
        const double t = qFuzzyIsNull(length) ? 0.0 : (target - cumulative[segment - 1]) / length;
        return curve[segment - 1] + (curve[segment] - curve[segment - 1]) * t;
    }
    if ((destination.pathType == QStringLiteral("follow") || destination.pathType == QStringLiteral("gate")
         || destination.pathType == QStringLiteral("pivot")) && !destination.pathPoints.isEmpty()) {
        QVector<QPointF> points{from}; points += destination.pathPoints; points.push_back(to);
        QVector<double> lengths; double total = 0.0;
        for (int i = 1; i < points.size(); ++i) {
            total += std::hypot(points[i].x() - points[i - 1].x(), points[i].y() - points[i - 1].y());
            lengths.push_back(total);
        }
        const double target = progress * total;
        for (int i = 0; i < lengths.size(); ++i) if (target <= lengths[i]) {
            const double previous = i == 0 ? 0.0 : lengths[i - 1];
            const double t = qFuzzyIsNull(lengths[i] - previous) ? 0.0 : (target - previous) / (lengths[i] - previous);
            return points[i] + (points[i + 1] - points[i]) * t;
        }
    }
    return from + (to - from) * progress;
}

double DrillProject::pathDistance(int performerIndex, int destinationSet) const
{
    if (destinationSet <= 0 || destinationSet >= m_sets.size()) return 0.0;
    const QPointF from = placementAt(performerIndex, destinationSet - 1).position;
    const Placement destination = placementAt(performerIndex, destinationSet);
    if (destination.pathType == QStringLiteral("direct") || destination.pathType == QStringLiteral("delayed"))
        return std::hypot(destination.position.x() - from.x(), destination.position.y() - from.y());
    if ((destination.pathType == QStringLiteral("follow") || destination.pathType == QStringLiteral("gate")
         || destination.pathType == QStringLiteral("pivot")) && !destination.pathPoints.isEmpty()) {
        double distance = 0.0; QPointF previous = from;
        for (const auto &point : destination.pathPoints) {
            distance += std::hypot(point.x() - previous.x(), point.y() - previous.y()); previous = point;
        }
        return distance + std::hypot(destination.position.x() - previous.x(), destination.position.y() - previous.y());
    }
    double distance = 0.0; QPointF previous = pathPosition(performerIndex, destinationSet, 0.0);
    for (int sample = 1; sample <= 32; ++sample) {
        const QPointF point = pathPosition(performerIndex, destinationSet, sample / 32.0);
        distance += std::hypot(point.x() - previous.x(), point.y() - previous.y()); previous = point;
    }
    return distance;
}

double DrillProject::transitionDistance(int performerIndex, int destinationSet) const
{
    if (destinationSet <= 0 || destinationSet >= m_sets.size()) return 0.0;
    return pathDistance(performerIndex, destinationSet);
}

double DrillProject::performerTotalDistance(int performerIndex) const
{
    ensureAnalyticsCache();
    return performerIndex >= 0 && performerIndex < m_cachedTotalDistances.size()
        ? m_cachedTotalDistances[performerIndex] : 0.0;
}

bool DrillProject::performerHasWarning(int performerIndex) const
{
    ensureAnalyticsCache();
    return performerIndex >= 0 && performerIndex < m_cachedWarnings.size()
        ? m_cachedWarnings[performerIndex] : false;
}

void DrillProject::ensureAnalyticsCache() const
{
    if (m_analyticsValid) return;
    const int count = m_performers.size();
    m_cachedTotalDistances.fill(0.0, count);
    m_cachedSetDistances.fill(0.0, count);
    m_cachedWarnings.fill(false, count);
    m_cachedEnsembleTotal = 0.0; m_cachedLongestMove = 0.0; m_cachedWarningCount = 0;
    const double plannedSteps = m_currentSet >= 0 && m_currentSet < m_sets.size()
        ? m_sets[m_currentSet].counts * m_sets[m_currentSet].stepMultiplier : 0.0;
    for (int performer = 0; performer < count; ++performer) {
        for (int set = 1; set < m_sets.size(); ++set) {
            const double distance = transitionDistance(performer, set);
            m_cachedTotalDistances[performer] += distance;
            m_cachedLongestMove = qMax(m_cachedLongestMove, distance);
            if (set == m_currentSet) m_cachedSetDistances[performer] = distance;
        }
        m_cachedEnsembleTotal += m_cachedTotalDistances[performer];
        if ((plannedSteps <= 0.0 && m_cachedSetDistances[performer] > 0.01)
            || (plannedSteps > 0.0 && m_cachedSetDistances[performer] / plannedSteps > 1.25))
            m_cachedWarnings[performer] = true;
    }
    constexpr double cellSize = 1.5;
    QHash<qint64, QVector<int>> grid;
    auto keyFor = [](int x, int y) { return (qint64(x) << 32) ^ quint32(y); };
    for (int performer = 0; performer < count; ++performer) {
        const QPointF point = placementAt(performer, m_currentSet).position;
        const int cellX = qFloor(point.x() / cellSize), cellY = qFloor(point.y() / cellSize);
        for (int y = cellY - 1; y <= cellY + 1; ++y) for (int x = cellX - 1; x <= cellX + 1; ++x) {
            for (int other : grid.value(keyFor(x, y))) {
                const QPointF otherPoint = placementAt(other, m_currentSet).position;
                if (std::hypot(point.x() - otherPoint.x(), point.y() - otherPoint.y()) < cellSize)
                    m_cachedWarnings[performer] = m_cachedWarnings[other] = true;
            }
        }
        grid[keyFor(cellX, cellY)].push_back(performer);
    }
    for (bool warning : std::as_const(m_cachedWarnings)) if (warning) ++m_cachedWarningCount;
    m_analyticsValid = true;
}

QPointF DrillProject::clampPosition(QPointF point) const
{
    point.setX(std::clamp(point.x(), canvasMinX(), canvasMaxX()));
    point.setY(std::clamp(point.y(), canvasMinY(), canvasMaxY()));
    return point;
}

void DrillProject::ensurePlacements()
{
    QPointF fallback{80.0, fieldDepthSteps() / 2.0};
    auto ensureSets = [this, fallback](QVector<DrillSet> &sets) {
        for (auto &set : sets) {
            for (const auto &person : m_performers) {
                for (auto &variant : set.variants)
                    if (!variant.placements.contains(person.id))
                        variant.placements.insert(person.id, Placement{fallback, 0.0});
                for (auto &variant : set.archivedVariants)
                    if (!variant.placements.contains(person.id))
                        variant.placements.insert(person.id, Placement{fallback, 0.0});
            }
        }
    };
    ensureSets(m_sets);
    ensureSets(m_archivedSets);
}

QColor DrillProject::sectionColor(const QString &section)
{
    const uint hue = qHash(section.toLower()) % 360;
    return QColor::fromHsl(static_cast<int>(hue), 180, 135);
}
