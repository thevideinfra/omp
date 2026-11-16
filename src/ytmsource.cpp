#include "ytmsource.h"

#include <QBuffer>
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

#include <memory>

namespace {
const QString kAccount = QStringLiteral("ytmusic@youtube");
}

YtmSource::YtmSource(Library &library, Playlists &playlists, Secrets &secrets, const QString &helper, QObject *parent)
    : QObject(parent)
    , m_searchSongs(Track::roleNames())
    , m_searchAlbums({"key", "title", "subtitle", "meta", "cover"})
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
    auto lists = std::make_shared<QList<Playlists::ServerList>>();
    const auto onLine = [this, tracks, lists](const QByteArray &line) {
        const QJsonObject o = QJsonDocument::fromJson(line).object();
        const QString kind = o.value("kind").toString();
        if (kind == QLatin1String("track")) {
            bool ok = false;
            const Track t = Track::fromJsonLine(line, &ok);
            if (ok)
                tracks->append(t);
        } else if (kind == QLatin1String("playlist")) {
            Playlists::ServerList l;
            l.name = o.value("name").toString();
            l.id = o.value("id").toString();
            l.serverId = QString::fromLatin1(kId);
            l.source = QStringLiteral("YouTube Music");
            l.readOnly = true;
            for (const QJsonValue &v : o.value("paths").toArray())
                l.paths << v.toString();
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

void YtmSource::finishSync(const QVector<Track> &tracks, const QList<Playlists::ServerList> &lists)
{
    const QString id = QString::fromLatin1(kId);
    m_library.setRemoteTracks(id, tracks);
    m_library.writeRemoteCache(id);
    publish(lists);
    setState(QStringLiteral("connected"), tracks.isEmpty() ? tr("Connected, but the account has no songs")
                                                          : tr("Connected: %1 songs").arg(tracks.size()));
}

void YtmSource::publish(const QList<Playlists::ServerList> &lists)
{
    m_lists = lists;
    m_playlists.setServerLists(QString::fromLatin1(kId), lists);
}

bool YtmSource::canShowMore() const
{
    return !m_searching && m_lastLimit > 0 && m_lastLimit < kMostResults
           && qMax(m_searchSongs.rowCount(), m_searchAlbums.rowCount()) >= m_lastLimit;
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
        }
        emit albumFetched(browseId, purpose, out);
    });
}

void YtmSource::logout()
{
    const QString id = QString::fromLatin1(kId);
    m_loginJson.clear();
    m_secrets.clear(kAccount);
    m_library.setRemoteTracks(id, {});
    m_library.removeRemoteCache(id);
    publish({});
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
