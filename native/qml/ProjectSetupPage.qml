import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    signal cancelled()
    signal createRequested(string name, string fieldPreset, string lightingPreset)
    color: MarchCraftTheme.surface
    radius: MarchCraftTheme.radiusLarge
    border.color: MarchCraftTheme.divider
    property alias projectName: nameField.text
    focus: visible
    onVisibleChanged: if (visible) { nameField.text = "Untitled Show"; nameField.forceActiveFocus() }
    Component.onCompleted: nameField.text = "Untitled Show"

    RowLayout {
        anchors.fill: parent; anchors.margins: 32; spacing: 44
        ColumnLayout {
            Layout.preferredWidth: 300; Layout.alignment: Qt.AlignTop; spacing: 10
            Label { text: "NEW PROJECT"; font.pixelSize: 20; font.bold: true; font.letterSpacing: 1; color: MarchCraftTheme.textPrimary }
            Label { text: "A clean workspace with the essentials ready. You can change these settings later."; Layout.fillWidth: true; wrapMode: Text.Wrap; color: MarchCraftTheme.textSecondary; lineHeight: 1.2 }
            Item { Layout.fillHeight: true }
            AppButton { text: "Back"; flat: true; onClicked: root.cancelled() }
        }
        Rectangle { Layout.fillHeight: true; width: 1; color: MarchCraftTheme.divider }
        GridLayout {
            Layout.fillWidth: true; Layout.alignment: Qt.AlignVCenter; columns: 1; rowSpacing: 8
            Label { text: "PROJECT NAME"; color: MarchCraftTheme.textMuted; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1 }
            TextField {
                id: nameField; Layout.fillWidth: true; placeholderText: "Untitled Show"; selectByMouse: true
                color: MarchCraftTheme.textPrimary; placeholderTextColor: MarchCraftTheme.textMuted
                selectionColor: MarchCraftTheme.accent; selectedTextColor: "white"
                background: Rectangle { color: MarchCraftTheme.input; radius: MarchCraftTheme.radiusSmall; border.width: nameField.activeFocus ? 2 : 1; border.color: nameField.activeFocus ? MarchCraftTheme.accentHover : MarchCraftTheme.divider }
            }
            Label { text: "FIELD"; color: MarchCraftTheme.textMuted; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1; Layout.topMargin: 8 }
            ComboBox { id: field; Layout.fillWidth: true; textRole: "text"; valueRole: "value"; model: [{text:"High School",value:"hs"},{text:"College",value:"college"},{text:"Professional",value:"nfl"},{text:"Indoor",value:"indoor"}] }
            Label { text: "ENVIRONMENT"; color: MarchCraftTheme.textMuted; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1; Layout.topMargin: 8 }
            ComboBox { id: lighting; Layout.fillWidth: true; textRole: "text"; valueRole: "value"; model: [{text:"Daylight",value:"lighting.daylight"},{text:"Overcast",value:"lighting.overcast"},{text:"Sunset",value:"lighting.sunset"},{text:"Night game",value:"lighting.night"},{text:"Indoor",value:"lighting.indoor"}] }
            AppButton { text: "Create project"; highlighted: true; enabled: nameField.text.trim().length > 0; Layout.fillWidth: true; Layout.topMargin: 18; onClicked: root.createRequested(nameField.text.trim(), field.currentValue, lighting.currentValue) }
        }
    }
}
