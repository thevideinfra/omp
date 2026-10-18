import QtQuick
import "Scale.js" as S

// Text in the user's font, scaled by the font size setting. `px` is the size
// at normal settings and the shell's 12px base.
Text {
    id: root
    property real px: 13
    color: theme.foreground
    font.pixelSize: S.fs(px)
    font.family: ui.fontFamily !== "" ? ui.fontFamily : Qt.application.font.family
    // Single-line height that follows font changes immediately; Text's own
    // implicitHeight only updates in polish, which hidden windows never run.
    readonly property real metricHeight: lineMetrics.height
    TextMetrics { id: lineMetrics; font: root.font; text: "Ag" }
}
