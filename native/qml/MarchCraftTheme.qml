pragma Singleton

import QtQuick

QtObject {
    property string themeId: "graphite"
    readonly property bool midnight: themeId === "midnight"
    readonly property bool warm: themeId === "warm"
    readonly property var themes: [
        { text: "Graphite · blue", value: "graphite" },
        { text: "Midnight · teal", value: "midnight" },
        { text: "Warm charcoal · amber", value: "warm" }
    ]
    readonly property color canvas: midnight ? "#101b26" : warm ? "#211e1b" : "#17191d"
    readonly property color surface: midnight ? "#182735" : warm ? "#2b2723" : "#202328"
    readonly property color surfaceRaised: midnight ? "#223646" : warm ? "#37312b" : "#292d33"
    readonly property color surfaceHover: midnight ? "#2d4557" : warm ? "#463e35" : "#343941"
    readonly property color panel: midnight ? "#14222e" : warm ? "#26221e" : "#1c1f24"
    readonly property color panelHeader: midnight ? "#1c2e3e" : warm ? "#302a25" : "#252930"
    readonly property color input: midnight ? "#0f1b26" : warm ? "#1c1916" : "#16181c"
    readonly property color divider: midnight ? "#354b5c" : warm ? "#4a4137" : "#3a4049"
    readonly property color dividerStrong: midnight ? "#526e80" : warm ? "#6c5d4c" : "#535b67"
    readonly property color textPrimary: "#f1f3f5"
    readonly property color textSecondary: midnight ? "#bdd0dc" : warm ? "#d4c9bb" : "#c1c7d0"
    readonly property color textMuted: midnight ? "#8fa6b6" : warm ? "#b0a18f" : "#89929e"
    readonly property color textDisabled: "#68717d"
    readonly property color accent: midnight ? "#287d89" : warm ? "#976b35" : "#3f78bd"
    readonly property color accentHover: midnight ? "#3698a4" : warm ? "#b58749" : "#5790d2"
    readonly property color accentPressed: midnight ? "#20636e" : warm ? "#795328" : "#2d609b"
    readonly property color selection: midnight ? "#244c59" : warm ? "#57432c" : "#284c73"
    readonly property color success: "#5d9b76"
    readonly property color warning: "#c6954d"
    readonly property color danger: "#c9666d"
    readonly property string fontFamily: "Segoe UI"
    readonly property int radiusSmall: 5
    readonly property int radiusLarge: 8
    readonly property int motionFast: workspaceController.systemAnimationsEnabled ? 120 : 1
    readonly property int motionMedium: workspaceController.systemAnimationsEnabled ? 200 : 1
    readonly property int motionScreen: workspaceController.systemAnimationsEnabled ? 280 : 1
    readonly property real pressScale: workspaceController.systemAnimationsEnabled ? 0.975 : 1.0
    readonly property int iconSmall: 16
    readonly property int iconMedium: 20
    readonly property int focusWidth: 2
}
