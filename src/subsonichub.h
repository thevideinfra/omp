#pragma once

#include <QMap>
#include <QObject>

#include "rowsmodel.h"
#include "servers.h"

class Library;
class Player;
class Playlists;
class Secrets;
class SubsonicSource;

// All the Subsonic servers: one SubsonicSource each, kept in step with the Servers list. This is
// what the UI talks to (`subsonic` in QML).
class SubsonicHub : public QObject
{
    Q_OBJECT
    // One row per server: id, title (the software), host, url, user, type, active (switched on), state, status.
    Q_PROPERTY(RowsModel *servers READ servers CONSTANT)
    // What the player bar shows: a sync in progress, or a server that is offline; empty otherwise.
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    SubsonicHub(Servers &servers, Library &library, Playlists &playlists, Secrets &secrets,
                QObject *parent = nullptr);

    RowsModel *servers() { return &m_rows; }
    QString status() const;
    SubsonicSource *source(const QString &id) const { return m_sources.value(id); }

    // Starts every server (its cached songs at once, then a sync).
    void start();
    // Follows what the player plays, to scrobble server songs.
    void attach(Player &player);
    // The stream address of a server song's path, or an empty string.
    QString streamUrl(const QString &path) const;

    Q_INVOKABLE void sync();
    Q_INVOKABLE void toggleStar(const QString &path);
    // Checks a server before it is saved; the answer arrives as `testResult`.
    Q_INVOKABLE void testConnection(const QString &url, const QString &user, const QString &password);
    // Adds a server and keeps its password in the keyring; returns its id (empty when the address or user is not usable).
    Q_INVOKABLE QString addServer(const QString &url, const QString &user, const QString &password, bool enabled);
    // An empty password keeps the saved one.
    Q_INVOKABLE bool updateServer(const QString &id, const QString &url, const QString &user,
                                  const QString &password, bool enabled);
    Q_INVOKABLE void setServerEnabled(const QString &id, bool enabled);
    // Forgets a server: its password, songs, cache and playlists.
    Q_INVOKABLE void removeServer(const QString &id);

signals:
    void statusChanged();
    void testResult(bool ok, const QString &message);

private:
    static QString account(const ServerConfig &c) { return c.user + QLatin1Char('@') + c.url; }
    static QString softwareName(const QString &type);
    void onServersChanged();
    void refresh();

    Servers &m_servers;
    Library &m_library;
    Playlists &m_playlists;
    Secrets &m_secrets;
    QMap<QString, SubsonicSource *> m_sources;
    RowsModel m_rows;
    bool m_started = false;
};
