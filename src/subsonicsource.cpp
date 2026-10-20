#include "subsonicsource.h"

#include "player.h"

#include <QDebug>
#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QSaveFile>

SubsonicSource::SubsonicSource(Settings &settings, Library &library, Playlists &playlists,
                               Secrets &secrets, SubsonicClient &client, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_library(library)
    , m_playlists(playlists)
    , m_secrets(secrets)
    , m_client(client)
{
    connect(&m_client, &SubsonicClient::serverInfo, this, [this](const QString &type, const QString &) {
        m_settings.set("serverType", type);
    });
    connect(&m_client, &SubsonicClient::libraryProgress, this, [this](int done, int total) {
        setStatus(tr("Syncing %1/%2").arg(done).arg(total), true);
    });
    connect(&m_client, &SubsonicClient::libraryFetched, this, [this](const QVector<Track> &tracks) {
        m_library.setRemoteTracks(tracks);
        m_library.writeRemoteCache();
        setStatus(tracks.isEmpty() ? tr("Connected, but the server has no songs (has it scanned its music folder?)")
                                   : tr("Connected: %1 songs").arg(tracks.size()),
                  false, QStringLiteral("connected"));
        m_client.fetchPlaylists();   // the playlists follow the songs
    });
    connect(&m_client, &SubsonicClient::playlistsFetched, this, [this](const QVariantList &lists) {
        publishLists(lists);
    });
    connect(&m_client, &SubsonicClient::playlistsFailed, this, [this](const QString &message) {
        qWarning() << "cannot fetch the server playlists:" << message;   // the songs are still there
        publishLists(m_serverPlaylists);   // at least Favorites, and the playlists of the last sync
    });
    connect(&m_client, &SubsonicClient::libraryFailed, this, [this](const QString &message) {
        setStatus(tr("Server offline: %1").arg(message), false, QStringLiteral("offline"));   // the cached songs stay
    });
    m_library.setCoverFetcher([this](const QString &coverId, const QString &dest) {
        if (!m_client.configured())
            return false;   // asked again once the server settings are in
        m_client.download(m_client.coverUrl(coverId, 600), [this, coverId, dest](bool ok, const QByteArray &bytes) {
            if (!ok || bytes.isEmpty() || !QDir().mkpath(QFileInfo(dest).absolutePath()))
                return;
            QByteArray copy = bytes;
            QBuffer buffer(&copy);
            buffer.open(QIODevice::ReadOnly);
            if (!QImageReader(&buffer).canRead())   // a server error page or a damaged image is not a cover
                return;
            QSaveFile f(dest);
            if (f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size() && f.commit())
                m_library.coverFetched(coverId);
        });
        return true;
    });
    connect(&m_scrobbles, &ScrobbleTracker::nowPlaying, this, [this](const QString &id) {
        if (m_settings.serverEnabled() && m_client.configured())
            m_client.scrobble(id, false);
    });
    connect(&m_scrobbles, &ScrobbleTracker::submit, this, [this](const QString &id) {
        if (m_settings.serverEnabled() && m_client.configured())
            m_client.scrobble(id, true);
    });
    connect(&m_settings, &Settings::changed, this, &SubsonicSource::onSettingsChanged);
    m_signature = QStringLiteral("%1|%2|%3").arg(m_settings.serverEnabled()).arg(m_settings.serverUrl(), m_settings.serverUser());
}

QString SubsonicSource::account() const
{
    return m_settings.serverUser() + QLatin1Char('@') + m_settings.serverUrl();
}

void SubsonicSource::setStatus(const QString &status, bool syncing, const QString &state)
{
    const QString newState = !state.isEmpty() ? state : syncing ? QStringLiteral("syncing") : QStringLiteral("off");
    if (status == m_status && syncing == m_syncing && newState == m_state)
        return;
    m_status = status;
    m_syncing = syncing;
    m_state = newState;
    emit statusChanged();
}

void SubsonicSource::start()
{
    m_library.loadRemoteCache();
    sync();
}

void SubsonicSource::attach(Player &player)
{
    const auto feed = [this, &player] {
        noteProgress(player.current().value(QStringLiteral("path")).toString(), player.position(), player.duration());
    };
    connect(&player, &Player::currentChanged, this, feed);
    connect(&player, &Player::positionChanged, this, feed);
}

void SubsonicSource::toggleStar(const QString &path)
{
    if (!path.startsWith(QLatin1String("subsonic:")) || !m_client.configured())
        return;
    const bool now = !m_library.isStarred(path);
    m_library.setStarred(path, now);
    publishLists(m_serverPlaylists);   // Favorites follows
    m_client.setStarred(path.mid(9), now, [this, path, now](bool ok, const QString &err) {
        if (ok)
            return;
        m_library.setStarred(path, !now);   // the server refused: put it back
        publishLists(m_serverPlaylists);
        setStatus(tr("Could not %1 the song: %2").arg(now ? tr("star") : tr("unstar"), err), false, m_state);
    });
}

void SubsonicSource::noteProgress(const QString &path, double position, double duration)
{
    m_scrobbles.update(path, position, duration);
}

void SubsonicSource::onSettingsChanged()
{
    const QString now = QStringLiteral("%1|%2|%3").arg(m_settings.serverEnabled()).arg(m_settings.serverUrl(), m_settings.serverUser());
    if (now == m_signature)
        return;
    m_signature = now;
    sync();
}

void SubsonicSource::sync()
{
    if (m_syncing)
        return;
    if (!m_settings.serverEnabled() || m_settings.serverUrl().isEmpty() || m_settings.serverUser().isEmpty()) {
        setStatus({});
        return;
    }
    setStatus(tr("Connecting…"), true);
    if (m_passwordFor == account()) {
        fetchWith(m_password);
        return;
    }
    auto *conn = new QMetaObject::Connection;
    *conn = connect(&m_secrets, &Secrets::looked, this, [this, conn](const QString &password, bool found) {
        disconnect(*conn);
        delete conn;
        if (!found) {
            setStatus(tr("Server password not found in the keyring: save the server again"), false, QStringLiteral("offline"));
            return;
        }
        m_password = password;
        m_passwordFor = account();
        fetchWith(password);
    });
    m_secrets.lookup(account());
}

// The server's playlists, then "Favorites" (the starred songs), after the local ones.
void SubsonicSource::publishLists(const QVariantList &serverPlaylists)
{
    m_serverPlaylists = serverPlaylists;
    QList<Playlists::ServerList> lists;
    for (const QVariant &v : serverPlaylists)
        lists.append({v.toMap().value("name").toString(), v.toMap().value("paths").toStringList()});
    const QStringList starred = m_library.starredRemotePaths();
    if (!starred.isEmpty())
        lists.append({tr("Favorites"), starred});
    m_playlists.setServerLists(lists);
}

void SubsonicSource::fetchWith(const QString &password)
{
    m_client.setServer(m_settings.serverUrl(), m_settings.serverUser(), password);
    m_client.ping();   // which server software it is, for the Libraries card
    m_client.fetchLibrary();
}

void SubsonicSource::testConnection(const QString &url, const QString &user, const QString &password)
{
    auto *probe = new SubsonicClient(this);
    probe->setServer(url, user, password);
    connect(probe, &SubsonicClient::pingDone, this, [this, probe](bool ok, const QString &message) {
        emit testResult(ok, ok ? tr("Connected") : message);
        probe->deleteLater();
    });
    probe->ping();
}

void SubsonicSource::saveServer(const QString &url, const QString &user, const QString &password, bool enabled)
{
    m_password = password;
    m_passwordFor = user + QLatin1Char('@') + url;   // what account() will be once the settings land
    m_secrets.store(m_passwordFor, password);
    m_settings.set("serverUrl", url);
    m_settings.set("serverUser", user);
    m_settings.set("serverEnabled", enabled);
    sync();   // a change in the settings starts one too; a running sync is not doubled
}

void SubsonicSource::remove()
{
    m_secrets.clear(account());
    m_password.clear();
    m_passwordFor.clear();
    m_library.setRemoteTracks({});
    m_library.writeRemoteCache();
    m_playlists.setServerLists({});
    m_settings.set("serverEnabled", false);
    m_settings.set("serverUrl", QString());
    m_settings.set("serverUser", QString());
    setStatus({});
}
