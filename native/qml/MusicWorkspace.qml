import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property var drillProjectContext
    required property var transportContext
    signal midiImportRequested()
    signal musicXmlImportRequested()
    signal audioImportRequested()
    signal advancedToolRequested(string tool)
    signal editorRequested()

    color: MarchCraftTheme.canvas
    property int selectionAnchor: -1

    function formatTime(milliseconds) {
        var seconds = Math.max(0, Math.floor(Number(milliseconds || 0) / 1000))
        var minutes = Math.floor(seconds / 60)
        return minutes + ":" + String(seconds % 60).padStart(2, "0")
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label { text: "Music"; color: MarchCraftTheme.textPrimary; font.pixelSize: 24; font.bold: true }
                Label {
                    text: "Prepare score structure and rehearsal audio, or skip this workspace and write drill immediately."
                    color: MarchCraftTheme.textSecondary
                    font.pixelSize: 11
                }
            }
            AppButton { text: "Import MIDI…"; onClicked: root.midiImportRequested() }
            AppButton { text: "Import MusicXML…"; onClicked: root.musicXmlImportRequested() }
            AppButton { text: drillProjectContext.audioSource ? "Replace audio…" : "Attach audio…"; onClicked: root.audioImportRequested() }
            AppButton { text: "Continue to Editor"; highlighted: true; onClicked: root.editorRequested() }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle {
                Layout.fillWidth: true; height: 52; radius: MarchCraftTheme.radiusLarge
                color: MarchCraftTheme.surface; border.color: MarchCraftTheme.divider
                ColumnLayout { anchors.centerIn: parent; spacing: 0
                    Label { text: drillProjectContext.musicLoaded ? drillProjectContext.musicMeasureCount : "—"; color: MarchCraftTheme.textPrimary; font.bold: true; font.pixelSize: 17; Layout.alignment: Qt.AlignHCenter }
                    Label { text: "MEASURES"; color: MarchCraftTheme.textMuted; font.pixelSize: 8; font.bold: true; font.letterSpacing: 1; Layout.alignment: Qt.AlignHCenter }
                }
            }
            Rectangle {
                Layout.fillWidth: true; height: 52; radius: MarchCraftTheme.radiusLarge
                color: MarchCraftTheme.surface; border.color: MarchCraftTheme.divider
                ColumnLayout { anchors.centerIn: parent; spacing: 0
                    Label { text: drillProjectContext.musicLoaded ? drillProjectContext.musicTrackCount : "—"; color: MarchCraftTheme.textPrimary; font.bold: true; font.pixelSize: 17; Layout.alignment: Qt.AlignHCenter }
                    Label { text: "TRACKS"; color: MarchCraftTheme.textMuted; font.pixelSize: 8; font.bold: true; font.letterSpacing: 1; Layout.alignment: Qt.AlignHCenter }
                }
            }
            Rectangle {
                Layout.fillWidth: true; height: 52; radius: MarchCraftTheme.radiusLarge
                color: MarchCraftTheme.surface; border.color: MarchCraftTheme.divider
                ColumnLayout { anchors.centerIn: parent; spacing: 0
                    Label { text: drillProjectContext.musicLoaded ? root.formatTime(drillProjectContext.musicDurationMs) : "—"; color: MarchCraftTheme.textPrimary; font.bold: true; font.pixelSize: 17; Layout.alignment: Qt.AlignHCenter }
                    Label { text: "SCORE LENGTH"; color: MarchCraftTheme.textMuted; font.pixelSize: 8; font.bold: true; font.letterSpacing: 1; Layout.alignment: Qt.AlignHCenter }
                }
            }
            Rectangle {
                Layout.fillWidth: true; height: 52; radius: MarchCraftTheme.radiusLarge
                color: MarchCraftTheme.surface; border.color: drillProjectContext.audioSource ? MarchCraftTheme.success : MarchCraftTheme.divider
                ColumnLayout { anchors.centerIn: parent; spacing: 0
                    Label { text: drillProjectContext.audioSource ? "ATTACHED" : "OPTIONAL"; color: drillProjectContext.audioSource ? MarchCraftTheme.success : MarchCraftTheme.textPrimary; font.bold: true; font.pixelSize: 12; Layout.alignment: Qt.AlignHCenter }
                    Label { text: "REHEARSAL AUDIO"; color: MarchCraftTheme.textMuted; font.pixelSize: 8; font.bold: true; font.letterSpacing: 1; Layout.alignment: Qt.AlignHCenter }
                }
            }
        }

        Rectangle {
            visible: !drillProjectContext.musicLoaded
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: MarchCraftTheme.radiusLarge
            color: MarchCraftTheme.panel
            border.color: MarchCraftTheme.divider
            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(820, parent.width - 48)
                spacing: 14
                Label { text: "Music is optional"; color: MarchCraftTheme.textPrimary; font.pixelSize: 24; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                Label {
                    text: drillProjectContext.audioSource
                          ? "Your rehearsal audio is attached. Add a score for measure-aware planning, or continue without one."
                          : "Import a score for measure-aware timing and planning, attach rehearsal audio only, or continue straight to the field."
                    color: MarchCraftTheme.textSecondary
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    AppButton { text: "Import MIDI"; highlighted: true; Layout.fillWidth: true; Layout.preferredHeight: 48; onClicked: root.midiImportRequested() }
                    AppButton { text: "Import MusicXML"; Layout.fillWidth: true; Layout.preferredHeight: 48; onClicked: root.musicXmlImportRequested() }
                    AppButton { text: drillProjectContext.audioSource ? "Replace audio" : "Audio only"; Layout.fillWidth: true; Layout.preferredHeight: 48; onClicked: root.audioImportRequested() }
                }
                RowLayout {
                    visible: drillProjectContext.audioSource
                    Layout.alignment: Qt.AlignHCenter
                    AppButton { text: "Timing and sync…"; onClicked: root.advancedToolRequested("timing") }
                    AppButton { text: "Audio offset…"; onClicked: root.advancedToolRequested("offset") }
                }
                AppButton { text: "Skip music and open Editor"; flat: true; Layout.alignment: Qt.AlignHCenter; onClicked: root.editorRequested() }
            }
        }

        SplitView {
            visible: drillProjectContext.musicLoaded
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal
            handle: Rectangle { implicitWidth: 6; color: SplitHandle.pressed ? MarchCraftTheme.accent : SplitHandle.hovered ? MarchCraftTheme.dividerStrong : MarchCraftTheme.divider }

            Frame {
                SplitView.preferredWidth: 300
                SplitView.minimumWidth: 250
                padding: 0
                background: Rectangle { color: MarchCraftTheme.panel; border.color: MarchCraftTheme.divider; radius: MarchCraftTheme.radiusLarge }
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0
                    RowLayout {
                        Layout.fillWidth: true; Layout.margins: 10
                        Label { text: "MEASURES"; color: MarchCraftTheme.textSecondary; font.bold: true; font.pixelSize: 10; font.letterSpacing: 1; Layout.fillWidth: true }
                        Label { text: "Shift-click to select a range"; color: MarchCraftTheme.textMuted; font.pixelSize: 9 }
                    }
                    ListView {
                        id: measureList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: drillProjectContext.musicMeasureCount
                        spacing: 2
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        delegate: Rectangle {
                            id: measureRow
                            required property int index
                            property var info: drillProjectContext.musicMeasureInfo(index)
                            width: ListView.view.width
                            height: 44
                            color: info.selected ? MarchCraftTheme.selection : measureHover.hovered ? MarchCraftTheme.surfaceHover : index % 2 ? MarchCraftTheme.surface : MarchCraftTheme.panel
                            border.color: info.selected ? MarchCraftTheme.accent : "transparent"
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10; spacing: 8
                                Rectangle {
                                    width: 5; height: 26; radius: 2
                                    color: info.sections && info.sections.length > 0 ? info.sections[0].color : MarchCraftTheme.dividerStrong
                                }
                                Label { text: "m" + info.number; color: MarchCraftTheme.textPrimary; font.bold: true; Layout.preferredWidth: 54 }
                                ColumnLayout {
                                    Layout.fillWidth: true; spacing: 0
                                    Label { text: info.numerator + "/" + info.denominator + "  ·  " + Number(info.tempo || 0).toFixed(0) + " BPM"; color: MarchCraftTheme.textSecondary; font.pixelSize: 10 }
                                    Label { text: info.noteCount + " notes" + (info.partial ? " · pickup" : ""); color: MarchCraftTheme.textMuted; font.pixelSize: 9 }
                                }
                                Label { visible: info.setIndex >= 0; text: "SET " + (info.setIndex + 1); color: MarchCraftTheme.accentHover; font.bold: true; font.pixelSize: 8 }
                            }
                            HoverHandler { id: measureHover }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: function(mouse) {
                                    if ((mouse.modifiers & Qt.ShiftModifier) !== 0 && root.selectionAnchor >= 0)
                                        drillProjectContext.setMusicSelection(root.selectionAnchor, measureRow.index)
                                    else {
                                        root.selectionAnchor = measureRow.index
                                        drillProjectContext.setMusicSelection(measureRow.index, measureRow.index)
                                    }
                                }
                                onDoubleClicked: transportContext.seekTick(measureRow.info.startTick)
                            }
                        }
                    }
                }
            }

            Frame {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 420
                padding: 14
                background: Rectangle { color: MarchCraftTheme.surface; border.color: MarchCraftTheme.divider; radius: MarchCraftTheme.radiusLarge }
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 1
                            Label { text: "SCORE STRUCTURE"; color: MarchCraftTheme.textSecondary; font.bold: true; font.pixelSize: 10; font.letterSpacing: 1 }
                            Label {
                                text: drillProjectContext.musicSelectionStart >= 0
                                      ? "Selected measures " + (Math.min(drillProjectContext.musicSelectionStart, drillProjectContext.musicSelectionEnd) + 1) + "–" + (Math.max(drillProjectContext.musicSelectionStart, drillProjectContext.musicSelectionEnd) + 1)
                                      : "Choose measures to group or use for authoring tools."
                                color: MarchCraftTheme.textMuted; font.pixelSize: 10
                            }
                        }
                        AppButton { text: "Group selection…"; enabled: drillProjectContext.musicSelectionStart >= 0; onClicked: root.advancedToolRequested("group") }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 86
                        radius: MarchCraftTheme.radiusLarge
                        color: MarchCraftTheme.panel
                        border.color: MarchCraftTheme.divider
                        ListView {
                            anchors.fill: parent; anchors.margins: 8
                            orientation: ListView.Horizontal
                            spacing: 6
                            clip: true
                            model: drillProjectContext.musicSections
                            delegate: Rectangle {
                                required property var modelData
                                width: 160; height: 66; radius: MarchCraftTheme.radiusSmall
                                color: MarchCraftTheme.surfaceRaised; border.color: modelData.color
                                ColumnLayout { anchors.fill: parent; anchors.margins: 8; spacing: 2
                                    Label { text: modelData.name; color: MarchCraftTheme.textPrimary; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                                    Label { text: modelData.type.toUpperCase(); color: modelData.color; font.bold: true; font.pixelSize: 8 }
                                    Label { text: "m" + modelData.startNumber + "–" + modelData.endNumber; color: MarchCraftTheme.textMuted; font.pixelSize: 10 }
                                }
                            }
                            Label { visible: drillProjectContext.musicSections.length === 0; anchors.centerIn: parent; text: "No authored sections yet"; color: MarchCraftTheme.textMuted }
                        }
                    }

                    Label { text: "AUTHORING TOOLS"; color: MarchCraftTheme.textSecondary; font.bold: true; font.pixelSize: 10; font.letterSpacing: 1; Layout.topMargin: 4 }
                    GridLayout {
                        columns: 2
                        Layout.fillWidth: true
                        AppButton { text: "Music sections…"; Layout.fillWidth: true; onClicked: root.advancedToolRequested("sections") }
                        AppButton { text: "Timeline markers…"; Layout.fillWidth: true; onClicked: root.advancedToolRequested("markers") }
                        AppButton { text: "Timing and sync…"; Layout.fillWidth: true; onClicked: root.advancedToolRequested("timing") }
                        AppButton { text: "Audio offset…"; Layout.fillWidth: true; onClicked: root.advancedToolRequested("offset") }
                        AppButton { text: "Map sets to music…"; Layout.fillWidth: true; onClicked: root.advancedToolRequested("mapping") }
                        AppButton { text: "Generate sets from selection…"; Layout.fillWidth: true; enabled: drillProjectContext.musicSelectionStart >= 0; onClicked: root.advancedToolRequested("generation") }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: MarchCraftTheme.divider }
                    RowLayout {
                        Layout.fillWidth: true
                        AppButton { text: "Analyze for set planning…"; onClicked: root.advancedToolRequested("planning") }
                        AppButton { text: "Import details…"; onClicked: root.advancedToolRequested("diagnostics") }
                        Item { Layout.fillWidth: true }
                        Label { text: drillProjectContext.musicSourceType.toUpperCase(); color: MarchCraftTheme.accentHover; font.bold: true; font.pixelSize: 9 }
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            Frame {
                SplitView.preferredWidth: 330
                SplitView.minimumWidth: 280
                padding: 0
                background: Rectangle { color: MarchCraftTheme.panel; border.color: MarchCraftTheme.divider; radius: MarchCraftTheme.radiusLarge }
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0
                    RowLayout {
                        Layout.fillWidth: true; Layout.margins: 10
                        Label { text: "TRACK MIXER"; color: MarchCraftTheme.textSecondary; font.bold: true; font.pixelSize: 10; font.letterSpacing: 1; Layout.fillWidth: true }
                        Label { text: drillProjectContext.musicTrackCount; color: MarchCraftTheme.textMuted }
                    }
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: drillProjectContext.musicTrackCount
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        delegate: Rectangle {
                            id: trackRow
                            required property int index
                            property var info: drillProjectContext.musicTrackInfo(index)
                            width: ListView.view.width; height: 62
                            color: index % 2 ? MarchCraftTheme.surface : MarchCraftTheme.panel
                            ColumnLayout {
                                anchors.fill: parent; anchors.margins: 7; spacing: 3
                                RowLayout {
                                    Layout.fillWidth: true
                                    CheckBox { checked: trackRow.info.selected; onClicked: drillProjectContext.setMusicTrackSelected(trackRow.index, checked) }
                                    Label { text: trackRow.info.name; color: MarchCraftTheme.textPrimary; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                                    AppToolButton { text: "M"; checkable: true; checked: trackRow.info.muted; ToolTip.text: "Mute"; ToolTip.visible: hovered; onClicked: drillProjectContext.setMusicTrackMuted(trackRow.index, checked) }
                                    AppToolButton { text: "S"; checkable: true; checked: trackRow.info.solo; ToolTip.text: "Solo"; ToolTip.visible: hovered; onClicked: drillProjectContext.setMusicTrackSolo(trackRow.index, checked) }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { text: trackRow.info.noteCount + " notes"; color: MarchCraftTheme.textMuted; font.pixelSize: 9; Layout.preferredWidth: 68 }
                                    Slider { Layout.fillWidth: true; from: 0; to: 1; value: trackRow.info.volume; onMoved: drillProjectContext.setMusicTrackVolume(trackRow.index, value) }
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 52
            radius: MarchCraftTheme.radiusLarge
            color: MarchCraftTheme.panelHeader
            border.color: MarchCraftTheme.divider
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 8
                AppToolButton { text: "|◀"; ToolTip.text: "First set"; ToolTip.visible: hovered; onClicked: transportContext.firstSet() }
                AppToolButton { text: transportContext.playing ? "❚❚" : "▶"; ToolTip.text: "Play / pause"; ToolTip.visible: hovered; onClicked: transportContext.playPause() }
                AppToolButton { text: "■"; ToolTip.text: "Stop"; ToolTip.visible: hovered; onClicked: transportContext.stop() }
                AppToolButton { text: "▶|"; ToolTip.text: "Next set"; ToolTip.visible: hovered; onClicked: transportContext.nextSet() }
                Label { text: root.formatTime(transportContext.currentMs) + " / " + root.formatTime(transportContext.durationMs); color: MarchCraftTheme.accentHover; font.family: "Consolas"; Layout.preferredWidth: 100 }
                Slider {
                    Layout.fillWidth: true
                    from: 0; to: Math.max(1, transportContext.durationMs)
                    value: transportContext.currentMs
                    onMoved: transportContext.seekMs(value)
                }
                ComboBox {
                    Layout.preferredWidth: 150
                    model: ["MIDI synth", "Rehearsal audio", "Mute"]
                    currentIndex: drillProjectContext.playbackSource === "rehearsal" ? 1 : drillProjectContext.playbackSource === "mute" ? 2 : 0
                    onActivated: drillProjectContext.playbackSource = currentIndex === 1 ? "rehearsal" : currentIndex === 2 ? "mute" : "midi"
                }
            }
        }
    }
}
