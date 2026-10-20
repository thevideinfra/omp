import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Scale.js" as S

// Adds songs to a playlist, or makes a new one. Open it with openFor(tracks)
// (tracks are song maps with a path) or openNew(tracks) for naming a new playlist.
Popup {
    id: root
    property var tracks: []
    property bool newOnly: false
    property string targetServer: ""   // set when the new playlist is to be made on a server

    function openFor(list) { tracks = list; newOnly = false; targetServer = ""; nameField.text = ""; open() }
    function openNew(list, serverId) { tracks = list; newOnly = true; targetServer = serverId ?? ""; nameField.text = ""; open() }

    readonly property var paths: tracks.map(t => t.path)

    modal: true
    focus: true
    padding: S.sp(14)
    width: Math.min(S.sp(380), (parent ? parent.width : S.sp(380)) - S.sp(40))
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: nameField.forceActiveFocus()

    background: Rectangle {
        radius: S.sp(8)
        color: theme.background
        border.width: 1
        border.color: Qt.alpha(theme.accent, 0.45)
    }

    function create() {
        const name = nameField.text.trim()
        if (name === "") return
        if (targetServer !== "") {   // on a server: it makes the playlist, then it shows up in the list
            subsonic.createPlaylist(targetServer, name, paths)
            close()
            return
        }
        if (playlists.createWith(name, paths) !== "") close()
    }

    contentItem: ColumnLayout {
        spacing: S.sp(10)

        UiText {
            Layout.fillWidth: true
            font.bold: true
            elide: Text.ElideRight
            text: root.newOnly && root.paths.length === 0 ? qsTr("New playlist")
                  : root.newOnly ? qsTr("Save %1 songs as a playlist").arg(root.paths.length)
                  : root.paths.length === 1 ? qsTr("Add “%1” to a playlist").arg(root.tracks[0].title ?? "")
                  : qsTr("Add %1 songs to a playlist").arg(root.paths.length)
        }

        ListView {
            id: list
            visible: !root.newOnly && count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? Math.min(S.sp(220), contentHeight) : 0
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: playlists.list
            FastWheel { view: list }
            delegate: Rectangle {
                required property string title
                required property string subtitle
                required property bool readOnly   // Favorites cannot take songs
                required property string serverId
                required property string playlistId
                visible: !readOnly
                width: ListView.view.width
                height: readOnly ? 0 : S.sp(34)
                radius: S.sp(6)
                color: rowArea.containsMouse ? Qt.alpha(theme.accent, 0.12) : "transparent"
                UiText { x: S.sp(10); anchors.verticalCenter: parent.verticalCenter; text: title }
                UiText {
                    anchors.right: parent.right
                    anchors.rightMargin: S.sp(10)
                    anchors.verticalCenter: parent.verticalCenter
                    text: subtitle
                    color: theme.dimForeground
                    px: 12
                }
                MouseArea {
                    id: rowArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (serverId !== "") subsonic.addToPlaylist(serverId, playlistId, root.paths)   // on a server
                        else playlists.addPaths(title, root.paths)
                        root.close()
                    }
                }
            }
        }

        UiText {
            visible: !root.newOnly && list.count === 0
            Layout.fillWidth: true
            text: qsTr("No playlists yet. Name one below.")
            color: theme.dimForeground
            px: 12
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: S.sp(8)
            TextField {
                id: nameField
                Layout.fillWidth: true
                placeholderText: qsTr("New playlist name")
                color: theme.foreground
                placeholderTextColor: theme.muted
                selectByMouse: true
                font.pixelSize: S.fs(13)
                background: Rectangle { radius: S.sp(6); color: theme.surface }
                onAccepted: root.create()
            }
            Rectangle {
                Layout.preferredWidth: createText.implicitWidth + S.sp(20)
                Layout.preferredHeight: nameField.height
                radius: S.sp(6)
                color: createArea.containsMouse ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
                border.width: 1
                border.color: createArea.containsMouse ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12)
                UiText { id: createText; anchors.centerIn: parent; text: qsTr("Create") }
                MouseArea {
                    id: createArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.create()
                }
            }
        }
    }
}
