import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Rectangle {
    id: root
    anchors.fill: parent
    color: MarchCraftTheme.surface
    signal workspaceRequested
    property bool initialized: false
    property bool customFilename: false
    property bool brandingEdited: false
    property string presetTitle: "Drill diagrams"
    property real previewSeconds: 0
    property real zoom: 1
    readonly property bool video: String(options.format).startsWith("video")
    readonly property bool folderOutput: options.format === "png" || (options.format === "pdf" && (options.split || options.content === "both"))
    readonly property string destination: folder.text + (folderOutput ? "" : "/" + filename.text)
    readonly property var builtins: ["Drill diagrams", "Performer coordinates", "Coordinates + drill diagrams", "PNG charts", "2D video", "3D video"]
    function invalidate() {
        prepared = false;
        playback.stop();
        if (visible && !exportController.busy)
            refreshTimer.restart();
    }
    function updateFilename() {
        if (!customFilename)
            filename.text = exportController.suggestedName(options.content, options.format);
        else if (!folderOutput)
            filename.text = filename.text.replace(/\.[^.]+$/, "") + "." + (video ? "mp4" : options.format === "print" ? "pdf" : options.format);
    }
    function resetForProject() {
        initialized = false;
        brandingEdited = false;
        customFilename = false;
        logoPath = "";
        removeLogo = false;
        if (!exportController.busy)
            prepared = false;
    }
    function enter() {
        workspaceRequested();
        if (exportController.busy)
            return;
        if (!initialized) {
            openFor("charts", "pdf");
            return;
        }
        if (!brandingEdited)
            company.text = exportController.branding.company || "";
        updateFilename();
        exportController.refresh();
        invalidate();
    }
    function applyPreset(name) {
        if (builtins.indexOf(name) >= 0) {
            const next = Object.assign({}, options, {
                format: "pdf",
                content: "charts",
                paper: "Letter",
                landscape: true,
                performerLabelSize: 6,
                fontSize: 9,
                markerSize: 2,
                margin: 10,
                split: false,
                dpi: 300,
                fps: 30,
                height: 1080,
                audio: "silent",
                camera: "director",
                grid: true,
                labels: true,
                symbols: false,
                props: true,
                notes: true,
                headings: true,
                numbers: true,
                companyLogo: true,
                marchcraftLogo: true,
                monochrome: false,
                framing: "field"
            });
            if (name === "Performer coordinates")
                next.content = "coordinates";
            if (name === "Coordinates + drill diagrams")
                next.content = "both";
            if (name === "PNG charts")
                next.format = "png";
            if (name === "2D video")
                next.format = "video2d";
            if (name === "3D video")
                next.format = "video3d";
            options = next;
        } else
            options = exportController.loadPreset(name);
        presetTitle = name;
        customFilename = false;
        updateFilename();
        invalidate();
    }
    function refreshPreview() {
        if (exportController.busy)
            return;
        const next = Object.assign({}, options);
        next.basename = filename.text.replace(/\.(pdf|png|csv|mp4)$/i, "");
        next.brandingCompany = company.text;
        next.brandingLogoPath = logoPath;
        next.brandingRemoveLogo = removeLogo;
        prepared = exportController.prepare(next);
        pageIndex = Math.min(pageIndex, Math.max(0, exportController.pages.length - 1));
        if (prepared && !video && options.format !== "csv" && pageIndex > 0)
            exportController.preview(pageIndex);
        previewSeconds = 0;
    }
    onVisibleChanged: if (!visible)
        playback.stop()
    Timer {
        id: refreshTimer
        interval: 250
        onTriggered: root.refreshPreview()
    }
    Timer {
        id: playback
        interval: 1000 / 30
        repeat: true
        onTriggered: {
            root.previewSeconds = Math.min(exportController.duration, root.previewSeconds + interval / 1000);
            exportController.previewTime(root.previewSeconds);
            if (root.previewSeconds >= exportController.duration)
                stop();
        }
    }
    property var options: ({})
    property bool prepared: false
    property int pageIndex: 0
    property string logoPath: ""
    property bool removeLogo: false
    function change(key, value) {
        const next = Object.assign({}, options);
        next[key] = value;
        options = next;
        presetTitle = "Custom";
        zoom = 1;
        if (key === "format" && value !== "pdf" && options.content === "both") {
            const fixed = Object.assign({}, options);
            fixed.content = "charts";
            options = fixed;
        }
        if (key === "format" || key === "content")
            updateFilename();
        invalidate();
    }
    function toggleId(key, id, checked) {
        let values = (options[key] || []).slice();
        if (checked && values.indexOf(id) < 0)
            values.push(id);
        if (!checked)
            values = values.filter(function (v) {
                return v !== id;
            });
        change(key, values);
    }
    function openFor(content, format) {
        workspaceRequested();
        if (exportController.busy)
            return;
        if (initialized) {
            presetTitle = "Custom";
            const next = Object.assign({}, options);
            next.content = content;
            next.format = format;
            options = next;
            updateFilename();
            invalidate();
            return;
        }
        presetTitle = format === "csv" ? "Custom" : format === "video2d" ? "2D video" : format === "video3d" ? "3D video" : format === "png" ? "PNG charts" : content === "coordinates" ? "Performer coordinates" : "Drill diagrams";
        initialized = true;
        options = {
            content: content,
            format: format,
            scope: "all",
            variants: "active",
            paper: "Letter",
            landscape: true,
            grid: true,
            labels: true,
            props: true,
            notes: true,
            headings: true,
            numbers: true,
            subsets: true,
            companyLogo: true,
            marchcraftLogo: true,
            framing: "field",
            dpi: 300,
            fontSize: 9,
            performerLabelSize: 6,
            markerSize: 2,
            margin: 10,
            fps: 30,
            height: 1080,
            audio: "silent",
            camera: "director"
        };
        logoPath = "";
        removeLogo = false;
        company.text = exportController.branding.company || "";
        folder.text = exportController.destinationFolder();
        updateFilename();
        prepared = false;
        exportController.refresh();
        invalidate();
        pageIndex = 0;
    }
    component Section: ColumnLayout {
        property string title
        property bool expanded: true
        default property alias settings: body.data
        Layout.fillWidth: true
        spacing: 10
        data: [
            ToolButton {
                implicitHeight: 36
                font.pixelSize: 12
                contentItem: Label {
                    text: parent.text
                    color: MarchCraftTheme.textPrimary
                    font: parent.font
                    verticalAlignment: Text.AlignVCenter
                }
                text: (parent.expanded ? "−  " : "+  ") + parent.title
                font.bold: true
                Layout.fillWidth: true
                onClicked: parent.expanded = !parent.expanded
            },
            ColumnLayout {
                id: body
                Layout.fillWidth: true
                spacing: 8
                visible: parent.expanded
            },
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: MarchCraftTheme.divider
            }
        ]
    }
    component Choice: RowLayout {
        required property string keyName
        required property string caption
        required property var values
        required property var captions
        Layout.fillWidth: true
        Label {
            text: caption
            Layout.preferredWidth: 125
        }
        ComboBox {
            implicitHeight: 32
            font.pixelSize: 12
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
        Label {
            text: caption
            Layout.preferredWidth: 125
        }
        SpinBox {
            implicitHeight: 32
            font.pixelSize: 12
            from: minimum
            to: maximum
            editable: true
            value: root.options[keyName] === undefined ? defaultValue : root.options[keyName]
            onValueModified: root.change(keyName, value)
        }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Export"
                font.pixelSize: 24
                font.bold: true
            }
            Label {
                text: drillProject.showName
                Layout.fillWidth: true
                elide: Text.ElideRight
                color: MarchCraftTheme.textSecondary
            }
            Label {
                text: "Preset"
            }
            ComboBox {
                implicitHeight: 32
                font.pixelSize: 12
                Layout.preferredWidth: 260
                model: root.builtins.concat(exportController.presetNames).concat(["Custom"])
                currentIndex: Math.max(0, model.indexOf(root.presetTitle))
                enabled: !exportController.busy
                onActivated: if (currentText !== "Custom")
                    root.applyPreset(currentText)
            }
        }
        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            ScrollView {
                SplitView.preferredWidth: 370
                SplitView.minimumWidth: 340
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width
                    enabled: !exportController.busy
                    spacing: 8
                    Section {
                        title: "Format"
                        Choice {
                            keyName: "format"
                            caption: "Output"
                            values: ["pdf", "png", "csv", "print", "video2d", "video3d"]
                            captions: ["PDF document", "PNG pages", "Analytics CSV", "Print", "2D animation MP4", "3D animation MP4"]
                        }
                        Choice {
                            keyName: "content"
                            caption: "Document"
                            values: root.options.format === "pdf" ? ["charts", "coordinates", "both"] : ["charts", "coordinates"]
                            captions: root.options.format === "pdf" ? ["Drill diagrams", "Coordinate sheets", "Diagrams + coordinates"] : ["Drill diagrams", "Coordinate sheets"]
                            visible: !String(root.options.format).startsWith("video") && root.options.format !== "csv"
                        }
                    }
                    Section {
                        title: "Content"
                        Choice {
                            keyName: "scope"
                            caption: "Movements"
                            values: ["all", "active", "selected"]
                            captions: ["Entire show", "Active movement", "Choose movements"]
                        }
                        Repeater {
                            model: root.options.scope === "selected" ? exportController.choices : []
                            CheckBox {
                                required property var modelData
                                text: modelData.name
                                checked: (root.options.movements || []).indexOf(modelData.id) >= 0
                                onToggled: root.toggleId("movements", modelData.id, checked)
                            }
                        }
                        Label {
                            text: "Sets: blank = all; numbers, ranges or individual selections"
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        TextField {
                            placeholderTextColor: "#929caa"
                            implicitHeight: 32
                            font.pixelSize: 12
                            Layout.fillWidth: true
                            placeholderText: "Examples: 1-8, 12, 14A"
                            text: typeof root.options.sets === "string" ? root.options.sets : ""
                            onTextEdited: root.change("sets", text)
                        }
                        Toggle {
                            keyName: "subsets"
                            text: "Include subsets"
                        }
                        Choice {
                            keyName: "variants"
                            caption: "Variants"
                            values: ["active", "all", "selected"]
                            captions: ["Active variants", "All variants", "Choose individual variants"]
                        }
                        Repeater {
                            model: root.options.variants === "selected" ? exportController.choices : []
                            ColumnLayout {
                                required property var modelData
                                property string movementName: modelData.name
                                Label {
                                    text: parent.movementName
                                    font.bold: true
                                }
                                Repeater {
                                    model: parent.modelData.sets
                                    ColumnLayout {
                                        required property var modelData
                                        property string setNumber: modelData.number
                                        Repeater {
                                            model: parent.modelData.variants
                                            CheckBox {
                                                required property var modelData
                                                text: "Set " + parent.setNumber + " / " + modelData.label + " " + modelData.name
                                                checked: (root.options.variantIds || []).indexOf(modelData.id) >= 0
                                                onToggled: root.toggleId("variantIds", modelData.id, checked)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        TextField {
                            placeholderTextColor: "#929caa"
                            implicitHeight: 32
                            font.pixelSize: 12
                            Layout.fillWidth: true
                            placeholderText: "Performer labels, comma separated (blank = all)"
                            text: root.options.performers || ""
                            onTextEdited: root.change("performers", text)
                        }
                        TextField {
                            placeholderTextColor: "#929caa"
                            implicitHeight: 32
                            font.pixelSize: 12
                            Layout.fillWidth: true
                            placeholderText: "Sections, comma separated (blank = all)"
                            text: root.options.sections || ""
                            onTextEdited: root.change("sections", text)
                        }
                    }
                    Section {
                        title: "Branding"
                        expanded: false
                        visible: root.options.format !== "csv"
                        TextField {
                            id: company
                            placeholderTextColor: "#929caa"
                            implicitHeight: 32
                            font.pixelSize: 12
                            Layout.fillWidth: true
                            placeholderText: "Company name"
                            onTextEdited: {
                                root.brandingEdited = true;
                                root.invalidate();
                            }
                        }
                        RowLayout {
                            Button {
                                implicitHeight: 32
                                font.pixelSize: 12
                                text: "Choose logo…"
                                onClicked: logoDialog.open()
                            }
                            Button {
                                implicitHeight: 32
                                font.pixelSize: 12
                                text: "Remove logo"
                                onClicked: {
                                    root.brandingEdited = true;
                                    root.removeLogo = true;
                                    root.logoPath = "";
                                    root.invalidate();
                                }
                            }
                        }
                        Label {
                            text: root.removeLogo ? "Logo will be removed" : root.logoPath.length ? "New logo selected" : exportController.branding.logo ? "Company logo embedded in show" : "No company logo"
                        }
                        Toggle {
                            keyName: "companyLogo"
                            text: "Show company logo"
                        }
                        Toggle {
                            keyName: "marchcraftLogo"
                            text: "Show black-and-white MarchCraft logo beside it"
                        }
                        Button {
                            implicitHeight: 32
                            font.pixelSize: 12
                            text: "Save branding to show"
                            onClicked: {
                                if (exportController.setBranding(company.text, root.logoPath, root.removeLogo)) {
                                    root.brandingEdited = false;
                                    root.logoPath = "";
                                    root.removeLogo = false;
                                    root.invalidate();
                                }
                            }
                        }
                    }
                    Section {
                        title: "Layout"
                        expanded: !root.video
                        visible: root.options.format !== "csv"
                        Choice {
                            keyName: "paper"
                            caption: "Paper"
                            values: ["Letter", "Legal", "Tabloid", "A4", "A3"]
                            captions: values
                        }
                        Toggle {
                            keyName: "landscape"
                            text: "Landscape"
                        }
                        NumberInput {
                            keyName: "margin"
                            caption: "Margins (mm)"
                            minimum: 5
                            maximum: 35
                            defaultValue: 10
                        }
                        NumberInput {
                            keyName: "fontSize"
                            caption: "Page text (pt)"
                            minimum: 6
                            maximum: 18
                            defaultValue: 9
                        }
                        NumberInput {
                            keyName: "performerLabelSize"
                            caption: "Performer labels (pt)"
                            minimum: 4
                            maximum: 18
                            defaultValue: 6
                        }
                        NumberInput {
                            keyName: "markerSize"
                            caption: "Marker radius (pt)"
                            minimum: 1
                            maximum: 6
                            defaultValue: 2
                        }
                        Choice {
                            keyName: "framing"
                            caption: "Field framing"
                            values: ["field", "fit", "custom"]
                            captions: ["Full field + off-field positions", "Fit selected performers", "Custom region (steps)"]
                        }
                        GridLayout {
                            visible: root.options.framing === "custom"
                            columns: 2
                            Repeater {
                                model: [
                                    {
                                        key: "cropX",
                                        label: "Left X",
                                        value: 0
                                    },
                                    {
                                        key: "cropY",
                                        label: "Front Y",
                                        value: 0
                                    },
                                    {
                                        key: "cropWidth",
                                        label: "Width",
                                        value: 160
                                    },
                                    {
                                        key: "cropHeight",
                                        label: "Depth",
                                        value: 85.333
                                    }
                                ]
                                TextField {
                                    placeholderTextColor: "#929caa"
                                    implicitHeight: 32
                                    font.pixelSize: 12
                                    required property var modelData
                                    Layout.preferredWidth: 170
                                    placeholderText: modelData.label
                                    text: root.options[modelData.key] === undefined ? modelData.value : root.options[modelData.key]
                                    validator: DoubleValidator {
                                        bottom: -1000
                                        top: 1000
                                    }
                                    onTextEdited: root.change(modelData.key, Number(text))
                                }
                            }
                        }
                        Flow {
                            Layout.fillWidth: true
                            Layout.preferredHeight: childrenRect.height
                            Toggle {
                                keyName: "grid"
                                text: "Grid"
                            }
                            Toggle {
                                keyName: "labels"
                                text: "Labels"
                            }
                            Toggle {
                                keyName: "symbols"
                                text: "Symbols"
                            }
                            Toggle {
                                keyName: "props"
                                text: "Props"
                            }
                            Toggle {
                                keyName: "notes"
                                text: "Instructions"
                            }
                            Toggle {
                                keyName: "headings"
                                text: "Headings"
                            }
                            Toggle {
                                keyName: "numbers"
                                text: "Page numbers"
                            }
                            Toggle {
                                keyName: "monochrome"
                                text: "Monochrome"
                            }
                        }
                    }
                    Section {
                        title: "Advanced"
                        expanded: root.video
                        Toggle {
                            keyName: "split"
                            text: "Separate PDF per movement"
                            visible: root.options.format === "pdf"
                        }
                        Choice {
                            keyName: "dpi"
                            caption: "PNG resolution"
                            values: [150, 300, 600]
                            captions: ["150 DPI", "300 DPI", "600 DPI"]
                            visible: root.options.format === "png"
                        }
                        ColumnLayout {
                            visible: String(root.options.format).startsWith("video")
                            Label {
                                text: "Video"
                                font.bold: true
                            }
                            Choice {
                                keyName: "height"
                                caption: "Resolution"
                                values: [720, 1080]
                                captions: ["720p", "1080p"]
                            }
                            Choice {
                                keyName: "fps"
                                caption: "Frame rate"
                                values: [30, 60]
                                captions: ["30 fps", "60 fps"]
                            }
                            Choice {
                                keyName: "audio"
                                caption: "Sound"
                                values: ["silent", "recording", "midi"]
                                captions: ["Silent", "Attached recording", "Synthesized MIDI"]
                            }
                            Choice {
                                keyName: "camera"
                                caption: "3D camera"
                                values: ["director", "overhead", "field"]
                                captions: ["Director", "Overhead", "Field level"]
                                visible: root.options.format === "video3d"
                            }
                            TextField {
                                placeholderTextColor: "#929caa"
                                implicitHeight: 32
                                font.pixelSize: 12
                                Layout.fillWidth: true
                                text: exportController.ffmpeg
                                placeholderText: "Installed FFmpeg executable"
                                onEditingFinished: exportController.ffmpeg = text
                            }
                            Button {
                                implicitHeight: 32
                                font.pixelSize: 12
                                text: "Locate FFmpeg…"
                                onClicked: encoderDialog.open()
                            }
                        }
                        RowLayout {
                            TextField {
                                id: presetName
                                placeholderTextColor: "#929caa"
                                implicitHeight: 32
                                font.pixelSize: 12
                                placeholderText: "Preset name"
                                Layout.fillWidth: true
                            }
                            Button {
                                implicitHeight: 32
                                font.pixelSize: 12
                                text: "Save preset"
                                onClicked: exportController.savePreset(presetName.text, root.options)
                            }
                        }
                    }
                }
            }
            Rectangle {
                SplitView.fillWidth: true
                color: "#161a20"
                radius: 8
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12
                    RowLayout {
                        Label {
                            text: root.options.format === "csv" ? "DATA PREVIEW" : root.video ? "ANIMATION PREVIEW" : "PAGE PREVIEW"
                            color: "#bdc7d5"
                            font.pixelSize: 11
                            font.bold: true
                            font.letterSpacing: 1
                            Layout.fillWidth: true
                        }
                        ComboBox {
                            implicitHeight: 32
                            font.pixelSize: 12
                            visible: !root.video && root.options.format !== "csv"
                            model: ["Fit", "125%", "150%", "200%"]
                            onActivated: root.zoom = [1, 1.25, 1.5, 2][currentIndex]
                        }
                    }
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Flickable {
                            anchors.fill: parent
                            clip: true
                            visible: root.prepared && root.options.format !== "csv" && root.options.format !== "video3d"
                            contentWidth: width * root.zoom
                            contentHeight: height * root.zoom
                            Image {
                                width: parent.parent.width * root.zoom
                                height: parent.parent.height * root.zoom
                                source: exportController.previewUrl
                                fillMode: Image.PreserveAspectFit
                                cache: false
                            }
                            ScrollBar.vertical: ScrollBar {}
                            ScrollBar.horizontal: ScrollBar {}
                        }
                        Loader {
                            anchors.centerIn: parent
                            width: Math.min(parent.width, parent.height * 16 / 9)
                            height: width * 9 / 16
                            active: root.visible && root.prepared && root.options.format === "video3d" && !exportController.busy && exportController.renderProject !== null
                            sourceComponent: Item {
                                Loader {
                                    anchors.fill: parent
                                    Component.onCompleted: setSource("ThreeDView.qml", {
                                        projectModel: exportController.renderProject,
                                        exportMode: true
                                    })
                                    onLoaded: item.setCameraPreset(exportController.camera)
                                    Connections {
                                        target: exportController
                                        function onRenderProjectChanged() {
                                            if (parent.item)
                                                parent.item.projectModel = exportController.renderProject;
                                        }
                                    }
                                }
                                Image {
                                    anchors.top: parent.top
                                    width: parent.width
                                    height: width * 64 / 792
                                    source: exportController.previewOverlay
                                    cache: false
                                }
                            }
                        }
                        ScrollView {
                            anchors.fill: parent
                            visible: root.prepared && root.options.format === "csv"
                            clip: true
                            contentWidth: 1800
                            ListView {
                                implicitWidth: 1800
                                model: root.options.format === "csv" ? exportController.tableRows : []
                                delegate: Row {
                                    required property var modelData
                                    required property int index
                                    property bool heading: index === 0
                                    Repeater {
                                        model: parent.modelData
                                        Rectangle {
                                            required property var modelData
                                            width: 150
                                            height: 40
                                            color: parent.heading ? "#343e4d" : "#252c35"
                                            border.color: "#485363"
                                            Text {
                                                anchors.fill: parent
                                                anchors.margins: 8
                                                text: parent.modelData
                                                color: "white"
                                                font.pixelSize: 11
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        Label {
                            anchors.centerIn: parent
                            width: parent.width - 40
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            visible: !root.prepared
                            color: "#bac3d0"
                            text: refreshTimer.running ? "Updating preview…" : exportController.message || "Select content to preview your export."
                        }
                    }
                    RowLayout {
                        visible: !root.video && root.options.format !== "csv"
                        enabled: root.prepared && !exportController.busy
                        Button {
                            implicitHeight: 32
                            font.pixelSize: 12
                            text: "‹"
                            enabled: root.pageIndex > 0
                            onClicked: exportController.preview(--root.pageIndex)
                        }
                        ComboBox {
                            implicitHeight: 32
                            font.pixelSize: 12
                            Layout.fillWidth: true
                            model: exportController.pages
                            currentIndex: root.pageIndex
                            onActivated: {
                                root.pageIndex = currentIndex;
                                exportController.preview(currentIndex);
                            }
                        }
                        Label {
                            text: exportController.pages.length ? (root.pageIndex + 1) + " / " + exportController.pages.length : "0 pages"
                            color: "#bac3d0"
                        }
                        Button {
                            implicitHeight: 32
                            font.pixelSize: 12
                            text: "›"
                            enabled: root.pageIndex + 1 < exportController.pages.length
                            onClicked: exportController.preview(++root.pageIndex)
                        }
                    }
                    RowLayout {
                        visible: root.video
                        enabled: root.prepared && !exportController.busy
                        Button {
                            implicitHeight: 32
                            font.pixelSize: 12
                            text: playback.running ? "Pause" : "Play"
                            onClicked: {
                                if (playback.running)
                                    playback.stop();
                                else {
                                    if (root.previewSeconds >= exportController.duration)
                                        root.previewSeconds = 0;
                                    playback.start();
                                }
                            }
                        }
                        Slider {
                            Layout.fillWidth: true
                            from: 0
                            to: exportController.duration
                            value: root.previewSeconds
                            onMoved: {
                                playback.stop();
                                root.previewSeconds = value;
                                exportController.previewTime(value);
                            }
                        }
                        Label {
                            text: root.previewSeconds.toFixed(1) + " / " + exportController.duration.toFixed(1) + " s"
                            color: "#bac3d0"
                        }
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: MarchCraftTheme.divider
        }
        RowLayout {
            visible: root.options.format !== "print"
            enabled: !exportController.busy
            spacing: 12
            Label {
                text: "Location"
            }
            TextField {
                id: folder
                placeholderTextColor: "#929caa"
                implicitHeight: 32
                font.pixelSize: 12
                objectName: "exportFolder"
                Layout.fillWidth: true
                placeholderText: "Output folder"
            }
            Label {
                text: root.folderOutput ? "Name prefix" : "Filename"
            }
            TextField {
                id: filename
                placeholderTextColor: "#929caa"
                implicitHeight: 32
                font.pixelSize: 12
                objectName: "exportFilename"
                Layout.preferredWidth: 280
                onTextEdited: {
                    root.customFilename = true;
                    if (root.folderOutput)
                        exportController.setBasename(text.replace(/\.(pdf|png|csv|mp4)$/i, ""));
                }
            }
            Button {
                implicitHeight: 32
                font.pixelSize: 12
                text: "Browse…"
                onClicked: {
                    if (root.folderOutput) {
                        folderDialog.currentFolder = exportController.fileUrl(folder.text);
                        folderDialog.open();
                    } else {
                        outputDialog.currentFolder = exportController.fileUrl(folder.text);
                        outputDialog.selectedFile = exportController.fileUrl(root.destination);
                        outputDialog.open();
                    }
                }
            }
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: root.folderOutput ? 54 : 28
            visible: root.options.format !== "print"
            clip: true
            ListView {
                model: root.prepared && filename.text.length >= 0 ? exportController.plannedFiles(root.destination) : []
                delegate: ItemDelegate {
                    required property string modelData
                    required property int index
                    width: ListView.view.width
                    height: 26
                    text: (exportController.fileExists(modelData) ? "Exists · " : "") + modelData
                    font.pixelSize: 11
                    onClicked: {
                        if (!root.video && root.options.format !== "csv" && !exportController.busy) {
                            root.pageIndex = exportController.firstPageForFile(index);
                            exportController.preview(root.pageIndex);
                        }
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: modelData
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Label {
                    Layout.fillWidth: true
                    text: exportController.message
                    elide: Text.ElideRight
                    color: MarchCraftTheme.textSecondary
                }
                ProgressBar {
                    Layout.fillWidth: true
                    visible: exportController.busy
                    value: exportController.progress
                }
            }
            CheckBox {
                id: overwrite
                text: "Replace existing files"
                enabled: !exportController.busy
                visible: root.options.format !== "print"
            }
            Button {
                implicitHeight: 32
                font.pixelSize: 12
                text: "Open file"
                visible: !exportController.busy && exportController.progress === 1
                onClicked: exportController.openResult(false)
            }
            Button {
                implicitHeight: 32
                font.pixelSize: 12
                text: "Open folder"
                visible: !exportController.busy && exportController.progress === 1
                onClicked: exportController.openResult(true)
            }
            Button {
                implicitHeight: 32
                font.pixelSize: 12
                text: "Cancel export"
                visible: exportController.busy
                onClicked: exportController.cancel()
            }
            Button {
                implicitHeight: 32
                font.pixelSize: 12
                text: root.options.format === "print" ? "Print…" : "Export"
                highlighted: true
                enabled: root.prepared && !exportController.busy
                onClicked: {
                    playback.stop();
                    exportController.start(root.destination, overwrite.checked);
                }
            }
        }
    }
    FileDialog {
        id: logoDialog
        title: "Company logo"
        nameFilters: ["Images (*.png *.jpg *.jpeg)"]
        onAccepted: {
            root.brandingEdited = true;
            root.logoPath = selectedFile.toString();
            root.removeLogo = false;
            root.invalidate();
        }
    }
    FileDialog {
        id: encoderDialog
        title: "Choose FFmpeg"
        nameFilters: ["Executable (*.exe)"]
        onAccepted: exportController.ffmpeg = selectedFile.toString()
    }
    FileDialog {
        id: outputDialog
        title: "Export destination"
        fileMode: FileDialog.SaveFile
        defaultSuffix: root.video ? "mp4" : root.options.format === "csv" ? "csv" : "pdf"
        nameFilters: root.video ? ["MP4 (*.mp4)"] : root.options.format === "csv" ? ["CSV (*.csv)"] : ["PDF (*.pdf)"]
        onAccepted: {
            const path = exportController.localPath(selectedFile.toString()).replace(/\\/g, "/");
            folder.text = path.slice(0, path.lastIndexOf("/"));
            filename.text = path.slice(path.lastIndexOf("/") + 1);
            root.customFilename = true;
        }
    }
    FolderDialog {
        id: folderDialog
        title: "Export folder"
        onAccepted: folder.text = exportController.localPath(selectedFolder.toString())
    }
}
