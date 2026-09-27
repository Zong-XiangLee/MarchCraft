import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 8

    property bool showAction: true
    property string actionText: "Add roster sections"
    property int totalPerformers: 0
    signal submitted(var rows)

    function recalculate() {
        var total = 0
        for (var i = 0; i < sectionRows.count; ++i)
            total += Number(sectionRows.get(i).count || 0)
        totalPerformers = total
    }

    function rowsData() {
        var rows = []
        for (var i = 0; i < sectionRows.count; ++i) {
            var row = sectionRows.get(i)
            rows.push({
                section: row.section,
                prefix: row.prefix,
                count: row.count,
                instrument: row.instrument
            })
        }
        return rows
    }

    function resetDefaults() {
        sectionRows.clear()
        sectionRows.append({section: "Trumpets", prefix: "T", count: 24, instrument: "Trumpet"})
        sectionRows.append({section: "Mellophones", prefix: "M", count: 12, instrument: "Mellophone"})
        sectionRows.append({section: "Trombones", prefix: "B", count: 16, instrument: "Trombone"})
        sectionRows.append({section: "Euphoniums", prefix: "E", count: 10, instrument: "Euphonium"})
        sectionRows.append({section: "Tubas", prefix: "U", count: 12, instrument: "Tuba"})
        recalculate()
    }

    function addEmptyRow() {
        sectionRows.append({section: "New section", prefix: "P", count: 8, instrument: "Unassigned"})
        recalculate()
    }

    function preview(prefix, count) {
        var clean = String(prefix || "P").trim().toUpperCase() || "P"
        var amount = Math.max(0, Number(count || 0))
        return amount > 1 ? clean + "1–" + clean + amount : amount === 1 ? clean + "1" : "—"
    }

    ListModel { id: sectionRows }

    Component.onCompleted: resetDefaults()

    RowLayout {
        Layout.fillWidth: true
        Label {
            text: "SECTION PLAN"
            color: MarchCraftTheme.textSecondary
            font.bold: true
            font.pixelSize: 10
            font.letterSpacing: 1
        }
        Item { Layout.fillWidth: true }
        Label {
            text: root.totalPerformers + " performers"
            color: MarchCraftTheme.accentHover
            font.bold: true
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 30
        color: MarchCraftTheme.panelHeader
        radius: MarchCraftTheme.radiusSmall
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 42
            spacing: 8
            Label { text: "SECTION"; color: MarchCraftTheme.textMuted; font.bold: true; font.pixelSize: 9; Layout.preferredWidth: 130 }
            Label { text: "PREFIX"; color: MarchCraftTheme.textMuted; font.bold: true; font.pixelSize: 9; Layout.preferredWidth: 64 }
            Label { text: "COUNT"; color: MarchCraftTheme.textMuted; font.bold: true; font.pixelSize: 9; Layout.preferredWidth: 80 }
            Label {
                text: "INSTRUMENT"
                color: MarchCraftTheme.textMuted
                font.bold: true
                font.pixelSize: 9
                Layout.fillWidth: true
                Layout.minimumWidth: 72
                elide: Text.ElideRight
                clip: true
            }
            Label {
                text: "LABELS"
                color: MarchCraftTheme.textMuted
                font.bold: true
                font.pixelSize: 9
                Layout.preferredWidth: 90
                elide: Text.ElideRight
                clip: true
            }
        }
    }

    ListView {
        id: rowsView
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(246, Math.max(52, sectionRows.count * 46))
        clip: true
        spacing: 4
        model: sectionRows
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        delegate: Rectangle {
            id: rowDelegate
            required property int index
            required property string section
            required property string prefix
            required property int count
            required property string instrument
            width: ListView.view.width
            height: 42
            radius: MarchCraftTheme.radiusSmall
            color: index % 2 ? MarchCraftTheme.surfaceRaised : MarchCraftTheme.panel
            border.color: MarchCraftTheme.divider
            RowLayout {
                anchors.fill: parent
                anchors.margins: 4
                spacing: 8
                TextField {
                    Layout.preferredWidth: 130
                    text: rowDelegate.section
                    selectByMouse: true
                    onEditingFinished: sectionRows.setProperty(rowDelegate.index, "section", text.trim())
                    Accessible.name: "Section name"
                }
                TextField {
                    Layout.preferredWidth: 64
                    text: rowDelegate.prefix
                    maximumLength: 8
                    horizontalAlignment: Text.AlignHCenter
                    selectByMouse: true
                    onEditingFinished: sectionRows.setProperty(rowDelegate.index, "prefix", text.trim().toUpperCase())
                    Accessible.name: "Label prefix"
                }
                SpinBox {
                    Layout.preferredWidth: 80
                    from: 1
                    to: 500
                    editable: true
                    value: rowDelegate.count
                    onValueModified: {
                        sectionRows.setProperty(rowDelegate.index, "count", value)
                        root.recalculate()
                    }
                    Accessible.name: "Performer count"
                }
                TextField {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 96
                    text: rowDelegate.instrument
                    selectByMouse: true
                    onEditingFinished: sectionRows.setProperty(rowDelegate.index, "instrument", text.trim())
                    Accessible.name: "Instrument"
                }
                Label {
                    Layout.preferredWidth: 90
                    text: root.preview(rowDelegate.prefix, rowDelegate.count)
                    color: MarchCraftTheme.textSecondary
                    font.family: "Consolas"
                    elide: Text.ElideRight
                }
                AppToolButton {
                    text: "×"
                    enabled: sectionRows.count > 1
                    ToolTip.text: "Remove section row"
                    ToolTip.visible: hovered
                    onClicked: {
                        sectionRows.remove(rowDelegate.index)
                        root.recalculate()
                    }
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        AppButton { text: "+ Add section"; flat: true; onClicked: root.addEmptyRow() }
        Label {
            Layout.fillWidth: true
            text: "Labels are previewed before any performer is created."
            color: MarchCraftTheme.textMuted
            font.pixelSize: 10
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
        }
        AppButton {
            visible: root.showAction
            text: root.actionText
            highlighted: true
            enabled: root.totalPerformers > 0
            onClicked: root.submitted(root.rowsData())
        }
    }
}
