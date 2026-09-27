import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    modal: true
    dim: false
    closePolicy: Popup.CloseOnEscape
    property bool showCloseButton: true
    property string headerHint: "Drag to reposition"

    onAboutToShow: {
        x = Math.max(8, (parent.width - width) / 2)
        y = Math.max(8, (parent.height - height) / 2)
    }

    header: Rectangle {
        implicitHeight: 48
        color: MarchCraftTheme.surfaceRaised
        MouseArea {
            objectName: "dialogDragHandle"
            anchors.fill: parent
            anchors.rightMargin: dialog.showCloseButton ? 46 : 0
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
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 8
            ColumnLayout {
                spacing: 0
                Layout.fillWidth: true
                Label { text: dialog.title; color: MarchCraftTheme.textPrimary; font.bold: true }
                Label { text: dialog.headerHint; color: MarchCraftTheme.textSecondary; font.pixelSize: 11 }
            }
            AppToolButton {
                objectName: "dialogCloseButton"
                visible: dialog.showCloseButton
                text: "×"
                font.pixelSize: 19
                ToolTip.visible: hovered
                ToolTip.text: "Close"
                onClicked: dialog.close()
            }
        }
    }
}
