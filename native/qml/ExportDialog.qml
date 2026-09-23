import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: root
    title: "Export show"
    palette.placeholderText: "#a4acb6"
    modal: true
    width: Math.min(1180, parent.width - 32)
    height: Math.min(850, parent.height - 32)
    anchors.centerIn: parent
    closePolicy: exportController.busy ? Popup.NoAutoClose : Popup.CloseOnEscape
    property var options: ({})
    property bool prepared: false
    property int pageIndex: 0
    property string logoPath: ""
    property bool removeLogo: false
    function change(key, value) {
        const next = Object.assign({}, options)
        next[key] = value
        options = next
        prepared = false
    }
    function toggleId(key, id, checked) {
        let values = (options[key] || []).slice()
        if (checked && values.indexOf(id) < 0) values.push(id)
        if (!checked) values = values.filter(function(v) { return v !== id })
        change(key, values)
    }
    function openFor(content, format) {
        options = { content: content, format: format, scope: "all", variants: "active", paper: "Letter", landscape: true,
            grid: true, labels: true, props: true, notes: true, headings: true, numbers: true, subsets: true,
            companyLogo: true, marchcraftLogo: true, framing: "field", dpi: 300, fontSize: 9, markerSize: 2,
            margin: 10, fps: 30, height: 1080, audio: "silent", camera: "director" }
        logoPath = ""
        removeLogo = false
        company.text = exportController.branding.company || ""
        destination.text = exportController.lastDestination()
        prepared = false
        exportController.refresh()
        open()
        prepared = exportController.prepare(options)
        pageIndex = 0
    }
    component Choice: RowLayout {
        required property string keyName
        required property string caption
        required property var values
        required property var captions
        Layout.fillWidth: true
        Label { text: caption; Layout.preferredWidth: 125 }
        ComboBox {
            Layout.fillWidth: true
            model: captions
            currentIndex: Math.max(0, values.indexOf(root.options[keyName]))
            onActivated: root.change(keyName, values[currentIndex])
        }
    }
    component Toggle: CheckBox {
        required property string keyName
        checked: root.options[keyName] === true
        onToggled: root.change(keyName, checked)
    }
    component NumberInput: RowLayout {
        required property string keyName
        required property string caption
        property int minimum: 1
        property int maximum: 100
        property int defaultValue: 1
        Label { text: caption; Layout.preferredWidth: 125 }
        SpinBox { from: minimum; to: maximum; editable: true; value: root.options[keyName] === undefined ? defaultValue : root.options[keyName]; onValueModified: root.change(keyName,value) }
    }
    contentItem: ColumnLayout {
        RowLayout {
            Layout.fillWidth: true
            Label { text: "1  Select & customize    →    2  Preview    →    3  Destination & export"; font.bold: true; Layout.fillWidth: true }
            ComboBox { id: preset; model: exportController.presetNames; Layout.preferredWidth: 160 }
            Button { text: "Load preset"; enabled: !exportController.busy && preset.currentText.length > 0; onClicked: { root.options = exportController.loadPreset(preset.currentText); root.prepared = false } }
        }
        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            ScrollView {
                SplitView.preferredWidth: 420
                SplitView.minimumWidth: 340
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width
                    enabled: !exportController.busy
                    spacing: 8
                    Choice { keyName: "format"; caption: "Output"; values: ["pdf","png","csv","print","video2d","video3d"]; captions: ["PDF document","PNG pages","Analytics CSV","Print","2D animation MP4","3D animation MP4"] }
                    Choice { keyName: "content"; caption: "Document"; values: ["charts","coordinates"]; captions: ["Full drill charts","Performer coordinate sheets"]; visible: !String(root.options.format).startsWith("video") && root.options.format !== "csv" }
                    Choice { keyName: "scope"; caption: "Movements"; values: ["all","active","selected"]; captions: ["Entire show","Active movement","Choose movements"] }
                    Repeater {
                        model: root.options.scope === "selected" ? exportController.choices : []
                        CheckBox { required property var modelData; text: modelData.name; checked: (root.options.movements || []).indexOf(modelData.id)>=0; onToggled: root.toggleId("movements",modelData.id,checked) }
                    }
                    Label { text: "Sets: blank = all; numbers, ranges or individual selections"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    TextField { Layout.fillWidth: true; placeholderText: "Examples: 1-8, 12, 14A"; text: typeof root.options.sets === "string" ? root.options.sets : ""; onTextEdited: root.change("sets",text) }
                    Toggle { keyName: "subsets"; text: "Include subsets" }
                    Choice { keyName: "variants"; caption: "Variants"; values: ["active","all","selected"]; captions: ["Active variants","All variants","Choose individual variants"] }
                    Repeater {
                        model: root.options.variants === "selected" ? exportController.choices : []
                        ColumnLayout {
                            required property var modelData
                            property string movementName: modelData.name
                            Label { text: parent.movementName; font.bold: true }
                            Repeater {
                                model: parent.modelData.sets
                                ColumnLayout {
                                    required property var modelData
                                    property string setNumber: modelData.number
                                    Repeater {
                                        model: parent.modelData.variants
                                        CheckBox { required property var modelData; text: "Set " + parent.setNumber + " / " + modelData.label + " " + modelData.name; checked: (root.options.variantIds || []).indexOf(modelData.id)>=0; onToggled: root.toggleId("variantIds",modelData.id,checked) }
                                    }
                                }
                            }
                        }
                    }
                    TextField { Layout.fillWidth: true; placeholderText: "Performer labels, comma separated (blank = all)"; text: root.options.performers || ""; onTextEdited: root.change("performers",text) }
                    TextField { Layout.fillWidth: true; placeholderText: "Sections, comma separated (blank = all)"; text: root.options.sections || ""; onTextEdited: root.change("sections",text) }
                    Label { text: "Branding"; font.bold: true }
                    TextField { id: company; Layout.fillWidth: true; placeholderText: "Company name"; onTextEdited: root.prepared = false }
                    RowLayout {
                        Button { text: "Choose logo…"; onClicked: logoDialog.open() }
                        Button { text: "Remove logo"; onClicked: { root.removeLogo = true; root.logoPath = ""; root.prepared = false } }
                    }
                    Label { text: root.removeLogo ? "Logo will be removed" : root.logoPath.length ? "New logo selected" : exportController.branding.logo ? "Company logo embedded in show" : "No company logo" }
                    Toggle { keyName: "companyLogo"; text: "Show company logo" }
                    Toggle { keyName: "marchcraftLogo"; text: "Show black-and-white MarchCraft logo beside it" }
                    Button { text: "Save branding to show"; onClicked: { if(exportController.setBranding(company.text,root.logoPath,root.removeLogo)) {root.logoPath="";root.removeLogo=false;root.prepared=false} } }
                    Label { text: "Page layout & field"; font.bold: true }
                    Choice { keyName: "paper"; caption: "Paper"; values: ["Letter","Legal","Tabloid","A4","A3"]; captions: values }
                    Toggle { keyName: "landscape"; text: "Landscape" }
                    NumberInput { keyName: "margin"; caption: "Margins (mm)"; minimum: 5; maximum: 35; defaultValue: 10 }
                    NumberInput { keyName: "fontSize"; caption: "Label size (pt)"; minimum: 6; maximum: 18; defaultValue: 9 }
                    NumberInput { keyName: "markerSize"; caption: "Marker radius (pt)"; minimum: 1; maximum: 6; defaultValue: 2 }
                    Choice { keyName: "framing"; caption: "Field framing"; values: ["field","fit","custom"]; captions: ["Full field + off-field positions","Fit selected performers","Custom region (steps)"] }
                    GridLayout {
                        visible: root.options.framing === "custom"
                        columns: 2
                        Repeater {
                            model: [{key:"cropX", label:"Left X", value:0},{key:"cropY",label:"Front Y",value:0},{key:"cropWidth",label:"Width",value:160},{key:"cropHeight",label:"Depth",value:85.333}]
                            TextField { required property var modelData; Layout.preferredWidth: 170; placeholderText: modelData.label; text: root.options[modelData.key] === undefined ? modelData.value : root.options[modelData.key]; validator: DoubleValidator { bottom: -1000; top: 1000 } onTextEdited: root.change(modelData.key,Number(text)) }
                        }
                    }
                    Flow {
                        Layout.fillWidth: true; Layout.preferredHeight: childrenRect.height
                        Toggle { keyName: "grid"; text: "Grid" }
                        Toggle { keyName: "labels"; text: "Labels" }
                        Toggle { keyName: "symbols"; text: "Symbols" }
                        Toggle { keyName: "props"; text: "Props" }
                        Toggle { keyName: "notes"; text: "Instructions" }
                        Toggle { keyName: "headings"; text: "Headings" }
                        Toggle { keyName: "numbers"; text: "Page numbers" }
                        Toggle { keyName: "monochrome"; text: "Monochrome" }
                    }
                    TextField { Layout.fillWidth: true; visible: root.options.format === "png"; placeholderText: "Image filename prefix (default: chart)"; text: root.options.basename || ""; onTextEdited: root.change("basename",text) }
                    Toggle { keyName: "split"; text: "Separate PDF per movement"; visible: root.options.format === "pdf" }
                    Choice { keyName: "dpi"; caption: "PNG resolution"; values: [150,300,600]; captions: ["150 DPI","300 DPI","600 DPI"]; visible: root.options.format === "png" }
                    ColumnLayout {
                        visible: String(root.options.format).startsWith("video")
                        Label { text: "Video"; font.bold: true }
                        Choice { keyName: "height"; caption: "Resolution"; values: [720,1080]; captions: ["720p","1080p"] }
                        Choice { keyName: "fps"; caption: "Frame rate"; values: [30,60]; captions: ["30 fps","60 fps"] }
                        Choice { keyName: "audio"; caption: "Sound"; values: ["silent","recording","midi"]; captions: ["Silent","Attached recording","Synthesized MIDI"] }
                        Choice { keyName: "camera"; caption: "3D camera"; values: ["director","overhead","field"]; captions: ["Director","Overhead","Field level"]; visible: root.options.format === "video3d" }
                        TextField { Layout.fillWidth: true; text: exportController.ffmpeg; placeholderText: "Installed FFmpeg executable"; onEditingFinished: exportController.ffmpeg = text }
                        Button { text: "Locate FFmpeg…"; onClicked: encoderDialog.open() }
                    }
                    RowLayout {
                        TextField { id: presetName; placeholderText: "Preset name"; Layout.fillWidth: true }
                        Button { text: "Save preset"; onClicked: exportController.savePreset(presetName.text,root.options) }
                    }
                }
            }
            ColumnLayout {
                SplitView.fillWidth: true
                RowLayout {
                    Button { text: "Update preview"; enabled: !exportController.busy; highlighted: !root.prepared; onClicked: { if (!exportController.setBranding(company.text,root.logoPath,root.removeLogo)) return; root.logoPath="";root.removeLogo=false;root.prepared=exportController.prepare(root.options);root.pageIndex=0 } }
                    Button { text: "‹"; enabled: root.prepared && root.pageIndex>0 && !exportController.busy; onClicked: exportController.preview(--root.pageIndex) }
                    Label { text: (root.pageIndex+1) + " / " + exportController.pages.length }
                    Button { text: "›"; enabled: root.prepared && root.pageIndex+1<exportController.pages.length && !exportController.busy; onClicked: exportController.preview(++root.pageIndex) }
                }
                Image { Layout.fillWidth: true; Layout.fillHeight: true; fillMode: Image.PreserveAspectFit; source: exportController.previewUrl; cache: false }
                ComboBox { Layout.fillWidth: true; model: exportController.pages; currentIndex: root.pageIndex; enabled: root.prepared && !exportController.busy; onActivated: {root.pageIndex=currentIndex;exportController.preview(currentIndex)} }
                RowLayout {
                    Layout.fillWidth: true
                    TextField { id: destination; Layout.fillWidth: true; placeholderText: "Output file or folder"; enabled: !exportController.busy }
                    Button { text: "Browse…"; enabled: !exportController.busy; onClicked: {if(root.options.format==="png" || (root.options.format==="pdf" && root.options.split)) folderDialog.open(); else outputDialog.open()} }
                }
                ScrollView { Layout.fillWidth: true; Layout.preferredHeight: 65; Label { width: parent.width; text: root.prepared ? exportController.plannedFiles(destination.text).join("\n") : "Update preview to review planned files."; wrapMode: Text.WrapAnywhere } }
                CheckBox { id: overwrite; text: "Replace existing files"; enabled: !exportController.busy }
            }
        }
        Label { Layout.fillWidth: true; text: exportController.message; wrapMode: Text.WordWrap }
        ProgressBar { Layout.fillWidth: true; visible: exportController.busy; value: exportController.progress }
        RowLayout {
            Layout.fillWidth: true
            Button { text: "Open file"; enabled: !exportController.busy && exportController.progress===1; onClicked: exportController.openResult(false) }
            Button { text: "Open folder"; enabled: !exportController.busy && exportController.progress===1; onClicked: exportController.openResult(true) }
            Item { Layout.fillWidth: true }
            Button { text: exportController.busy ? "Cancel export" : "Close"; onClicked: {if(exportController.busy) exportController.cancel();else root.close()} }
            Button { text: root.options.format==="print" ? "Choose printer & print…" : "Export"; highlighted: true; enabled: root.prepared && !exportController.busy; onClicked: exportController.start(destination.text,overwrite.checked) }
        }
    }
    FileDialog { id: logoDialog; title: "Company logo"; nameFilters: ["Images (*.png *.jpg *.jpeg)"]; onAccepted: {root.logoPath=selectedFile.toString();root.removeLogo=false;root.prepared=false} }
    FileDialog { id: encoderDialog; title: "Choose FFmpeg"; nameFilters: ["Executable (*.exe)"]; onAccepted: exportController.ffmpeg=selectedFile.toString() }
    FileDialog { id: outputDialog; title: "Export destination"; fileMode: FileDialog.SaveFile; nameFilters: root.options.format === "csv" ? ["CSV (*.csv)"] : String(root.options.format).startsWith("video") ? ["MP4 (*.mp4)"] : ["PDF (*.pdf)"]; onAccepted: destination.text=selectedFile.toString() }
    FolderDialog { id: folderDialog; title: "Export folder"; onAccepted: destination.text=selectedFolder.toString() }
}
