#include "subsonicclient.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDateTime>
#include <QJsonArray>
#include <QPointer>
#include <QQueue>
#include <QTimeZone>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QUrlQuery>

namespace {
const char kApiVersion[] = "1.16.1";
}

SubsonicClient::SubsonicClient(QObject *parent)
    : QObject(parent)
{
}

void SubsonicClient::setServer(const QString &url, const QString &user, const QString &password)
{
    m_base = url.trimmed();
    while (m_base.endsWith('/'))
        m_base.chop(1);
    m_user = user;
    m_password = password;
    // One salt per server setting; a fresh random one makes each login token unguessable.
    m_salt = QString::number(QRandomGenerator::global()->generate64(), 16).left(12);
}

QString SubsonicClient::token(const QString &password, const QString &salt)
{
    return QString::fromLatin1(QCryptographicHash::hash((password + salt).toUtf8(), QCryptographicHash::Md5).toHex());
}

QUrl SubsonicClient::endpoint(const QString &method, const Params &extra) const
{
    QUrl url(m_base + QStringLiteral("/rest/") + method);
    QUrlQuery q;
    q.addQueryItem("u", m_user);
    q.addQueryItem("t", token(m_password, m_salt));
    q.addQueryItem("s", m_salt);
    q.addQueryItem("v", QString::fromLatin1(kApiVersion));
    q.addQueryItem("c", QStringLiteral("omp"));
    q.addQueryItem("f", QStringLiteral("json"));
    for (const auto &[key, value] : extra)
        q.addQueryItem(key, value);
    url.setQuery(q);
    return url;
}

QUrl SubsonicClient::streamUrl(const QString &id) const
{
    return endpoint(QStringLiteral("stream"), {{"id", id}});
}

QUrl SubsonicClient::coverUrl(const QString &coverId, int size) const
{
    return endpoint(QStringLiteral("getCoverArt"), {{"id", coverId}, {"size", QString::number(size)}});
}

void SubsonicClient::call(const QString &method, const Params &extra, Callback done)
{
    QNetworkRequest req(endpoint(method, extra));
    req.setTransferTimeout(m_timeoutMs);
    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            done(false, {}, reply->errorString());
            return;
        }
        QJsonParseError err;
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll(), &err).object();
        const QJsonObject resp = root.value("subsonic-response").toObject();
        if (resp.isEmpty()) {
            done(false, {}, tr("Not a Subsonic server"));
            return;
        }
        if (resp.value("status").toString() != QLatin1String("ok")) {
            done(false, resp, resp.value("error").toObject().value("message").toString(tr("Server error")));
            return;
        }
        done(true, resp, {});
    });
}

void SubsonicClient::download(const QUrl &url, std::function<void(bool, const QByteArray &)> done)
{
    QNetworkRequest req(url);
    req.setTransferTimeout(m_timeoutMs);
    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        done(reply->error() == QNetworkReply::NoError, reply->readAll());
    });
}

void SubsonicClient::ping()
{
    call(QStringLiteral("ping"), {}, [this](bool ok, const QJsonObject &resp, const QString &message) {
        if (ok)
            emit serverInfo(resp.value("type").toString().toLower(), resp.value("serverVersion").toString());
        emit pingDone(ok, message);
    });
}

Track SubsonicClient::parseSong(const QJsonObject &o, const QString &serverId)
{
    Track t;
    t.path = Track::remotePath(serverId, o.value("id").toVariant().toString());
    t.title = o.value("title").toString();
    t.artist = o.value("artist").toString();
    if (t.artist.isEmpty())
        t.artist = QStringLiteral("Unknown Artist");
    t.albumArtist = o.value("displayAlbumArtist").toString();
    if (t.albumArtist.isEmpty())
        t.albumArtist = t.artist;
    t.album = o.value("album").toString();
    if (t.album.isEmpty())
        t.album = QStringLiteral("Unknown Album");
    t.track = o.value("track").toInt();
    t.disc = o.value("discNumber").toInt();
    t.year = o.value("year").toInt();
    t.duration = o.value("duration").toDouble();
    t.size = o.value("size").toInteger();
    const QDateTime created = QDateTime::fromString(o.value("created").toString(), Qt::ISODate);
    t.mtime = created.isValid() ? created.toSecsSinceEpoch() : 0;
    t.coverId = o.value("coverArt").toString();
    t.starred = o.contains("starred");
    return t;
}

namespace {
// The state of one library fetch, shared by its callbacks.
struct Fetch
{
    QStringList albumIds;
    int next = 0;
    int inflight = 0;
    int done = 0;
    bool failed = false;
    QVector<QVector<Track>> byAlbum;   // in album order
};
constexpr int kParallel = 4;
}

void SubsonicClient::fetchLibrary()
{
    auto fetch = std::make_shared<Fetch>();
    QPointer<SubsonicClient> self(this);

    // Albums come in pages; then each album's songs, a few requests at a time.
    auto pump = std::make_shared<std::function<void()>>();
    *pump = [self, fetch, pump] {
        if (!self || fetch->failed)
            return;
        while (fetch->inflight < kParallel && fetch->next < fetch->albumIds.size()) {
            const int index = fetch->next++;
            ++fetch->inflight;
            self->call(QStringLiteral("getAlbum"), {{"id", fetch->albumIds.at(index)}},
                       [self, fetch, pump, index](bool ok, const QJsonObject &resp, const QString &err) {
                           if (!self || fetch->failed)
                               return;
                           --fetch->inflight;
                           if (!ok) {
                               fetch->failed = true;
                               emit self->libraryFailed(err);
                               return;
                           }
                           QVector<Track> songs;
                           for (const QJsonValue &s : resp.value("album").toObject().value("song").toArray())
                               songs.append(parseSong(s.toObject(), self->m_serverId));
                           fetch->byAlbum[index] = songs;
                           ++fetch->done;
                           emit self->libraryProgress(fetch->done, fetch->albumIds.size());
                           (*pump)();
                       });
        }
        if (fetch->done == fetch->albumIds.size() && fetch->inflight == 0) {
            QVector<Track> all;
            for (const QVector<Track> &songs : std::as_const(fetch->byAlbum))
                all += songs;
            emit self->libraryFetched(all);
        }
    };

    auto page = std::make_shared<std::function<void(int)>>();
    *page = [self, fetch, pump, page](int offset) {
        if (!self)
            return;
        self->call(QStringLiteral("getAlbumList2"),
                   {{"type", "alphabeticalByName"}, {"size", QString::number(self->m_pageSize)},
                    {"offset", QString::number(offset)}},
                   [self, fetch, pump, page, offset](bool ok, const QJsonObject &resp, const QString &err) {
                       if (!self)
                           return;
                       if (!ok) {
                           emit self->libraryFailed(err);
                           return;
                       }
                       const QJsonArray albums = resp.value("albumList2").toObject().value("album").toArray();
                       for (const QJsonValue &a : albums)
                           fetch->albumIds << a.toObject().value("id").toVariant().toString();
                       if (albums.size() >= self->m_pageSize) {
                           (*page)(offset + albums.size());
                           return;
                       }
                       fetch->byAlbum.resize(fetch->albumIds.size());
                       (*pump)();
                   });
    };
    (*page)(0);
}

void SubsonicClient::fetchPlaylists()
{
    QPointer<SubsonicClient> self(this);
    call(QStringLiteral("getPlaylists"), {}, [self](bool ok, const QJsonObject &resp, const QString &err) {
        if (!self)
            return;
        if (!ok) {
            emit self->playlistsFailed(err);
            return;
        }
        struct Item { QString id, name; };
        auto items = std::make_shared<QList<Item>>();
        for (const QJsonValue &p : resp.value("playlists").toObject().value("playlist").toArray())
            items->append({p.toObject().value("id").toVariant().toString(), p.toObject().value("name").toString()});
        if (items->isEmpty()) {
            emit self->playlistsFetched({});
            return;
        }
        auto result = std::make_shared<QVariantList>();
        auto pending = std::make_shared<int>(items->size());
        auto failed = std::make_shared<bool>(false);
        result->resize(items->size());
        for (int i = 0; i < items->size(); ++i) {
            self->call(QStringLiteral("getPlaylist"), {{"id", items->at(i).id}},
                       [self, items, result, pending, failed, i](bool ok2, const QJsonObject &r, const QString &e) {
                           if (!self || *failed)
                               return;
                           if (!ok2) {
                               *failed = true;
                               emit self->playlistsFailed(e);
                               return;
                           }
                           QStringList paths;
                           for (const QJsonValue &s : r.value("playlist").toObject().value("entry").toArray())
                               paths << parseSong(s.toObject(), self->m_serverId).path;
                           (*result)[i] = QVariantMap{{"name", items->at(i).name}, {"id", items->at(i).id}, {"paths", paths}};
                           if (--*pending == 0)
                               emit self->playlistsFetched(*result);
                       });
        }
    });
}

void SubsonicClient::scrobble(const QString &id, bool submission)
{
    call(QStringLiteral("scrobble"),
         {{"id", id}, {"submission", submission ? QStringLiteral("true") : QStringLiteral("false")}},
         [id](bool ok, const QJsonObject &, const QString &err) {
             if (!ok)
                 qWarning() << "scrobble failed for" << id << err;
         });
}

void SubsonicClient::setStarred(const QString &id, bool starred, std::function<void(bool, const QString &)> done)
{
    call(starred ? QStringLiteral("star") : QStringLiteral("unstar"), {{"id", id}},
         [done](bool ok, const QJsonObject &, const QString &err) { done(ok, err); });
}
