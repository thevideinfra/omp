import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import "Fmt.js" as Fmt
import "Scale.js" as S

ApplicationWindow {
    id: win
    objectName: "mainWindow"
    title: "OMP"
    width: 1000
    height: 640
    minimumWidth: 520
    minimumHeight: 360
    visible: false
    color: theme.background

    readonly property var tabNames: ["Albums", "Artists", "Songs", "Playlists", "Libraries", "Queue", "Settings"]
    function isTab(name) { return tabNames[tab] === name }
    readonly property bool drillTab: isTab("Albums") || isTab("Artists") || isTab("Playlists")   // list on the left, songs on the right
    property int tab: 0
    // The tabs the left panel shows (Settings always); Tab cycles through these.
    readonly property var shownTabs: tabNames.map((n, i) => i).filter(i => tabNames[i] === "Settings" || settingsStore.hiddenTabs.indexOf(tabNames[i]) < 0)
    function ensureShownTab() {
        if (shownTabs.indexOf(tab) < 0) tab = shownTabs[0]
    }
    Connections { target: settingsStore; function onChanged() { win.ensureShownTab() } }
    Component.onCompleted: ensureShownTab()
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

    // Up/Down moves the selection; with Shift, in the Queue, it moves the selected song itself.
    function moveSelection(delta, shift) {
        const l = currentList()
        if (shift && isTab("Queue") && !searching) {
            const from = l.currentIndex, to = from + delta
            if (to >= 0 && to < player.queue.count) {
                player.moveRow(from, to)
                l.currentIndex = to
                l.positionViewAtIndex(to, ListView.Contain)
            }
            return
        }
        l.move(delta)
    }

    function showContext(tracks, x, y, canRemove, folderPath, queueRow) {
        contextMenu.queueRow = queueRow ?? -1
        contextMenu.queueCount = player.queue.count
        contextMenu.tracks = tracks
        contextMenu.canRemove = canRemove
        contextMenu.folderPath = folderPath ?? ""
        contextMenu.canGoArtist = settingsStore.hiddenTabs.indexOf("Artists") < 0   // a hidden tab is not a place to go to
        contextMenu.canGoAlbum = settingsStore.hiddenTabs.indexOf("Albums") < 0
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
        case Qt.Key_Tab: {
            const at = win.shownTabs.indexOf(win.tab)
            win.tab = win.shownTabs[(at + 1) % win.shownTabs.length]
            searchField.text = ""
            break
        }
        case Qt.Key_J:
        case Qt.Key_Down: win.moveSelection(1, e.modifiers & Qt.ShiftModifier); break
        case Qt.Key_K:
        case Qt.Key_Up: win.moveSelection(-1, e.modifiers & Qt.ShiftModifier); break
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
            if (win.isTab("Libraries")) library.up()
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
                        Row {
                            anchors.centerIn: parent
                            spacing: S.sp(8)
                            TabIcon {
                                anchors.verticalCenter: parent.verticalCenter
                                width: S.sp(30)
                                height: S.sp(30)
                                kind: "logo"
                                ink: theme.accent
                                ink2: theme.foreground
                            }
                            UiText {
                                visible: !rail.collapsed
                                anchors.verticalCenter: parent.verticalCenter
                                text: "OMP"
                                color: theme.accent
                                px: 18
                            }
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
                            model: win.tabNames.filter(n => n !== "Settings" && settingsStore.hiddenTabs.indexOf(n) < 0)
                            delegate: RailRow {
                                required property string modelData
                                width: parent.width
                                tabIndex: win.tabNames.indexOf(modelData)
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

                        // Libraries: at the top, add a music folder; inside one, go back and see where you are.
                        ChipButton {
                            visible: win.isTab("Libraries") && !win.searching && library.folderPath === ""
                            Layout.preferredHeight: searchField.height
                            text: qsTr("+ Add folder…")
                            onClicked: addFolderDialog.open()
                        }
                        ChipButton {
                            visible: win.isTab("Libraries") && !win.searching && library.folderPath !== ""
                            Layout.preferredHeight: searchField.height
                            text: qsTr("← Back")
                            onClicked: { library.up(); keyRoot.forceActiveFocus() }
                        }
                        UiText {
                            visible: win.isTab("Libraries") && !win.searching && library.folderPath !== ""
                            Layout.maximumWidth: S.sp(360)
                            text: library.folderPath
                            color: theme.dimForeground
                            elide: Text.ElideMiddle
                            px: 12
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
                            onClicked: keyRoot.forceActiveFocus()   // Folders: names and paths, no table header
                            songRows: true
                            id: folderList
                            model: library.folder
                            footerComponent: serverCard
                            slotIcon: "libraries"
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
                                if (r.isDir)   // a folder: its songs; a music folder at the top can also be removed
                                    win.showContext(library.tracksUnder(r.path), x, y, false, library.folderPath === "" ? r.path : "")
                                else
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
                            onContextRequested: (row, x, y) => win.showContext([player.queue.get(row)], x, y, false, "", row)
                            removable: true
                            reorderable: true
                            onRemoveRequested: (row) => { player.removeAt(row); currentIndex = Math.min(row, player.queue.count - 1) }
                            onMoveRequested: (from, to) => { player.moveRow(from, to); currentIndex = to }
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
                          : subsonic.status.indexOf("Server offline") === 0 || subsonic.status.indexOf("Syncing") === 0 ? subsonic.status
                          : player.sleepAfterSong ? qsTr("Sleep: after this song")
                          : player.sleepRemaining >= 0 ? qsTr("Sleep in %1").arg(Fmt.time(player.sleepRemaining)) : ""
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
                                id: sleepButton
                                icon: "sleep"
                                active: player.sleepRemaining >= 0 || player.sleepAfterSong
                                onClicked: {
                                    const p = sleepButton.mapToItem(null, 0, 0)
                                    sleepMenu.openAt(p.x, p.y - sleepMenu.implicitHeight - S.sp(6))
                                    keyRoot.forceActiveFocus()
                                }
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

    FolderDialog {
        id: addFolderDialog
        title: qsTr("Add a music folder")
        onAccepted: { settingsStore.addFolder(selectedFolder.toString()); keyRoot.forceActiveFocus() }
    }
    // The Subsonic server, listed with the music folders for status only: greyed out when it is not connected.
    Component {
        id: serverCard
        Item {
            id: card
            readonly property bool shown: library.folderPath === "" && settingsStore.serverUrl !== ""
            readonly property string link: subsonic.state
            readonly property bool live: link === "connected" || link === "syncing"
            // The server software the last ping reported (remembered, so it shows when the server is down).
            readonly property string software: {
                const t = settingsStore.serverType
                const known = { "navidrome": "Navidrome", "airsonic": "Airsonic", "airsonic-advanced": "Airsonic-Advanced",
                                "gonic": "Gonic", "subsonic": "Subsonic" }
                return t === "" ? qsTr("Subsonic server") : (known[t] ?? t.charAt(0).toUpperCase() + t.slice(1))
            }
            readonly property string host: settingsStore.serverUrl.replace(/^https?:\/\//, "").replace(/\/$/, "")
            width: folderList.width
            height: shown ? S.sp(64) : 0
            visible: shown
            opacity: live ? 1 : 0.5
            Rectangle {
                anchors.fill: parent
                anchors.topMargin: S.sp(8)
                color: Qt.alpha(theme.foreground, 0.045)
                Rectangle { width: 3; height: parent.height; color: card.live ? theme.accent : Qt.alpha(theme.foreground, 0.3) }
                Rectangle {   // the same icon slot as a folder row, so the two line up
                    id: cardIcon
                    x: S.sp(8)
                    anchors.verticalCenter: parent.verticalCenter
                    width: folderList.artSize
                    height: folderList.artSize
                    radius: S.sp(4)
                    color: Qt.alpha(theme.foreground, 0.06)
                    TabIcon {
                        anchors.centerIn: parent
                        width: parent.width * 0.5
                        height: width
                        kind: "server"
                        ink: card.live ? theme.accent : Qt.alpha(theme.foreground, 0.55)
                    }
                }
                Column {
                    anchors { left: cardIcon.right; leftMargin: S.sp(10); right: pill.left; rightMargin: S.sp(10); verticalCenter: parent.verticalCenter }
                    UiText { width: parent.width; text: card.software + "  ·  " + card.host; elide: Text.ElideRight }
                    UiText {
                        width: parent.width
                        text: card.link === "off" ? qsTr("Switched off in Settings") : subsonic.status
                        color: theme.dimForeground
                        px: 12
                        elide: Text.ElideRight
                    }
                }
                Rectangle {
                    id: pill
                    anchors { right: parent.right; rightMargin: S.sp(12); verticalCenter: parent.verticalCenter }
                    width: pillText.implicitWidth + S.sp(16)
                    height: pillText.implicitHeight + S.sp(6)
                    radius: height / 2
                    color: "transparent"
                    border.width: 1
                    border.color: card.live ? Qt.alpha(theme.accent, 0.6) : Qt.alpha(theme.foreground, 0.3)
                    UiText {
                        id: pillText
                        anchors.centerIn: parent
                        text: card.link === "connected" ? qsTr("connected") : card.link === "syncing" ? qsTr("syncing")
                              : card.link === "offline" ? qsTr("offline") : qsTr("off")
                        color: card.live ? theme.accent : theme.dimForeground
                        px: 11
                    }
                }
            }
        }
    }
    SleepMenu { id: sleepMenu; onClosed: keyRoot.forceActiveFocus() }
    PlaylistPicker { id: picker }
    ColumnMenu { id: columnMenu; onClosed: keyRoot.forceActiveFocus() }
    SongMenu {
        id: contextMenu
        onNewPlaylist: (tracks) => picker.openNew(tracks)
        onAddToPlaylistNamed: (name, tracks) => playlists.addPaths(name, tracks.map(t => t.path))
        onGoToArtist: (path) => win.goTo("Artists", artistsTab, library.artistKeyFor(path))
        onGoToAlbum: (path) => win.goTo("Albums", albumsTab, library.albumKeyFor(path))
        onRemoveFolder: (path) => settingsStore.removeFolder(path)
        onRemoveFromPlaylist: { const l = win.currentList(); if (l.removeSelected) l.removeSelected() }
        onClosed: keyRoot.forceActiveFocus()
    }
    EqualizerPopup { id: eqPopup }
}
