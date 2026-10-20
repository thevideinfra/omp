#pragma once

#include <QList>
#include <QObject>
#include <QString>

class Settings;

// One Subsonic server OMP can use. The password is not here: it is in the keyring.
struct ServerConfig
{
    QString id;       // "s1", "s2"...: never reused, it names the server's cache and its songs' paths
    QString url;
    QString user;
    QString type;     // the server software its last ping reported ("navidrome"...), may be empty
    bool enabled = false;
};

// The list of servers, kept in servers.json next to config.toml.
class Servers : public QObject
{
    Q_OBJECT
public:
    explicit Servers(const QString &file = defaultPath(), QObject *parent = nullptr);

    static QString defaultPath();

    QList<ServerConfig> list() const { return m_servers; }
    ServerConfig get(const QString &id) const;   // an empty id when there is no such server

    // Returns the new id, or an empty string for an address that is not http(s) or an empty user.
    QString add(const QString &url, const QString &user, bool enabled);
    bool update(const QString &id, const QString &url, const QString &user, bool enabled);
    void setType(const QString &id, const QString &type);
    bool remove(const QString &id);

    // Takes over the single server of the old config.toml keys as "s1", once, when there are no servers yet.
    bool migrateFrom(Settings &settings);

signals:
    void changed();

private:
    static QString cleanUrl(const QString &url);
    void save() const;

    QString m_file;
    QList<ServerConfig> m_servers;
    int m_nextNumber = 1;
};
