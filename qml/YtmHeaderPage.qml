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
    property string songSortKey: ""          // the songs as given until a header is clicked
    property bool songSortDescending: false
    property string albumSortKey: ""         // an artist's albums and singles, as given until a sort is chosen
    property bool albumSortDescending: false
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

    // The songs as shown: sorted by the header that was clicked, else as given.
    readonly property var shown: { songSortKey; songSortDescending; return sortSongs(tracks) }
    function sortSongs(list) {
        if (songSortKey === "") return list
        const key = songSortKey, sign = songSortDescending ? -1 : 1
        const text = (a, b) => (a ?? "").localeCompare(b ?? "", undefined, { sensitivity: "base", numeric: true })
        const copy = list.slice()
        copy.sort((a, b) => {
            let c = 0
            switch (key) {
            case "title": c = text(a.title, b.title); break
            case "artist": c = text(a.artist, b.artist); break
            case "album": c = text(a.album, b.album); break
            case "albumartist": c = text(a.albumArtist, b.albumArtist); break
            case "year": c = (a.year ?? 0) === (b.year ?? 0) ? 0 : (a.year ?? 0) === 0 ? 1 : (b.year ?? 0) === 0 ? -1 : (a.year - b.year); break
            case "track": c = ((a.disc ?? 0) - (b.disc ?? 0)) || ((a.track ?? 0) - (b.track ?? 0)); break
            case "length": c = (a.duration ?? 0) - (b.duration ?? 0); break
            case "favorite": c = (b.starred ? 1 : 0) - (a.starred ? 1 : 0); break
            }
            return c !== 0 ? sign * c : text(a.title, b.title)
        })
        return copy
    }
    function sortSongsBy(key) {
        if (songSortKey === key) songSortDescending = !songSortDescending
        else { songSortKey = key; songSortDescending = false }
    }
    function sortAlbumsBy(key) {
        if (albumSortKey === key) albumSortDescending = !albumSortDescending
        else { albumSortKey = key; albumSortDescending = false }
    }
    // Cards by year or by name (a card without a year goes last).
    function sortCards(list) {
        if (albumSortKey === "") return list
        const sign = albumSortDescending ? -1 : 1
        const year = c => parseInt(c.subtitle) || 0
        const copy = list.slice()
        copy.sort((a, b) => {
            let c = 0
            if (albumSortKey === "year") c = year(a) === year(b) ? 0 : year(a) === 0 ? 1 : year(b) === 0 ? -1 : sign * (year(a) - year(b))
            else c = sign * (a.title ?? "").localeCompare(b.title ?? "", undefined, { sensitivity: "base", numeric: true })
            return c
        })
        return copy
    }
    onAlbumSortKeyChanged: rebuild()
    onAlbumSortDescendingChanged: rebuild()

    function shuffled() {
        const copy = shown.slice()
        for (let i = copy.length - 1; i > 0; --i) {
            const j = Math.floor(Math.random() * (i + 1))
            const t = copy[i]; copy[i] = copy[j]; copy[j] = t
        }
        return copy
    }

    function rebuild() {
        songModel.clear()
        shown.forEach(t => songModel.append({ title: t.title ?? "", subtitle: (t.artist ?? "") + " — " + (t.album ?? ""),
                                               meta: t.meta ?? "", cover: t.path ?? "" }))
        albumModel.clear()
        const row = a => ({ key: a.key, title: a.title, subtitle: a.subtitle, cover: a.cover })
        sortCards(page.albums ?? []).forEach(a => albumModel.append(row(a)))
        const singles = sortCards(page.singles ?? [])
        if (singles.length > 0) {   // the singles and EPs follow the albums, after a divider
            albumModel.append({ key: "", title: qsTr("Singles and EPs"), subtitle: "", cover: "--divider--" })
            singles.forEach(a => albumModel.append(row(a)))
        }
    }
    onShownChanged: rebuild()
    onPageChanged: { renaming = false; confirmDelete = false; rebuild() }
    Component.onCompleted: rebuild()

    ListModel { id: songModel }
    ListModel { id: albumModel }
    // The song a radio starts from: the first one shown.
    readonly property string radioPath: shown.length > 0 ? (shown[0].path ?? "") : ""

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
                        Pill { text: "▶  " + qsTr("Play"); primary: true; enabled: root.shown.length > 0; onClicked: root.playRequested(root.shown, 0) }
                        Pill { text: qsTr("Shuffle"); enabled: root.shown.length > 0; onClicked: root.playRequested(root.shuffled(), 0) }
                        Pill { text: qsTr("Add to queue"); enabled: root.shown.length > 0; onClicked: root.enqueueRequested(root.shown) }
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

            YtmShelf {   // an artist's albums, then its singles and EPs after a divider
                visible: root.kind === "artist" && albumModel.count > 0
                Layout.fillWidth: true
                heading: qsTr("Albums")
                model: albumModel
                sortOptions: [["year", qsTr("Year")], ["title", qsTr("Name")]]
                sortKey: root.albumSortKey
                sortDescending: root.albumSortDescending
                onSortRequested: (key) => root.sortAlbumsBy(key)
                onOpened: (row) => { const a = albumModel.get(row); if (a.key !== "") root.albumOpened(a.key, a.title, a.subtitle, a.cover) }
                onPlayed: (row) => { const a = albumModel.get(row); if (a.key !== "") root.albumPlayRequested(a.key) }
                onContextRequested: (row, x, y) => { const a = albumModel.get(row); if (a.key !== "") root.albumMenuRequested(a.key, x, y) }
            }
            }
        }

        UiText {
            visible: root.shown.length > 0
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
            sortable: true
            sortKey: root.songSortKey
            sortDescending: root.songSortDescending
            onSortRequested: (key) => root.sortSongsBy(key)
            onActivated: (row) => root.playRequested(root.shown, row)
            onEnqueued: (row) => root.enqueueRequested([root.shown[row]])
            onMenuRequested: (row) => root.pickRequested([root.shown[row]])
            onContextRequested: (row, x, y) => root.contextRequested([root.shown[row]], x, y, root.kind === "playlist" && (root.page.editable ?? false))
            onRemoveRequested: (row) => root.removeRequested(root.shown[row].playlistIndex ?? row)
        }
    }

    Timer { id: confirmTimer; interval: 4000; onTriggered: root.confirmDelete = false }
}
