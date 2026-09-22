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
#include <QUuid>
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
using MarchCraft::Performer;
using MarchCraft::Placement;

#include "ProjectAlgorithms.h"
#include "ProjectStorage.h"

using namespace MarchCraft::ProjectAlgorithms;
using namespace MarchCraft::ProjectStorage;

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



DrillProject::DrillProject(QObject *parent)
    : DrillProject(false, parent)
{
}

DrillProject::DrillProject(bool backgroundWorker, QObject *parent)
    : QAbstractListModel(parent)
{
    m_backgroundWorkerClone = backgroundWorker;
    QSettings settings;
    m_formationAssignmentMode = MarchCraft::assignmentModeName(MarchCraft::assignmentModeFromName(
        settings.value(QStringLiteral("formation/assignmentMode"), QStringLiteral("rehearsalSafe")).toString()));
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
    m_fieldStyle = settings.value(QStringLiteral("view/fieldStyle"), QStringLiteral("realistic")).toString();
    if (m_fieldStyle != QStringLiteral("editor")) m_fieldStyle = QStringLiteral("realistic");
    m_showFieldGrid = settings.value(QStringLiteral("view/showFieldGrid"), false).toBool();
    m_fieldGridInterval = settings.value(QStringLiteral("view/fieldGridInterval"), 1.0).toDouble();
    m_fieldGridColor = settings.value(QStringLiteral("view/fieldGridColor"), QStringLiteral("#7dd3fc")).toString();
    m_fieldGridOpacity = qBound(0.02, settings.value(QStringLiteral("view/fieldGridOpacity"), 0.18).toDouble(), 0.8);
    m_measurementUnit = settings.value(QStringLiteral("view/measurementUnit"), QStringLiteral("steps")).toString();
    if (!backgroundWorker) {
        m_audioDecoder = new QAudioDecoder(this);
        m_midiWatcher = new QFutureWatcher<MarchCraft::MidiImportResult>(this);
        connect(m_midiWatcher, &QFutureWatcher<MarchCraft::MidiImportResult>::finished, this, [this] {
            const auto result = m_midiWatcher->result();
            if (m_midiImportRevision != m_projectRevision) { emit musicChanged(); return; }
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
    }
    m_autosaveTimer.setSingleShot(true);
    m_autosaveTimer.setInterval(800);
    connect(&m_autosaveTimer, &QTimer::timeout, this, &DrillProject::autosave);
    connect(&m_undo, &QUndoStack::canUndoChanged, this, &DrillProject::historyChanged);
    connect(&m_undo, &QUndoStack::canRedoChanged, this, &DrillProject::historyChanged);
    connect(this, &QAbstractItemModel::modelAboutToBeReset, this, [this] {
        ++m_projectRevision; cancelFormationPreview();
    });
    connect(this, &DrillProject::selectionChanged, this, &DrillProject::cancelFormationPreview);
    connect(&m_undo, &QUndoStack::cleanChanged, this, [this](bool clean) {
        if (m_dirty == !clean) return;
        m_dirty = !clean;
        if (clean) m_autosaveTimer.stop();
        emit dirtyChanged();
    });
    newProject();
    m_autosaveTimer.stop();
    m_undo.clear();
    m_dirty = false;
}

void DrillProject::setFormationAssignmentMode(const QString &mode)
{
    const QString next = MarchCraft::assignmentModeName(MarchCraft::assignmentModeFromName(mode));
    if (next == m_formationAssignmentMode) return;
    m_formationAssignmentMode = next;
    QSettings().setValue(QStringLiteral("formation/assignmentMode"), next);
    emit editorSettingsChanged();
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
void DrillProject::setFieldStyle(const QString &value)
{
    if ((value != QStringLiteral("realistic") && value != QStringLiteral("editor")) || value == m_fieldStyle)
        return;
    m_fieldStyle = value;
    QSettings().setValue(QStringLiteral("view/fieldStyle"), value);
    emit editorSettingsChanged();
}

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
    case GaitElapsedCountsRole: return cachedAnimationStateAt(index.row()).elapsedCounts;
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

void DrillProject::savePerformerDetails(int row, const QString &label, const QString &name,
                                        const QString &instrument, const QString &section,
                                        const QString &notes, const QString &equipmentId)
{
    if (row < -1 || row >= m_performers.size()) return;
    m_undo.beginMacro(row < 0 ? QStringLiteral("Add performer") : QStringLiteral("Edit performer"));
    if (row < 0) {
        addPerformer(label, instrument, section);
        row = m_performers.size() - 1;
    }
    updatePerformer(row, label, name, instrument, section, notes);
    setData(index(row), equipmentId, InstrumentAssetRole);
    m_undo.endMacro();
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
            {GaitElapsedCountsRole, "gaitElapsedCounts"},
            {TravelPathTypeRole, "travelPathType"}, {ClosingTransitionRole, "closingTransition"},
            {TotalDistanceRole, "totalDistance"},
            {WarningRole, "hasWarning"}, {VisibleRole, "performerVisible"},
            {LockedRole, "performerLocked"}, {BodyRigRole, "bodyRigId"},
            {UniformRole, "uniformId"}, {SkinPaletteRole, "skinPaletteId"},
            {InstrumentAssetRole, "instrumentAssetId"}, {EquipmentAssetRole, "equipmentAssetId"},
            {PerformerHeightRole, "performerHeightMeters"},
            {PerformerRoleRole, "performerRole"}};
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
    m_transitionPaths.clear();
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
    const auto before = toJson();
    m_playbackSource = next; emit transportSettingsChanged(); commitSnapshot(before, QStringLiteral("Playback source changed")); emit projectChanged();
}

void DrillProject::setMidiMasterVolume(double value)
{
    value = qBound(0.0, value, 1.0); if (qFuzzyCompare(m_midiMasterVolume, value)) return;
    const auto before = toJson();
    m_midiMasterVolume = value; emit transportSettingsChanged(); commitSnapshot(before, QStringLiteral("MIDI volume changed")); emit projectChanged();
}

void DrillProject::setLoopEnabled(bool value)
{
    if (m_loopEnabled == value) return;
    const auto before = toJson();
    m_loopEnabled = value;
    emit transportSettingsChanged(); commitSnapshot(before, QStringLiteral("Loop setting changed")); emit projectChanged();
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
    ++m_animationStateRevision;
    emit playheadChanged();
    if (!m_performers.isEmpty())
        emit dataChanged(index(0), index(m_performers.size() - 1),
                         {XRole, YRole, FacingRole, TravelHeadingRole, TravelStepsPerCountRole,
                          LocomotionModeRole, GaitPhaseRole, GaitElapsedCountsRole, ClosingTransitionRole});
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

void DrillProject::newProject()
{
    if (m_audioDecoder) m_audioDecoder->stop();
    m_transitionPaths.clear();
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
    m_musicSections.clear();
    m_musicSelectionStart = m_musicSelectionEnd = -1;
    m_audioOffsetMs = 0.0; m_waveformPeaks.clear();
    m_projectPath.clear();
    m_currentSet = 0;
    m_selectedSetStart = m_selectedSetEnd = 0;
    m_playbackSource = QStringLiteral("midi"); m_midiMasterVolume = 0.75; m_loopEnabled = false;
    m_playhead = 0.0;
    m_playbackActive = false;
    endResetModel();
    emit playbackActiveChanged();
    emit playheadChanged();
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
    if (!canGroupSelection()) {
        setStatus(selectedCount() < 2 ? QStringLiteral("Select at least two performers to group")
                                      : QStringLiteral("The selected performers are already grouped"));
        return;
    }
    QVector<QString> selected;
    for (const auto &performer : m_performers) if (performer.selected) selected.push_back(performer.id);
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
    if (m_currentSet < 0 || !canRemoveSelectionFromGroup()) return;
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
    if (m_currentSet < 0 || !canUngroupSelection()) return;
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

void DrillProject::commitSnapshot(const QJsonObject &before, const QString &text)
{
    const auto after = toJson();
    if (before == after)
        return;
    m_transitionPaths.clear();
    m_analyticsValid = false;
    m_undo.push(new ProjectStateCommand(this, before, after, text));
    markDirty(text);
    if (!m_backgroundWorkerClone) { invalidateClinic(); analyzeTransition(m_currentSet); }
    emit statisticsChanged();
}

void DrillProject::markDirty(const QString &message)
{
    ++m_projectRevision;
    if (!m_backgroundWorkerClone) cancelFormationPreview();
    if (!m_dirty) {
        m_dirty = true;
        emit dirtyChanged();
    }
    if (!message.isEmpty()) setStatus(message);
    if (!m_backgroundWorkerClone) m_autosaveTimer.start();
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
    m_transitionPaths.clear();
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
