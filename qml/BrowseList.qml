import QtQuick
import "Scale.js" as S

ListView {
    id: root
    property int playingRow: -1
    property int artSize: S.sp(40)   // thumbnail edge
    property bool sections: false    // group rows under their "initial" (a letter header)
    signal activated(int row)
    signal enqueued(int row)
    signal clicked()
    signal menuRequested(int row)   // right-click, or the m key

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
    function pick() { if (count > 0) menuRequested(currentIndex) }

    FastWheel { view: root }

    section.property: sections ? "initial" : ""
    section.criteria: ViewSection.FullString
    section.delegate: Item {
        required property string section
        width: ListView.view.width
        height: section === "" ? 0 : S.sp(26)
        visible: section !== ""
        UiText {
            x: S.sp(12)
            anchors.verticalCenter: parent.verticalCenter
            text: parent.section
            color: theme.accent
            px: 12
        }
    }

    delegate: Rectangle {
        required property int index
        required property string title
        required property string subtitle
        required property string meta
        required property string cover   // a track path; the art file is looked up lazily
        readonly property string artSource: { library.coverRevision; return cover !== "" ? library.coverFor(cover) : "" }

        width: ListView.view.width
        height: Math.max(S.sp(52), root.artSize + S.sp(12), textColumn.implicitHeight + S.sp(8))
        color: ListView.isCurrentItem ? Qt.alpha(theme.accent, 0.12)
               : index % 2 === 1 ? Qt.alpha(theme.foreground, 0.045) : "transparent"   // alternating rows

        Image {
            id: art
            x: S.sp(8)
            anchors.verticalCenter: parent.verticalCenter
            width: artSource !== "" ? root.artSize : 0
            height: root.artSize
            source: artSource
            sourceSize.width: root.artSize * 2
            sourceSize.height: root.artSize * 2
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
        }

        Column {
            id: textColumn
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
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: (mouse) => {
                root.currentIndex = index
                if (mouse.button === Qt.RightButton) root.menuRequested(index)
                root.clicked()
            }
            onDoubleClicked: { root.activated(index); root.clicked() }
        }
    }
}
