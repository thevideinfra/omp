#pragma once

#include <QObject>

#include "library.h"
#include "playlists.h"
#include "scrobbletracker.h"
#include "secrets.h"
#include "settings.h"
#include "subsonicclient.h"

// Keeps the server's songs in the library: shows the cached ones at once, syncs
// in the background, and keeps the cache when the server cannot be reached.
class Player;

class SubsonicSource : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool syncing READ syncing NOTIFY statusChanged)

public:
    SubsonicSource(Settings &settings, Library &library, Playlists &playlists, Secrets &secrets,
                   SubsonicClient &client, QObject *parent = nullptr);

    QString status() const { return m_status; }
    bool syncing() const { return m_syncing; }

    void start();
    // Follows what the player plays, to scrobble server songs.
    void attach(Player &player);
    // The playing song's path, position and duration (what attach() feeds; also for tests).
    void noteProgress(const QString &path, double position, double duration);
    Q_INVOKABLE void sync();
    // Checks a server before it is saved; the answer arrives as `testResult`.
    Q_INVOKABLE void testConnection(const QString &url, const QString &user, const QString &password);
    // Stores the settings and the password, then syncs.
    Q_INVOKABLE void saveServer(const QString &url, const QString &user, const QString &password, bool enabled);
    // Forgets the server: password, cached songs and settings.
    Q_INVOKABLE void remove();

signals:
    void statusChanged();
    void testResult(bool ok, const QString &message);

private:
    QString account() const;
    void setStatus(const QString &status, bool syncing = false);
    void onSettingsChanged();
    void fetchWith(const QString &password);
    void publishLists(const QVariantList &serverPlaylists);

    Settings &m_settings;
    Library &m_library;
    Playlists &m_playlists;
    Secrets &m_secrets;
    SubsonicClient &m_client;
    QVariantList m_serverPlaylists;   // what the last sync fetched
    ScrobbleTracker m_scrobbles;
    QString m_status;
    bool m_syncing = false;
    QString m_signature;   // enabled|url|user: a change means the password and library are stale
    QString m_password;
    QString m_passwordFor; // the account m_password belongs to
};
