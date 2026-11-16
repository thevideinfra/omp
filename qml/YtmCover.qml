import QtQuick
import "Scale.js" as S

// A square cover with rounded corners (or a circle for an artist). `cover` is the path of a song whose art
// is shown; without art, a quiet placeholder with an icon. The corners are cut by painting the background
// over them, so it needs no shader.
Item {
    id: root
    property string cover
    property string icon: "albums"
    property bool round: false
    property real corner: S.sp(8)
    property color backdrop: theme.background   // what the cut corners show
    readonly property string source: { library.coverRevision; return cover !== "" ? library.coverFor(cover) : "" }
    readonly property bool ready: art.status === Image.Ready
    readonly property real radius_: round ? width / 2 : corner

    Rectangle {
        anchors.fill: parent
        radius: root.radius_
        color: Qt.alpha(theme.foreground, 0.07)
    }
    Image {
        id: art
        anchors.fill: parent
        source: root.source
        sourceSize.width: Math.max(1, root.width * 2)
        sourceSize.height: Math.max(1, root.height * 2)
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
    }
    TabIcon {
        visible: !root.ready
        anchors.centerIn: parent
        width: parent.width * 0.4
        height: width
        kind: root.icon
        ink: Qt.alpha(theme.foreground, 0.35)
    }

    Canvas {   // the area outside the rounded shape, in the background colour
        id: corners
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const w = width, h = height, r = Math.min(root.radius_, w / 2, h / 2)
            if (r <= 0) return
            ctx.fillStyle = root.backdrop
            ctx.beginPath()
            ctx.moveTo(0, r); ctx.arc(r, r, r, Math.PI, 1.5 * Math.PI)       // each corner: a square with a quarter circle cut out
            ctx.lineTo(0, 0); ctx.closePath(); ctx.fill()
            ctx.beginPath()
            ctx.moveTo(w - r, 0); ctx.arc(w - r, r, r, 1.5 * Math.PI, 2 * Math.PI)
            ctx.lineTo(w, 0); ctx.closePath(); ctx.fill()
            ctx.beginPath()
            ctx.moveTo(w, h - r); ctx.arc(w - r, h - r, r, 0, 0.5 * Math.PI)
            ctx.lineTo(w, h); ctx.closePath(); ctx.fill()
            ctx.beginPath()
            ctx.moveTo(r, h); ctx.arc(r, h - r, r, 0.5 * Math.PI, Math.PI)
            ctx.lineTo(0, h); ctx.closePath(); ctx.fill()
        }
        Connections {
            target: root
            function onWidthChanged() { corners.requestPaint() }
            function onHeightChanged() { corners.requestPaint() }
            function onRadius_Changed() { corners.requestPaint() }
            function onBackdropChanged() { corners.requestPaint() }
        }
        Component.onCompleted: requestPaint()
    }
}
