import QtQuick
import "Scale.js" as S

// A chip: wide enough for its glyph, tinted and outlined in the accent on hover.
Rectangle {
    id: root
    property string glyph
    property bool big: false
    signal clicked()

    implicitWidth: Math.max(big ? S.sp(56) : S.sp(46), label.implicitWidth + S.sp(20))
    implicitHeight: big ? S.sp(40) : S.sp(34)
    radius: S.sp(8)
    color: area.containsMouse ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
    border.width: 1
    border.color: area.containsMouse ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12)

    // The pause glyph sits high in most fonts, so it is drawn as two bars.
    readonly property bool bars: glyph === "❚❚"

    Row {
        visible: root.bars
        anchors.centerIn: parent
        spacing: S.sp(root.big ? 5 : 4)
        Repeater {
            model: 2
            Rectangle {
                width: S.sp(root.big ? 5 : 4)
                height: S.sp(root.big ? 16 : 12)
                radius: S.sp(1)
                color: area.containsMouse ? theme.accent : theme.foreground
            }
        }
    }

    UiText {
        id: label
        visible: !root.bars
        anchors.centerIn: parent
        text: root.glyph
        px: root.big ? 18 : 13
        color: area.containsMouse ? theme.accent : theme.foreground
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
