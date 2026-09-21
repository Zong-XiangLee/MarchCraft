pragma Singleton

import QtQuick

QtObject {
    // A quiet, information-first desktop-tool palette.  The value contrasts
    // are deliberately close so the field and selection state remain the
    // visual focus instead of the application chrome.
    readonly property color canvas: "#17191d"
    readonly property color surface: "#202328"
    readonly property color surfaceRaised: "#292d33"
    readonly property color surfaceHover: "#343941"
    readonly property color panel: "#1c1f24"
    readonly property color panelHeader: "#252930"
    readonly property color input: "#16181c"
    readonly property color divider: "#3a4049"
    readonly property color dividerStrong: "#535b67"
    readonly property color textPrimary: "#f1f3f5"
    readonly property color textSecondary: "#c1c7d0"
    readonly property color textMuted: "#89929e"
    readonly property color textDisabled: "#68717d"
    readonly property color accent: "#3f78bd"
    readonly property color accentHover: "#5790d2"
    readonly property color accentPressed: "#2d609b"
    readonly property color selection: "#284c73"
    readonly property color success: "#5d9b76"
    readonly property color warning: "#c6954d"
    readonly property color danger: "#c9666d"
    readonly property string fontFamily: "Segoe UI"
    readonly property int radiusSmall: 3
    readonly property int radiusLarge: 4
    readonly property int motionFast: workspaceController.systemAnimationsEnabled ? 120 : 1
    readonly property int motionMedium: workspaceController.systemAnimationsEnabled ? 200 : 1
    readonly property int motionScreen: workspaceController.systemAnimationsEnabled ? 280 : 1
    readonly property real pressScale: workspaceController.systemAnimationsEnabled ? 0.975 : 1.0
    readonly property int iconSmall: 16
    readonly property int iconMedium: 20
    readonly property int focusWidth: 2
}
