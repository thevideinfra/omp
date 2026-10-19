import QtQuick
import "Scale.js" as S

// Mouse-wheel scrolling for a Flickable: a fixed distance per wheel notch
// (about one and a half rows). On Wayland a wheel notch reports a pixel delta of only
// 15, so wheel events (no scroll phase) use the angle delta instead. Touchpad
// gestures have a scroll phase and keep their own pixel deltas.
WheelHandler {
    id: root
    property Flickable view
    property real notch: S.sp(70)

    parent: view
    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
    onWheel: (event) => {
        if (!view) return
        const gesture = event.phase !== Qt.NoScrollPhase && event.pixelDelta.y !== 0
        const dy = gesture ? event.pixelDelta.y : event.angleDelta.y / 120 * notch
        const top = view.originY
        const bottom = view.originY + Math.max(0, view.contentHeight - view.height)
        view.cancelFlick()
        view.contentY = Math.max(top, Math.min(view.contentY - dy, bottom))
    }
}
