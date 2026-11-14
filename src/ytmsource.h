#pragma once

#include <QNetworkAccessManager>
#include <QObject>

#include "library.h"
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

private:
    struct Result
    {
        int exitCode = -1;
        QByteArray out;
        QString error;      // the helper's message, or why it could not run
        QString errorCode;  // "auth", "missing" or "other"
    };
    // Runs the helper with `input` on stdin; `lineRead` sees each line of output as it comes.
    void runHelper(const QString &command, const QByteArray &input, std::function<void(const QByteArray &)> lineRead,
                   std::function<void(const Result &)> done);
    void setState(const QString &state, const QString &status);
    void finishSync(const QVector<Track> &tracks, const QList<Playlists::ServerList> &lists);
    void publish(const QList<Playlists::ServerList> &lists);

    Library &m_library;
    Playlists &m_playlists;
    Secrets &m_secrets;
    QString m_helper;
    QString m_loginJson;       // what the keyring holds: the pasted login, or a browser spec
    bool m_unsaved = false;    // a login not yet proven by a sync, so not yet in the keyring
    QVariantList m_browsers;
    QString m_state = QStringLiteral("off");
    QString m_status;
    bool m_running = false;
    QList<Playlists::ServerList> m_lists;   // from the last sync
    QNetworkAccessManager m_net;
};
