import QtQuick
import "Scale.js" as S

// Cards in a grid, as many columns as fit. `model` has the rows' key, title, subtitle and cover.
// It answers the keyboard like the other lists (move, activate, enqueue).
GridView {
    id: root
    property bool round: false
    property string icon: "albums"
    readonly property int columns: Math.max(2, Math.floor(width / S.sp(190)))
    signal opened(int row)
    signal played(int row)
    signal queued(int row)
    signal menuRequested(int row)
    signal contextRequested(int row, real x, real y)

    function move(delta) { step(delta * columns) }
    function step(delta) {
        if (count === 0) return
        currentIndex = Math.max(0, Math.min(count - 1, (currentIndex < 0 ? 0 : currentIndex) + delta))
        positionViewAtIndex(currentIndex, GridView.Contain)
    }
    function activate() { if (count > 0 && currentIndex >= 0) opened(currentIndex) }
    function enqueue() { if (count > 0 && currentIndex >= 0) queued(currentIndex) }
    function pick() { if (count > 0 && currentIndex >= 0) menuRequested(currentIndex) }
    function focusRight() { step(1) }
    function back() { return false }
    function removeSelected() {}

    clip: true
    cellWidth: Math.floor(width / columns)
    cellHeight: cellWidth + S.sp(52)
    currentIndex: -1
    boundsBehavior: Flickable.StopAtBounds

    FastWheel { view: root; notch: S.sp(90) }

    delegate: Item {
        id: cell
        required property int index
        required property string title
        required property string subtitle
        required property string cover
        width: root.cellWidth
        height: root.cellHeight
        YtmCard {
            anchors.horizontalCenter: parent.horizontalCenter
            cardWidth: root.cellWidth - S.sp(8)
            title: cell.title
            subtitle: cell.subtitle
            cover: cell.cover
            icon: root.icon
            round: root.round
            current: root.currentIndex === cell.index
            onActivated: { root.currentIndex = cell.index; root.opened(cell.index) }
            onPlayed: root.played(cell.index)
            onContext: (x, y) => { root.currentIndex = cell.index; root.contextRequested(cell.index, x, y) }
        }
    }
}
