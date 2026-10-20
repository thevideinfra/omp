import QtQuick
import "Scale.js" as S

// The small picture for a tab in the left rail: albums, artists, songs, playlists,
// folders, queue or settings. Drawn, so it takes the text colour.
Canvas {
    id: root
    property string kind
    property color ink: theme.foreground

    width: S.sp(20)
    height: S.sp(20)
    onInkChanged: requestPaint()
    onKindChanged: requestPaint()
    onWidthChanged: requestPaint()
    Component.onCompleted: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        ctx.scale(width / 20, height / 20)   // a 20 x 20 grid
        ctx.strokeStyle = ink
        ctx.fillStyle = ink
        ctx.lineWidth = 1.6
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        const line = (x1, y1, x2, y2) => { ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke() }
        const circle = (x, y, r, fill) => { ctx.beginPath(); ctx.arc(x, y, r, 0, 2 * Math.PI); if (fill) ctx.fill(); else ctx.stroke() }
        const triangle = (x1, y1, x2, y2, x3, y3) => {
            ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.lineTo(x3, y3); ctx.closePath(); ctx.fill()
        }
        switch (kind) {
        case "albums":
            circle(10, 10, 7.5, false)
            circle(10, 10, 2.2, true)
            break
        case "artists":
            circle(10, 6.5, 3.2, false)
            ctx.beginPath(); ctx.arc(10, 18, 6.5, Math.PI, 2 * Math.PI); ctx.stroke()
            break
        case "songs":
            line(13, 3.5, 13, 14)
            circle(10.5, 14.5, 2.6, true)
            ctx.beginPath(); ctx.moveTo(13, 3.5); ctx.quadraticCurveTo(17.5, 5, 16, 10); ctx.stroke()
            break
        case "playlists":
            line(3, 5, 11, 5); line(3, 10, 11, 10); line(3, 15, 8, 15)
            circle(14.5, 14.5, 2.2, true)
            line(16.7, 14.5, 16.7, 6)
            break
        case "libraries":
        case "folders":
            ctx.beginPath()
            ctx.moveTo(2.5, 5); ctx.lineTo(8, 5); ctx.lineTo(9.8, 7.2); ctx.lineTo(17.5, 7.2)
            ctx.lineTo(17.5, 16); ctx.lineTo(2.5, 16); ctx.closePath()
            ctx.stroke()
            break
        case "queue":
            triangle(2.5, 3, 2.5, 8, 6.6, 5.5)
            line(9, 5.5, 17.5, 5.5); line(9, 10.5, 17.5, 10.5); line(9, 15.5, 17.5, 15.5)
            line(2.5, 10.5, 6, 10.5); line(2.5, 15.5, 6, 15.5)
            break
        case "settings":
            circle(10, 10, 2.8, false)
            circle(10, 10, 6, false)
            for (let i = 0; i < 8; ++i) {
                const a = i * Math.PI / 4
                line(10 + Math.cos(a) * 6.8, 10 + Math.sin(a) * 6.8, 10 + Math.cos(a) * 8.8, 10 + Math.sin(a) * 8.8)
            }
            break
        }
    }
}
