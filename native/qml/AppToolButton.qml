import QtQuick
import QtQuick.Controls

ToolButton {
    id: control
    implicitHeight: 28
    implicitWidth: Math.max(28, contentItem.implicitWidth + 12)
    font.family: MarchCraftTheme.fontFamily
    font.pixelSize: 12
    opacity: 1
    scale: down && enabled ? MarchCraftTheme.pressScale : 1
    Behavior on scale { NumberAnimation { duration: MarchCraftTheme.motionFast; easing.type: Easing.OutCubic } }

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
             : control.checked || control.highlighted ? MarchCraftTheme.selection
             : control.down ? MarchCraftTheme.input
             : control.hovered ? MarchCraftTheme.surfaceHover : "transparent"
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? MarchCraftTheme.accentHover
                    : control.checked || control.highlighted ? MarchCraftTheme.accent : "transparent"
        Behavior on color { ColorAnimation { duration: MarchCraftTheme.motionFast } }
        Behavior on border.color { ColorAnimation { duration: MarchCraftTheme.motionFast } }
    }
}
