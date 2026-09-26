import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property var drillProjectContext
    required property var performerDialogContext
    required property var bulkEditDialogContext
    required property var inspectorContext
    required property var windowContext

    color: MarchCraftTheme.canvas
    property var sectionOptions: ["All sections"]

    function focusSearch() {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    function refreshSections() {
        var values = ["All sections"]
        for (var i = 0; i < drillProjectContext.performerCount; ++i) {
            var section = String(drillProjectContext.performerInfo(i).section || "Unassigned")
            if (values.indexOf(section) < 0) values.push(section)
        }
        sectionOptions = values
        if (sectionFilter.currentIndex >= values.length) sectionFilter.currentIndex = 0
    }

    function activatePerformer(index, mode) {
        drillProjectContext.selectPerformerMode(index, mode || 0)
        windowContext.activePerformer = index
        inspectorContext.refresh()
    }

    Component.onCompleted: refreshSections()
    Connections {
        target: drillProjectContext
        function onPerformerCountChanged() { root.refreshSections() }
        function onProjectChanged() { root.refreshSections() }
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
                Label { text: "Roster"; color: MarchCraftTheme.textPrimary; font.pixelSize: 24; font.bold: true }
                Label {
                    text: "Build the ensemble once, then revisit labels, sections, instruments, and groups at any time."
                    color: MarchCraftTheme.textSecondary
                    font.pixelSize: 11
                }
            }
            Rectangle {
                radius: MarchCraftTheme.radiusLarge
                color: MarchCraftTheme.surfaceRaised
                border.color: MarchCraftTheme.divider
                implicitWidth: countSummary.implicitWidth + 28
                implicitHeight: 44
                Label {
                    id: countSummary
                    anchors.centerIn: parent
                    text: drillProjectContext.performerCount + " performers"
                    color: MarchCraftTheme.accentHover
                    font.bold: true
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            TextField {
                id: searchField
                Layout.fillWidth: true
                Layout.maximumWidth: 410
                placeholderText: "Search labels, names, instruments, or sections"
                placeholderTextColor: MarchCraftTheme.textMuted
                selectByMouse: true
            }
            ComboBox {
                id: sectionFilter
                Layout.preferredWidth: 180
                model: root.sectionOptions
            }
            AppButton {
                text: "+ Performer"
                highlighted: true
                onClicked: { performerDialogContext.editing = false; performerDialogContext.open() }
            }
            AppButton {
                text: "Edit"
                enabled: drillProjectContext.selectedCount === 1 && windowContext.activePerformer >= 0
                onClicked: { performerDialogContext.editing = true; performerDialogContext.open() }
            }
            AppButton {
                text: "Bulk edit…"
                enabled: drillProjectContext.selectedCount > 0
                onClicked: bulkEditDialogContext.open()
            }
            AppButton {
                text: "Group"
                enabled: drillProjectContext.canGroupSelection
                onClicked: drillProjectContext.groupSelected()
            }
            AppButton {
                text: "Remove"
                enabled: drillProjectContext.selectedCount > 0
                onClicked: removeDialog.open()
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal
            handle: Rectangle {
                implicitWidth: 6
                color: SplitHandle.pressed ? MarchCraftTheme.accent : SplitHandle.hovered ? MarchCraftTheme.dividerStrong : MarchCraftTheme.divider
            }

            Frame {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 520
                padding: 0
                background: Rectangle { color: MarchCraftTheme.panel; border.color: MarchCraftTheme.divider; radius: MarchCraftTheme.radiusLarge }
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0
                    Rectangle {
                        Layout.fillWidth: true
                        height: 34
                        color: MarchCraftTheme.panelHeader
                        radius: MarchCraftTheme.radiusLarge
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            Label { text: "LABEL"; color: MarchCraftTheme.textMuted; font.bold: true; font.pixelSize: 9; Layout.preferredWidth: 92 }
                            Label { text: "PERFORMER"; color: MarchCraftTheme.textMuted; font.bold: true; font.pixelSize: 9; Layout.fillWidth: true }
                            Label { text: "INSTRUMENT"; color: MarchCraftTheme.textMuted; font.bold: true; font.pixelSize: 9; Layout.preferredWidth: 150 }
                            Label { text: "SECTION"; color: MarchCraftTheme.textMuted; font.bold: true; font.pixelSize: 9; Layout.preferredWidth: 130 }
                            Label { text: "STATUS"; color: MarchCraftTheme.textMuted; font.bold: true; font.pixelSize: 9; Layout.preferredWidth: 78 }
                        }
                    }
                    ListView {
                        id: rosterList
                        property int selectionAnchor: -1
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: drillProjectContext
                        spacing: 0
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        visible: drillProjectContext.performerCount > 0
                        delegate: Rectangle {
                            id: performerRow
                            required property int index
                            required property string label
                            required property string performerName
                            required property string instrument
                            required property string section
                            required property color performerColor
                            required property bool isSelected
                            required property bool hasWarning
                            required property bool performerVisible
                            required property bool performerLocked
                            width: ListView.view.width
                            height: visible ? 43 : 0
                            visible: {
                                var query = searchField.text.toLowerCase()
                                var matchesQuery = query.length === 0 || (label + " " + performerName + " " + instrument + " " + section).toLowerCase().includes(query)
                                var matchesSection = sectionFilter.currentIndex === 0 || section === sectionFilter.currentText
                                return matchesQuery && matchesSection
                            }
                            color: isSelected ? MarchCraftTheme.selection : rowHover.hovered ? MarchCraftTheme.surfaceHover : index % 2 ? MarchCraftTheme.surface : MarchCraftTheme.panel
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 8
                                Rectangle { width: 6; height: 26; radius: 3; color: performerRow.performerColor }
                                Label { text: performerRow.label; font.bold: true; Layout.preferredWidth: 78; elide: Text.ElideRight }
                                Label { text: performerRow.performerName || "—"; color: performerRow.performerName ? MarchCraftTheme.textPrimary : MarchCraftTheme.textMuted; Layout.fillWidth: true; elide: Text.ElideRight }
                                Label { text: performerRow.instrument; color: MarchCraftTheme.textSecondary; Layout.preferredWidth: 150; elide: Text.ElideRight }
                                Label { text: performerRow.section; color: MarchCraftTheme.textSecondary; Layout.preferredWidth: 130; elide: Text.ElideRight }
                                RowLayout {
                                    Layout.preferredWidth: 78
                                    spacing: 5
                                    Label { visible: performerRow.hasWarning; text: "⚠"; color: MarchCraftTheme.danger }
                                    Label { visible: performerRow.performerLocked; text: "LOCK"; color: MarchCraftTheme.warning; font.pixelSize: 8; font.bold: true }
                                    Label { visible: !performerRow.performerVisible; text: "HIDDEN"; color: MarchCraftTheme.textMuted; font.pixelSize: 8; font.bold: true }
                                }
                            }
                            HoverHandler { id: rowHover }
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                                onClicked: function(mouse) {
                                    var ctrl = (mouse.modifiers & Qt.ControlModifier) !== 0
                                    var shift = (mouse.modifiers & Qt.ShiftModifier) !== 0
                                    if (shift && rosterList.selectionAnchor >= 0)
                                        drillProjectContext.selectPerformerRange(rosterList.selectionAnchor, performerRow.index, ctrl)
                                    else {
                                        root.activatePerformer(performerRow.index, ctrl ? 1 : 0)
                                        rosterList.selectionAnchor = performerRow.index
                                    }
                                }
                                onDoubleClicked: {
                                    root.activatePerformer(performerRow.index, 0)
                                    performerDialogContext.editing = true
                                    performerDialogContext.open()
                                }
                            }
                        }
                    }
                    ColumnLayout {
                        visible: drillProjectContext.performerCount === 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.leftMargin: 20
                        Layout.rightMargin: 20
                        spacing: 8
                        Item { Layout.fillHeight: true }
                        Label { text: "No performers yet"; color: MarchCraftTheme.textPrimary; font.pixelSize: 20; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                        Label { text: "Add one performer or build complete sections in the batch panel."; color: MarchCraftTheme.textSecondary; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true }
                        Item { Layout.fillHeight: true }
                    }
                }
            }

            Frame {
                SplitView.preferredWidth: 510
                SplitView.minimumWidth: 390
                padding: 16
                background: Rectangle { color: MarchCraftTheme.surface; border.color: MarchCraftTheme.divider; radius: MarchCraftTheme.radiusLarge }
                ScrollView {
                    anchors.fill: parent
                    contentWidth: availableWidth
                    ColumnLayout {
                        width: parent.width
                        spacing: 14
                        Label { text: "ADD COMPLETE SECTIONS"; color: MarchCraftTheme.textSecondary; font.bold: true; font.pixelSize: 10; font.letterSpacing: 1 }
                        Label {
                            text: "Preview every label before creating performers. Existing labels are preserved and duplicates are skipped safely."
                            color: MarchCraftTheme.textSecondary
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                        RosterBatchEditor {
                            id: batchEditor
                            Layout.fillWidth: true
                            actionText: drillProjectContext.performerCount === 0 ? "Create roster" : "Add sections"
                            onSubmitted: function(rows) { if (drillProjectContext.batchCreateRoster(rows)) root.refreshSections() }
                        }
                        Rectangle { Layout.fillWidth: true; height: 1; color: MarchCraftTheme.divider; Layout.topMargin: 6 }
                        Label { text: "SELECTION TOOLS"; color: MarchCraftTheme.textSecondary; font.bold: true; font.pixelSize: 10; font.letterSpacing: 1 }
                        RowLayout {
                            Layout.fillWidth: true
                            TextField { id: labelPrefix; text: "P"; maximumLength: 8; Layout.fillWidth: true; placeholderText: "Prefix" }
                            AppButton {
                                text: "Auto-label selected"
                                enabled: drillProjectContext.selectedCount > 0 && labelPrefix.text.trim().length > 0
                                onClicked: drillProjectContext.autoLabel(labelPrefix.text.trim().toUpperCase())
                            }
                        }
                        Label {
                            text: drillProjectContext.selectedCount === 0 ? "Select roster rows to edit, group, relabel, or remove them."
                                  : drillProjectContext.selectedCount + " selected · actions apply across the full project roster."
                            color: MarchCraftTheme.textMuted
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: removeDialog
        title: "Remove selected performers?"
        modal: true
        width: 440
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.NoButton
        contentItem: ColumnLayout {
            Label {
                text: "This removes " + drillProjectContext.selectedCount + " performer" + (drillProjectContext.selectedCount === 1 ? "" : "s") + " from every set. You can undo the change."
                color: MarchCraftTheme.textSecondary
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AppButton { text: "Cancel"; onClicked: removeDialog.close() }
                AppButton { text: "Remove"; highlighted: true; onClicked: { drillProjectContext.removeSelectedPerformers(); removeDialog.close() } }
            }
        }
    }
}
