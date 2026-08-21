#pragma once

#include "DrillTypes.h"
#include "MusicDocument.h"

#include <QAbstractListModel>
#include <QJsonObject>
#include <QTimer>
#include <QSizeF>
#include <QSet>
#include <QUndoStack>
#include <QVariantList>

class QAudioDecoder;
class TransportController;
template<typename T> class QFutureWatcher;

class DrillProject final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString showName READ showName WRITE setShowName NOTIFY projectChanged)
    Q_PROPERTY(int performerCount READ performerCount NOTIFY performerCountChanged)
    Q_PROPERTY(QString fieldPreset READ fieldPreset WRITE setFieldPreset NOTIFY projectChanged)
    Q_PROPERTY(double fieldWidthSteps READ fieldWidthSteps CONSTANT)
    Q_PROPERTY(double metersPerStep READ metersPerStep CONSTANT)
    Q_PROPERTY(int fieldInsertCount READ fieldInsertCount CONSTANT)
    Q_PROPERTY(double fieldDepthSteps READ fieldDepthSteps NOTIFY projectChanged)
    Q_PROPERTY(double canvasMinX READ canvasMinX CONSTANT)
    Q_PROPERTY(double canvasMaxX READ canvasMaxX CONSTANT)
    Q_PROPERTY(double canvasMinY READ canvasMinY CONSTANT)
    Q_PROPERTY(double canvasMaxY READ canvasMaxY NOTIFY projectChanged)
    Q_PROPERTY(double frontHashSteps READ frontHashSteps NOTIFY projectChanged)
    Q_PROPERTY(double backHashSteps READ backHashSteps NOTIFY projectChanged)
    Q_PROPERTY(int currentSetIndex READ currentSetIndex WRITE setCurrentSetIndex NOTIFY currentSetChanged)
    Q_PROPERTY(int selectedSetStartIndex READ selectedSetStartIndex NOTIFY setRangeChanged)
    Q_PROPERTY(int selectedSetEndIndex READ selectedSetEndIndex NOTIFY setRangeChanged)
    Q_PROPERTY(int setCount READ setCount NOTIFY setsChanged)
    Q_PROPERTY(int currentVariantCount READ currentVariantCount NOTIFY setsChanged)
    Q_PROPERTY(int currentArchivedVariantCount READ currentArchivedVariantCount NOTIFY setsChanged)
    Q_PROPERTY(int currentVariantIndex READ currentVariantIndex WRITE activateVariant NOTIFY currentSetChanged)
    Q_PROPERTY(int archivedSetCount READ archivedSetCount NOTIFY setsChanged)
    Q_PROPERTY(int currentShapeCount READ currentShapeCount NOTIFY shapesChanged)
    Q_PROPERTY(QString shapePlacementMode READ shapePlacementMode WRITE setShapePlacementMode NOTIFY editorSettingsChanged)
    Q_PROPERTY(bool showShapeGuides READ showShapeGuides WRITE setShowShapeGuides NOTIFY editorSettingsChanged)
    Q_PROPERTY(bool showTransitionPaths READ showTransitionPaths WRITE setShowTransitionPaths NOTIFY editorSettingsChanged)
    Q_PROPERTY(QString performerMarkerStyle READ performerMarkerStyle WRITE setPerformerMarkerStyle NOTIFY editorSettingsChanged)
    Q_PROPERTY(int performerMarkerSize READ performerMarkerSize WRITE setPerformerMarkerSize NOTIFY editorSettingsChanged)
    Q_PROPERTY(QString markerGeometry READ markerGeometry WRITE setMarkerGeometry NOTIFY editorSettingsChanged)
    Q_PROPERTY(QString markerFillColor READ markerFillColor WRITE setMarkerFillColor NOTIFY editorSettingsChanged)
    Q_PROPERTY(QString markerOutlineColor READ markerOutlineColor WRITE setMarkerOutlineColor NOTIFY editorSettingsChanged)
    Q_PROPERTY(int markerOutlineWidth READ markerOutlineWidth WRITE setMarkerOutlineWidth NOTIFY editorSettingsChanged)
    Q_PROPERTY(QString markerLabelMode READ markerLabelMode WRITE setMarkerLabelMode NOTIFY editorSettingsChanged)
    Q_PROPERTY(int markerLabelFontSize READ markerLabelFontSize WRITE setMarkerLabelFontSize NOTIFY editorSettingsChanged)
    Q_PROPERTY(QString markerLabelColor READ markerLabelColor WRITE setMarkerLabelColor NOTIFY editorSettingsChanged)
    Q_PROPERTY(bool markerFacingVisible READ markerFacingVisible WRITE setMarkerFacingVisible NOTIFY editorSettingsChanged)
    Q_PROPERTY(QString markerFacingColor READ markerFacingColor WRITE setMarkerFacingColor NOTIFY editorSettingsChanged)
    Q_PROPERTY(QString markerWarningColor READ markerWarningColor WRITE setMarkerWarningColor NOTIFY editorSettingsChanged)
    Q_PROPERTY(bool showFieldGrid READ showFieldGrid WRITE setShowFieldGrid NOTIFY editorSettingsChanged)
    Q_PROPERTY(double fieldGridInterval READ fieldGridInterval WRITE setFieldGridInterval NOTIFY editorSettingsChanged)
    Q_PROPERTY(QString fieldGridColor READ fieldGridColor WRITE setFieldGridColor NOTIFY editorSettingsChanged)
    Q_PROPERTY(double fieldGridOpacity READ fieldGridOpacity WRITE setFieldGridOpacity NOTIFY editorSettingsChanged)
    Q_PROPERTY(QString measurementUnit READ measurementUnit WRITE setMeasurementUnit NOTIFY editorSettingsChanged)
    Q_PROPERTY(QString venuePreset READ venuePreset WRITE setVenuePreset NOTIFY sceneChanged)
    Q_PROPERTY(QString lightingPreset READ lightingPreset WRITE setLightingPreset NOTIFY sceneChanged)
    Q_PROPERTY(QString graphicsProfile READ graphicsProfile WRITE setGraphicsProfile NOTIFY sceneChanged)
    Q_PROPERTY(QString venuePrimaryColor READ venuePrimaryColor WRITE setVenuePrimaryColor NOTIFY sceneChanged)
    Q_PROPERTY(QString venueSecondaryColor READ venueSecondaryColor WRITE setVenueSecondaryColor NOTIFY sceneChanged)
    Q_PROPERTY(QString turfColor READ turfColor WRITE setTurfColor NOTIFY sceneChanged)
    Q_PROPERTY(QString scoreboardText READ scoreboardText WRITE setScoreboardText NOTIFY sceneChanged)
    Q_PROPERTY(double crowdDensity READ crowdDensity WRITE setCrowdDensity NOTIFY sceneChanged)
    Q_PROPERTY(bool debug3D READ debug3D WRITE setDebug3D NOTIFY sceneChanged)
    Q_PROPERTY(QVariantList props READ props NOTIFY propsChanged)
    Q_PROPERTY(QString currentSetName READ currentSetName NOTIFY currentSetChanged)
    Q_PROPERTY(int currentSetCounts READ currentSetCounts NOTIFY currentSetChanged)
    Q_PROPERTY(QString openingBehavior READ openingBehavior NOTIFY setsChanged)
    Q_PROPERTY(int openingCounts READ openingCounts NOTIFY setsChanged)
    Q_PROPERTY(double openingDurationMs READ openingDurationMs NOTIFY timingChanged)
    Q_PROPERTY(double playhead READ playhead WRITE setPlayhead NOTIFY playheadChanged)
    Q_PROPERTY(bool playbackActive READ playbackActive WRITE setPlaybackActive NOTIFY playbackActiveChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    Q_PROPERTY(QString projectPath READ projectPath NOTIFY projectChanged)
    Q_PROPERTY(QString audioSource READ audioSource WRITE setAudioSource NOTIFY projectChanged)
    Q_PROPERTY(double bpm READ bpm WRITE setBpm NOTIFY projectChanged)
    Q_PROPERTY(int meterRegionCount READ meterRegionCount NOTIFY timingChanged)
    Q_PROPERTY(int tempoRegionCount READ tempoRegionCount NOTIFY timingChanged)
    Q_PROPERTY(bool musicLoaded READ musicLoaded NOTIFY musicChanged)
    Q_PROPERTY(bool midiImporting READ midiImporting NOTIFY musicChanged)
    Q_PROPERTY(QString musicSourceType READ musicSourceType NOTIFY musicChanged)
    Q_PROPERTY(int musicMeasureCount READ musicMeasureCount NOTIFY musicChanged)
    Q_PROPERTY(int musicTrackCount READ musicTrackCount NOTIFY musicChanged)
    Q_PROPERTY(double musicDurationMs READ musicDurationMs NOTIFY musicChanged)
    Q_PROPERTY(QString musicDiagnostics READ musicDiagnostics NOTIFY musicChanged)
    Q_PROPERTY(int musicSelectionStart READ musicSelectionStart NOTIFY musicChanged)
    Q_PROPERTY(int musicSelectionEnd READ musicSelectionEnd NOTIFY musicChanged)
    Q_PROPERTY(QString playbackSource READ playbackSource WRITE setPlaybackSource NOTIFY transportSettingsChanged)
    Q_PROPERTY(double midiMasterVolume READ midiMasterVolume WRITE setMidiMasterVolume NOTIFY transportSettingsChanged)
    Q_PROPERTY(bool loopEnabled READ loopEnabled WRITE setLoopEnabled NOTIFY transportSettingsChanged)
    Q_PROPERTY(int waveformPeakCount READ waveformPeakCount NOTIFY waveformChanged)
    Q_PROPERTY(double audioOffsetMs READ audioOffsetMs WRITE setAudioOffsetMs NOTIFY musicChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(int selectedGroupedCount READ selectedGroupedCount NOTIFY selectionChanged)
    Q_PROPERTY(bool selectionIsExactGroup READ selectionIsExactGroup NOTIFY selectionChanged)
    Q_PROPERTY(double averageDistance READ averageDistance NOTIFY statisticsChanged)
    Q_PROPERTY(double totalDistance READ totalDistance NOTIFY statisticsChanged)
    Q_PROPERTY(double longestDistance READ longestDistance NOTIFY statisticsChanged)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY statisticsChanged)
    Q_PROPERTY(QVariantMap selectionMetrics READ selectionMetrics NOTIFY statisticsChanged)
    Q_PROPERTY(bool formationPreviewActive READ formationPreviewActive NOTIFY formationPreviewChanged)
    Q_PROPERTY(bool formationPreviewBusy READ formationPreviewBusy NOTIFY formationPreviewChanged)
    Q_PROPERTY(QVariantList formationPreviewPoints READ formationPreviewPoints NOTIFY formationPreviewChanged)
    Q_PROPERTY(QVariantMap formationPreviewMetrics READ formationPreviewMetrics NOTIFY formationPreviewChanged)
    Q_PROPERTY(QString formationPreviewMode READ formationPreviewMode NOTIFY formationPreviewChanged)
    Q_PROPERTY(QVariantList clinicIssues READ clinicIssues NOTIFY clinicChanged)
    Q_PROPERTY(int clinicIssueCount READ clinicIssueCount NOTIFY clinicChanged)
    Q_PROPERTY(QString capabilityProfile READ capabilityProfile WRITE setCapabilityProfile NOTIFY clinicChanged)
    Q_PROPERTY(double maximumStepsPerCount READ maximumStepsPerCount WRITE setMaximumStepsPerCount NOTIFY clinicChanged)
    Q_PROPERTY(double collisionClearance READ collisionClearance WRITE setCollisionClearance NOTIFY clinicChanged)
    Q_PROPERTY(double directionChangeDegrees READ directionChangeDegrees WRITE setDirectionChangeDegrees NOTIFY clinicChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        LabelRole,
        NameRole,
        SectionRole,
        InstrumentRole,
        SymbolRole,
        ColorRole,
        NotesRole,
        XRole,
        YRole,
        FromXRole,
        FromYRole,
        FacingRole,
        SelectedRole,
        SetDistanceRole,
        TravelHeadingRole,
        TravelStepsPerCountRole,
        LocomotionModeRole,
        GaitPhaseRole,
        TravelPathTypeRole,
        ClosingTransitionRole,
        TotalDistanceRole,
        WarningRole,
        VisibleRole,
        LockedRole,
        BodyRigRole,
        UniformRole,
        SkinPaletteRole,
        InstrumentAssetRole,
        EquipmentAssetRole,
        PerformerHeightRole,
        PerformerRoleRole
    };

    explicit DrillProject(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString showName() const { return m_showName; }
    int performerCount() const { return m_performers.size(); }
    void setShowName(const QString &value);
    QString fieldPreset() const { return m_fieldPreset; }
    void setFieldPreset(const QString &value);
    double fieldWidthSteps() const { return 160.0; }
    double metersPerStep() const { return MarchCraft::FieldTransform::MetersPerStep; }
    int fieldInsertCount() const { return static_cast<int>(fieldWidthSteps() / 8.0) * 4; }
    Q_INVOKABLE double fieldInsertStep(int column) const;
    double fieldDepthSteps() const;
    double canvasMinX() const { return -16.0; }
    double canvasMaxX() const { return 176.0; }
    double canvasMinY() const { return -8.0; }
    double canvasMaxY() const { return fieldDepthSteps() + 8.0; }
    double frontHashSteps() const;
    double backHashSteps() const;
    int currentSetIndex() const { return m_currentSet; }
    void setCurrentSetIndex(int value);
    int selectedSetStartIndex() const { return m_selectedSetStart; }
    int selectedSetEndIndex() const { return m_selectedSetEnd; }
    int setCount() const { return m_sets.size(); }
    int currentVariantCount() const;
    int currentArchivedVariantCount() const;
    int currentVariantIndex() const;
    int archivedSetCount() const { return m_archivedSets.size(); }
    int currentShapeCount() const;
    QString shapePlacementMode() const { return m_shapePlacementMode; }
    void setShapePlacementMode(const QString &mode);
    bool showShapeGuides() const { return m_showShapeGuides; }
    void setShowShapeGuides(bool value);
    bool showTransitionPaths() const { return m_showTransitionPaths; }
    QString performerMarkerStyle() const { return m_performerMarkerStyle; }
    void setPerformerMarkerStyle(const QString &value);
    int performerMarkerSize() const { return m_performerMarkerSize; }
    void setPerformerMarkerSize(int value);
    QString markerGeometry() const { return m_markerGeometry; } void setMarkerGeometry(const QString &value);
    QString markerFillColor() const { return m_markerFillColor; } void setMarkerFillColor(const QString &value);
    QString markerOutlineColor() const { return m_markerOutlineColor; } void setMarkerOutlineColor(const QString &value);
    int markerOutlineWidth() const { return m_markerOutlineWidth; } void setMarkerOutlineWidth(int value);
    QString markerLabelMode() const { return m_markerLabelMode; } void setMarkerLabelMode(const QString &value);
    int markerLabelFontSize() const { return m_markerLabelFontSize; }
    void setMarkerLabelFontSize(int value);
    QString markerLabelColor() const { return m_markerLabelColor; } void setMarkerLabelColor(const QString &value);
    bool markerFacingVisible() const { return m_markerFacingVisible; } void setMarkerFacingVisible(bool value);
    QString markerFacingColor() const { return m_markerFacingColor; } void setMarkerFacingColor(const QString &value);
    QString markerWarningColor() const { return m_markerWarningColor; } void setMarkerWarningColor(const QString &value);
    bool showFieldGrid() const { return m_showFieldGrid; } void setShowFieldGrid(bool value);
    double fieldGridInterval() const { return m_fieldGridInterval; } void setFieldGridInterval(double value);
    QString fieldGridColor() const { return m_fieldGridColor; } void setFieldGridColor(const QString &value);
    double fieldGridOpacity() const { return m_fieldGridOpacity; } void setFieldGridOpacity(double value);
    QString measurementUnit() const { return m_measurementUnit; } void setMeasurementUnit(const QString &value);
    QString venuePreset() const { return m_venue.venueId; } void setVenuePreset(const QString &value);
    QString lightingPreset() const { return m_venue.lightingId; } void setLightingPreset(const QString &value);
    QString graphicsProfile() const { return m_venue.graphicsProfile; } void setGraphicsProfile(const QString &value);
    QString venuePrimaryColor() const { return m_venue.primaryColor.name(); } void setVenuePrimaryColor(const QString &value);
    QString venueSecondaryColor() const { return m_venue.secondaryColor.name(); } void setVenueSecondaryColor(const QString &value);
    QString turfColor() const { return m_venue.turfColor.name(); } void setTurfColor(const QString &value);
    QString scoreboardText() const { return m_venue.scoreboardText; } void setScoreboardText(const QString &value);
    double crowdDensity() const { return m_venue.crowdDensity; } void setCrowdDensity(double value);
    bool debug3D() const { return m_venue.debugOverlay; } void setDebug3D(bool value);
    QVariantList props() const;
    void setShowTransitionPaths(bool value);
    QString currentSetName() const;
    int currentSetCounts() const;
    QString openingBehavior() const { return m_openingBehavior; }
    int openingCounts() const { return m_openingCounts; }
    double openingDurationMs() const;
    double playhead() const { return m_playhead; }
    void setPlayhead(double value);
    bool playbackActive() const { return m_playbackActive; }
    void setPlaybackActive(bool value);
    bool dirty() const { return m_dirty; }
    QString projectPath() const { return m_projectPath; }
    QString audioSource() const { return m_audioSource; }
    void setAudioSource(const QString &value);
    double bpm() const { return m_bpm; }
    void setBpm(double value);
    int meterRegionCount() const { return m_meterRegions.size(); }
    int tempoRegionCount() const { return m_tempoRegions.size(); }
    bool musicLoaded() const { return m_music.loaded(); }
    bool midiImporting() const;
    QString musicSourceType() const { return m_music.sourceType; }
    int musicMeasureCount() const { return m_music.measures.size(); }
    int musicTrackCount() const { return m_music.tracks.size(); }
    double musicDurationMs() const { return m_music.durationMs; }
    QString musicDiagnostics() const { return m_music.diagnostics.join(QStringLiteral(" · ")); }
    int musicSelectionStart() const { return m_musicSelectionStart; }
    int musicSelectionEnd() const { return m_musicSelectionEnd; }
    QString playbackSource() const { return m_playbackSource; }
    void setPlaybackSource(const QString &value);
    double midiMasterVolume() const { return m_midiMasterVolume; }
    void setMidiMasterVolume(double value);
    bool loopEnabled() const { return m_loopEnabled; }
    void setLoopEnabled(bool value);
    int waveformPeakCount() const { return m_waveformPeaks.size(); }
    double audioOffsetMs() const { return m_audioOffsetMs; }
    void setAudioOffsetMs(double value);
    QString statusMessage() const { return m_statusMessage; }
    int selectedCount() const;
    int selectedGroupedCount() const;
    bool selectionIsExactGroup() const;
    double averageDistance() const;
    double totalDistance() const;
    double longestDistance() const;
    int warningCount() const;
    QVariantMap selectionMetrics() const;
    bool formationPreviewActive() const { return m_formationPreview.active; }
    bool formationPreviewBusy() const { return m_formationPreviewBusy; }
    QVariantList formationPreviewPoints() const;
    QVariantMap formationPreviewMetrics() const { return m_formationPreview.metrics; }
    QString formationPreviewMode() const { return m_formationPreview.mode; }
    QVariantList clinicIssues() const { return m_clinicIssues; }
    int clinicIssueCount() const { return m_clinicIssues.size(); }
    QString capabilityProfile() const { return m_capability.name; }
    void setCapabilityProfile(const QString &value);
    double maximumStepsPerCount() const { return m_capability.maximumStepsPerCount; }
    void setMaximumStepsPerCount(double value);
    double collisionClearance() const { return m_capability.collisionClearance; }
    void setCollisionClearance(double value);
    double directionChangeDegrees() const { return m_capability.directionChangeDegrees; }
    void setDirectionChangeDegrees(double value);
    bool canUndo() const { return m_undo.canUndo(); }
    bool canRedo() const { return m_undo.canRedo(); }

    Q_INVOKABLE void newProject();
    Q_INVOKABLE void loadDemo();
    Q_INVOKABLE bool importCoordinateJson(const QString &urlOrPath);
    Q_INVOKABLE bool saveProject(const QString &urlOrPath = {});
    Q_INVOKABLE bool loadProject(const QString &urlOrPath);
    Q_INVOKABLE bool exportCsv(const QString &urlOrPath) const;
    Q_INVOKABLE bool exportCoordinatePdf(const QString &urlOrPath) const;
    Q_INVOKABLE bool importMusicXml(const QString &urlOrPath);
    Q_INVOKABLE bool importMidi(const QString &urlOrPath);
    Q_INVOKABLE void importMidiAsync(const QString &urlOrPath);
    Q_INVOKABLE void attachAudio(const QString &urlOrPath);
    Q_INVOKABLE QVariantMap musicMeasureInfo(int index) const;
    Q_INVOKABLE QVariantMap musicTrackInfo(int index) const;
    Q_INVOKABLE void setMusicTrackSelected(int index, bool selected);
    Q_INVOKABLE void setMusicTrackMuted(int index, bool muted);
    Q_INVOKABLE void setMusicTrackSolo(int index, bool solo);
    Q_INVOKABLE void setMusicTrackVolume(int index, double volume);
    Q_INVOKABLE void selectSetRange(int index, bool extend = false);
    Q_INVOKABLE void setMusicSelection(int startMeasure, int endMeasure);
    Q_INVOKABLE QVariantList previewSetGeneration(int startMeasure, int endMeasure,
                                                  const QString &mode = QStringLiteral("subdivide"),
                                                  int subdivision = 16, double stepMultiplier = 1.0) const;
    Q_INVOKABLE bool commitSetGeneration(int startMeasure, int endMeasure,
                                         const QString &mode = QStringLiteral("subdivide"),
                                         int subdivision = 16, double stepMultiplier = 1.0);
    Q_INVOKABLE bool commitSetGenerationPlan(const QVariantList &segments);
    Q_INVOKABLE QVariantList previewSetMapping() const;
    Q_INVOKABLE bool applySetMapping(const QVariantList &measureIndices);
    Q_INVOKABLE void setSetStepMultiplier(int setIndex, double multiplier);
    Q_INVOKABLE double waveformPeak(int index) const;
    Q_INVOKABLE void addAudioAnchor(double audioMs, qint64 musicTick);
    Q_INVOKABLE void clearAudioAnchors();
    Q_INVOKABLE qint64 musicTickForAudioMs(double audioMs) const;
    Q_INVOKABLE double audioMsForMusicTick(qint64 tick) const;
    Q_INVOKABLE QVariantMap meterRegionInfo(int index) const;
    Q_INVOKABLE QVariantMap tempoRegionInfo(int index) const;
    Q_INVOKABLE void setMeterRegion(qint64 startTick, qint64 endTick, int numerator,
                                    int denominator, qint64 pulseTicks, const QString &grouping = {});
    Q_INVOKABLE void setTempoRegion(qint64 startTick, qint64 endTick, double startBpm,
                                    double endBpm, const QString &name = {});
    Q_INVOKABLE void removeMeterRegion(int index);
    Q_INVOKABLE void removeTempoRegion(int index);
    Q_INVOKABLE void recalculateCounts();
    Q_INVOKABLE void setCurrentSetCounts(int counts);
    Q_INVOKABLE void setOpeningBehavior(const QString &behavior, int counts);
    Q_INVOKABLE double transitionDurationMs(int destinationSet) const;
    Q_INVOKABLE double showDurationMs() const;
    Q_INVOKABLE bool setShowTimeMs(double milliseconds);
    Q_INVOKABLE bool setShowAudioTimeMs(double audioMilliseconds);
    Q_INVOKABLE QString effectiveTempoText(int destinationSet) const;

    Q_INVOKABLE void addPerformer(const QString &label, const QString &instrument,
                                  const QString &section, double x = 80.0, double y = 28.0);
    Q_INVOKABLE void batchAddPerformers(const QString &prefix, int count,
                                        const QString &instrument, const QString &section);
    Q_INVOKABLE void removeSelectedPerformers();
    Q_INVOKABLE void updatePerformer(int row, const QString &label, const QString &name,
                                     const QString &instrument, const QString &section,
                                     const QString &notes);
    Q_INVOKABLE void setPerformerColor(int row, const QString &color);
    Q_INVOKABLE void setPerformerAppearance(int row, const QString &bodyRigId,
                                            const QString &uniformId, const QString &skinPaletteId,
                                            const QString &instrumentAssetId, double heightMeters);
    Q_INVOKABLE void updateSelectedPerformers(const QString &instrument, const QString &section,
                                              const QString &color, double facing,
                                              int visibleMode = -1, int lockedMode = -1);
    Q_INVOKABLE void autoLabel(const QString &prefix);
    Q_INVOKABLE QString addProp(const QString &definitionId, double x, double y);
    Q_INVOKABLE void updateProp(const QString &id, double x, double y, double rotation,
                                double scaleX, double scaleY, double scaleZ);
    Q_INVOKABLE void removeProp(const QString &id);

    Q_INVOKABLE void addSet(const QString &name, int counts, bool subset = false);
    Q_INVOKABLE void batchAddSets(int numberOfSets, int counts);
    Q_INVOKABLE void duplicateCurrentSet();
    Q_INVOKABLE void duplicateSetAt(int index);
    Q_INVOKABLE void insertSetAt(int index);
    Q_INVOKABLE void archiveSetAt(int index);
    Q_INVOKABLE void moveSet(int from, int to);
    Q_INVOKABLE bool setLabelsNeedRenumbering() const;
    Q_INVOKABLE void renumberSets();
    Q_INVOKABLE void removeCurrentSet();
    Q_INVOKABLE void createVariant(const QString &name = {}, const QString &caption = {});
    Q_INVOKABLE void activateVariant(int index);
    Q_INVOKABLE void archiveCurrentVariant();
    Q_INVOKABLE void archiveCurrentSet();
    Q_INVOKABLE void restoreArchivedSet(int index);
    Q_INVOKABLE void purgeArchivedSet(int index);
    Q_INVOKABLE void restoreArchivedVariant(int index);
    Q_INVOKABLE void purgeArchivedVariant(int index);
    Q_INVOKABLE void updateCurrentSet(const QString &number, const QString &name,
                                      const QString &caption, const QString &measure,
                                      int counts, bool subset);

    Q_INVOKABLE void selectPerformer(int row, bool additive);
    Q_INVOKABLE void selectPerformerMode(int row, int mode);
    Q_INVOKABLE void selectPerformerRange(int anchorRow, int row, bool additive);
    Q_INVOKABLE void selectInRect(double x1, double y1, double x2, double y2, bool additive);
    Q_INVOKABLE void selectInPolygon(const QVariantList &points, bool additive);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void groupSelected(const QString &name = {});
    Q_INVOKABLE void ungroupSelected();
    Q_INVOKABLE void removeSelectedFromGroup();
    Q_INVOKABLE void selectGroupForPerformer(int row, bool additive = false);
    Q_INVOKABLE QVariantMap performerGroupInfo(int row) const;
    Q_INVOKABLE void beginMove(int row, bool additive);
    Q_INVOKABLE void previewMove(double dx, double dy, bool lockX = false, bool lockY = false);
    Q_INVOKABLE void endMove();
    Q_INVOKABLE QVariantMap selectedBounds() const;
    Q_INVOKABLE int selectedShapeIndex() const;
    Q_INVOKABLE void beginScale();
    Q_INVOKABLE void previewScale(double factor);
    Q_INVOKABLE void endScale();
    Q_INVOKABLE void beginRotate();
    Q_INVOKABLE void previewRotate(double degrees);
    Q_INVOKABLE void endRotate();
    Q_INVOKABLE void nudgeSelected(double dx, double dy);
    Q_INVOKABLE void distributeLine(double x1, double y1, double x2, double y2);
    Q_INVOKABLE void distributeArc(double cx, double cy, double radius,
                                   double startDegrees, double endDegrees);
    Q_INVOKABLE void distributeRectangle(double x, double y, double width, double height);
    Q_INVOKABLE QVariantMap formationDefaults(const QString &type, const QString &placementMode = {}) const;
    Q_INVOKABLE QVariantMap formationEstimate(const QString &type, const QVariantMap &options = {}) const;
    Q_INVOKABLE QVariantMap previewFormation(const QString &type, const QVariantMap &options = {},
                                              const QString &assignmentMode = QStringLiteral("rehearsalSafe"));
    Q_INVOKABLE void requestFormationPreview(const QString &type, const QVariantMap &options = {},
                                              const QString &assignmentMode = QStringLiteral("rehearsalSafe"));
    Q_INVOKABLE void requestFreehandPreview(const QVariantList &points,
                                            const QString &movementMode = QStringLiteral("rehearsalSafe"),
                                            bool createGroup = false,
                                            const QString &recognitionMode = QStringLiteral("auto"));
    Q_INVOKABLE bool commitFormationPreview();
    Q_INVOKABLE void cancelFormationPreview();
    Q_INVOKABLE void createFormation(const QString &type, const QVariantMap &options = {});
    Q_INVOKABLE void createFreehandFormation(const QVariantList &points,
                                              const QString &movementMode = QStringLiteral("balanced"),
                                              bool createGroup = false,
                                              const QString &recognitionMode = QStringLiteral("auto"));
    Q_INVOKABLE QString formatDistance(double steps, int precision = 1) const;
    Q_INVOKABLE void mirrorSelected(bool horizontal);
    Q_INVOKABLE void snapSelected(double grid);
    Q_INVOKABLE void faceSelected(double degrees);
    Q_INVOKABLE void setSelectedTransitionPath(const QString &type, const QVariantList &controlPoints = {});
    Q_INVOKABLE QVariantList transitionPathSamples(int performerRow, int samples = 24) const;
    Q_INVOKABLE QVariantMap shapeInfo(int index) const;
    Q_INVOKABLE void removeShape(int index, bool bakePlacements = true);
    Q_INVOKABLE void copyShapeToAdjacent(int index, int direction);
    Q_INVOKABLE QVariantList analyzeTransition(int destinationSet = -1);
    Q_INVOKABLE QVariantList scanShow();
    Q_INVOKABLE QVariantMap clinicIssue(int index) const;
    Q_INVOKABLE bool previewSuggestion(const QString &suggestionId);
    Q_INVOKABLE bool acceptSuggestion(const QString &suggestionId);
    Q_INVOKABLE void dismissIssue(const QString &issueId);
    Q_INVOKABLE bool selectClinicIssue(const QString &issueId);
    Q_INVOKABLE QVariantList suggestNextSet();

    Q_INVOKABLE QVariantMap setInfo(int index) const;
    Q_INVOKABLE QVariantMap variantInfo(int index) const;
    Q_INVOKABLE QVariantMap archivedSetInfo(int index) const;
    Q_INVOKABLE QVariantMap archivedVariantInfo(int index) const;
    Q_INVOKABLE QVariantMap performerInfo(int row) const;
    Q_INVOKABLE QString coordinateFor(int row, int setIndex = -1) const;
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

signals:
    void projectChanged();
    void currentSetChanged();
    void setsChanged();
    void playheadChanged();
    void playbackActiveChanged();
    void dirtyChanged();
    void statusMessageChanged();
    void selectionChanged();
    void statisticsChanged();
    void historyChanged();
    void performerCountChanged();
    void timingChanged();
    void shapesChanged();
    void editorSettingsChanged();
    void sceneChanged();
    void propsChanged();
    void musicChanged();
    void waveformChanged();
    void setRangeChanged();
    void transportSettingsChanged();
    void formationPreviewChanged();
    void clinicChanged();

private:
    friend class ProjectStateCommand;
    friend class TransportController;
    QJsonObject toJson() const;
    bool restoreJson(const QJsonObject &object, bool preservePath = true);
    void commitSnapshot(const QJsonObject &before, const QString &text);
    void markDirty(const QString &message = {});
    void setStatus(const QString &message);
    void autosave();
    void emitAllDataChanged();
    QString localPath(const QString &urlOrPath) const;
    MarchCraft::Placement placementAt(int performerIndex, int setIndex) const;
    QPointF interpolatedPosition(int performerIndex) const;
    double interpolatedFacing(int performerIndex) const;
    MarchCraft::AnimationState animationStateAt(int performerIndex) const;
    const MarchCraft::AnimationState &cachedAnimationStateAt(int performerIndex) const;
    QPointF pathPosition(int performerIndex, int destinationSet, double progress) const;
    double pathDistance(int performerIndex, int destinationSet) const;
    double transitionDistance(int performerIndex, int destinationSet) const;
    double performerTotalDistance(int performerIndex) const;
    bool performerHasWarning(int performerIndex) const;
    void ensureAnalyticsCache() const;
    QPointF clampPosition(QPointF point) const;
    void ensurePlacements();
    int pulsesBetween(qint64 startTick, qint64 endTick) const;
    qint64 advancePulses(qint64 startTick, int pulses) const;
    double millisecondsBetween(qint64 startTick, qint64 endTick) const;
    void applyMusicDocument(MarchCraft::MusicDocument document, const QString &undoText);
    void rebuildTimingFromMusic();
    int musicMeasureAtTick(qint64 tick) const;
    void startWaveformDecode();
    static QColor sectionColor(const QString &section);
    QVector<int> assignedTargetIndices(const QVector<int> &performerRows,
                                       const QVector<QPointF> &targets,
                                       bool closed, const QString &mode) const;
    QVariantMap assignmentMetrics(const QVector<int> &performerRows,
                                  const QVector<QPointF> &targets,
                                  const QVector<int> &assignment) const;
    QVector<int> minimumCostAssignment(const QVector<QVector<double>> &costs) const;
    void invalidateClinic();
    QString issueLabelList(const QVector<int> &rows, int limit = 6) const;

    struct FormationPreviewState {
        bool active = false;
        QString type;
        QString mode;
        QVariantMap options;
        MarchCraft::FormationShape shape;
        QHash<QString, QPointF> placements;
        QHash<QString, QPointF> sourcePlacements;
        QVariantMap metrics;
    };

    QString m_showName{QStringLiteral("Untitled Show")};
    QString m_fieldPreset{QStringLiteral("hs")};
    QString m_audioSource;
    QString m_projectPath;
    QString m_statusMessage{QStringLiteral("Ready")};
    double m_bpm = 120.0;
    double m_playhead = 0.0;
    bool m_playbackActive = false;
    int m_currentSet = 0;
    int m_selectedSetStart = 0;
    int m_selectedSetEnd = 0;
    bool m_dirty = false;
    QVector<MarchCraft::Performer> m_performers;
    QVector<MarchCraft::DrillSet> m_sets;
    QVector<MarchCraft::DrillSet> m_archivedSets;
    mutable QVector<MarchCraft::AnimationState> m_animationStateCache;
    mutable QVector<quint64> m_animationStateCacheRevisions;
    quint64 m_animationStateRevision = 1;
    QVector<MarchCraft::MeterRegion> m_meterRegions{{}};
    QVector<MarchCraft::TempoRegion> m_tempoRegions{{}};
    MarchCraft::MusicDocument m_music;
    int m_musicSelectionStart = -1;
    int m_musicSelectionEnd = -1;
    QString m_playbackSource{QStringLiteral("midi")};
    double m_midiMasterVolume = 0.75;
    bool m_loopEnabled = false;
    double m_audioOffsetMs = 0.0;
    QVector<double> m_waveformPeaks;
    QAudioDecoder *m_audioDecoder = nullptr;
    QFutureWatcher<MarchCraft::MidiImportResult> *m_midiWatcher = nullptr;
    QUndoStack m_undo;
    QTimer m_autosaveTimer;
    QJsonObject m_moveBefore;
    QHash<QString, QPointF> m_moveStarts;
    QHash<QString, QVector<QPointF>> m_shapePointStarts;
    QHash<QString, QPointF> m_shapeAnchorStarts;
    QHash<QString, double> m_shapeRotationStarts;
    QHash<QString, QSizeF> m_shapeSizeStarts;
    QPointF m_rotatePivot;
    QString m_shapePlacementMode{QStringLiteral("selection")};
    bool m_showShapeGuides = false;
    bool m_showTransitionPaths = true;
    QString m_performerMarkerStyle{QStringLiteral("colored")};
    int m_performerMarkerSize = 14;
    QString m_markerGeometry{QStringLiteral("circle")};
    QString m_markerFillColor{QStringLiteral("section")};
    QString m_markerOutlineColor{QStringLiteral("#e7f5ed")};
    int m_markerOutlineWidth = 1;
    QString m_markerLabelMode{QStringLiteral("adaptive")};
    int m_markerLabelFontSize = 10;
    QString m_markerLabelColor{QStringLiteral("#f0f7f3")};
    bool m_markerFacingVisible = true;
    QString m_markerFacingColor{QStringLiteral("#f8fafc")};
    QString m_markerWarningColor{QStringLiteral("#fb7185")};
    bool m_showFieldGrid = false;
    double m_fieldGridInterval = 1.0;
    QString m_fieldGridColor{QStringLiteral("#7dd3fc")};
    double m_fieldGridOpacity = 0.18;
    QString m_measurementUnit{QStringLiteral("steps")};
    FormationPreviewState m_formationPreview;
    bool m_generatingFormationPreview = false;
    int m_formationPreviewSourceSet = -1;
    bool m_formationPreviewInsertsNext = false;
    bool m_formationPreviewBusy = false;
    quint64 m_formationPreviewGeneration = 0;
    bool m_backgroundWorkerClone = false;
    MarchCraft::CapabilityProfile m_capability;
    QVariantList m_clinicIssues;
    QSet<QString> m_dismissedClinicIssues;
    QVariantMap m_pendingSuggestion;
    QSet<QString> m_highlightedClinicProps;
    QString m_openingBehavior{QStringLiteral("move")};
    int m_openingCounts = 8;
    MarchCraft::VenueConfiguration m_venue;
    QVector<MarchCraft::PropInstance> m_props;
    mutable bool m_analyticsValid = false;
    mutable QVector<double> m_cachedTotalDistances;
    mutable QVector<double> m_cachedSetDistances;
    mutable QVector<bool> m_cachedWarnings;
    mutable double m_cachedEnsembleTotal = 0.0;
    mutable double m_cachedLongestMove = 0.0;
    mutable int m_cachedWarningCount = 0;
};
