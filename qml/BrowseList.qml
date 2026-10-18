import QtQuick

ListView {
    id: root
    property int playingRow: -1
    signal activated(int row)
    signal enqueued(int row)

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
        height: 52
        color: ListView.isCurrentItem ? theme.selection : "transparent"

        Image {
            id: art
            x: 8
            anchors.verticalCenter: parent.verticalCenter
            width: cover !== "" ? 40 : 0
            height: 40
            source: cover
            sourceSize.width: 80
            sourceSize.height: 80
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
        }

        Column {
            anchors {
                left: art.right
                leftMargin: 10
                right: metaText.left
                rightMargin: 10
                verticalCenter: parent.verticalCenter
            }
            Text {
                width: parent.width
                text: title
                color: index === root.playingRow ? theme.accent : theme.foreground
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: subtitle
                color: theme.dimForeground
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }

        Text {
            id: metaText
            anchors {
                right: parent.right
                rightMargin: 12
                verticalCenter: parent.verticalCenter
            }
            text: meta
            color: theme.dimForeground
            font.pixelSize: 12
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.currentIndex = index
            onDoubleClicked: root.activated(index)
        }
    }
}
