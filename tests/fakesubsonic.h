#pragma once

#include <functional>

#include <QCryptographicHash>
#include <QMap>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

// A tiny Subsonic server for tests: GET /rest/<method> with the salted-token login
// checked, answering from canned bodies (or a handler), on 127.0.0.1.
class FakeSubsonic : public QObject
{
    Q_OBJECT
public:
    explicit FakeSubsonic(QObject *parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *s = m_server.nextPendingConnection())
                connect(s, &QTcpSocket::readyRead, this, [this, s] { onData(s); });
        });
        m_server.listen(QHostAddress::LocalHost, 0);
    }

    QString url() const { return QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort()); }
    void setLogin(const QString &user, const QString &password) { m_user = user; m_password = password; }
    // The JSON "subsonic-response" payload members for a method, e.g. {"artists":{...}}.
    void setResult(const QString &method, const QByteArray &membersJson) { m_results.insert(method, membersJson); }
    // The members for a method computed from the request (paging, ids).
    void setHandler(const QString &method, std::function<QByteArray(const QUrlQuery &)> handler)
    {
        m_handlers.insert(method, handler);
    }
    // Serves getAlbumList2 (paged) and getAlbum for albums with the given song counts.
    void serveAlbums(const QMap<QString, int> &songsPerAlbum)
    {
        setHandler("getAlbumList2", [songsPerAlbum](const QUrlQuery &q) {
            const int off = q.queryItemValue("offset").toInt(), size = q.queryItemValue("size").toInt();
            const QStringList ids = songsPerAlbum.keys();
            QStringList items;
            for (int i = off; i < qMin(int(ids.size()), off + size); ++i)
                items << QStringLiteral("{\"id\":\"%1\"}").arg(ids.at(i));
            return QByteArray("\"albumList2\":{\"album\":[") + items.join(',').toUtf8() + "]}";
        });
        setHandler("getAlbum", [songsPerAlbum](const QUrlQuery &q) {
            const QString id = q.queryItemValue("id");
            QStringList songs;
            for (int i = 1; i <= songsPerAlbum.value(id); ++i)
                songs << QStringLiteral("{\"id\":\"%1-%2\",\"title\":\"T%2\",\"artist\":\"Amy\",\"album\":\"%1\",\"track\":%2,\"coverArt\":\"%1\"}").arg(id).arg(i);
            return QByteArray("\"album\":{\"id\":\"") + id.toUtf8() + "\",\"song\":[" + songs.join(',').toUtf8() + "]}";
        });
    }
    // A raw body (a cover, a stream) with its content type.
    void setBinary(const QString &method, const QByteArray &body, const QByteArray &type = "image/jpeg")
    {
        m_binary.insert(method, {body, type});
    }
    // Do not answer this method (to test timeouts).
    void ignore(const QString &method) { m_ignored.insert(method); }

    QList<QUrl> requests;
    int count(const QString &method) const
    {
        int n = 0;
        for (const QUrl &u : requests)
            n += u.path() == QStringLiteral("/rest/") + method;
        return n;
    }

    static QByteArray ok(const QByteArray &members = {})
    {
        return "{\"subsonic-response\":{\"status\":\"ok\",\"version\":\"1.16.1\""
               + (members.isEmpty() ? QByteArray() : "," + members) + "}}";
    }
    static QByteArray failed(int code, const QByteArray &message)
    {
        return "{\"subsonic-response\":{\"status\":\"failed\",\"version\":\"1.16.1\",\"error\":{\"code\":"
               + QByteArray::number(code) + ",\"message\":\"" + message + "\"}}}";
    }

private:
    void onData(QTcpSocket *s)
    {
        m_buffers[s] += s->readAll();
        if (!m_buffers[s].contains("\r\n\r\n"))
            return;
        const QByteArray line = m_buffers.take(s).split('\r').first();   // GET /rest/ping?... HTTP/1.1
        const QUrl url(QStringLiteral("http://x") + QString::fromUtf8(line.split(' ').value(1)));
        requests << url;
        const QString method = url.path().mid(QStringLiteral("/rest/").size());
        if (m_ignored.contains(method))
            return;
        const QUrlQuery q(url);
        QByteArray body, type = "application/json";
        const QString salt = q.queryItemValue("s");
        const QByteArray want = QCryptographicHash::hash((m_password + salt).toUtf8(), QCryptographicHash::Md5).toHex();
        if (q.queryItemValue("u") != m_user || q.queryItemValue("t").toUtf8() != want) {
            body = failed(40, "Wrong username or password");
        } else if (m_binary.contains(method)) {
            body = m_binary.value(method).first;
            type = m_binary.value(method).second;
        } else if (m_handlers.contains(method)) {
            const QByteArray members = m_handlers.value(method)(q);
            body = members.startsWith("ERR:") ? failed(70, members.mid(4)) : ok(members);
        } else if (m_results.contains(method)) {
            body = ok(m_results.value(method));
        } else {
            body = failed(70, "Not found");
        }
        s->write("HTTP/1.1 200 OK\r\nContent-Type: " + type + "\r\nContent-Length: "
                 + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
        s->disconnectFromHost();
    }

    QTcpServer m_server;
    QString m_user = QStringLiteral("amy"), m_password = QStringLiteral("sesame");
    QMap<QString, QByteArray> m_results;
    QMap<QString, std::function<QByteArray(const QUrlQuery &)>> m_handlers;
    QMap<QString, QPair<QByteArray, QByteArray>> m_binary;
    QSet<QString> m_ignored;
    QHash<QTcpSocket *, QByteArray> m_buffers;
};
