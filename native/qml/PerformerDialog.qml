import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia
import QtCore

Dialog {
    // Explicit dependencies supplied by the application shell.
    required property var assetCatalogContext
    required property var drillProjectContext
    required property var inspectorContext
    required property var windowContext

    id: performerDialog
    property bool editing: false
    title: editing ? "Edit performer" : "Add performer"
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 420
    onOpened: {
        const p = editing ? drillProjectContext.performerInfo(windowContext.activePerformer) : ({})
        performerLabel.text = p.label || ""
        performerName.text = p.name || ""
        performerInstrument.currentIndex = Math.max(0, performerInstrument.find(p.instrument || "Trumpet"))
        performerSection.text = p.section || "Winds"
        performerNotes.text = p.notes || ""
        performerAsset.currentIndex = Math.max(0, performerAsset.indexOfValue(p.instrumentAssetId || "instrument.generic"))
    }
    contentItem: ColumnLayout {
        Label { text: "Label" }
        TextField { placeholderTextColor: MarchCraftTheme.textMuted; id: performerLabel; Layout.fillWidth: true; placeholderText: "T01" }
        Label { text: "Name" }
        TextField { placeholderTextColor: MarchCraftTheme.textMuted; id: performerName; Layout.fillWidth: true; placeholderText: "Optional student name" }
        Label { text: "Instrument / role" }
        ComboBox {
            id: performerInstrument; Layout.fillWidth: true; editable: true
            model: ["Trumpet", "Mellophone", "Trombone", "Baritone", "Tuba", "Flute", "Clarinet", "Alto Sax", "Tenor Sax", "Percussion", "Guard", "Drum Major", "Prop", "Unassigned"]
        }
        Label { text: "Section" }
        TextField { placeholderTextColor: MarchCraftTheme.textMuted; id: performerSection; Layout.fillWidth: true; placeholderText: "Brass" }
        Label { text: "Equipment for clearance analysis" }
        ComboBox { id: performerAsset; Layout.fillWidth: true; model: assetCatalogContext.instruments; textRole: "label"; valueRole: "id" }
        Label { text: "Notes" }
        TextArea { placeholderTextColor: MarchCraftTheme.textMuted; id: performerNotes; Layout.fillWidth: true; Layout.preferredHeight: 64 }
        AppButton {
            text: performerDialog.editing ? "Save changes" : "Add to field"
            highlighted: true; Layout.alignment: Qt.AlignRight
            onClicked: {
                drillProjectContext.savePerformerDetails(performerDialog.editing ? windowContext.activePerformer : -1,
                    performerLabel.text, performerName.text, performerInstrument.editText,
                    performerSection.text, performerNotes.text, performerAsset.currentValue)
                performerDialog.close(); inspectorContext.refresh()
            }
        }
    }
}
