import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    modal: true
    dim: false
    closePolicy: Popup.CloseOnEscape

    onAboutToShow: {
        x = Math.max(8, (parent.width - width) / 2)
        y = Math.max(8, (parent.height - height) / 2)
    }

    header: Rectangle {
        implicitHeight: 48
        color: MarchCraftTheme.surfaceRaised
        Column {
            anchors.left: parent.left; anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            Label { text: dialog.title; color: MarchCraftTheme.textPrimary; font.bold: true }
            Label { text: "Drag this title bar to move"; color: MarchCraftTheme.textSecondary; font.pixelSize: 11 }
        }
        MouseArea {
            objectName: "dialogDragHandle"
            anchors.fill: parent
            cursorShape: Qt.SizeAllCursor
            preventStealing: true
            property point pressPosition
            property point dialogPosition
            onPressed: function(mouse) {
                pressPosition = mapToItem(dialog.parent, mouse.x, mouse.y)
                dialogPosition = Qt.point(dialog.x, dialog.y)
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                const point = mapToItem(dialog.parent, mouse.x, mouse.y)
                dialog.x = Math.max(8, Math.min(dialog.parent.width - dialog.width - 8,
                                               dialogPosition.x + point.x - pressPosition.x))
                dialog.y = Math.max(8, Math.min(dialog.parent.height - dialog.height - 8,
                                               dialogPosition.y + point.y - pressPosition.y))
            }
        }
    }
}
