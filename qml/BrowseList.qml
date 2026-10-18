import QtQuick
import "Scale.js" as S

ListView {
    id: root
    property int playingRow: -1
    signal activated(int row)
    signal enqueued(int row)
    signal clicked()

    clip: true
    currentIndex: 0
    boundsBehavior: Flickable.StopAtBounds
    keyNavigationEnabled: false

    function move(delta) {
        currentIndex = Math.max(0, Math.min(count - 1, currentIndex + delta))
        positionViewAtIndex(currentIndex, ListView.Contain)
    }
    function activate() { if (count > 0) activated(currentIndex) }
    function enqueue() { if (count > 0) enqueued(currentIndex) }

    delegate: Rectangle {
        required property int index
        required property string title
        required property string subtitle
        required property string meta
        required property string cover

        width: ListView.view.width
        height: S.sp(52)
        color: ListView.isCurrentItem ? theme.selection : "transparent"

        Image {
            id: art
            x: S.sp(8)
            anchors.verticalCenter: parent.verticalCenter
            width: cover !== "" ? S.sp(40) : 0
            height: S.sp(40)
            source: cover
            sourceSize.width: 80
            sourceSize.height: 80
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
        }

        Column {
            anchors {
                left: art.right
                leftMargin: S.sp(10)
                right: metaText.left
                rightMargin: S.sp(10)
                verticalCenter: parent.verticalCenter
            }
            UiText {
                width: parent.width
                text: title
                color: index === root.playingRow ? theme.accent : theme.foreground
                elide: Text.ElideRight
            }
            UiText {
                width: parent.width
                text: subtitle
                color: theme.dimForeground
                px: 12
                elide: Text.ElideRight
            }
        }

        UiText {
            id: metaText
            anchors {
                right: parent.right
                rightMargin: S.sp(12)
                verticalCenter: parent.verticalCenter
            }
            text: meta
            color: theme.dimForeground
            px: 12
        }

        MouseArea {
            anchors.fill: parent
            onClicked: { root.currentIndex = index; root.clicked() }
            onDoubleClicked: { root.activated(index); root.clicked() }
        }
    }
}
