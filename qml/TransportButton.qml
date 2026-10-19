import QtQuick
import "Scale.js" as S

// A chip: wide enough for its glyph, tinted and outlined in the accent on hover.
Rectangle {
    id: root
    property string glyph
    property string icon   // "shuffle", "repeat", "repeatOne", "next" or "previous": drawn, not typeset
    property bool big: false
    property bool active: false   // a toggle that is on
    signal clicked()

    implicitWidth: Math.max(big ? S.sp(56) : S.sp(46), label.implicitWidth + S.sp(20))
    implicitHeight: big ? S.sp(40) : S.sp(34)
    radius: S.sp(8)
    readonly property bool lit: area.containsMouse || active
    color: lit ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
    border.width: 1
    border.color: lit ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12)

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
                color: root.lit ? theme.accent : theme.foreground
            }
        }
    }

    Canvas {
        id: art
        visible: root.icon !== ""
        anchors.centerIn: parent
        width: S.sp(20)
        height: S.sp(20)
        readonly property color ink: root.lit ? theme.accent : theme.foreground
        onInkChanged: requestPaint()
        Connections {
            target: root
            function onIconChanged() { art.requestPaint() }
        }
        onWidthChanged: requestPaint()
        Component.onCompleted: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.scale(width / 20, height / 20)   // draw on a 20 x 20 grid
            ctx.strokeStyle = ink
            ctx.fillStyle = ink
            ctx.lineWidth = 1.8
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            const arrow = (x, y, dx, dy) => {   // head at (x, y) pointing along (dx, dy)
                ctx.beginPath()
                ctx.moveTo(x, y)
                ctx.lineTo(x - 4 * dx - 3 * dy, y - 4 * dy + 3 * dx)
                ctx.lineTo(x - 4 * dx + 3 * dy, y - 4 * dy - 3 * dx)
                ctx.closePath()
                ctx.fill()
            }
            if (root.icon === "next" || root.icon === "previous") {
                const d = root.icon === "next" ? 1 : -1   // a triangle and a bar, facing d
                const bar = d > 0 ? 15 : 5
                const tip = d > 0 ? 13 : 7
                ctx.beginPath()
                ctx.moveTo(tip - d * 9, 4)
                ctx.lineTo(tip, 10)
                ctx.lineTo(tip - d * 9, 16)
                ctx.closePath()
                ctx.fill()
                ctx.beginPath()
                ctx.moveTo(bar + d * 1, 4)
                ctx.lineTo(bar + d * 1, 16)
                ctx.stroke()
            } else if (root.icon === "shuffle") {
                ctx.beginPath()
                ctx.moveTo(2, 5); ctx.lineTo(6, 5); ctx.bezierCurveTo(10, 5, 10, 15, 14, 15); ctx.lineTo(15, 15)
                ctx.moveTo(2, 15); ctx.lineTo(6, 15); ctx.bezierCurveTo(10, 15, 10, 5, 14, 5); ctx.lineTo(15, 5)
                ctx.stroke()
                arrow(19, 5, 1, 0)
                arrow(19, 15, 1, 0)
            } else {
                ctx.beginPath()   // a loop: top edge runs right, bottom edge runs left
                ctx.moveTo(4, 9); ctx.lineTo(4, 8); ctx.quadraticCurveTo(4, 4, 8, 4); ctx.lineTo(14, 4)
                ctx.moveTo(16, 11); ctx.lineTo(16, 12); ctx.quadraticCurveTo(16, 16, 12, 16); ctx.lineTo(6, 16)
                ctx.stroke()
                arrow(18, 4, 1, 0)
                arrow(2, 16, -1, 0)
                if (root.icon === "repeatOne") {
                    ctx.beginPath()
                    ctx.moveTo(9, 8.5); ctx.lineTo(10.5, 7.5); ctx.lineTo(10.5, 12.5)
                    ctx.stroke()
                }
            }
        }
    }

    UiText {
        id: label
        visible: !root.bars && root.icon === ""
        anchors.centerIn: parent
        text: root.glyph
        px: root.big ? 18 : 13
        color: root.lit ? theme.accent : theme.foreground
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
