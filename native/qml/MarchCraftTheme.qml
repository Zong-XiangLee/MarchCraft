pragma Singleton

import QtQuick

QtObject {
    readonly property color canvas: "#0b0f14"
    readonly property color surface: "#121821"
    readonly property color surfaceRaised: "#18212c"
    readonly property color surfaceHover: "#202b38"
    readonly property color divider: "#293443"
    readonly property color dividerStrong: "#3b4b5f"
    readonly property color textPrimary: "#f2f5f7"
    readonly property color textSecondary: "#a5afbc"
    readonly property color textMuted: "#778392"
    readonly property color textDisabled: "#66717f"
    readonly property color accent: "#5b8def"
    readonly property color accentHover: "#74a2ff"
    readonly property color accentPressed: "#4675c9"
    readonly property color success: "#5ead83"
    readonly property color warning: "#d6a75d"
    readonly property color danger: "#e07178"
    readonly property string fontFamily: "Segoe UI Variable"
    readonly property int radiusSmall: 6
    readonly property int radiusLarge: 8
    readonly property int motionFast: workspaceController.systemAnimationsEnabled ? 120 : 1
    readonly property int motionMedium: workspaceController.systemAnimationsEnabled ? 200 : 1
    readonly property int motionScreen: workspaceController.systemAnimationsEnabled ? 280 : 1
    readonly property real pressScale: workspaceController.systemAnimationsEnabled ? 0.975 : 1.0
    readonly property int iconSmall: 16
    readonly property int iconMedium: 20
    readonly property int focusWidth: 2
}
