import QtQuick
import QtQuick.Controls

Button {
    id: control
    implicitHeight: 30
    implicitWidth: Math.max(76, contentItem.implicitWidth + leftPadding + rightPadding)
    leftPadding: 11
    rightPadding: 11
    font.family: MarchCraftTheme.fontFamily
    font.pixelSize: 12
    opacity: 1
    scale: down && enabled ? MarchCraftTheme.pressScale : 1
    Behavior on scale { NumberAnimation { duration: MarchCraftTheme.motionFast; easing.type: Easing.OutCubic } }

    contentItem: Text {
        text: control.text
        font: control.font
        color: !control.enabled ? MarchCraftTheme.textDisabled
                                : control.highlighted ? "#ffffff"
                                : control.checked ? MarchCraftTheme.accentHover : MarchCraftTheme.textPrimary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: MarchCraftTheme.radiusSmall
        color: !control.enabled ? (control.flat ? "transparent" : "#131923")
             : control.highlighted ? (control.down ? MarchCraftTheme.accentPressed
                                                   : control.hovered ? MarchCraftTheme.accentHover : MarchCraftTheme.accent)
             : control.checked ? (control.down ? "#213b58" : MarchCraftTheme.selection)
             : control.flat ? (control.hovered ? MarchCraftTheme.surfaceHover : "transparent")
             : control.down ? MarchCraftTheme.input
             : control.hovered ? MarchCraftTheme.surfaceHover : MarchCraftTheme.surfaceRaised
        border.width: control.activeFocus ? 2 : control.flat ? 0 : 1
        border.color: !control.enabled ? "#222c38"
                    : control.activeFocus ? MarchCraftTheme.accentHover
                    : control.highlighted ? "transparent"
                    : control.checked ? MarchCraftTheme.accent : MarchCraftTheme.divider
        Behavior on color { ColorAnimation { duration: MarchCraftTheme.motionFast } }
        Behavior on border.color { ColorAnimation { duration: MarchCraftTheme.motionFast } }
    }
}
