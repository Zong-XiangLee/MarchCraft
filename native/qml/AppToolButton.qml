import QtQuick
import QtQuick.Controls

ToolButton {
    id: control
    implicitHeight: 32
    implicitWidth: Math.max(32, contentItem.implicitWidth + 14)
    font.family: MarchCraftTheme.fontFamily
    font.pixelSize: 12
    opacity: 1

    contentItem: Text {
        text: control.text
        font: control.font
        color: !control.enabled ? MarchCraftTheme.textDisabled : MarchCraftTheme.textPrimary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: MarchCraftTheme.radiusSmall
        color: !control.enabled ? "transparent"
             : control.checked || control.highlighted ? "#243653"
             : control.down ? "#111720"
             : control.hovered ? MarchCraftTheme.surfaceHover : "transparent"
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? MarchCraftTheme.accentHover
                    : control.checked || control.highlighted ? "#365987" : "transparent"
    }
}
