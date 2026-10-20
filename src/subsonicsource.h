#pragma once

#include <QObject>

#include "library.h"
#include "playlists.h"
#include "scrobbletracker.h"
#include "secrets.h"
#include "servers.h"
#include "subsonicclient.h"

// One Subsonic server: keeps its songs in the library (the cached ones at once, then a
// background sync) and keeps the cache when the server cannot be reached. The
// SubsonicHub runs one of these per server.
class SubsonicSource : public QObject
{
    Q_OBJECT
public:
    SubsonicSource(const QString &serverId, Servers &servers, Library &library, Playlists &playlists,
                   Secrets &secrets, QObject *parent = nullptr);

    QString id() const { return m_id; }
    ServerConfig config() const { return m_servers.get(m_id); }
    // The address without "https://", for display.
    QString host() const;
    QString status() const { return m_status; }
    bool syncing() const { return m_syncing; }
    // "off" (switched off), "syncing", "connected" or "offline".
    QString state() const { return m_state; }
    // What this server's playlists are labelled with in the Playlists tab.
    void setLabel(const QString &label);

    void start();
    void sync();
    // The password to use from now on for the account "user@url" (the hub stores it in the keyring as well).
    void setPassword(const QString &password, const QString &forAccount);
    QUrl streamUrl(const QString &songId) const { return m_client.streamUrl(songId); }
    // Downloads a cover to a file; false when the server is not set up yet.
    bool fetchCover(const QString &coverId, const QString &dest);
    // Stars or unstars one of its songs at once and tells the server; undone if it refuses.
    void toggleStar(const QString &path);
    // The playing song, to scrobble it (a song of another server counts as nothing playing).
    void noteProgress(const QString &path, double position, double duration);
    // Playlist editing on the server (only this server's own songs can go in); the playlists are
    // fetched again after a change, and a refusal shows in the status.
    void createPlaylist(const QString &name, const QStringList &paths);
    void renamePlaylist(const QString &playlistId, const QString &name);
    void deletePlaylist(const QString &playlistId);
    void addToPlaylist(const QString &playlistId, const QStringList &paths);
    void removeFromPlaylist(const QString &playlistId, int index);
    // Forgets what it knows: its songs, cache and playlists (not the keyring; the hub does that).
    void forget();

signals:
    void statusChanged();

private:
    QString account() const;
    void setStatus(const QString &status, bool syncing = false, const QString &state = QString());
    void onServersChanged();
    void fetchWith(const QString &password);
    void publishLists(const QVariantList &serverPlaylists);
    QStringList ownSongIds(const QStringList &paths) const;
    SubsonicClient::PlaylistDone afterEdit(const QString &what);

    QString m_id;
    Servers &m_servers;
    Library &m_library;
    Playlists &m_playlists;
    Secrets &m_secrets;
    SubsonicClient m_client;
    QVariantList m_serverPlaylists;   // what the last sync fetched
    ScrobbleTracker m_scrobbles;
    QString m_label = QStringLiteral("server");
    QString m_state = QStringLiteral("off");
    QString m_status;
    bool m_syncing = false;
    QString m_signature;   // enabled|url|user: a change means the password and library are stale
    QString m_password;
    QString m_passwordFor;   // the account m_password belongs to
};
