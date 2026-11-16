import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Scale.js" as S

// An album, a playlist or an artist: a big cover, the title and buttons, then the songs (an artist also
// has a shelf of its albums). `page` is { kind, key, title, subtitle, meta, cover, albumId, playlistId,
// editable, albums: [{ key, title, subtitle, cover }] }; `tracks` are the song maps shown.
Item {
    id: root
    property var page: ({})
    property var tracks: []
    property bool renaming: false
    property bool confirmDelete: false
    readonly property string kind: page.kind ?? ""
    readonly property alias list: songs   // the list the keyboard works on
    signal playRequested(var tracks, int start)
    signal enqueueRequested(var tracks)
    signal contextRequested(var tracks, real x, real y, bool canRemove)
    signal pickRequested(var tracks)
    signal albumOpened(string key, string title, string subtitle, string cover)
    signal albumPlayRequested(string key)
    signal albumMenuRequested(string key, real x, real y)
    signal radioRequested(string path)
    signal renameRequested(string name)
    signal deleteRequested()
    signal removeRequested(int index)
    signal likeAlbumRequested(string albumId)
    signal clicked()

    function removeSelected() { songs.removeSelected() }

    function shuffled() {
        const copy = tracks.slice()
        for (let i = copy.length - 1; i > 0; --i) {
            const j = Math.floor(Math.random() * (i + 1))
            const t = copy[i]; copy[i] = copy[j]; copy[j] = t
        }
        return copy
    }

    function rebuild() {
        songModel.clear()
        tracks.forEach(t => songModel.append({ title: t.title ?? "", subtitle: (t.artist ?? "") + " — " + (t.album ?? ""),
                                               meta: t.meta ?? "", cover: t.path ?? "" }))
        albumModel.clear()
        ;(page.albums ?? []).forEach(a => albumModel.append({ key: a.key, title: a.title, subtitle: a.subtitle, cover: a.cover }))
        singleModel.clear()
        ;(page.singles ?? []).forEach(a => singleModel.append({ key: a.key, title: a.title, subtitle: a.subtitle, cover: a.cover }))
    }
    onTracksChanged: rebuild()
    onPageChanged: { renaming = false; confirmDelete = false; rebuild() }
    Component.onCompleted: rebuild()

    ListModel { id: songModel }
    ListModel { id: albumModel }
    ListModel { id: singleModel }
    // The song a radio starts from: the first one shown.
    readonly property string radioPath: tracks.length > 0 ? (tracks[0].path ?? "") : ""

    component Pill: Rectangle {
        id: pill
        property string text
        property bool primary: false
        property bool danger: false
        signal clicked()
        implicitWidth: label.implicitWidth + S.sp(32)
        implicitHeight: S.sp(38)
        radius: height / 2
        opacity: enabled ? 1 : 0.4
        color: primary ? (area.containsMouse ? Qt.lighter(theme.accent, 1.1) : theme.accent)
               : area.containsMouse ? Qt.alpha(danger ? theme.danger : theme.foreground, 0.16) : Qt.alpha(theme.foreground, 0.09)
        UiText {
            id: label
            anchors.centerIn: parent
            text: pill.text
            font.bold: pill.primary
            color: pill.primary ? theme.background : pill.danger && area.containsMouse ? theme.danger : theme.foreground
        }
        MouseArea { id: area; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: pill.clicked() }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Flickable {   // the header and the shelves; it scrolls when they are taller than the page leaves room for
            id: top
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(topColumn.implicitHeight, Math.max(S.sp(200), root.height - S.sp(250)))
            contentWidth: width
            contentHeight: topColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            FastWheel { view: top; notch: S.sp(90) }

            ColumnLayout {
                id: topColumn
                width: top.width
                spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: S.sp(16)
                spacing: S.sp(18)

                YtmCover {
                    Layout.preferredWidth: S.sp(168)
                    Layout.preferredHeight: S.sp(168)
                    cover: root.page.cover ?? ""
                    icon: root.kind === "artist" ? "artists" : root.kind === "playlist" ? "playlists" : "albums"
                    round: root.kind === "artist"
                    corner: S.sp(10)
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: S.sp(6)
                    UiText { text: root.kind === "artist" ? qsTr("ARTIST") : root.kind === "playlist" ? qsTr("PLAYLIST") : qsTr("ALBUM"); color: theme.dimForeground; px: 11 }
                    UiText {
                        visible: !root.renaming
                        Layout.fillWidth: true
                        text: root.page.title ?? ""
                        font.bold: true
                        px: 28
                        elide: Text.ElideRight
                    }
                    TextField {
                        id: renameField
                        visible: root.renaming
                        Layout.fillWidth: true
                        color: theme.foreground
                        selectByMouse: true
                        font.pixelSize: S.fs(18)
                        font.family: ui.fontFamily !== "" ? ui.fontFamily : Qt.application.font.family
                        background: Rectangle { radius: S.sp(6); color: theme.surface }
                        onVisibleChanged: if (visible) { text = root.page.title ?? ""; forceActiveFocus(); selectAll() }
                        onAccepted: { root.renaming = false; root.renameRequested(text) }
                        Keys.onEscapePressed: { root.renaming = false; root.clicked() }
                    }
                    UiText { visible: text !== ""; Layout.fillWidth: true; text: root.page.subtitle ?? ""; color: theme.dimForeground; elide: Text.ElideRight }
                    UiText { visible: text !== ""; Layout.fillWidth: true; text: root.page.meta ?? ""; color: theme.dimForeground; px: 12; elide: Text.ElideRight }

                    RowLayout {
                        Layout.topMargin: S.sp(8)
                        spacing: S.sp(8)
                        Pill { text: "▶  " + qsTr("Play"); primary: true; enabled: root.tracks.length > 0; onClicked: root.playRequested(root.tracks, 0) }
                        Pill { text: qsTr("Shuffle"); enabled: root.tracks.length > 0; onClicked: root.playRequested(root.shuffled(), 0) }
                        Pill { text: qsTr("Add to queue"); enabled: root.tracks.length > 0; onClicked: root.enqueueRequested(root.tracks) }
                        Pill { visible: ytm.loggedIn && root.radioPath.indexOf("ytm:") === 0; text: qsTr("Radio"); onClicked: root.radioRequested(root.radioPath) }
                        Pill {
                            visible: root.kind === "album" && (root.page.albumId ?? "") !== "" && ytm.loggedIn
                            text: qsTr("＋ Library")
                            onClicked: root.likeAlbumRequested(root.page.albumId)
                        }
                        Pill { visible: root.kind === "playlist" && (root.page.editable ?? false); text: qsTr("Rename"); onClicked: root.renaming = !root.renaming }
                        Pill {
                            visible: root.kind === "playlist" && (root.page.editable ?? false)
                            text: root.confirmDelete ? qsTr("Really delete?") : qsTr("Delete")
                            danger: true
                            onClicked: { if (root.confirmDelete) root.deleteRequested(); else { root.confirmDelete = true; confirmTimer.restart() } }
                        }
                    }
                }
            }

            YtmShelf {   // an artist's albums
                visible: root.kind === "artist" && albumModel.count > 0
                Layout.fillWidth: true
                heading: qsTr("Albums")
                model: albumModel
                onOpened: (row) => { const a = albumModel.get(row); root.albumOpened(a.key, a.title, a.subtitle, a.cover) }
                onPlayed: (row) => root.albumPlayRequested(albumModel.get(row).key)
                onContextRequested: (row, x, y) => root.albumMenuRequested(albumModel.get(row).key, x, y)
            }

            YtmShelf {   // singles and EPs
                visible: root.kind === "artist" && singleModel.count > 0
                Layout.fillWidth: true
                heading: qsTr("Singles and EPs")
                model: singleModel
                onOpened: (row) => { const a = singleModel.get(row); root.albumOpened(a.key, a.title, a.subtitle, a.cover) }
                onPlayed: (row) => root.albumPlayRequested(singleModel.get(row).key)
                onContextRequested: (row, x, y) => root.albumMenuRequested(singleModel.get(row).key, x, y)
            }
            }
        }

        UiText {
            visible: root.tracks.length > 0
            Layout.leftMargin: S.sp(16)
            Layout.topMargin: S.sp(6)
            Layout.bottomMargin: S.sp(4)
            text: root.kind === "artist" ? ((root.page.top ?? false) ? qsTr("Top songs") : qsTr("Songs")) : ""
            font.bold: true
            px: 16
        }

        BrowseList {
            id: songs
            Layout.fillWidth: true
            Layout.fillHeight: true
            songRows: true
            table: true
            columns: settingsStore.songColumns
            model: songModel
            removable: root.kind === "playlist" && (root.page.editable ?? false)
            onClicked: root.clicked()
            onActivated: (row) => root.playRequested(root.tracks, row)
            onEnqueued: (row) => root.enqueueRequested([root.tracks[row]])
            onMenuRequested: (row) => root.pickRequested([root.tracks[row]])
            onContextRequested: (row, x, y) => root.contextRequested([root.tracks[row]], x, y, root.kind === "playlist" && (root.page.editable ?? false))
            onRemoveRequested: (row) => root.removeRequested(root.tracks[row].playlistIndex ?? row)
        }
    }

    Timer { id: confirmTimer; interval: 4000; onTriggered: root.confirmDelete = false }
}
