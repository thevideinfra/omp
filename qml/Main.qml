import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Fmt.js" as Fmt
import "Scale.js" as S

ApplicationWindow {
    id: win
    objectName: "mainWindow"
    title: "omamusic"
    width: 1000
    height: 640
    minimumWidth: 520
    minimumHeight: 360
    visible: false
    color: theme.background

    readonly property var tabNames: ["Albums", "Artists", "Songs", "Playlists", "Folders", "Queue", "Settings"]
    function isTab(name) { return tabNames[tab] === name }
    readonly property bool drillTab: isTab("Albums") || isTab("Artists") || isTab("Playlists")   // list on the left, songs on the right
    property int tab: 0
    readonly property bool searching: searchField.text.length > 0

    // Close-to-tray, unless there is no tray to bring the window back from.
    onClosing: (close) => {
        if (shell.trayAvailable()) { close.accepted = false; visible = false }
        else shell.quitApp()
    }

    // A click on a column header: the first click sorts ascending, the next on the same one descending.
    function sortSongsBy(key) {
        if (library.songSort === key) {
            library.songDescending = !library.songDescending
        } else {
            library.songSort = key
            library.songDescending = false
        }
    }

    // Opens a tab with this row selected.
    function goTo(tabName, tabItem, key) {
        searchField.text = ""
        win.tab = tabNames.indexOf(tabName)
        tabItem.selectKey(key)
        keyRoot.forceActiveFocus()
    }

    function showContext(tracks, x, y, canRemove) {
        contextMenu.tracks = tracks
        contextMenu.canRemove = canRemove
        contextMenu.openAt(x, y)
    }

    function currentList() {
        return stack.children[stack.currentIndex]
    }

    // Called by Shell::showSettings() (the panel's gear).
    function openSettings() {
        win.tab = tabNames.indexOf("Settings")
        searchField.text = ""
        keyRoot.forceActiveFocus()
    }

    function handleKey(e) {
        if (e.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) {
            e.accepted = false
            return
        }
        e.accepted = true
        switch (e.key) {
        case Qt.Key_Space: player.togglePause(); break
        case Qt.Key_N: player.next(); break
        case Qt.Key_P: player.previous(); break
        case Qt.Key_H: player.seekBy(-10); break
        case Qt.Key_L: player.seekBy(10); break
        case Qt.Key_Slash: searchField.forceActiveFocus(); break
        case Qt.Key_Tab: win.tab = (win.tab + 1) % win.tabNames.length; searchField.text = ""; break
        case Qt.Key_J:
        case Qt.Key_Down: currentList().move(1); break
        case Qt.Key_K:
        case Qt.Key_Up: currentList().move(-1); break
        case Qt.Key_Return:
        case Qt.Key_Enter: currentList().activate(); break
        case Qt.Key_A: currentList().enqueue(); break
        case Qt.Key_Right:
            if (!win.searching && win.drillTab) currentList().focusRight()
            break
        case Qt.Key_Left:
            if (!win.searching && win.drillTab) currentList().back()
            break
        case Qt.Key_R: library.rescan(); subsonic.sync(); break
        case Qt.Key_S: player.shuffle = !player.shuffle; break
        case Qt.Key_M: { const l = currentList(); if (l.pick) l.pick() } break
        case Qt.Key_Delete: { const l = currentList(); if (l.removeSelected) l.removeSelected() } break
        case Qt.Key_O: player.cycleRepeat(); break
        case Qt.Key_F: subsonic.toggleStar(player.current.path ?? ""); break
        case Qt.Key_Backspace:
            if (win.searching) break
            if (win.isTab("Folders")) library.up()
            else if (win.drillTab) currentList().back()
            break
        case Qt.Key_Escape:
            if (!win.searching && win.drillTab && currentList().back()) break
            searchField.text = ""
            break
        default: e.accepted = false
        }
    }

    Item {
        id: keyRoot
        anchors.fill: parent
        focus: true
        Keys.onPressed: (e) => win.handleKey(e)

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // left rail: names and pictures, or only the pictures when collapsed
                Rectangle {
                    id: rail
                    readonly property bool collapsed: settingsStore.railCollapsed
                    Layout.fillHeight: true
                    Layout.preferredWidth: collapsed ? S.sp(60) : S.sp(168)
                    color: theme.sunken
                    clip: true
                    Behavior on Layout.preferredWidth { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

                    // Room for a logo; the name stands in until there is one.
                    Item {
                        id: logoArea
                        objectName: "logoArea"
                        x: S.sp(8)
                        y: S.sp(8)
                        width: parent.width - S.sp(16)
                        height: S.sp(64)
                        UiText {
                            anchors.centerIn: parent
                            text: rail.collapsed ? "om" : "omamusic"
                            color: theme.accent
                            px: 16
                        }
                    }

                    // One row of the rail: a picture and a name, or only the picture when collapsed.
                    component RailRow: Rectangle {
                        id: tabRow
                        required property int tabIndex
                        required property string label
                        readonly property bool current: win.tab === tabIndex && !win.searching
                        height: S.sp(38)
                        radius: S.sp(6)
                        color: current ? Qt.alpha(theme.accent, 0.12)
                               : tabArea.containsMouse ? Qt.alpha(theme.foreground, 0.06) : "transparent"
                        border.width: current ? 1 : 0
                        border.color: Qt.alpha(theme.accent, 0.45)
                        TabIcon {
                            id: tabIcon
                            x: rail.collapsed ? (parent.width - width) / 2 : S.sp(10)
                            anchors.verticalCenter: parent.verticalCenter
                            kind: tabRow.label.toLowerCase()
                            ink: win.tab === tabRow.tabIndex ? theme.accent : theme.foreground
                        }
                        UiText {
                            visible: !rail.collapsed
                            anchors.verticalCenter: parent.verticalCenter
                            x: S.sp(10) + tabIcon.width + S.sp(10)
                            text: tabRow.label
                            color: win.tab === tabRow.tabIndex ? theme.accent : theme.foreground
                        }
                        MouseArea {
                            id: tabArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: { win.tab = tabRow.tabIndex; searchField.text = ""; keyRoot.forceActiveFocus() }
                        }
                        ToolTip.visible: rail.collapsed && tabArea.containsMouse
                        ToolTip.text: tabRow.label
                        ToolTip.delay: 400
                    }

                    Column {
                        anchors.fill: parent
                        anchors.margins: S.sp(8)
                        anchors.topMargin: logoArea.y + logoArea.height + S.sp(8)
                        anchors.bottomMargin: S.sp(100)
                        spacing: S.sp(4)
                        Repeater {
                            model: win.tabNames.filter(n => n !== "Settings")
                            delegate: RailRow {
                                required property int index
                                required property string modelData
                                width: parent.width
                                tabIndex: index
                                label: modelData
                            }
                        }
                    }

                    // Settings sits at the bottom, above the collapse button.
                    RailRow {
                        anchors.bottom: collapseButton.top
                        anchors.bottomMargin: S.sp(8)
                        x: S.sp(8)
                        width: parent.width - S.sp(16)
                        tabIndex: win.tabNames.indexOf("Settings")
                        label: "Settings"
                    }

                    // Collapse or expand the rail.
                    Rectangle {
                        id: collapseButton
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: S.sp(8)
                        x: S.sp(8)
                        width: parent.width - S.sp(16)
                        height: S.sp(34)
                        radius: S.sp(6)
                        color: collapseArea.containsMouse ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
                        border.width: 1
                        border.color: collapseArea.containsMouse ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12)
                        UiText {
                            anchors.centerIn: parent
                            text: rail.collapsed ? "»" : "«  " + qsTr("Collapse")
                            px: 14
                        }
                        MouseArea {
                            id: collapseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { settingsStore.set("railCollapsed", !rail.collapsed); keyRoot.forceActiveFocus() }
                        }
                    }
                }

                // The divider between the rail and the first column.
                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 2
                    color: Qt.alpha(theme.accent, 0.35)
                }

                // content
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: S.sp(8)
                        spacing: S.sp(8)

                        TextField {
                            id: searchField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Search ( / )")
                            color: theme.foreground
                            placeholderTextColor: theme.muted
                            selectByMouse: true
                            font.pixelSize: S.fs(13)
                            font.family: ui.fontFamily !== "" ? ui.fontFamily : Qt.application.font.family
                            background: Rectangle { radius: S.sp(6); color: theme.surface }
                            onTextChanged: library.setQuery(text)
                            Keys.onEscapePressed: { text = ""; keyRoot.forceActiveFocus() }
                            Keys.onReturnPressed: keyRoot.forceActiveFocus()
                        }

                        Rectangle {
                            visible: win.isTab("Folders") && !win.searching
                            Layout.preferredWidth: S.sp(60)
                            Layout.preferredHeight: searchField.height
                            radius: S.sp(6)
                            color: upArea.containsMouse ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
                            border.width: 1
                            border.color: upArea.containsMouse ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12)
                            UiText { anchors.centerIn: parent; text: qsTr("Up") }
                            MouseArea { id: upArea; anchors.fill: parent; hoverEnabled: true; onClicked: { library.up(); keyRoot.forceActiveFocus() } }
                        }

                        Rectangle {
                            visible: win.isTab("Queue") && !win.searching
                            Layout.preferredWidth: S.sp(140)
                            Layout.preferredHeight: searchField.height
                            radius: S.sp(6)
                            color: saveArea.containsMouse ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
                            border.width: 1
                            border.color: saveArea.containsMouse ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12)
                            UiText { anchors.centerIn: parent; text: qsTr("Save as playlist") }
                            MouseArea {
                                id: saveArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: { if (player.queue.count > 0) picker.openNew(player.queue.all()) }
                            }
                        }

                        Rectangle {
                            visible: win.isTab("Queue") && !win.searching
                            Layout.preferredWidth: S.sp(110)
                            Layout.preferredHeight: searchField.height
                            radius: S.sp(6)
                            color: clearArea.containsMouse ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
                            border.width: 1
                            border.color: clearArea.containsMouse ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12)
                            UiText { anchors.centerIn: parent; text: qsTr("Clear queue") }
                            MouseArea { id: clearArea; anchors.fill: parent; hoverEnabled: true; onClicked: { player.clearQueue(); keyRoot.forceActiveFocus() } }
                        }
                    }

                    StackLayout {
                        id: stack
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: win.searching ? win.tabNames.length : win.tab

                        DrillList {   // Albums
                            id: albumsTab
                            kind: "album"
                            model: library.albums
                            onClicked: keyRoot.forceActiveFocus()
                            onPickRequested: (tracks) => picker.openFor(tracks)
                            onContextRequested: (tracks, x, y, canRemove) => win.showContext(tracks, x, y, canRemove)
                        }

                        DrillList {   // Artists
                            id: artistsTab
                            kind: "artist"
                            model: library.artists
                            onClicked: keyRoot.forceActiveFocus()
                            onPickRequested: (tracks) => picker.openFor(tracks)
                            onContextRequested: (tracks, x, y, canRemove) => win.showContext(tracks, x, y, canRemove)
                        }

                        BrowseList {
                            onClicked: keyRoot.forceActiveFocus()   // Songs
                            songRows: true
                            table: true
                            columns: settingsStore.songColumns
                            onColumnsRequested: (x, y) => columnMenu.openAt(x, y)
                            sortable: true
                            sortKey: library.songSort
                            sortDescending: library.songDescending
                            onSortRequested: (key) => win.sortSongsBy(key)
                            model: library.songs
                            onActivated: (row) => player.playTracks([library.songs.get(row)], 0)
                            onEnqueued: (row) => player.enqueue(library.songs.get(row))
                            onMenuRequested: (row) => picker.openFor([library.songs.get(row)])
                            onContextRequested: (row, x, y) => win.showContext([library.songs.get(row)], x, y, false)
                        }

                        DrillList {   // Playlists
                            id: playlistsTab
                            kind: "playlist"
                            model: playlists.list
                            onClicked: keyRoot.forceActiveFocus()
                            onPickRequested: (tracks) => picker.openFor(tracks)
                            onContextRequested: (tracks, x, y, canRemove) => win.showContext(tracks, x, y, canRemove)
                            onNewPlaylistRequested: picker.openNew([])
                        }

                        BrowseList {
                            onClicked: keyRoot.forceActiveFocus()   // Folders
                            songRows: true
                            table: true
                            columns: settingsStore.songColumns
                            onColumnsRequested: (x, y) => columnMenu.openAt(x, y)
                            id: folderList
                            model: library.folder
                            Connections {
                                target: library
                                function onFolderPathChanged() { folderList.currentIndex = 0 }
                            }
                            onActivated: (row) => {
                                const rows = library.folder.all()
                                const r = rows[row]
                                if (r.isDir) {
                                    library.browse(r.path)
                                    return
                                }
                                player.playTracks([r], 0)
                            }
                            onEnqueued: (row) => {
                                const r = library.folder.get(row)
                                if (!r.isDir)
                                    player.enqueue(r)
                            }
                            onMenuRequested: (row) => {
                                const r = library.folder.get(row)
                                if (!r.isDir)
                                    picker.openFor([r])
                            }
                            onContextRequested: (row, x, y) => {
                                const r = library.folder.get(row)
                                if (!r.isDir)
                                    win.showContext([r], x, y, false)
                            }
                        }

                        BrowseList {
                            onClicked: keyRoot.forceActiveFocus()   // Queue
                            songRows: true
                            table: true
                            columns: settingsStore.songColumns
                            onColumnsRequested: (x, y) => columnMenu.openAt(x, y)
                            model: player.queue
                            playingRow: player.currentIndex
                            onActivated: (row) => player.jumpTo(row)
                            onMenuRequested: (row) => picker.openFor([player.queue.get(row)])
                            onContextRequested: (row, x, y) => win.showContext([player.queue.get(row)], x, y, false)
                        }

                        SettingsForm { }   // Settings

                        BrowseList {
                            onClicked: keyRoot.forceActiveFocus()   // Search results
                            songRows: true
                            table: true
                            columns: settingsStore.songColumns
                            onColumnsRequested: (x, y) => columnMenu.openAt(x, y)
                            model: library.results
                            onActivated: (row) => player.playTracks([library.results.get(row)], 0)
                            onEnqueued: (row) => player.enqueue(library.results.get(row))
                            onMenuRequested: (row) => picker.openFor([library.results.get(row)])
                            onContextRequested: (row, x, y) => win.showContext([library.results.get(row)], x, y, false)
                        }
                    }
                }
            }

            // The line between the columns and the player.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 2
                color: Qt.alpha(theme.accent, 0.35)
            }

            // bottom playback bar
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: S.sp(84)
                color: theme.sunken

                UiText {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.rightMargin: S.sp(10)
                    anchors.topMargin: S.sp(2)
                    px: 11
                    color: theme.dimForeground
                    elide: Text.ElideRight
                    width: parent.width - S.sp(20)
                    horizontalAlignment: Text.AlignRight
                    text: player.error !== "" ? player.error
                          : library.scanning ? qsTr("Scanning…")
                          : subsonic.status.indexOf("Server offline") === 0 || subsonic.status.indexOf("Syncing") === 0 ? subsonic.status : ""
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: S.sp(10)
                    spacing: S.sp(14)

                    Rectangle {
                        Layout.preferredWidth: S.sp(64)
                        Layout.preferredHeight: S.sp(64)
                        color: theme.surface
                        Image {
                            anchors.fill: parent
                            source: { library.coverRevision; return library.coverFor(player.current.path ?? "") }
                            fillMode: Image.PreserveAspectCrop
                            sourceSize.width: 128
                            sourceSize.height: 128
                            asynchronous: true
                        }
                    }

                    Column {
                        Layout.preferredWidth: S.sp(220)
                        UiText {
                            width: parent.width
                            text: player.current.title ?? qsTr("Nothing playing")
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        UiText {
                            width: parent.width
                            text: player.current.artist ?? ""
                            color: theme.dimForeground
                            elide: Text.ElideRight
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: S.sp(2)
                        Row {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: S.sp(8)
                            TransportButton {
                                icon: "shuffle"
                                active: player.shuffle
                                onClicked: { player.shuffle = !player.shuffle; keyRoot.forceActiveFocus() }
                            }
                            TransportButton { icon: "previous"; onClicked: { player.previous(); keyRoot.forceActiveFocus() } }
                            TransportButton { glyph: player.playing ? "❚❚" : "▶"; big: true; onClicked: { player.togglePause(); keyRoot.forceActiveFocus() } }
                            TransportButton { icon: "next"; onClicked: { player.next(); keyRoot.forceActiveFocus() } }
                            TransportButton {
                                icon: player.repeat === "one" ? "repeatOne" : "repeat"
                                active: player.repeat !== "off"
                                onClicked: { player.cycleRepeat(); keyRoot.forceActiveFocus() }
                            }
                            TransportButton {   // a server song: star it
                                icon: "heart"
                                visible: (player.current.path ?? "").indexOf("subsonic:") === 0
                                active: { library.starredRevision; return library.isStarred(player.current.path ?? "") }
                                onClicked: { subsonic.toggleStar(player.current.path ?? ""); keyRoot.forceActiveFocus() }
                            }
                            TransportButton {
                                icon: "eq"
                                active: player.eqPreset !== "Flat"
                                onClicked: eqPopup.open()
                            }
                        }
                        RowLayout {
                            // A fixed length, however wide the window is.
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: S.sp(460)
                            Layout.maximumWidth: S.sp(460)
                            UiText { text: Fmt.time(player.position); color: theme.dimForeground; px: 12 }
                            SeekBar {
                                Layout.fillWidth: true
                                value: player.duration > 0 ? player.position / player.duration : 0
                                onSeeked: (f) => { player.seekTo(f * player.duration); keyRoot.forceActiveFocus() }
                            }
                            UiText { text: Fmt.time(player.duration); color: theme.dimForeground; px: 12 }
                        }
                    }

                    RowLayout {
                        Layout.preferredWidth: S.sp(200)
                        Layout.maximumWidth: S.sp(200)
                        UiText { text: qsTr("vol"); color: theme.dimForeground; px: 12 }
                        SeekBar {
                            Layout.fillWidth: true
                            value: player.volume / 100
                            onSeeked: (f) => { player.volume = Math.round(f * 100); keyRoot.forceActiveFocus() }
                        }
                        UiText {
                            Layout.preferredWidth: S.sp(40)
                            horizontalAlignment: Text.AlignRight
                            text: player.volume + "%"
                            color: theme.dimForeground
                            px: 12
                        }
                    }
                }
            }
        }
    }

    PlaylistPicker { id: picker }
    ColumnMenu { id: columnMenu; onClosed: keyRoot.forceActiveFocus() }
    SongMenu {
        id: contextMenu
        onAddToPlaylist: (tracks) => picker.openFor(tracks)
        onGoToArtist: (path) => win.goTo("Artists", artistsTab, library.artistKeyFor(path))
        onGoToAlbum: (path) => win.goTo("Albums", albumsTab, library.albumKeyFor(path))
        onRemoveFromPlaylist: { const l = win.currentList(); if (l.removeSelected) l.removeSelected() }
        onClosed: keyRoot.forceActiveFocus()
    }
    EqualizerPopup { id: eqPopup }
}
