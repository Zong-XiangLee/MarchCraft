import QtQuick
import QtQuick.Controls

MenuItem {
    id: control
    implicitHeight: 32
    implicitWidth: Math.max(190, contentItem.implicitWidth + 28)
    font.family: MarchCraftTheme.fontFamily
    font.pixelSize: 12
    opacity: 1

    contentItem: Text {
        leftPadding: 10
        rightPadding: 10
        text: control.text
        font: control.font
        color: control.enabled ? MarchCraftTheme.textPrimary : MarchCraftTheme.textDisabled
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 4
        color: control.enabled && control.highlighted ? MarchCraftTheme.surfaceHover : "transparent"
    }
}
