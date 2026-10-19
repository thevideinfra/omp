#pragma once

#include <functional>

#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPair>
#include <QUrl>

// Talks to a Subsonic-API server (Navidrome, Airsonic, Subsonic): salted-token
// login, JSON answers, a timeout on every request so a dead server never hangs.
class SubsonicClient : public QObject
{
    Q_OBJECT
public:
    using Params = QList<QPair<QString, QString>>;
    // ok, the "subsonic-response" object, an error message when not ok.
    using Callback = std::function<void(bool, const QJsonObject &, const QString &)>;

    explicit SubsonicClient(QObject *parent = nullptr);

    void setServer(const QString &url, const QString &user, const QString &password);
    void setTimeoutMs(int ms) { m_timeoutMs = ms; }
    bool configured() const { return !m_base.isEmpty() && !m_user.isEmpty(); }

    // md5(password + salt) as lower-case hex.
    static QString token(const QString &password, const QString &salt);

    QUrl endpoint(const QString &method, const Params &extra = {}) const;
    QUrl streamUrl(const QString &id) const;
    QUrl coverUrl(const QString &coverId, int size) const;

    // Runs a method and hands the parsed answer to `done` (always called, once).
    void call(const QString &method, const Params &extra, Callback done);
    // Downloads a URL to memory (covers); `done(ok, bytes)`.
    void download(const QUrl &url, std::function<void(bool, const QByteArray &)> done);

    void ping();

signals:
    void pingDone(bool ok, const QString &message);

private:
    QNetworkAccessManager m_nam;
    QString m_base;
    QString m_user;
    QString m_password;
    QString m_salt;
    int m_timeoutMs = 15000;
};
