#pragma once

#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariantMap>
#include <memory>
#include <vector>

class DrillProject;
class QPainter;
class QPdfWriter;
class QPrinter;
class MidiSynthWorker;
class QFile;

struct ExportOptions
{
    QString format, content, paper, scope, variants, framing, audio, camera, basename;
    QStringList movements, sets, performers, sections, variantIds;
    bool landscape, monochrome, grid, labels, symbols, props, notes, headings, numbers, companyLogo,
        marchcraftLogo, subsets, split;
    double margin, fontSize, markerSize;
    int dpi, fps, height;
    QRectF crop;
    static ExportOptions fromMap(const QVariantMap &map);
};

class ExportController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString previewUrl READ previewUrl NOTIFY changed)
    Q_PROPERTY(QStringList pages READ pages NOTIFY changed)
    Q_PROPERTY(QStringList files READ files NOTIFY changed)
    Q_PROPERTY(QVariantList choices READ choices NOTIFY changed)
    Q_PROPERTY(QVariantMap branding READ branding NOTIFY changed)
    Q_PROPERTY(QStringList presetNames READ presetNames NOTIFY changed)
    Q_PROPERTY(QString ffmpeg READ ffmpeg WRITE setFfmpeg NOTIFY changed)
    Q_PROPERTY(QObject *renderProject READ renderProject NOTIFY renderProjectChanged)
    Q_PROPERTY(int videoWidth READ videoWidth NOTIFY changed)
    Q_PROPERTY(int videoHeight READ videoHeight NOTIFY changed)
    Q_PROPERTY(QString camera READ camera NOTIFY changed)
  public:
    explicit ExportController(DrillProject *project, QObject *parent = nullptr);
    ~ExportController() override;
    bool busy() const { return m_busy; }
    double progress() const { return m_progress; }
    QString message() const { return m_message; }
    QString previewUrl() const { return m_previewUrl; }
    QStringList pages() const { return m_pageNames; }
    QStringList files() const { return m_files; }
    QVariantList choices() const;
    QVariantMap branding() const;
    QStringList presetNames() const;
    QString ffmpeg() const;
    void setFfmpeg(const QString &path);
    QObject *renderProject() const;
    int videoWidth() const { return m_options.height * 16 / 9; }
    int videoHeight() const { return m_options.height; }
    QString camera() const { return m_options.camera; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool setBranding(const QString &name, const QString &logo, bool removeLogo = false);
    Q_INVOKABLE void savePreset(const QString &name, const QVariantMap &options);
    Q_INVOKABLE QVariantMap loadPreset(const QString &name) const;
    Q_INVOKABLE QString lastDestination() const;
    Q_INVOKABLE bool prepare(const QVariantMap &options);
    Q_INVOKABLE void preview(int page);
    Q_INVOKABLE QStringList plannedFiles(const QString &destination) const;
    Q_INVOKABLE bool start(const QString &destination, bool overwrite = false);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void submitFrame(const QString &path);
    Q_INVOKABLE void openResult(bool folder);
  signals:
    void changed();
    void renderProjectChanged();
    void captureFrame(const QString &path);
    void finished(bool success);

  private:
    struct Chart
    {
        int clone = 0;
        int set = 0;
        QString variant;
        double startMs = 0;
        double durationMs = 0;
    };
    struct Page
    {
        int chart = 0;
        QStringList notes;
        bool continuation = false;
        int performer = -1;
        QVector<int> rows;
    };
    void drawPage(QPainter &painter, const QRectF &target, int page, bool animated = false);
    void drawHeader(QPainter &painter, const QRectF &page, DrillProject &project);
    void drawChart(QPainter &painter, const QRectF &area, DrillProject &project, bool animated);
    void activateChart(int chart);
    QSizeF pageSize() const;
    void tick();
    void fail(const QString &message);
    void finish();
    void beginVideo();
    void encodeFrame(QImage image);
    void beginMux();
    bool publish();
    DrillProject *m_project;
    ExportOptions m_options;
    std::vector<std::unique_ptr<DrillProject>> m_clones;
    QVector<Chart> m_charts;
    QVector<QStringList> m_originalVariants;
    QVector<Page> m_pages;
    QStringList m_pageNames, m_files, m_staged;
    QString m_message, m_previewUrl;
    std::unique_ptr<QTemporaryDir> m_temp;
    std::unique_ptr<QPdfWriter> m_pdf;
    std::unique_ptr<QPainter> m_painter;
    std::unique_ptr<QPrinter> m_printer;
    std::unique_ptr<MidiSynthWorker> m_synth;
    std::unique_ptr<QFile> m_audioFile;
    QProcess m_encoder;
    QTimer m_timer;
    bool m_busy = false, m_cancel = false, m_muxing = false, m_waitingFrame = false;
    double m_progress = 0;
    qint64 m_prerollFrames = 0;
    int m_nextPage = 0, m_activeChart = -1, m_frame = 0, m_totalFrames = 0, m_audioChart = -1;
    QVector<int> m_frameEnds;
    QString m_encoderError;
};
