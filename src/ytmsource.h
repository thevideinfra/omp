#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>

#include "library.h"
#include "rowsmodel.h"
#include "playlists.h"
#include "secrets.h"

// YouTube Music: the account's songs and playlists, read by the omp-ytm helper (ytmusicapi), kept in
// the library as the source "ytm" with the cached ones at once and a background sync. The login (the
// request headers of a signed-in browser) lives in the keyring. Songs play through mpv and yt-dlp.
class YtmSource : public QObject
{
    Q_OBJECT
    // "off" (no login), "syncing", "connected", "expired" (the login no longer works), "offline" or
    // "missing" (the helper, ytmusicapi or yt-dlp is not installed).
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool syncing READ syncing NOTIFY changed)
    // The browser profiles a login can be taken from: label and spec ("browser:<browser>:<folder>").
    Q_PROPERTY(QVariantList browsers READ browsers NOTIFY browsersChanged)
    // The last search: song rows (the library's song roles) and album rows (key, title, subtitle, meta, cover).
    Q_PROPERTY(RowsModel *searchSongs READ searchSongs CONSTANT)
    Q_PROPERTY(RowsModel *searchAlbums READ searchAlbums CONSTANT)
    // The songs played lately on YouTube Music, most recent first (the Home page's "Listen again").
    Q_PROPERTY(RowsModel *history READ history CONSTANT)
    Q_PROPERTY(bool searching READ searching NOTIFY searchChanged)
    Q_PROPERTY(QString searchStatus READ searchStatus NOTIFY searchChanged)
    // The last search came back as full as asked for, and a bigger one is allowed.
    Q_PROPERTY(bool canShowMore READ canShowMore NOTIFY searchChanged)
    // True with a login (changes can be made); and the latest word on a change ("Added 2 songs to Mix", or why it failed).
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)

public:
    static constexpr const char *kId = "ytm";

    YtmSource(Library &library, Playlists &playlists, Secrets &secrets, const QString &helper,
              QObject *parent = nullptr);

    QString state() const { return m_state; }
    QString status() const { return m_status; }
    bool syncing() const { return m_state == QLatin1String("syncing"); }

    // Shows the cached songs, then syncs when there is a login.
    void start();
    // The address mpv plays for a song of this source.
    static QString streamUrl(const QString &path);
    // Downloads a cover (its id is the picture's address) to a file; false when it is not an address.
    bool fetchCover(const QString &coverId, const QString &dest);

    QVariantList browsers() const { return m_browsers; }
    RowsModel *searchSongs() { return &m_searchSongs; }
    RowsModel *searchAlbums() { return &m_searchAlbums; }
    RowsModel *history() { return &m_history; }
    // Reads the play history unless it was read in the last ten minutes (or `force`); needs a login.
    Q_INVOKABLE void refreshHistory(bool force = false);
    bool searching() const { return m_searching; }
    QString searchStatus() const { return m_searchStatus; }
    bool canShowMore() const;
    bool loggedIn() const { return !m_loginJson.isEmpty(); }
    QString message() const { return m_message; }

    // Playlists, as on a Subsonic server (the `serverId` is only there to match, it is always "ytm"). Only YouTube
    // Music songs ("ytm:") go in. They show at once and are undone by a sync when YouTube refuses.
    Q_INVOKABLE void createPlaylist(const QString &serverId, const QString &name, const QStringList &paths);
    Q_INVOKABLE void renamePlaylist(const QString &serverId, const QString &playlistId, const QString &name);
    Q_INVOKABLE void deletePlaylist(const QString &serverId, const QString &playlistId);
    Q_INVOKABLE void addToPlaylist(const QString &serverId, const QString &playlistId, const QStringList &paths);
    Q_INVOKABLE void removeFromPlaylist(const QString &serverId, const QString &playlistId, int index);
    // Likes or unlikes a song (the heart), and puts an album into the YouTube Music library.
    Q_INVOKABLE void toggleLike(const QString &path);
    Q_INVOKABLE void likeAlbum(const QString &browseId);
    // Looks up songs and albums on YouTube Music (the answer fills searchSongs and searchAlbums).
    Q_INVOKABLE void search(const QString &query, int limit = 40);
    // The same search asking for the most results there is (120).
    Q_INVOKABLE void showMore() { search(m_lastQuery, kMostResults); }
    // Reads the songs of an album from YouTube Music; they come back with `albumFetched` (empty on failure).
    // `purpose` is handed back untouched ("play", "queue", "menu"...).
    Q_INVOKABLE void fetchAlbum(const QString &browseId, const QString &purpose);
    // Reads an artist's page (by channel id): { name, description, cover, tracks (top songs), albums, singles } where the
    // albums and singles are card rows (key, title, subtitle, meta, cover). Empty on failure. Comes back with `artistFetched`.
    Q_INVOKABLE void fetchArtist(const QString &channelId);
    // A radio of songs like this one (a YouTube Music song's path); the songs come back with `radioFetched`, the song first.
    Q_INVOKABLE void startRadio(const QString &path);
    // Looks for the browsers on this computer (the answer arrives as `browsers`).
    Q_INVOKABLE void findBrowsers();
    // Logs in with the cookies of a browser profile (a spec from `browsers`), read again on every sync.
    Q_INVOKABLE void loginWithBrowser(const QString &spec);
    // Takes pasted request headers (a "Copy as cURL" from music.youtube.com), keeps the login and syncs.
    Q_INVOKABLE void login(const QString &pasted);
    // Forgets the login, the songs, the cache and the playlists.
    Q_INVOKABLE void logout();
    Q_INVOKABLE void sync();

signals:
    void changed();
    void browsersChanged();
    void searchChanged();
    void messageChanged();
    void albumFetched(const QString &browseId, const QString &purpose, const QVariantList &tracks);
    void artistFetched(const QString &channelId, const QVariantMap &page);
    void radioFetched(const QVariantList &tracks);

private:
    struct Item { QString path, setId; };   // a song of a playlist, and YouTube's id for it in that playlist
    struct PlaylistData { QString id, name; bool editable = false; QList<Item> items; };
    struct Result
    {
        int exitCode = -1;
        QByteArray out;
        QString error;      // the helper's message, or why it could not run
        QString errorCode;  // "auth", "missing" or "other"
    };
    // Runs the helper with `input` on stdin; `lineRead` sees each line of output as it comes.
    void runHelper(const QString &command, const QStringList &args, const QByteArray &input, std::function<void(const QByteArray &)> lineRead,
                   std::function<void(const Result &)> done);
    void setState(const QString &state, const QString &status);
    void finishSync(const QVector<Track> &tracks, const QList<PlaylistData> &lists);
    void publish();
    void setMessage(const QString &message);
    // Runs a change on YouTube Music; `ok(output)` on success. A failure says so and syncs, so what is shown is what is there.
    void edit(const QString &command, const QStringList &args, std::function<void(const QJsonObject &)> ok, const QString &failure,
              std::function<void()> undo = {});
    static QStringList videoIds(const QStringList &paths);   // the YouTube Music songs among them, by video id
    PlaylistData *playlist(const QString &id);

    Library &m_library;
    Playlists &m_playlists;
    Secrets &m_secrets;
    QString m_helper;
    QString m_loginJson;       // what the keyring holds: the pasted login, or a browser spec
    bool m_unsaved = false;    // a login not yet proven by a sync, so not yet in the keyring
    QVariantList m_browsers;
    RowsModel m_searchSongs;
    RowsModel m_searchAlbums;
    RowsModel m_history;
    QHash<QString, QVariantList> m_albumCache;   // albums already read, by browse id (until log out)
    qint64 m_historyAt = 0;     // when it was read, in ms since the epoch
    bool m_historyBusy = false;
    bool m_searching = false;
    QString m_searchStatus;
    static constexpr int kMostResults = 120;
    QString m_lastQuery;
    int m_lastLimit = 0;
    int m_searchRun = 0;       // a newer search makes the answer of an older one worthless
    QString m_state = QStringLiteral("off");
    QString m_status;
    bool m_running = false;
    QList<PlaylistData> m_data;   // the playlists as of the last sync and the changes since
    QString m_message;
    QNetworkAccessManager m_net;
};
