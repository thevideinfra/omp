import QtQuick
import "Scale.js" as S

// A text chip: tinted and outlined in the accent on hover.
Rectangle {
    id: root
    property string text
    property bool danger: false
    property bool enabled: true
    signal clicked()

    implicitWidth: label.implicitWidth + S.sp(24)
    implicitHeight: S.sp(30)
    radius: S.sp(6)
    opacity: enabled ? 1 : 0.4
    readonly property bool lit: area.containsMouse && enabled
    color: lit ? Qt.alpha(danger ? theme.danger : theme.accent, 0.15) : Qt.alpha(theme.foreground, 0.05)
    border.width: 1
    border.color: lit ? Qt.alpha(danger ? theme.danger : theme.accent, 0.55) : Qt.alpha(theme.foreground, 0.12)

    UiText {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.danger && root.lit ? theme.danger : theme.foreground
    }
    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: if (root.enabled) root.clicked()
    }
}
