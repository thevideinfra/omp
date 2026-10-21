import QtQuick
import "Logo.js" as Logo

// The OMP logo (the wordmark, or the badge when `badge` is set) in one colour (the accent), drawn from its path so it stays sharp at any size.
// It keeps its proportions and sits in the middle of whatever size it is given.
Canvas {
    id: root
    property color ink: theme.accent
    property bool badge: false

    onInkChanged: requestPaint()
    onBadgeChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    Component.onCompleted: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const gw = badge ? Logo.badgeWidth : Logo.width
        const gh = badge ? Logo.badgeHeight : Logo.height
        const k = Math.min(width / gw, height / gh)
        ctx.translate((width - gw * k) / 2, (height - gh * k) / 2)
        ctx.scale(k, k)
        ctx.fillStyle = ink
        ctx.path = badge ? Logo.badgePath : Logo.path
        ctx.fill()
    }
}
