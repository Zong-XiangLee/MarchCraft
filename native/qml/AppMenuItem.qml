import QtQuick
import QtQuick.Controls

MenuItem {
    id: control
    implicitHeight: 32
    implicitWidth: Math.max(240, contentItem.implicitWidth + 20)
    font.family: MarchCraftTheme.fontFamily
    font.pixelSize: 12
    opacity: 1

    indicator: Text {
        x: 10
        width: 16
        anchors.verticalCenter: parent.verticalCenter
        visible: control.checkable
        text: control.checked ? "✓" : ""
        color: MarchCraftTheme.accentHover
        font.family: control.font.family
        font.pixelSize: control.font.pixelSize
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    contentItem: Item {
        implicitWidth: label.contentWidth + shortcutLabel.contentWidth
                       + (shortcutLabel.visible ? 32 : 0) + (control.checkable ? 22 : 0) + 20
        implicitHeight: 32

        Text {
            id: label
            anchors.left: parent.left
            anchors.leftMargin: control.checkable ? 32 : 10
            anchors.right: shortcutLabel.visible ? shortcutLabel.left : parent.right
            anchors.rightMargin: shortcutLabel.visible ? 18 : 10
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            font: control.font
            color: control.enabled ? MarchCraftTheme.textPrimary : MarchCraftTheme.textDisabled
            elide: Text.ElideRight
        }
        Text {
            id: shortcutLabel
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: control.action && control.action.shortcut ? control.action.shortcut : ""
            visible: text.length > 0
            font: control.font
            color: control.enabled ? MarchCraftTheme.textSecondary : MarchCraftTheme.textDisabled
        }
    }

    background: Rectangle {
        radius: 4
        color: control.enabled && control.highlighted ? MarchCraftTheme.surfaceHover : "transparent"
    }
}
