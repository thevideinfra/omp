#include "ytmsource.h"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSaveFile>
#include <QTimer>

#include <memory>

namespace {
const QString kAccount = QStringLiteral("ytmusic@youtube");
}

YtmSource::YtmSource(Library &library, Playlists &playlists, Secrets &secrets, const QString &helper, QObject *parent)
    : QObject(parent)
    , m_searchSongs(Track::roleNames())
    , m_searchAlbums({"key", "title", "subtitle", "meta", "cover"})
    , m_history(Track::roleNames())
    , m_library(library)
    , m_playlists(playlists)
    , m_secrets(secrets)
    , m_helper(helper)
{
}

void YtmSource::setState(const QString &state, const QString &status)
{
    if (state == m_state && status == m_status)
        return;
    m_state = state;
    m_status = status;
    emit changed();
}

QString YtmSource::streamUrl(const QString &path)
{
    return Track::isYoutubePath(path) ? QStringLiteral("https://music.youtube.com/watch?v=") + Track::songIdOf(path) : QString();
}

void YtmSource::start()
{
    m_library.loadRemoteCache(QString::fromLatin1(kId));
    m_secrets.lookup(kAccount, [this](const QString &login, bool found) {
        if (!found || login.isEmpty()) {
            setState(QStringLiteral("off"), QString());
            return;
        }
        m_loginJson = login;
        sync();
    });
}

void YtmSource::runHelper(const QString &command, const QStringList &args, const QByteArray &input,
                          std::function<void(const QByteArray &)> lineRead, std::function<void(const Result &)> done)
{
    auto *p = new QProcess(this);
    auto buffer = std::make_shared<QByteArray>();
    auto all = std::make_shared<QByteArray>();
    auto finish = [this, p, all, done](int code, const QString &startError) {
        Result r;
        r.exitCode = code;
        r.out = *all;
        const QByteArray err = p->readAllStandardError().trimmed();
        const QJsonObject o = QJsonDocument::fromJson(err.split('\n').last()).object();
        r.error = startError.isEmpty() ? (o.contains("error") ? o.value("error").toString() : QString::fromUtf8(err)) : startError;
        r.errorCode = !startError.isEmpty() ? QStringLiteral("missing") : o.value("code").toString();
        p->deleteLater();
        done(r);
    };
    connect(p, &QProcess::readyReadStandardOutput, this, [p, buffer, all, lineRead] {
        const QByteArray chunk = p->readAllStandardOutput();
        all->append(chunk);
        buffer->append(chunk);
        int nl;
        while ((nl = buffer->indexOf('\n')) >= 0) {
            const QByteArray line = buffer->left(nl);
            buffer->remove(0, nl + 1);
            if (lineRead && !line.trimmed().isEmpty())
                lineRead(line);
        }
    });
    connect(p, &QProcess::errorOccurred, this, [p, finish](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            finish(-1, tr("The helper omp-ytm could not be started"));
    });
    connect(p, &QProcess::finished, this, [p, buffer, all, lineRead, finish](int code, QProcess::ExitStatus) {
        all->append(p->readAllStandardOutput());   // the rest, with no newline at the end
        if (lineRead && !buffer->trimmed().isEmpty())
            lineRead(*buffer);
        finish(code, QString());
    });
    p->start(m_helper, QStringList{command} + args);
    p->write(input);
    p->closeWriteChannel();
}

void YtmSource::findBrowsers()
{
    runHelper(QStringLiteral("browsers"), {}, {}, nullptr, [this](const Result &r) {
        m_browsers.clear();
        if (r.exitCode == 0)
            for (const QJsonValue &v : QJsonDocument::fromJson(r.out).array())
                m_browsers << v.toObject().toVariantMap();
        emit browsersChanged();
    });
}

void YtmSource::loginWithBrowser(const QString &spec)
{
    if (!spec.startsWith(QLatin1String("browser:")) || m_running)
        return;
    m_loginJson = spec;
    m_unsaved = true;   // kept in the keyring once a sync has worked
    sync();
}

void YtmSource::login(const QString &pasted)
{
    if (pasted.trimmed().isEmpty())
        return;
    setState(QStringLiteral("syncing"), tr("Logging in"));
    runHelper(QStringLiteral("login"), {}, pasted.toUtf8(), nullptr, [this](const Result &r) {
        if (r.exitCode != 0) {
            setState(r.errorCode == QLatin1String("missing") ? QStringLiteral("missing") : QStringLiteral("off"),
                     r.error.isEmpty() ? tr("The pasted text is not usable") : r.error);
            return;
        }
        m_loginJson = QString::fromUtf8(r.out).trimmed();
        m_unsaved = true;
        sync();
    });
}

void YtmSource::sync()
{
    if (m_loginJson.isEmpty() || m_running)
        return;
    m_running = true;
    setState(QStringLiteral("syncing"), tr("Syncing"));
    auto tracks = std::make_shared<QVector<Track>>();
    auto lists = std::make_shared<QList<PlaylistData>>();
    const auto onLine = [this, tracks, lists](const QByteArray &line) {
        const QJsonObject o = QJsonDocument::fromJson(line).object();
        const QString kind = o.value("kind").toString();
        if (kind == QLatin1String("track")) {
            bool ok = false;
            const Track t = Track::fromJsonLine(line, &ok);
            if (ok)
                tracks->append(t);
        } else if (kind == QLatin1String("playlist")) {
            PlaylistData l;
            l.name = o.value("name").toString();
            l.id = o.value("id").toString();
            l.editable = o.value("editable").toBool();
            const QJsonArray paths = o.value("paths").toArray(), ids = o.value("set_ids").toArray();
            for (int i = 0; i < paths.size(); ++i)
                l.items.append({paths.at(i).toString(), i < ids.size() ? ids.at(i).toString() : QString()});
            lists->append(l);
        } else if (kind == QLatin1String("progress")) {
            setState(QStringLiteral("syncing"), tr("Syncing %1/%2").arg(o.value("done").toInt()).arg(o.value("total").toInt()));
        }
    };
    runHelper(QStringLiteral("sync"), {}, m_loginJson.toUtf8(), onLine, [this, tracks, lists](const Result &r) {
        m_running = false;
        if (r.exitCode == 0) {
            if (m_unsaved)
                m_secrets.store(kAccount, m_loginJson);
            m_unsaved = false;
            finishSync(*tracks, *lists);
            return;
        }
        if (m_unsaved) {   // a login that has never worked is dropped, not kept for later
            m_unsaved = false;
            m_loginJson.clear();
            setState(r.errorCode == QLatin1String("missing") ? QStringLiteral("missing") : QStringLiteral("off"),
                     r.error.isEmpty() ? tr("That login does not work") : r.error);
            return;
        }
        // The cached songs stay in every case.
        if (r.errorCode == QLatin1String("auth"))
            setState(QStringLiteral("expired"), m_loginJson.startsWith(QLatin1String("browser:"))
                                                    ? tr("Not signed in: open music.youtube.com in that browser and sign in")
                                                    : tr("Login expired: log in again"));
        else if (r.errorCode == QLatin1String("missing"))
            setState(QStringLiteral("missing"), r.error);
        else
            setState(QStringLiteral("offline"), tr("YouTube Music cannot be read: %1").arg(r.error));
    });
}

void YtmSource::finishSync(const QVector<Track> &tracks, const QList<PlaylistData> &lists)
{
    const QString id = QString::fromLatin1(kId);
    m_library.setRemoteTracks(id, tracks);
    m_library.writeRemoteCache(id);
    m_data = lists;
    publish();
    setState(QStringLiteral("connected"), tracks.isEmpty() ? tr("Connected, but the account has no songs")
                                                          : tr("Connected: %1 songs").arg(tracks.size()));
    refreshHistory(true);   // the Home page's "Listen again" follows
}

void YtmSource::publish()
{
    QList<Playlists::ServerList> lists;
    for (const PlaylistData &p : std::as_const(m_data)) {
        Playlists::ServerList l;
        l.name = p.name;
        l.id = p.id;
        l.serverId = QString::fromLatin1(kId);
        l.source = QStringLiteral("YouTube Music");
        l.readOnly = !p.editable;   // only the playlists of your own can be changed
        for (const Item &i : p.items)
            l.paths << i.path;
        lists.append(l);
    }
    m_playlists.setServerLists(QString::fromLatin1(kId), lists);
}

void YtmSource::setMessage(const QString &message)
{
    if (message == m_message)
        return;
    m_message = message;
    emit messageChanged();
    if (!message.isEmpty())   // it is a passing note: it goes after a while unless something newer replaced it
        QTimer::singleShot(8000, this, [this, message] {
            if (m_message == message) {
                m_message.clear();
                emit messageChanged();
            }
        });
}

QStringList YtmSource::videoIds(const QStringList &paths)
{
    QStringList ids;
    for (const QString &p : paths) {
        const QString normalized = Track::normalized(p);
        const QString id = Track::songIdOf(normalized);
        if (Track::isYoutubePath(normalized) && !id.startsWith(QLatin1String("album:")) && !ids.contains(id))   // an album stand-in is no song
            ids << id;
    }
    return ids;
}

YtmSource::PlaylistData *YtmSource::playlist(const QString &id)
{
    for (PlaylistData &p : m_data)
        if (p.id == id)
            return &p;
    return nullptr;
}

void YtmSource::edit(const QString &command, const QStringList &args, std::function<void(const QJsonObject &)> ok,
                     const QString &failure, std::function<void()> undo)
{
    runHelper(command, args, m_loginJson.toUtf8(), nullptr, [this, ok, failure, undo](const Result &r) {
        if (r.exitCode == 0) {
            if (ok)
                ok(QJsonDocument::fromJson(r.out.split('\n').first()).object());
            return;
        }
        if (undo)
            undo();
        setMessage(failure + QStringLiteral(": ") + (r.error.isEmpty() ? tr("YouTube Music said no") : r.error));
        if (r.errorCode == QLatin1String("auth"))
            setState(QStringLiteral("expired"), tr("Login expired: log in again"));
        else
            sync();   // what is shown goes back to what YouTube has
    });
}

void YtmSource::createPlaylist(const QString &, const QString &name, const QStringList &paths)
{
    const QString title = name.trimmed();
    if (title.isEmpty())
        return;
    if (!loggedIn()) {
        setMessage(tr("Log in first"));
        return;
    }
    const QStringList ids = videoIds(paths);
    setMessage(tr("Creating “%1”…").arg(title));
    edit(QStringLiteral("playlist-create"), QStringList{title} + ids, [this, title, ids](const QJsonObject &o) {
        PlaylistData p;
        p.id = o.value("id").toString();
        p.name = title;
        p.editable = true;
        const QJsonObject setIds = o.value("set_ids").toObject();
        for (const QString &id : ids)
            p.items.append({Track::remotePath(QString::fromLatin1(kId), id), setIds.value(id).toString()});
        m_data.append(p);
        publish();
        setMessage(tr("Created “%1”").arg(title));
    }, tr("Cannot create the playlist"));
}

void YtmSource::renamePlaylist(const QString &, const QString &playlistId, const QString &name)
{
    const QString title = name.trimmed();
    PlaylistData *p = playlist(playlistId);
    if (!p || title.isEmpty() || !p->editable)
        return;
    if (!loggedIn()) {
        setMessage(tr("Log in first"));
        return;
    }
    p->name = title;   // at once; a refusal brings the old name back through the sync
    publish();
    edit(QStringLiteral("playlist-rename"), {playlistId, title}, {}, tr("Cannot rename the playlist"));
}

void YtmSource::deletePlaylist(const QString &, const QString &playlistId)
{
    PlaylistData *p = playlist(playlistId);
    if (!p || !p->editable)
        return;
    if (!loggedIn()) {
        setMessage(tr("Log in first"));
        return;
    }
    for (int i = 0; i < m_data.size(); ++i)
        if (m_data.at(i).id == playlistId)
            m_data.removeAt(i);
    publish();
    edit(QStringLiteral("playlist-delete"), {playlistId}, {}, tr("Cannot delete the playlist"));
}

void YtmSource::addToPlaylist(const QString &, const QString &playlistId, const QStringList &paths)
{
    PlaylistData *p = playlist(playlistId);
    if (!p || !p->editable)
        return;
    if (!loggedIn()) {
        setMessage(tr("Log in first"));
        return;
    }
    const QStringList ids = videoIds(paths);
    if (ids.isEmpty()) {
        setMessage(tr("Only YouTube Music songs can go in a YouTube Music playlist"));
        return;
    }
    const QString title = p->name;
    setMessage(tr("Adding to “%1”…").arg(title));
    edit(QStringLiteral("playlist-add"), QStringList{playlistId} + ids, [this, playlistId, ids, title](const QJsonObject &o) {
        PlaylistData *target = playlist(playlistId);   // looked up again: the lists may have been refreshed meanwhile
        if (!target)
            return;
        const QJsonObject setIds = o.value("set_ids").toObject();
        int added = 0;
        for (const QString &id : ids) {
            if (!setIds.contains(id))
                continue;   // a song that was already there is skipped by YouTube Music
            target->items.append({Track::remotePath(QString::fromLatin1(kId), id), setIds.value(id).toString()});
            ++added;
        }
        publish();
        setMessage(added == 0 ? tr("Already in “%1”").arg(title) : tr("Added %n song(s) to “%1”", nullptr, added).arg(title));
    }, tr("Cannot add to the playlist"));
}

void YtmSource::removeFromPlaylist(const QString &, const QString &playlistId, int index)
{
    PlaylistData *p = playlist(playlistId);
    if (!p || !p->editable || index < 0 || index >= p->items.size())
        return;
    if (!loggedIn()) {
        setMessage(tr("Log in first"));
        return;
    }
    const Item item = p->items.at(index);
    if (item.setId.isEmpty()) {   // YouTube Music only removes a song by the id it gave it in this playlist
        setMessage(tr("Cannot remove this song yet: press Refresh on the YouTube Music card first"));
        return;
    }
    p->items.removeAt(index);
    publish();
    edit(QStringLiteral("playlist-remove"), {playlistId, Track::songIdOf(item.path) + QLatin1Char('/') + item.setId}, {},
         tr("Cannot remove the song"));
}

void YtmSource::toggleLike(const QString &rawPath)
{
    const QString path = Track::normalized(rawPath);
    const QStringList ids = videoIds({path});
    if (ids.isEmpty())
        return;
    if (!loggedIn()) {
        setMessage(tr("Log in first"));
        return;
    }
    const bool now = !m_library.isStarred(path);
    m_library.setStarred(path, now);   // the heart changes at once
    edit(now ? QStringLiteral("like") : QStringLiteral("unlike"), ids, {}, now ? tr("Cannot like the song") : tr("Cannot unlike the song"),
         [this, path, now] { m_library.setStarred(path, !now); });
}

void YtmSource::likeAlbum(const QString &browseId)
{
    if (browseId.isEmpty())
        return;
    if (!loggedIn()) {
        setMessage(tr("Log in first"));
        return;
    }
    setMessage(tr("Adding the album to your library…"));
    edit(QStringLiteral("like-album"), {browseId}, [this](const QJsonObject &) { setMessage(tr("Added the album to your YouTube Music library")); },
         tr("Cannot add the album"));
}

bool YtmSource::canShowMore() const
{
    return !m_searching && m_lastLimit > 0 && m_lastLimit < kMostResults
           && qMax(m_searchSongs.rowCount(), m_searchAlbums.rowCount()) >= m_lastLimit;
}

void YtmSource::refreshHistory(bool force)
{
    constexpr qint64 kFreshMs = 10 * 60 * 1000;
    if (m_loginJson.isEmpty() || m_historyBusy)
        return;
    if (!force && m_historyAt > 0 && QDateTime::currentMSecsSinceEpoch() - m_historyAt < kFreshMs)
        return;
    m_historyBusy = true;
    auto tracks = std::make_shared<QVector<Track>>();
    const auto onLine = [tracks](const QByteArray &line) {
        bool ok = false;
        const Track t = Track::fromJsonLine(line, &ok);
        if (ok)
            tracks->append(t);
    };
    runHelper(QStringLiteral("history"), {}, m_loginJson.toUtf8(), onLine, [this, tracks](const Result &r) {
        m_historyBusy = false;
        if (r.exitCode != 0)
            return;   // what was there stays; the Home page just keeps its last row
        m_historyAt = QDateTime::currentMSecsSinceEpoch();
        m_library.setTransientTracks(QStringLiteral("history"), *tracks);
        QVector<QVariantMap> rows;
        for (const Track &t : std::as_const(*tracks))
            rows.append(t.toMap());
        m_history.setRows(rows);
    });
}

void YtmSource::search(const QString &query, int limit)
{
    const QString q = query.trimmed();
    if (q.isEmpty())
        return;
    if (m_loginJson.isEmpty()) {
        m_searchStatus = tr("Log in first");
        emit searchChanged();
        return;
    }
    const int run = ++m_searchRun;
    m_lastQuery = q;
    m_lastLimit = limit;
    m_searching = true;
    m_searchStatus = tr("Searching…");
    emit searchChanged();
    auto songs = std::make_shared<QVector<Track>>();
    struct Album { QString id, title, artist, type, cover; int year = 0; };
    auto albums = std::make_shared<QVector<Album>>();
    const auto onLine = [songs, albums](const QByteArray &line) {
        const QJsonObject o = QJsonDocument::fromJson(line).object();
        if (o.value("kind").toString() == QLatin1String("track")) {
            bool ok = false;
            const Track t = Track::fromJsonLine(line, &ok);
            if (ok)
                songs->append(t);
        } else if (o.value("kind").toString() == QLatin1String("album")) {
            albums->append({o.value("id").toString(), o.value("title").toString(), o.value("artist").toString(),
                            o.value("type").toString(), o.value("cover_id").toString(), o.value("year").toInt()});
        }
    };
    runHelper(QStringLiteral("search"), {q, QString::number(limit)}, m_loginJson.toUtf8(), onLine, [this, run, songs, albums](const Result &r) {
        if (run != m_searchRun)
            return;   // a newer search is under way
        m_searching = false;
        if (r.exitCode != 0) {
            if (r.errorCode == QLatin1String("auth"))
                setState(QStringLiteral("expired"), tr("Login expired: log in again"));
            m_searchStatus = r.errorCode == QLatin1String("auth") ? tr("Login expired: log in again")
                                                                  : tr("Search failed: %1").arg(r.error);
            emit searchChanged();
            return;
        }
        // The albums have no song to show a picture for, so each gets a stand-in track that carries the cover.
        QVector<Track> known = *songs;
        QVector<QVariantMap> songRows, albumRows;
        for (const Track &t : std::as_const(*songs))
            songRows.append(t.toMap());
        for (const Album &a : std::as_const(*albums)) {
            Track stand;
            stand.path = QStringLiteral("ytm:album:") + a.id;
            stand.title = a.title;
            stand.artist = stand.albumArtist = a.artist;
            stand.album = a.title;
            stand.year = a.year;
            stand.coverId = a.cover;
            known.append(stand);
            albumRows.append({{"key", a.id},
                              {"title", a.title},
                              {"subtitle", a.year > 0 ? QStringLiteral("%1  ·  %2").arg(a.artist).arg(a.year) : a.artist},
                              {"meta", a.type},
                              {"cover", stand.path}});
        }
        m_library.setTransientTracks(QStringLiteral("search"), known);
        m_searchSongs.setRows(songRows);
        m_searchAlbums.setRows(albumRows);
        m_searchStatus = songRows.isEmpty() && albumRows.isEmpty() ? tr("Nothing found") : QString();
        emit searchChanged();
    });
}

void YtmSource::fetchAlbum(const QString &browseId, const QString &purpose)
{
    if (m_loginJson.isEmpty() || browseId.isEmpty()) {
        emit albumFetched(browseId, purpose, {});
        return;
    }
    auto tracks = std::make_shared<QVector<Track>>();
    const auto onLine = [tracks](const QByteArray &line) {
        bool ok = false;
        const Track t = Track::fromJsonLine(line, &ok);
        if (ok)
            tracks->append(t);
    };
    runHelper(QStringLiteral("album"), {browseId}, m_loginJson.toUtf8(), onLine, [this, browseId, purpose, tracks](const Result &r) {
        QVariantList out;
        if (r.exitCode == 0) {
            m_library.setTransientTracks(QStringLiteral("album:") + browseId, *tracks);   // so covers and queues find them
            for (const Track &t : std::as_const(*tracks))
                out << t.toMap();
        } else {
            m_searchStatus = tr("Cannot read the album: %1").arg(r.error);
            emit searchChanged();
            setMessage(tr("Cannot read the album: %1").arg(r.error.isEmpty() ? tr("nothing came back") : r.error));   // so a click is never silent
        }
        emit albumFetched(browseId, purpose, out);
    });
}

void YtmSource::fetchArtist(const QString &channelId)
{
    if (m_loginJson.isEmpty() || channelId.isEmpty()) {
        emit artistFetched(channelId, {});
        return;
    }
    struct Release { QString id, title, artist, type, cover, section; int year = 0; };
    auto tracks = std::make_shared<QVector<Track>>();
    auto releases = std::make_shared<QVector<Release>>();
    auto head = std::make_shared<QJsonObject>();
    const auto onLine = [tracks, releases, head](const QByteArray &line) {
        const QJsonObject o = QJsonDocument::fromJson(line).object();
        const QString kind = o.value("kind").toString();
        if (kind == QLatin1String("track")) {
            bool ok = false;
            const Track t = Track::fromJsonLine(line, &ok);
            if (ok)
                tracks->append(t);
        } else if (kind == QLatin1String("album")) {
            releases->append({o.value("id").toString(), o.value("title").toString(), o.value("artist").toString(), o.value("type").toString(),
                              o.value("cover_id").toString(), o.value("section").toString(), o.value("year").toInt()});
        } else if (kind == QLatin1String("artist")) {
            *head = o;
        }
    };
    runHelper(QStringLiteral("artist"), {channelId}, m_loginJson.toUtf8(), onLine, [this, channelId, tracks, releases, head](const Result &r) {
        if (r.exitCode != 0) {
            setMessage(tr("Cannot read the artist: %1").arg(r.error));
            emit artistFetched(channelId, {});
            return;
        }
        QVector<Track> known = *tracks;
        QVariantList albums, singles;
        for (const Release &a : std::as_const(*releases)) {
            Track stand;   // a picture for the card, through a stand-in song that is never listed
            stand.path = QStringLiteral("ytm:album:") + a.id;
            stand.title = stand.album = a.title;
            stand.artist = stand.albumArtist = a.artist;
            stand.year = a.year;
            stand.coverId = a.cover;
            known.append(stand);
            const QVariantMap card{{"key", a.id}, {"title", a.title}, {"subtitle", a.year > 0 ? QString::number(a.year) : QString()},
                                   {"meta", a.type}, {"cover", stand.path}};
            (a.section == QLatin1String("singles") ? singles : albums) << card;
        }
        m_library.setTransientTracks(QStringLiteral("artist:") + channelId, known);
        QVariantList songs;
        for (const Track &t : std::as_const(*tracks))
            songs << t.toMap();
        emit artistFetched(channelId, {{"name", head->value("name").toString()}, {"description", head->value("description").toString()},
                                       {"coverId", head->value("cover_id").toString()}, {"tracks", songs}, {"albums", albums}, {"singles", singles}});
    });
}

void YtmSource::startRadio(const QString &path)
{
    const QStringList ids = videoIds({path});
    if (ids.isEmpty())
        return;
    if (!loggedIn()) {
        setMessage(tr("Log in first"));
        return;
    }
    setMessage(tr("Starting a radio…"));
    auto tracks = std::make_shared<QVector<Track>>();
    const auto onLine = [tracks](const QByteArray &line) {
        bool ok = false;
        const Track t = Track::fromJsonLine(line, &ok);
        if (ok)
            tracks->append(t);
    };
    runHelper(QStringLiteral("radio"), {ids.first(), QStringLiteral("50")}, m_loginJson.toUtf8(), onLine, [this, tracks](const Result &r) {
        if (r.exitCode != 0 || tracks->isEmpty()) {
            setMessage(tr("Cannot start the radio: %1").arg(r.error.isEmpty() ? tr("nothing came back") : r.error));
            return;
        }
        m_library.setTransientTracks(QStringLiteral("radio"), *tracks);
        QVariantList out;
        for (const Track &t : std::as_const(*tracks))
            out << t.toMap();
        setMessage(tr("Radio: %1 songs").arg(tracks->size()));
        emit radioFetched(out);
    });
}

void YtmSource::logout()
{
    const QString id = QString::fromLatin1(kId);
    m_loginJson.clear();
    m_secrets.clear(kAccount);
    m_library.setRemoteTracks(id, {});
    m_library.removeRemoteCache(id);
    m_data.clear();
    publish();
    m_history.setRows({});
    m_library.setTransientTracks(QStringLiteral("history"), {});
    m_historyAt = 0;
    setState(QStringLiteral("off"), QString());
}

bool YtmSource::fetchCover(const QString &coverId, const QString &dest)
{
    const QUrl url(coverId);
    if (!url.isValid() || url.scheme() != QLatin1String("https"))
        return false;
    QNetworkReply *reply = m_net.get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, coverId, dest] {
        reply->deleteLater();
        const QByteArray bytes = reply->error() == QNetworkReply::NoError ? reply->readAll() : QByteArray();
        if (bytes.isEmpty() || !QDir().mkpath(QFileInfo(dest).absolutePath()))
            return;
        QByteArray copy = bytes;
        QBuffer buffer(&copy);
        buffer.open(QIODevice::ReadOnly);
        if (!QImageReader(&buffer).canRead())   // an error page is not a cover
            return;
        QSaveFile f(dest);
        if (f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size() && f.commit())
            m_library.coverFetched(coverId);
    });
    return true;
}
