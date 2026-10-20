import QtQuick
import "Logo.js" as Logo

// The OMP logo in one colour (the accent), drawn from its path so it stays sharp at any size.
// It keeps its proportions and sits in the middle of whatever size it is given.
Canvas {
    id: root
    property color ink: theme.accent

    onInkChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    Component.onCompleted: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const k = Math.min(width / Logo.width, height / Logo.height)
        ctx.translate((width - Logo.width * k) / 2, (height - Logo.height * k) / 2)
        ctx.scale(k, k)
        ctx.fillStyle = ink
        ctx.path = Logo.path
        ctx.fill()
    }
}
