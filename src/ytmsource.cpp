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

void YtmSource::runHelper(const QString &command, const QByteArray &input,
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
    p->start(m_helper, {command});
    p->write(input);
    p->closeWriteChannel();
}

void YtmSource::login(const QString &pasted)
{
    if (pasted.trimmed().isEmpty())
        return;
    setState(QStringLiteral("syncing"), tr("Logging in"));
    runHelper(QStringLiteral("login"), pasted.toUtf8(), nullptr, [this](const Result &r) {
        if (r.exitCode != 0) {
            setState(r.errorCode == QLatin1String("missing") ? QStringLiteral("missing") : QStringLiteral("off"),
                     r.error.isEmpty() ? tr("The pasted text is not usable") : r.error);
            return;
        }
        m_loginJson = QString::fromUtf8(r.out).trimmed();
        m_secrets.store(kAccount, m_loginJson);
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
    runHelper(QStringLiteral("sync"), m_loginJson.toUtf8(), onLine, [this, tracks, lists](const Result &r) {
        m_running = false;
        if (r.exitCode == 0) {
            finishSync(*tracks, *lists);
            return;
        }
        // The cached songs stay in every case.
        if (r.errorCode == QLatin1String("auth"))
            setState(QStringLiteral("expired"), tr("Login expired: paste fresh request headers"));
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
