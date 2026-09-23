#include "ExportController.h"
#include "DrillProject.h"
#include "MidiSynthEngine.h"
#include "ProjectAlgorithms.h"
#include <QBuffer>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QPainter>
#include <QPdfWriter>
#include <QPrintDialog>
#include <QPrinter>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTextLayout>
#include <QUrl>
#include <QtMath>
#include <algorithm>

namespace
{
QString pathOf(const QString &s)
{
    const QUrl u(s);
    return u.isLocalFile() ? u.toLocalFile() : s;
}
QStringList list(const QVariant &v, bool expandRanges = false)
{
    if (v.metaType().id() == QMetaType::QString)
    {
        QStringList r;
        for (const auto &s : v.toString().split(QLatin1Char(','), Qt::SkipEmptyParts))
        {
            const auto range = s.trimmed().split(QLatin1Char('-'));
            bool a = false, b = false;
            const int first = range.value(0).toInt(&a), last = range.value(1).toInt(&b);
            if (expandRanges && range.size() == 2 && a && b && last >= first && last - first < 10000)
                for (int i = first; i <= last; ++i)
                    r << QString::number(i);
            else
                r << s.trimmed();
        }
        return r;
    }
    return v.toStringList();
}
QString safeName(QString s)
{
    for (auto &c : s)
        if (QStringLiteral("<>:\"/\\|?*").contains(c) || c.unicode() < 32)
            c = QLatin1Char('_');
    return s.left(100);
}
QString csv(QString s)
{
    s.replace(QChar(34), QString(2, QChar(34)));
    return QChar(34) + s + QChar(34);
}
QStringList wrap(const QString &text, const QFont &font, double width)
{
    QStringList result;
    for (const auto &paragraph : text.split(QLatin1Char('\n')))
    {
        if (paragraph.isEmpty())
        {
            result << QString{};
            continue;
        }
        QTextLayout layout(paragraph, font);
        QTextOption option;
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        layout.setTextOption(option);
        layout.beginLayout();
        while (true)
        {
            auto line = layout.createLine();
            if (!line.isValid())
                break;
            line.setLineWidth(width);
            result << paragraph.mid(line.textStart(), line.textLength());
        }
        layout.endLayout();
    }
    return result;
}
QImage grayscale(QImage image)
{
    image = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
        {
            const auto c = image.pixel(x, y);
            const int gray = qGray(c);
            image.setPixel(x, y, qRgba(gray, gray, gray, qAlpha(c)));
        }
    return image;
}
QFont font(double size, bool bold = false)
{
    QFont f(QStringLiteral("Arial"));
    f.setPixelSize(qRound(size));
    f.setBold(bold);
    return f;
}
} // namespace

ExportOptions ExportOptions::fromMap(const QVariantMap &m)
{
    ExportOptions o;
    o.basename = safeName(m.value(QStringLiteral("basename"), QStringLiteral("chart")).toString().trimmed());
    if (o.basename.isEmpty())
        o.basename = QStringLiteral("chart");
    o.format = m.value(QStringLiteral("format"), QStringLiteral("pdf")).toString();
    o.content = m.value(QStringLiteral("content"), QStringLiteral("charts")).toString();
    o.paper = m.value(QStringLiteral("paper"), QStringLiteral("Letter")).toString();
    o.scope = m.value(QStringLiteral("scope"), QStringLiteral("all")).toString();
    o.variants = m.value(QStringLiteral("variants"), QStringLiteral("active")).toString();
    o.framing = m.value(QStringLiteral("framing"), QStringLiteral("field")).toString();
    o.audio = m.value(QStringLiteral("audio"), QStringLiteral("silent")).toString();
    o.camera = m.value(QStringLiteral("camera"), QStringLiteral("director")).toString();
    o.movements = list(m.value(QStringLiteral("movements")));
    o.sets = list(m.value(QStringLiteral("sets")), true);
    o.performers = list(m.value(QStringLiteral("performers")));
    o.sections = list(m.value(QStringLiteral("sections")));
    o.variantIds = list(m.value(QStringLiteral("variantIds")));
    auto b = [&](const QString &key, bool def = true) { return m.value(key, def).toBool(); };
    o.landscape = b(QStringLiteral("landscape"));
    o.monochrome = b(QStringLiteral("monochrome"), false);
    o.grid = b(QStringLiteral("grid"));
    o.labels = b(QStringLiteral("labels"));
    o.symbols = b(QStringLiteral("symbols"), false);
    o.props = b(QStringLiteral("props"));
    o.notes = b(QStringLiteral("notes"));
    o.headings = b(QStringLiteral("headings"));
    o.numbers = b(QStringLiteral("numbers"));
    o.companyLogo = b(QStringLiteral("companyLogo"));
    o.marchcraftLogo = b(QStringLiteral("marchcraftLogo"));
    o.subsets = b(QStringLiteral("subsets"));
    o.split = b(QStringLiteral("split"), false);
    o.margin = qBound(5.0, m.value(QStringLiteral("margin"), 10).toDouble(), 35.0);
    o.fontSize = qBound(6.0, m.value(QStringLiteral("fontSize"), 9).toDouble(), 18.0);
    o.markerSize = qBound(1.0, m.value(QStringLiteral("markerSize"), 2).toDouble(), 6.0);
    o.dpi = m.value(QStringLiteral("dpi"), 300).toInt();
    if (!QList<int>{150, 300, 600}.contains(o.dpi))
        o.dpi = 300;
    o.fps = m.value(QStringLiteral("fps"), 30).toInt() == 60 ? 60 : 30;
    o.height = m.value(QStringLiteral("height"), 1080).toInt() == 720 ? 720 : 1080;
    o.crop =
        QRectF(m.value(QStringLiteral("cropX"), 0).toDouble(), m.value(QStringLiteral("cropY"), 0).toDouble(),
               m.value(QStringLiteral("cropWidth"), 160).toDouble(),
               m.value(QStringLiteral("cropHeight"), 85.333).toDouble());
    return o;
}

ExportController::ExportController(DrillProject *project, QObject *parent)
    : QObject(parent), m_project(project), m_temp(std::make_unique<QTemporaryDir>())
{
    m_timer.setInterval(0);
    connect(&m_timer, &QTimer::timeout, this, &ExportController::tick);
    connect(&m_encoder, &QProcess::readyReadStandardError, this,
            [this]
            {
                m_encoderError += QString::fromUtf8(m_encoder.readAllStandardError());
                m_encoderError = m_encoderError.right(6000);
            });
    connect(&m_encoder, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError e)
            {
                if (m_busy && e == QProcess::FailedToStart)
                    fail(QStringLiteral("Could not start FFmpeg: ") + m_encoder.errorString());
            });
    connect(&m_encoder, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status)
            {
                if (!m_busy)
                    return;
                if (code != 0 || status != QProcess::NormalExit)
                {
                    fail(QStringLiteral("Video encoding failed: ") + m_encoderError);
                    return;
                }
                if (m_muxing || m_options.audio == QStringLiteral("silent"))
                    finish();
                else
                    beginMux();
            });
    connect(project, &DrillProject::projectChanged, this,
            [this]
            {
                if (!m_busy)
                    emit changed();
            });
}
ExportController::~ExportController()
{
    m_timer.stop();
    m_encoder.disconnect(this);
    m_painter.reset();
    m_pdf.reset();
    m_printer.reset();
    if (m_encoder.state() != QProcess::NotRunning)
    {
        m_encoder.kill();
        m_encoder.waitForFinished(3000);
    }
}
QObject *ExportController::renderProject() const
{
    return m_activeChart >= 0 ? m_clones[m_charts[m_activeChart].clone].get() : nullptr;
}
QVariantMap ExportController::branding() const { return m_project->m_exportBranding.toVariantMap(); }
void ExportController::refresh() { emit changed(); }
QString ExportController::ffmpeg() const
{
    auto p = QSettings().value(QStringLiteral("export/ffmpeg")).toString();
    return p.isEmpty() ? QStandardPaths::findExecutable(QStringLiteral("ffmpeg")) : p;
}
void ExportController::setFfmpeg(const QString &path)
{
    QSettings().setValue(QStringLiteral("export/ffmpeg"), pathOf(path));
    emit changed();
}
QString ExportController::lastDestination() const
{
    return QSettings()
        .value(QStringLiteral("export/destination"),
               QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(QStringLiteral("MarchCraft.pdf")))
        .toString();
}
QStringList ExportController::presetNames() const
{
    QSettings s;
    s.beginGroup(QStringLiteral("export/presets"));
    return s.childKeys();
}
void ExportController::savePreset(const QString &name, const QVariantMap &options)
{
    if (!name.trimmed().isEmpty())
        QSettings().setValue(QStringLiteral("export/presets/") + safeName(name), options);
    emit changed();
}
QVariantMap ExportController::loadPreset(const QString &name) const
{
    return QSettings().value(QStringLiteral("export/presets/") + safeName(name)).toMap();
}
bool ExportController::setBranding(const QString &name, const QString &logo, bool removeLogo)
{
    if (m_busy)
        return false;
    auto next = m_project->m_exportBranding;
    next.insert(QStringLiteral("company"), name.trimmed());
    if (removeLogo)
        next.remove(QStringLiteral("logo"));
    else if (!logo.isEmpty())
    {
        QImageReader reader(pathOf(logo));
        reader.setAutoTransform(true);
        const auto size = reader.size();
        if (!size.isValid() || size.width() > 16000 || size.height() > 16000 ||
            !QList<QByteArray>{QByteArrayLiteral("png"), QByteArrayLiteral("jpeg"), QByteArrayLiteral("jpg")}
                 .contains(reader.format()))
        {
            m_message = QStringLiteral("Choose a PNG or JPEG logo, at most 16000 pixels per side.");
            emit changed();
            return false;
        }
        if (size.width() > 1600 || size.height() > 1600)
            reader.setScaledSize(size.scaled(1600, 1600, Qt::KeepAspectRatio));
        auto image = reader.read();
        if (image.isNull())
        {
            m_message = reader.errorString();
            emit changed();
            return false;
        }
        QByteArray data;
        QBuffer buffer(&data);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        next.insert(QStringLiteral("logo"), QString::fromLatin1(data.toBase64()));
    }
    if (next == m_project->m_exportBranding)
        return true;
    const auto before = m_project->toJson();
    m_project->m_exportBranding = next;
    m_project->commitSnapshot(before, QStringLiteral("Edit export branding"));
    emit m_project->projectChanged();
    emit changed();
    return true;
}
QVariantList ExportController::choices() const
{
    QVariantList result;
    const auto doc = m_project->toJson();
    const auto moves = doc.value(QStringLiteral("movements")).toArray();
    for (int i = 0; i < moves.size(); ++i)
    {
        auto move = moves[i].toObject();
        auto state = i == doc.value(QStringLiteral("currentMovement")).toInt()
                         ? doc
                         : move.value(QStringLiteral("state")).toObject();
        QVariantList sets;
        for (const auto &s : state.value(QStringLiteral("sets")).toArray())
        {
            auto set = s.toObject();
            QVariantList variants;
            for (const auto &v : set.value(QStringLiteral("variants")).toArray())
                variants << v.toObject().toVariantMap();
            sets << QVariantMap{{QStringLiteral("id"), set.value(QStringLiteral("id")).toString()},
                                {QStringLiteral("number"), set.value(QStringLiteral("number")).toString()},
                                {QStringLiteral("variants"), variants}};
        }
        result << QVariantMap{{QStringLiteral("id"), move.value(QStringLiteral("id")).toString()},
                              {QStringLiteral("name"), move.value(QStringLiteral("name")).toString()},
                              {QStringLiteral("sets"), sets}};
    }
    return result;
}
QSizeF ExportController::pageSize() const
{
    QPageSize::PageSizeId id = QPageSize::Letter;
    if (m_options.paper == QStringLiteral("A4"))
        id = QPageSize::A4;
    else if (m_options.paper == QStringLiteral("A3"))
        id = QPageSize::A3;
    else if (m_options.paper == QStringLiteral("Legal"))
        id = QPageSize::Legal;
    else if (m_options.paper == QStringLiteral("Tabloid"))
        id = QPageSize::Tabloid;
    QSizeF size = QPageSize(id).size(QPageSize::Point);
    if (m_options.landscape)
        size.transpose();
    return size;
}
bool ExportController::prepare(const QVariantMap &options)
{
    if (m_busy)
        return false;
    m_options = ExportOptions::fromMap(options);
    m_activeChart = -1;
    emit renderProjectChanged();
    m_clones.clear();
    m_originalVariants.clear();
    m_charts.clear();
    m_pages.clear();
    m_pageNames.clear();
    m_files.clear();
    m_previewUrl.clear();
    m_message.clear();
    m_progress = 0;
    if (!QStringList{QStringLiteral("pdf"), QStringLiteral("png"), QStringLiteral("csv"),
                     QStringLiteral("print"), QStringLiteral("video2d"), QStringLiteral("video3d")}
             .contains(m_options.format))
    {
        fail(QStringLiteral("Unsupported export format."));
        return false;
    }
    if (m_options.framing == QStringLiteral("custom") &&
        (m_options.crop.width() < 1 || m_options.crop.height() < 1 || m_options.crop.width() > 1000 ||
         m_options.crop.height() > 1000))
    {
        fail(QStringLiteral("Crop width and height must be between 1 and 1000 steps."));
        return false;
    }
    const auto snapshot = m_project->toJson();
    const auto moves = snapshot.value(QStringLiteral("movements")).toArray();
    for (int i = 0; i < moves.size(); ++i)
    {
        const auto move = moves[i].toObject();
        if (m_options.scope == QStringLiteral("active") && i != m_project->m_currentMovement)
            continue;
        if (m_options.scope == QStringLiteral("selected") &&
            !m_options.movements.contains(move.value(QStringLiteral("id")).toString()))
            continue;
        auto state = snapshot;
        if (i != m_project->m_currentMovement)
            DrillProject::overlayMovement(state, move.value(QStringLiteral("state")).toObject());
        state.insert(QStringLiteral("movements"),
                     QJsonArray{QJsonObject{{QStringLiteral("id"), move.value(QStringLiteral("id"))},
                                            {QStringLiteral("name"), move.value(QStringLiteral("name"))}}});
        state.insert(QStringLiteral("currentMovement"), 0);
        auto clone = std::unique_ptr<DrillProject>(new DrillProject(true, nullptr));
        if (!clone->restoreJson(state, true))
        {
            fail(QStringLiteral("Could not read movement snapshot."));
            return false;
        }
        clone->m_performers.erase(std::remove_if(clone->m_performers.begin(), clone->m_performers.end(),
                                                 [&](const auto &p)
                                                 {
                                                     return (!m_options.performers.isEmpty() &&
                                                             !m_options.performers.contains(p.id) &&
                                                             !m_options.performers.contains(p.label)) ||
                                                            (!m_options.sections.isEmpty() &&
                                                             !m_options.sections.contains(p.section));
                                                 }),
                                  clone->m_performers.end());
        clone->m_exportRendering = true;
        for (auto &p : clone->m_performers)
            p.selected = false;
        if (clone->m_performers.isEmpty())
            continue;
        for (int s = 0; s < clone->m_sets.size(); ++s)
        {
            const auto &set = clone->m_sets[s];
            if (!m_options.subsets && set.subset)
                continue;
            if (!m_options.sets.isEmpty() && !m_options.sets.contains(set.id) &&
                !m_options.sets.contains(set.number))
                continue;
            for (const auto &v : set.variants)
            {
                if (m_options.variants == QStringLiteral("active") && v.id != set.activeVariantId)
                    continue;
                if (m_options.variants == QStringLiteral("selected") && !m_options.variantIds.contains(v.id))
                    continue;
                const double start = s == 0 ? 0
                                            : clone->millisecondsBetween(0, clone->m_sets[s - 1].startTick) +
                                                  clone->openingDurationMs();
                const double duration = s == 0 ? clone->openingDurationMs() : clone->transitionDurationMs(s);
                if (m_options.format.startsWith(QStringLiteral("video")) && duration <= 0)
                    continue;
                m_charts.push_back({int(m_clones.size()), s, v.id, start, duration});
            }
        }
        QStringList originalVariants;
        for (const auto &set : clone->m_sets)
            originalVariants << set.activeVariantId;
        m_originalVariants << originalVariants;
        m_clones.push_back(std::move(clone));
    }
    if (m_charts.isEmpty())
    {
        fail(QStringLiteral("No sets and performers match this selection."));
        return false;
    }
    const auto size = pageSize();
    const double margin = m_options.margin * 72 / 25.4;
    const double width = size.width() - 2 * margin;
    for (int c = 0; c < m_charts.size(); ++c)
    {
        activateChart(c);
        auto &p = *m_clones[m_charts[c].clone];
        const auto &set = p.m_sets[m_charts[c].set];
        QStringList lines = m_options.notes
                                ? wrap(set.activeVariant().caption, font(m_options.fontSize), width - 8)
                                : QStringList{};
        if (m_options.content == QStringLiteral("coordinates") && m_options.format != QStringLiteral("csv") &&
            !m_options.format.startsWith(QStringLiteral("video")))
            continue;
        const int chartLines = qMax(1, int(65 / (m_options.fontSize + 3)));
        const int continuationLines =
            qMax(1, int((size.height() - 2 * margin - 110) / (m_options.fontSize + 3)));
        m_pages.push_back({c, lines.mid(0, chartLines), false, -1, {}});
        lines = lines.mid(chartLines);
        while (!lines.isEmpty())
        {
            m_pages.push_back({c, lines.mid(0, continuationLines), true, -1, {}});
            lines = lines.mid(continuationLines);
        }
    }
    if (m_options.content == QStringLiteral("coordinates") && m_options.format != QStringLiteral("csv") &&
        !m_options.format.startsWith(QStringLiteral("video")))
    {
        const int rows = qMax(1, int((size.height() - 2 * margin - 110) / (m_options.fontSize * 3 + 10)));
        for (int clone = 0; clone < int(m_clones.size()); ++clone)
            for (int person = 0; person < m_clones[clone]->m_performers.size(); ++person)
            {
                QVector<int> charts;
                for (int c = 0; c < m_charts.size(); ++c)
                    if (m_charts[c].clone == clone)
                        charts << c;
                for (int r = 0; r < charts.size(); r += rows)
                    m_pages.push_back({charts[r], {}, false, person, charts.mid(r, rows)});
            }
    }
    for (const auto &page : m_pages)
    {
        activateChart(page.chart);
        const auto &p = *m_clones[m_charts[page.chart].clone];
        const auto &s = p.m_sets[m_charts[page.chart].set];
        QString title = p.m_movements[0].name + QStringLiteral(" / Set ") + s.number + QStringLiteral(" / ") +
                        s.activeVariant().label + QStringLiteral(" ") + s.activeVariant().name;
        if (page.continuation)
            title += QStringLiteral(" / Instructions continued");
        if (page.performer >= 0)
            title += QStringLiteral(" / ") + p.m_performers[page.performer].label;
        m_pageNames << title;
    }
    m_activeChart = -1;
    emit renderProjectChanged();
    m_message = QStringLiteral("%1 pages, %2 selected set variants").arg(m_pages.size()).arg(m_charts.size());
    if (m_options.framing == QStringLiteral("custom"))
    {
        bool clipped = false;
        for (int c = 0; c < m_charts.size(); ++c)
        {
            activateChart(c);
            auto &p = *m_clones[m_charts[c].clone];
            for (int r = 0; r < p.m_performers.size(); ++r)
                clipped |= !m_options.crop.contains(p.placementAt(r, m_charts[c].set).position);
        }
        if (clipped)
            m_message += QStringLiteral(". Warning: custom crop excludes performers.");
    }
    preview(0);
    emit changed();
    return true;
}
void ExportController::activateChart(int chart)
{
    if (chart < 0 || chart >= m_charts.size())
        return;
    const auto &c = m_charts[chart];
    auto &p = *m_clones[c.clone];
    const bool changedProject = m_activeChart < 0 || m_charts[m_activeChart].clone != c.clone;
    for (int i = 0; i < p.m_sets.size(); ++i)
        p.m_sets[i].activeVariantId = m_originalVariants[c.clone][i];
    p.m_sets[c.set].activeVariantId = c.variant;
    p.m_transitionPaths.clear();
    p.m_currentSet = c.set;
    p.m_playbackSet = -1;
    p.m_playbackActive = false;
    p.m_playhead = 1;
    ++p.m_animationStateRevision;
    p.emitAllDataChanged();
    emit p.currentSetChanged();
    m_activeChart = chart;
    if (changedProject)
        emit renderProjectChanged();
}
void ExportController::drawHeader(QPainter &p, const QRectF &r, DrillProject &project)
{
    double x = r.left();
    const auto b = project.m_exportBranding;
    auto logo = [&](QImage image)
    {
        if (image.isNull())
            return;
        if (m_options.monochrome)
            image = grayscale(image);
        const QSizeF sz = QSizeF(image.size()).scaled(80, 38, Qt::KeepAspectRatio);
        p.drawImage(QRectF(x, r.top(), sz.width(), sz.height()), image);
        x += sz.width() + 8;
    };
    if (m_options.companyLogo)
        logo(QImage::fromData(QByteArray::fromBase64(b.value(QStringLiteral("logo")).toString().toLatin1())));
    if (m_options.marchcraftLogo)
        logo(grayscale(QImage(QStringLiteral(":/branding/marchcraft-logo.png"))));
    p.setPen(Qt::black);
    p.setFont(font(13, true));
    if (m_options.headings)
        p.drawText(QRectF(x + 5, r.top(), r.right() - x - 5, 32), Qt::AlignCenter | Qt::TextWordWrap,
                   project.showName());
    p.setFont(font(8));
    if (m_options.headings)
        p.drawText(QRectF(r.left(), r.top() + 40, r.width(), 14), Qt::AlignLeft,
                   b.value(QStringLiteral("company")).toString());
}
void ExportController::drawChart(QPainter &p, const QRectF &area, DrillProject &project, bool animated)
{
    const auto geometry = MarchCraft::fieldGeometry(project.fieldPreset());
    QRectF region(project.canvasMinX(), project.canvasMinY(), project.canvasMaxX() - project.canvasMinX(),
                  project.canvasMaxY() - project.canvasMinY());
    QVector<QPointF> points;
    for (int i = 0; i < project.m_performers.size(); ++i)
        points << (animated ? project.interpolatedPosition(i)
                            : project.placementAt(i, project.currentSetIndex()).position);
    if (m_options.framing == QStringLiteral("custom"))
        region = m_options.crop;
    else
    {
        QRectF bounds;
        bool first = true;
        for (const auto &pt : points)
        {
            QRectF b(pt - QPointF(3, 3), QSizeF(6, 6));
            bounds = first ? b : bounds.united(b);
            first = false;
        }
        if (m_options.props)
            for (const auto &value : project.props())
            {
                const auto prop = value.toMap();
                const QPointF center(prop.value(QStringLiteral("fieldX")).toDouble(),
                                     prop.value(QStringLiteral("fieldY")).toDouble());
                const double radius = std::hypot(prop.value(QStringLiteral("widthSteps")).toDouble(),
                                                 prop.value(QStringLiteral("depthSteps")).toDouble()) /
                                          2 +
                                      1;
                const QRectF box(center - QPointF(radius, radius), QSizeF(radius * 2, radius * 2));
                bounds = first ? box : bounds.united(box);
                first = false;
            }
        if (m_options.framing == QStringLiteral("fit") && !first)
            region = bounds;
        else if (!first)
            region = region.united(bounds);
    }
    const double scale = qMin(area.width() / region.width(), area.height() / region.height());
    const QRectF fitted(area.center().x() - region.width() * scale / 2,
                        area.center().y() - region.height() * scale / 2, region.width() * scale,
                        region.height() * scale);
    auto point = [&](double x, double y)
    {
        return QPointF(fitted.left() + (x - region.left()) * scale,
                       fitted.bottom() - (y - region.top()) * scale);
    };
    p.save();
    p.setClipRect(fitted);
    p.fillRect(area, Qt::white);
    if (m_options.grid)
        for (int axis = 0; axis < 2; ++axis)
        {
            const double lo = axis ? region.top() : region.left(),
                         hi = axis ? region.bottom() : region.right();
            for (int v = int(std::ceil(lo)); v <= hi; ++v)
            {
                const bool mid = v % int(MarchCraft::FieldTransform::GridMidlineSteps) == 0;
                p.setPen(QPen(QColor(mid ? QStringLiteral("#929292") : QStringLiteral("#d2d2d2")),
                              mid ? 0.55 : 0.22));
                if (axis)
                    p.drawLine(point(region.left(), v), point(region.right(), v));
                else
                    p.drawLine(point(v, region.top()), point(v, region.bottom()));
            }
        }
    p.setPen(QPen(Qt::black, 0.8));
    for (double x = 0; x <= 160; x += MarchCraft::FieldTransform::YardLineSteps)
        p.drawLine(point(x, 0), point(x, geometry.depth));
    p.drawLine(point(0, 0), point(160, 0));
    p.drawLine(point(0, geometry.depth), point(160, geometry.depth));
    for (double x = 0; x <= 160; x += 8)
        for (double y : {geometry.frontHash, geometry.backHash})
        {
            p.setPen(QPen(Qt::black, 2));
            p.drawLine(point(x - 0.55, y), point(x + 0.55, y));
        }
    p.setFont(font(qMax(8.0, scale * 2.1), true));
    p.setPen(QColor(QStringLiteral("#b8b8b8")));
    for (int x = 16; x < 160; x += 16)
        for (double y : {12.8, geometry.depth - 12.8})
        {
            auto pos = point(x, y);
            p.drawText(QRectF(pos.x() - 18, pos.y() - 10, 36, 20), Qt::AlignCenter,
                       QString::number(qRound(50 - std::abs(80 - x) * 5.0 / 8)));
        }
    if (m_options.props)
        for (const auto &value : project.props())
        {
            auto prop = value.toMap();
            const auto pos =
                point(prop[QStringLiteral("fieldX")].toDouble(), prop[QStringLiteral("fieldY")].toDouble());
            p.save();
            p.translate(pos);
            p.rotate(-prop[QStringLiteral("rotation")].toDouble());
            p.setPen(QPen(QColor(QStringLiteral("#777777")), 0.8));
            p.setBrush(Qt::NoBrush);
            const double w = prop[QStringLiteral("widthSteps")].toDouble() * scale,
                         h = prop[QStringLiteral("depthSteps")].toDouble() * scale;
            p.drawRect(QRectF(-w / 2, -h / 2, w, h));
            p.restore();
        }
    p.setFont(font(m_options.fontSize));
    QVector<QRectF> occupied;
    for (const auto &pt : points)
    {
        const auto pos = point(pt.x(), pt.y());
        occupied << QRectF(pos - QPointF(2, 2), QSizeF(4, 4));
    }
    for (int i = 0; i < points.size(); ++i)
    {
        const auto &person = project.m_performers[i];
        const auto pos = point(points[i].x(), points[i].y());
        const QColor color = m_options.monochrome ? QColor(Qt::black) : person.color;
        p.setPen(QPen(color, 0.5));
        p.setBrush(color);
        if (m_options.symbols && !person.symbol.isEmpty())
            p.drawText(QRectF(pos.x() - 4, pos.y() - 5, 8, 10), Qt::AlignCenter, person.symbol);
        else
            p.drawEllipse(pos, m_options.markerSize, m_options.markerSize);
        if (!m_options.labels)
            continue;
        const QSizeF labelSize(p.fontMetrics().horizontalAdvance(person.label) + 2, m_options.fontSize + 2);
        QRectF label(pos + QPointF(3, -labelSize.height() / 2), labelSize);
        bool placed = false;
        for (int ring = 0; ring < 10 && !placed; ++ring)
            for (int direction = 0; direction < 8; ++direction)
            {
                const double angle = direction * 3.141592653589793 / 4;
                const double distance = 4 + ring * 5;
                const QPointF offset(std::cos(angle) * distance, std::sin(angle) * distance);
                QRectF candidate(pos + offset +
                                     QPointF(offset.x() < 0 ? -labelSize.width() : 0,
                                             offset.y() < 0 ? -labelSize.height() : 0),
                                 labelSize);
                if (!fitted.contains(candidate))
                    continue;
                bool collision = false;
                for (const auto &other : occupied)
                    if (candidate.intersects(other))
                    {
                        collision = true;
                        break;
                    }
                if (!collision)
                {
                    label = candidate;
                    placed = true;
                    break;
                }
            }
        occupied << label;
        if (QLineF(pos, label.center()).length() > 12)
        {
            p.setPen(QPen(QColor(QStringLiteral("#999999")), 0.3));
            p.drawLine(pos, label.center());
        }
        p.setPen(Qt::black);
        p.drawText(label, Qt::AlignCenter, person.label);
    }
    p.restore();
}
void ExportController::drawPage(QPainter &p, const QRectF &target, int index, bool animated)
{
    const auto &page = m_pages[index];
    if (!animated)
        activateChart(page.chart);
    auto &project = *m_clones[m_charts[page.chart].clone];
    const auto &set = project.m_sets[m_charts[page.chart].set];
    const QSizeF size = pageSize();
    p.save();
    const double pageScale = qMin(target.width() / size.width(), target.height() / size.height());
    p.translate(target.center() - QPointF(size.width() * pageScale / 2, size.height() * pageScale / 2));
    p.scale(pageScale, pageScale);
    p.fillRect(QRectF(QPointF(), size), Qt::white);
    const double margin = m_options.margin * 72 / 25.4;
    const QRectF body(margin, margin, size.width() - 2 * margin, size.height() - 2 * margin);
    drawHeader(p, body, project);
    p.setPen(Qt::black);
    p.setFont(font(m_options.fontSize, true));
    if (page.performer >= 0)
    {
        const auto &person = project.m_performers[page.performer];
        p.drawText(QRectF(body.left(), body.top() + 58, body.width(), 25), Qt::AlignLeft,
                   person.label + QStringLiteral("  ") + person.name + QStringLiteral("  ") +
                       person.instrument + QStringLiteral(" / ") + project.m_movements[0].name);
        double y = body.top() + 90;
        const double rowHeight = m_options.fontSize * 3 + 10;
        for (int c : page.rows)
        {
            activateChart(c);
            auto &pr = *m_clones[m_charts[c].clone];
            const auto &s = pr.m_sets[m_charts[c].set];
            p.setFont(font(m_options.fontSize, true));
            p.drawText(QRectF(body.left(), y, body.width(), rowHeight / 2), Qt::AlignLeft,
                       QStringLiteral("Set %1 %2 | %3 | Measures %4 | %5 counts")
                           .arg(s.number, s.activeVariant().label, s.activeVariant().name, s.measure)
                           .arg(s.counts));
            p.setFont(font(m_options.fontSize));
            p.drawText(QRectF(body.left(), y + rowHeight / 2, body.width(), rowHeight / 2), Qt::AlignLeft,
                       pr.coordinateFor(page.performer, m_charts[c].set) +
                           QStringLiteral(" | Move %1 steps")
                               .arg(pr.transitionDistance(page.performer, m_charts[c].set), 0, 'f', 1));
            y += rowHeight;
            p.setPen(QColor(QStringLiteral("#cccccc")));
            p.drawLine(QPointF(body.left(), y - 2), QPointF(body.right(), y - 2));
            p.setPen(Qt::black);
        }
    }
    else
    {
        const double top = body.top() + 60;
        const double chartHeight = body.height() - 190;
        if (!page.continuation)
            drawChart(p, QRectF(body.left(), top, body.width(), chartHeight), project, animated);
        double y = page.continuation ? top : top + chartHeight + 4;
        if (!page.continuation)
        {
            p.setFont(font(7));
            p.drawText(QRectF(body.left(), y, body.width(), 12), Qt::AlignCenter,
                       QStringLiteral("Director Viewpoint"));
            y += 16;
        }
        p.setFont(font(m_options.fontSize + 1, true));
        p.drawText(QRectF(body.left(), y, body.width(), 30), Qt::TextWordWrap,
                   QStringLiteral("SET %1 %2  COUNTS: %3  MEASURES: %4  %5%6")
                       .arg(set.number, set.activeVariant().label)
                       .arg(project.setInfo(m_charts[page.chart].set)[QStringLiteral("counts")].toInt())
                       .arg(set.measure,
                            project.m_movements[0].name + QStringLiteral(" / ") + set.activeVariant().name,
                            page.continuation ? QStringLiteral(" - Instructions continued") : QString{}));
        y += 32;
        p.setFont(font(m_options.fontSize));
        for (const auto &line : page.notes)
        {
            p.drawText(QPointF(body.left() + 3, y + m_options.fontSize), line);
            y += m_options.fontSize + 3;
        }
    }
    if (m_options.numbers)
    {
        p.setFont(font(7));
        p.drawText(QRectF(body.left(), body.bottom() - 10, body.width(), 12), Qt::AlignRight,
                   QStringLiteral("Page %1 of %2").arg(index + 1).arg(m_pages.size()));
    }
    p.restore();
}
void ExportController::preview(int page)
{
    if (m_busy || page < 0 || page >= m_pages.size())
        return;
    const auto size = pageSize();
    QImage image((size * 1.6).toSize(), QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    drawPage(painter, QRectF(QPointF(), image.size()), page);
    painter.end();
    const QString path = m_temp->filePath(
        QStringLiteral("preview-%1-%2.png").arg(page).arg(QDateTime::currentMSecsSinceEpoch()));
    image.save(path);
    m_previewUrl = QUrl::fromLocalFile(path).toString();
    emit changed();
}
QStringList ExportController::plannedFiles(const QString &destination) const
{
    QString path = QFileInfo(pathOf(destination)).absoluteFilePath();
    QStringList files;
    if (m_options.format == QStringLiteral("print"))
        return {QStringLiteral("Selected printer")};
    if (m_options.format == QStringLiteral("png"))
        for (int i = 0; i < m_pages.size(); ++i)
            files << QDir(path).filePath(m_options.basename +
                                         QStringLiteral("-%1.png").arg(i + 1, 4, 10, QLatin1Char('0')));
    else if (m_options.split && m_options.format == QStringLiteral("pdf"))
    {
        for (int i = 0; i < int(m_clones.size()); ++i)
            if (std::any_of(m_charts.cbegin(), m_charts.cend(), [&](const auto &c) { return c.clone == i; }))
                files << QDir(path).filePath(QStringLiteral("%1-%2.pdf")
                                                 .arg(i + 1, 2, 10, QLatin1Char('0'))
                                                 .arg(safeName(m_clones[i]->m_movements[0].name)));
    }
    else
    {
        const QString ext =
            m_options.format.startsWith(QStringLiteral("video")) ? QStringLiteral("mp4") : m_options.format;
        if (!path.endsWith(QLatin1Char('.') + ext, Qt::CaseInsensitive))
            path += QLatin1Char('.') + ext;
        files << path;
    }
    return files;
}
bool ExportController::start(const QString &destination, bool overwrite)
{
    if (m_busy || m_pages.isEmpty())
        return false;
    m_files = plannedFiles(destination);
    m_staged.clear();
    m_encoderError.clear();
    if (m_options.format != QStringLiteral("print"))
        for (const auto &file : m_files)
        {
            if (QFileInfo::exists(file) && !overwrite)
            {
                m_message =
                    QStringLiteral("Output exists. Enable Replace existing files to continue: ") + file;
                emit changed();
                return false;
            }
            if (!QFileInfo(QFileInfo(file).absolutePath()).isDir())
            {
                m_message = QStringLiteral("Choose an existing destination folder.");
                emit changed();
                return false;
            }
        }
    if (m_options.format.startsWith(QStringLiteral("video")) && !QFileInfo::exists(ffmpeg()))
    {
        m_message = QStringLiteral("Choose an installed FFmpeg executable in Video settings.");
        emit changed();
        return false;
    }
    if (m_options.format == QStringLiteral("print"))
    {
        m_printer = std::make_unique<QPrinter>(QPrinter::HighResolution);
        m_printer->setPageSize(QPageSize(pageSize(), QPageSize::Point));
        m_printer->setFullPage(true);
        QPrintDialog dialog(m_printer.get());
        dialog.setMinMax(1,m_pages.size());
        dialog.setOption(QAbstractPrintDialog::PrintPageRange,true);
        if (dialog.exec() != QDialog::Accepted)
        {
            m_printer.reset();
            return false;
        }
        m_painter = std::make_unique<QPainter>(m_printer.get());
        if (!m_painter->isActive())
        {
            fail(QStringLiteral("Printer could not start."));
            return false;
        }
    }
    for (int i = 0; i < m_files.size(); ++i)
        m_staged << m_temp->filePath(
            QStringLiteral("output-%1.%2").arg(i).arg(QFileInfo(m_files[i]).suffix()));
    m_busy = true;
    m_cancel = false;
    m_progress = 0;
    m_nextPage = m_printer ? qMax(0,m_printer->fromPage()-1) : 0;
    m_frame = 0;
    m_audioChart = -1;
    m_muxing = false;
    m_message = QStringLiteral("Exporting...");
    if(m_options.format!=QStringLiteral("print"))QSettings().setValue(QStringLiteral("export/destination"),QFileInfo(pathOf(destination)).absoluteFilePath());
    emit changed();
    if (m_options.format.startsWith(QStringLiteral("video")))
        beginVideo();
    else
        m_timer.start();
    return m_busy;
}
void ExportController::tick()
{
    if (!m_busy)
        return;
    if (m_cancel)
    {
        fail(QStringLiteral("Export canceled. Existing files were preserved."));
        return;
    }
    if (m_options.format.startsWith(QStringLiteral("video")))
    {
        if (m_waitingFrame || m_encoder.state() != QProcess::Running ||
            m_encoder.bytesToWrite() > videoWidth() * videoHeight() * 4 * 2)
            return;
        if (m_frame >= m_totalFrames)
        {
            m_timer.stop();
            m_encoder.closeWriteChannel();
            return;
        }
        int c = 0;
        while (c < m_frameEnds.size() - 1 && m_frame >= m_frameEnds[c])
            ++c;
        if (m_activeChart != c)
            activateChart(c);
        auto &project = *m_clones[m_charts[c].clone];
        const int first = c == 0 ? 0 : m_frameEnds[c - 1];
        const double elapsed = (m_frame - first) * 1000.0 / m_options.fps;
        const auto &chart = m_charts[c];
        const double startTick = project.m_sets[qMax(0, chart.set - 1)].startTick;
        const double endTick = project.m_sets[chart.set].startTick;
        double lo = startTick, hi = endTick;
        for (int iteration = 0; iteration < 40; ++iteration)
        {
            const double mid = (lo + hi) / 2;
            if (project.millisecondsBetween(qint64(startTick), qint64(mid)) < elapsed)
                lo = mid;
            else
                hi = mid;
        }
        const double phase =
            endTick > startTick ? qBound(0.0, ((lo + hi) / 2 - startTick) / (endTick - startTick), 1.0) : 1.0;
        project.setPlaybackFrame(chart.set, phase);
        emit project.propsChanged();
        if (m_options.audio == QStringLiteral("midi"))
        {
            if (m_audioChart != c)
            {
                m_synth = std::make_unique<MidiSynthWorker>(nullptr, true);
                if (!m_synth->load(project.m_music))
                {
                    fail(QStringLiteral("MIDI synthesis unavailable: ") + m_synth->status());
                    return;
                }
                m_synth->setGain(project.m_midiMasterVolume);
                m_prerollFrames =
                    chart.set == 0
                        ? 0
                        : qRound64(project.m_music.millisecondsAt(project.m_sets[chart.set - 1].startTick) *
                                   48);
                m_audioChart = c;
            }
            if (m_prerollFrames > 0)
            {
                QByteArray scratch(8192 * 8, 0);
                int n = int(qMin<qint64>(m_prerollFrames, 8192));
                m_synth->renderOffline(scratch.data(), n * 8);
                m_prerollFrames -= n;
                return;
            }
            QByteArray pcm(48000 / m_options.fps * 8, 0);
            if (chart.set > 0)
                m_synth->renderOffline(pcm.data(), pcm.size());
            if (m_audioFile->write(pcm) != pcm.size())
            {
                fail(QStringLiteral("Could not write MIDI audio."));
                return;
            }
        }
        if (m_options.format == QStringLiteral("video3d"))
        {
            m_waitingFrame = true;
            emit captureFrame(m_temp->filePath(QStringLiteral("frame.png")));
        }
        else
        {
            QImage image(videoWidth(), videoHeight(), QImage::Format_RGB32);
            image.fill(Qt::white);
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            int page = 0;
            while (page < m_pages.size() - 1 && (m_pages[page].chart != c || m_pages[page].continuation))
                ++page;
            drawPage(painter, QRectF(QPointF(), image.size()), page, true);
            painter.end();
            encodeFrame(image);
        }
        return;
    }
    if (m_options.format == QStringLiteral("csv"))
    {
        QFile file(m_staged[0]);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            fail(file.errorString());
            return;
        }
        QTextStream out(&file);
        out << QStringLiteral("Movement,Performer,Name,Instrument,Section,Set,Variant,Measure,Counts,"
                              "Coordinate,Move steps,Instructions\n");
        for (int c = 0; c < m_charts.size(); ++c)
        {
            activateChart(c);
            auto &p = *m_clones[m_charts[c].clone];
            const auto &s = p.m_sets[m_charts[c].set];
            for (int r = 0; r < p.m_performers.size(); ++r)
            {
                const auto &person = p.m_performers[r];
                QStringList row{p.m_movements[0].name,
                                person.label,
                                person.name,
                                person.instrument,
                                person.section,
                                s.number,
                                s.activeVariant().label,
                                s.measure,
                                QString::number(s.counts),
                                p.coordinateFor(r, m_charts[c].set),
                                QString::number(p.transitionDistance(r, m_charts[c].set), 'f', 2),
                                s.activeVariant().caption};
                for (auto &cell : row)
                    cell = csv(cell);
                out << row.join(QLatin1Char(',')) << QLatin1Char('\n');
            }
        }
        out.flush();
        if (file.error() != QFile::NoError)
        {
            fail(file.errorString());
            return;
        }
        file.close();
        finish();
        return;
    }
    if (m_nextPage >= m_pages.size() || (m_printer && m_printer->toPage()>0 && m_nextPage>=m_printer->toPage()))
    {
        finish();
        return;
    }
    if (m_options.format == QStringLiteral("png"))
    {
        const QSize pixels = (pageSize() * (m_options.dpi / 72.0)).toSize();
        QImage image(pixels, QImage::Format_RGB32);
        image.setDotsPerMeterX(qRound(m_options.dpi / 0.0254));
        image.setDotsPerMeterY(qRound(m_options.dpi / 0.0254));
        if (image.isNull())
        {
            fail(QStringLiteral("Not enough memory for this PNG resolution."));
            return;
        }
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        drawPage(painter, QRectF(QPointF(), pixels), m_nextPage);
        painter.end();
        if (!image.save(m_staged[m_nextPage]))
        {
            fail(QStringLiteral("Could not write PNG page."));
            return;
        }
    }
    else
    {
        const bool newDocument = !m_painter || (m_options.split && m_nextPage > 0 &&
                                                m_charts[m_pages[m_nextPage].chart].clone !=
                                                    m_charts[m_pages[m_nextPage - 1].chart].clone);
        if (newDocument && m_options.format == QStringLiteral("pdf"))
        {
            m_painter.reset();
            m_pdf.reset();
            int output = 0;
            if (m_options.split)
            {
                for (int p = 1; p <= m_nextPage; ++p)
                    if (m_charts[m_pages[p].chart].clone != m_charts[m_pages[p - 1].chart].clone)
                        ++output;
            }
            m_pdf = std::make_unique<QPdfWriter>(m_staged[output]);
            m_pdf->setPageSize(QPageSize(pageSize(), QPageSize::Point));
            m_pdf->setPageMargins(QMarginsF(0, 0, 0, 0));
            m_pdf->setResolution(144);
            m_pdf->setCreator(QStringLiteral("MarchCraft"));
            m_painter = std::make_unique<QPainter>(m_pdf.get());
            if (!m_painter->isActive())
            {
                fail(QStringLiteral("Could not create PDF."));
                return;
            }
        }
        else if (m_nextPage > 0 && (!m_printer || m_nextPage>qMax(0,m_printer->fromPage()-1)))
        {
            bool ok = m_printer ? m_printer->newPage() : m_pdf->newPage();
            if (!ok)
            {
                fail(QStringLiteral("Could not create next page."));
                return;
            }
        }
        m_painter->setRenderHint(QPainter::Antialiasing);
        drawPage(*m_painter,
                 QRectF(0, 0, m_printer ? m_printer->width() : m_pdf->width(),
                        m_printer ? m_printer->height() : m_pdf->height()),
                 m_nextPage);
    }
    ++m_nextPage;
    m_progress = double(m_nextPage) / m_pages.size();
    emit changed();
}
bool ExportController::publish()
{
    for (int i = 0; i < m_staged.size(); ++i)
    {
        QFile source(m_staged[i]);
        QSaveFile target(m_files[i]);
        if (!source.open(QIODevice::ReadOnly) || !target.open(QIODevice::WriteOnly))
        {
            m_message =
                QStringLiteral(
                    "Could not publish %1. %2 earlier files were published; remaining files are unchanged.")
                    .arg(m_files[i])
                    .arg(i);
            return false;
        }
        while (!source.atEnd())
        {
            const auto bytes = source.read(1024 * 1024);
            if (bytes.isEmpty() && source.error() != QFile::NoError)
            {
                m_message = source.errorString();
                return false;
            }
            if (target.write(bytes) != bytes.size())
            {
                m_message = target.errorString();
                return false;
            }
        }
        if (!target.commit())
        {
            m_message = target.errorString();
            return false;
        }
    }
    return true;
}
void ExportController::finish()
{
    m_timer.stop();
    m_painter.reset();
    m_pdf.reset();
    m_printer.reset();
    m_audioFile.reset();
    m_synth.reset();
    if (m_options.format != QStringLiteral("print") && !publish())
    {
        fail(m_message);
        return;
    }
    m_busy = false;
    m_progress = 1;
    m_message = QStringLiteral("Export complete.");
    emit changed();
    emit finished(true);
}
void ExportController::fail(const QString &message)
{
    m_timer.stop();
    m_busy = false;
    m_waitingFrame = false;
    m_encoder.kill();
    if (m_encoder.state() != QProcess::NotRunning)
        m_encoder.waitForFinished(3000);
    m_painter.reset();
    m_pdf.reset();
    m_printer.reset();
    m_audioFile.reset();
    m_synth.reset();
    for (const auto &file : m_staged)
        QFile::remove(file);
    for (const auto &name :
         {QStringLiteral("silent.mp4"), QStringLiteral("audio.f32"), QStringLiteral("frame.png")})
        QFile::remove(m_temp->filePath(name));
    m_message = message;
    emit changed();
    emit finished(false);
}
void ExportController::cancel()
{
    if (!m_busy)
        return;
    m_cancel = true;
    fail(QStringLiteral("Export canceled. Existing files were preserved."));
}
void ExportController::openResult(bool folder)
{
    if (m_files.isEmpty() || m_options.format == QStringLiteral("print"))
        return;
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(folder ? QFileInfo(m_files[0]).absolutePath() : m_files[0]));
}
void ExportController::beginVideo()
{
    m_frameEnds.clear();
    m_totalFrames = 0;
    for (const auto &c : m_charts)
    {
        m_totalFrames += qMax(1, qRound(c.durationMs * m_options.fps / 1000.0));
        m_frameEnds << m_totalFrames;
    }
    for (const auto &c : m_charts)
    {
        const auto &p = *m_clones[c.clone];
        if (m_options.audio == QStringLiteral("recording") && !QFileInfo::exists(pathOf(p.audioSource())))
        {
            fail(QStringLiteral("A selected movement has no readable attached recording."));
            return;
        }
        if (m_options.audio == QStringLiteral("midi") && p.m_music.playbackEvents.isEmpty())
        {
            fail(QStringLiteral("A selected movement has no MIDI playback data."));
            return;
        }
    }
    if (m_options.audio == QStringLiteral("midi"))
    {
        m_audioFile = std::make_unique<QFile>(m_temp->filePath(QStringLiteral("audio.f32")));
        if (!m_audioFile->open(QIODevice::WriteOnly))
        {
            fail(m_audioFile->errorString());
            return;
        }
    }
    const QString output = m_options.audio == QStringLiteral("silent")
                               ? m_staged[0]
                               : m_temp->filePath(QStringLiteral("silent.mp4"));
    QStringList args{QStringLiteral("-y"),
                     QStringLiteral("-f"),
                     QStringLiteral("rawvideo"),
                     QStringLiteral("-pixel_format"),
                     QStringLiteral("rgba"),
                     QStringLiteral("-video_size"),
                     QStringLiteral("%1x%2").arg(videoWidth()).arg(videoHeight()),
                     QStringLiteral("-framerate"),
                     QString::number(m_options.fps),
                     QStringLiteral("-i"),
                     QStringLiteral("pipe:0"),
                     QStringLiteral("-an"),
                     QStringLiteral("-c:v"),
                     QStringLiteral("libx264"),
                     QStringLiteral("-preset"),
                     QStringLiteral("fast"),
                     QStringLiteral("-crf"),
                     QStringLiteral("20"),
                     QStringLiteral("-pix_fmt"),
                     QStringLiteral("yuv420p"),
                     QStringLiteral("-movflags"),
                     QStringLiteral("+faststart"),
                     output};
    m_encoder.start(ffmpeg(), args);
    activateChart(0);
    m_waitingFrame = false;
    m_timer.start();
}
void ExportController::submitFrame(const QString &path)
{
    if (!m_busy || !m_waitingFrame)
        return;
    m_waitingFrame = false;
    QImage image(pathOf(path));
    if (image.isNull())
    {
        fail(QStringLiteral("3D frame capture failed."));
        return;
    }
    encodeFrame(image);
}
void ExportController::encodeFrame(QImage image)
{
    if (image.size() != QSize(videoWidth(), videoHeight()))
        image = image.scaled(videoWidth(), videoHeight(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (m_options.format == QStringLiteral("video3d") && m_activeChart >= 0)
    {
        QPainter painter(&image);
        const double factor = image.width() / 792.0;
        painter.scale(factor, factor);
        painter.fillRect(QRectF(0, 0, 792, 64), Qt::white);
        drawHeader(painter, QRectF(12, 6, 768, 52), *m_clones[m_charts[m_activeChart].clone]);
    }
    image = image.convertToFormat(QImage::Format_RGBA8888);
    const qint64 bytes = qint64(image.width()) * image.height() * 4;
    if (m_encoder.write(reinterpret_cast<const char *>(image.constBits()), bytes) != bytes)
    {
        fail(QStringLiteral("Video encoder stopped accepting frames."));
        return;
    }
    ++m_frame;
    m_progress = double(m_frame) / m_totalFrames * 0.9;
    emit changed();
}
void ExportController::beginMux()
{
    m_audioFile.reset();
    m_synth.reset();
    m_muxing = true;
    m_message = QStringLiteral("Finishing synchronized audio...");
    emit changed();
    QStringList args{QStringLiteral("-y"), QStringLiteral("-i"),
                     m_temp->filePath(QStringLiteral("silent.mp4"))};
    if (m_options.audio == QStringLiteral("midi"))
        args << QStringLiteral("-f") << QStringLiteral("f32le") << QStringLiteral("-ar")
             << QStringLiteral("48000") << QStringLiteral("-ac") << QStringLiteral("2")
             << QStringLiteral("-i") << m_temp->filePath(QStringLiteral("audio.f32"))
             << QStringLiteral("-map") << QStringLiteral("0:v:0") << QStringLiteral("-map")
             << QStringLiteral("1:a:0");
    else
    {
        QStringList filters, labels;
        for (int c = 0; c < m_charts.size(); ++c)
        {
            const auto &chart = m_charts[c];
            const auto &p = *m_clones[chart.clone];
            args << QStringLiteral("-i") << pathOf(p.audioSource());
            const double outputDuration =
                double(m_frameEnds[c] - (c ? m_frameEnds[c - 1] : 0)) / m_options.fps;
            if (chart.set == 0)
            {
                const QString label = QStringLiteral("[a%1]").arg(labels.size());
                filters << QStringLiteral("anullsrc=r=48000:cl=stereo,atrim=duration=%1%2")
                               .arg(outputDuration, 0, 'f', 6)
                               .arg(label);
                labels << label;
                continue;
            }
            QVector<qint64> boundaries{p.m_sets[chart.set - 1].startTick};
            const qint64 endTick = p.m_sets[chart.set].startTick;
            if (p.m_music.loaded())
                for (const auto &anchor : p.m_music.audioAnchors)
                    if (anchor.musicTick > boundaries.first() && anchor.musicTick < endTick)
                        boundaries << anchor.musicTick;
            boundaries << endTick;
            std::sort(boundaries.begin(), boundaries.end());
            for (int segment = 1; segment < boundaries.size(); ++segment)
            {
                const qint64 from = boundaries[segment - 1], to = boundaries[segment];
                const double duration = p.millisecondsBetween(from, to) / chart.durationMs * outputDuration;
                const double start =
                    (p.m_music.loaded() ? p.audioMsForMusicTick(from)
                                        : p.millisecondsBetween(0, from) + p.m_audioOffsetMs) /
                    1000.0;
                const double end = (p.m_music.loaded() ? p.audioMsForMusicTick(to)
                                                       : p.millisecondsBetween(0, to) + p.m_audioOffsetMs) /
                                   1000.0;
                const QString label = QStringLiteral("[a%1]").arg(labels.size());
                QString filter;
                if (end <= 0)
                    filter = QStringLiteral("anullsrc=r=48000:cl=stereo");
                else
                {
                    filter = QStringLiteral("[%1:a]atrim=start=%2:end=%3,asetpts=PTS-STARTPTS")
                                 .arg(c + 1)
                                 .arg(qMax(0.0, start), 0, 'f', 6)
                                 .arg(end, 0, 'f', 6);
                    if (start < 0)
                        filter += QStringLiteral(",adelay=%1:all=1").arg(qRound(-start * 1000));
                    double ratio = qMax(0.001, (end - start) / qMax(0.001, duration));
                    while (ratio > 2)
                    {
                        filter += QStringLiteral(",atempo=2");
                        ratio /= 2;
                    }
                    while (ratio < 0.5)
                    {
                        filter += QStringLiteral(",atempo=0.5");
                        ratio *= 2;
                    }
                    filter += QStringLiteral(",atempo=%1,aresample=48000,aformat=channel_layouts=stereo")
                                  .arg(ratio, 0, 'f', 6);
                }
                filter += QStringLiteral(",apad,atrim=duration=%1,asetpts=PTS-STARTPTS%2")
                              .arg(duration, 0, 'f', 6)
                              .arg(label);
                filters << filter;
                labels << label;
            }
        }
        filters << labels.join(QStringLiteral("")) +
                       QStringLiteral("concat=n=%1:v=0:a=1[a]").arg(labels.size());
        args << QStringLiteral("-filter_complex") << filters.join(QLatin1Char(';')) << QStringLiteral("-map")
             << QStringLiteral("0:v:0") << QStringLiteral("-map") << QStringLiteral("[a]");
    }
    args << QStringLiteral("-c:v") << QStringLiteral("copy") << QStringLiteral("-c:a")
         << QStringLiteral("aac") << QStringLiteral("-shortest") << QStringLiteral("-movflags")
         << QStringLiteral("+faststart") << m_staged[0];
    m_encoder.start(ffmpeg(), args);
}
