#include "subsonicclient.h"

#include <QCryptographicHash>
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
    q.addQueryItem("c", QStringLiteral("omamusic"));
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
    call(QStringLiteral("ping"), {}, [this](bool ok, const QJsonObject &, const QString &message) {
        emit pingDone(ok, message);
    });
}
