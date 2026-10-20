#include "subsonicsource.h"

#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QRegularExpression>
#include <QSaveFile>

SubsonicSource::SubsonicSource(const QString &serverId, Servers &servers, Library &library, Playlists &playlists,
                               Secrets &secrets, QObject *parent)
    : QObject(parent)
    , m_id(serverId)
    , m_servers(servers)
    , m_library(library)
    , m_playlists(playlists)
    , m_secrets(secrets)
{
    m_client.setServerId(m_id);
    connect(&m_client, &SubsonicClient::serverInfo, this, [this](const QString &type, const QString &) {
        m_servers.setType(m_id, type);
    });
    connect(&m_client, &SubsonicClient::libraryProgress, this, [this](int done, int total) {
        setStatus(tr("Syncing %1/%2").arg(done).arg(total), true);
    });
    connect(&m_client, &SubsonicClient::libraryFetched, this, [this](const QVector<Track> &tracks) {
        m_library.setRemoteTracks(m_id, tracks);
        m_library.writeRemoteCache(m_id);
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
    connect(&m_scrobbles, &ScrobbleTracker::nowPlaying, this, [this](const QString &id) {
        if (config().enabled && m_client.configured())
            m_client.scrobble(id, false);
    });
    connect(&m_scrobbles, &ScrobbleTracker::submit, this, [this](const QString &id) {
        if (config().enabled && m_client.configured())
            m_client.scrobble(id, true);
    });
    connect(&m_servers, &Servers::changed, this, &SubsonicSource::onServersChanged);
    const ServerConfig c = config();
    m_signature = QStringLiteral("%1|%2|%3").arg(c.enabled).arg(c.url, c.user);
}

QString SubsonicSource::host() const
{
    QString u = config().url;
    u.remove(QRegularExpression(QStringLiteral("^https?://")));
    return u;
}

QString SubsonicSource::account() const
{
    return config().user + QLatin1Char('@') + config().url;
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

void SubsonicSource::setLabel(const QString &label)
{
    if (label == m_label)
        return;
    m_label = label;
    publishLists(m_serverPlaylists);
}

void SubsonicSource::start()
{
    m_library.loadRemoteCache(m_id);
    sync();
}

void SubsonicSource::setPassword(const QString &password, const QString &forAccount)
{
    m_password = password;
    m_passwordFor = forAccount;
}

void SubsonicSource::noteProgress(const QString &path, double position, double duration)
{
    m_scrobbles.update(Track::serverOf(path) == m_id ? path : QString(), position, duration);
}

bool SubsonicSource::fetchCover(const QString &coverId, const QString &dest)
{
    if (!m_client.configured())
        return false;   // asked again once the server is set up
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
}

void SubsonicSource::toggleStar(const QString &rawPath)
{
    const QString path = Track::normalized(rawPath);
    if (Track::serverOf(path) != m_id || !m_client.configured())
        return;
    const bool now = !m_library.isStarred(path);
    m_library.setStarred(path, now);
    publishLists(m_serverPlaylists);   // Favorites follows
    m_client.setStarred(Track::songIdOf(path), now, [this, path, now](bool ok, const QString &err) {
        if (ok)
            return;
        m_library.setStarred(path, !now);   // the server refused: put it back
        publishLists(m_serverPlaylists);
        setStatus(tr("Could not %1 the song: %2").arg(now ? tr("star") : tr("unstar"), err), false, m_state);
    });
}

void SubsonicSource::onServersChanged()
{
    const ServerConfig c = config();
    if (c.id.isEmpty())
        return;   // removed: the hub is about to drop this source
    const QString now = QStringLiteral("%1|%2|%3").arg(c.enabled).arg(c.url, c.user);
    if (now == m_signature)
        return;
    m_signature = now;
    sync();
}

void SubsonicSource::sync()
{
    if (m_syncing)
        return;
    const ServerConfig c = config();
    if (!c.enabled || c.url.isEmpty() || c.user.isEmpty()) {
        setStatus({});
        return;
    }
    setStatus(tr("Connecting…"), true);
    if (m_passwordFor == account()) {
        fetchWith(m_password);
        return;
    }
    const QString asked = account();
    m_secrets.lookup(asked, [this, asked](const QString &password, bool found) {
        if (account() != asked)
            return;   // the server was edited meanwhile: a newer sync is on its way
        if (!found) {
            setStatus(tr("Server password not found in the keyring: save the server again"), false, QStringLiteral("offline"));
            return;
        }
        m_password = password;
        m_passwordFor = asked;
        fetchWith(password);
    });
}

// The server's playlists, then "Favorites" (its starred songs), after the local ones.
void SubsonicSource::publishLists(const QVariantList &serverPlaylists)
{
    m_serverPlaylists = serverPlaylists;
    QList<Playlists::ServerList> lists;
    for (const QVariant &v : serverPlaylists) {
        Playlists::ServerList s;
        s.name = v.toMap().value("name").toString();
        s.paths = v.toMap().value("paths").toStringList();
        s.id = v.toMap().value("id").toString();
        s.serverId = m_id;
        s.source = m_label;
        s.readOnly = false;   // the server's own playlists can be edited; Favorites below cannot
        lists.append(s);
    }
    const QStringList starred = m_library.starredRemotePaths(m_id);
    if (!starred.isEmpty()) {
        Playlists::ServerList s;
        s.name = tr("Favorites");
        s.paths = starred;
        s.serverId = m_id;
        s.source = m_label;
        lists.append(s);
    }
    m_playlists.setServerLists(m_id, lists);
}

QStringList SubsonicSource::ownSongIds(const QStringList &paths) const
{
    QStringList ids;
    for (const QString &p : paths)
        if (Track::serverOf(Track::normalized(p)) == m_id)
            ids << Track::songIdOf(Track::normalized(p));
    return ids;
}

// What to do when an edit has been answered: fetch the playlists again, or say why it failed.
SubsonicClient::PlaylistDone SubsonicSource::afterEdit(const QString &what)
{
    return [this, what](bool ok, const QString &err) {
        if (ok)
            m_client.fetchPlaylists();
        else
            setStatus(tr("Could not %1: %2").arg(what, err), false, m_state);
    };
}

void SubsonicSource::createPlaylist(const QString &name, const QStringList &paths)
{
    if (!m_client.configured())
        return;
    const QStringList ids = ownSongIds(paths);
    m_client.createPlaylist(name, ids, afterEdit(tr("create the playlist")));
}

void SubsonicSource::renamePlaylist(const QString &playlistId, const QString &name)
{
    if (m_client.configured() && !name.trimmed().isEmpty())
        m_client.updatePlaylist(playlistId, name.trimmed(), {}, {}, afterEdit(tr("rename the playlist")));
}

void SubsonicSource::deletePlaylist(const QString &playlistId)
{
    if (m_client.configured())
        m_client.deletePlaylist(playlistId, afterEdit(tr("delete the playlist")));
}

void SubsonicSource::addToPlaylist(const QString &playlistId, const QStringList &paths)
{
    if (!m_client.configured())
        return;
    const QStringList ids = ownSongIds(paths);
    if (ids.isEmpty()) {
        setStatus(tr("Only songs from %1 can go in its playlists").arg(host()), false, m_state);
        return;
    }
    m_client.updatePlaylist(playlistId, QString(), ids, {}, afterEdit(tr("add the songs")));
}

void SubsonicSource::removeFromPlaylist(const QString &playlistId, int index)
{
    if (m_client.configured() && index >= 0)
        m_client.updatePlaylist(playlistId, QString(), {}, {index}, afterEdit(tr("remove the song")));
}

void SubsonicSource::fetchWith(const QString &password)
{
    m_client.setServer(config().url, config().user, password);
    m_client.ping();   // which server software it is, for the Libraries card
    m_client.fetchLibrary();
}

void SubsonicSource::forget()
{
    m_password.clear();
    m_passwordFor.clear();
    m_library.setRemoteTracks(m_id, {});
    m_library.removeRemoteCache(m_id);
    m_playlists.setServerLists(m_id, {});
    setStatus({});
}
