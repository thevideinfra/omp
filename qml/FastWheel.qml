import QtQuick
import "Scale.js" as S

// Mouse-wheel scrolling for a Flickable: a fixed distance per wheel notch
// (about three rows), where Qt's default step is tiny. Touchpads keep their
// own pixel deltas.
WheelHandler {
    id: root
    property Flickable view
    property real notch: S.sp(150)

    parent: view
    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
    onWheel: (event) => {
        if (!view) return
        const dy = event.pixelDelta.y !== 0 ? event.pixelDelta.y : event.angleDelta.y / 120 * notch
        const top = view.originY
        const bottom = view.originY + Math.max(0, view.contentHeight - view.height)
        view.cancelFlick()
        view.contentY = Math.max(top, Math.min(view.contentY - dy, bottom))
    }
}
