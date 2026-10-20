#include "subsonichub.h"

#include "library.h"
#include "player.h"
#include "playlists.h"
#include "secrets.h"
#include "subsonicclient.h"
#include "subsonicsource.h"

SubsonicHub::SubsonicHub(Servers &servers, Library &library, Playlists &playlists, Secrets &secrets,
                         QObject *parent)
    : QObject(parent)
    , m_servers(servers)
    , m_library(library)
    , m_playlists(playlists)
    , m_secrets(secrets)
    , m_rows({"id", "title", "host", "url", "user", "type", "active", "state", "status"})
{
    connect(&m_servers, &Servers::changed, this, &SubsonicHub::onServersChanged);
    // A server's cover is asked for by its id: hand the request to that server's source.
    m_library.setCoverFetcher([this](const QString &serverId, const QString &coverId, const QString &dest) {
        SubsonicSource *s = m_sources.value(serverId);
        return s && s->fetchCover(coverId, dest);
    });
}

QString SubsonicHub::softwareName(const QString &type)
{
    static const QHash<QString, QString> known{{"navidrome", "Navidrome"}, {"airsonic", "Airsonic"},
                                               {"airsonic-advanced", "Airsonic-Advanced"}, {"gonic", "Gonic"},
                                               {"subsonic", "Subsonic"}};
    if (type.isEmpty())
        return {};
    return known.value(type, type.left(1).toUpper() + type.mid(1));
}

void SubsonicHub::start()
{
    m_started = true;
    onServersChanged();
}

void SubsonicHub::onServersChanged()
{
    if (!m_started)
        return;
    const QList<ServerConfig> list = m_servers.list();
    // Drop the sources of servers that are gone, start the ones that are new.
    for (auto it = m_sources.begin(); it != m_sources.end();) {
        if (m_servers.get(it.key()).id.isEmpty()) {
            it.value()->deleteLater();
            it = m_sources.erase(it);
        } else {
            ++it;
        }
    }
    QStringList fresh;
    for (const ServerConfig &c : list) {
        if (m_sources.contains(c.id))
            continue;
        auto *s = new SubsonicSource(c.id, m_servers, m_library, m_playlists, m_secrets, this);
        connect(s, &SubsonicSource::statusChanged, this, &SubsonicHub::refresh);
        m_sources.insert(c.id, s);
        fresh << c.id;
    }
    // Playlists carry the name of their server's software; two of a kind are numbered.
    QHash<QString, int> total, seen;
    for (const ServerConfig &c : list)
        ++total[softwareName(c.type).isEmpty() ? QStringLiteral("server") : softwareName(c.type)];
    for (const ServerConfig &c : list) {
        const QString base = softwareName(c.type).isEmpty() ? QStringLiteral("server") : softwareName(c.type);
        m_sources.value(c.id)->setLabel(total.value(base) > 1 ? base + QLatin1Char(' ') + QString::number(++seen[base]) : base);
    }
    for (const QString &id : std::as_const(fresh))
        m_sources.value(id)->start();
    refresh();
}

void SubsonicHub::refresh()
{
    QVector<QVariantMap> rows;
    for (const ServerConfig &c : m_servers.list()) {
        SubsonicSource *s = m_sources.value(c.id);
        rows.append({{"id", c.id},
                     {"title", softwareName(c.type).isEmpty() ? tr("Subsonic server") : softwareName(c.type)},
                     {"host", s ? s->host() : c.url},
                     {"url", c.url},
                     {"user", c.user},
                     {"type", c.type},
                     {"active", c.enabled},
                     {"state", s ? s->state() : QStringLiteral("off")},
                     {"status", s ? s->status() : QString()}});
    }
    m_rows.setRows(rows);
    emit statusChanged();
}

QString SubsonicHub::status() const
{
    for (const SubsonicSource *s : m_sources)
        if (s->syncing())
            return s->status();
    for (const SubsonicSource *s : m_sources)
        if (s->state() == QLatin1String("offline"))
            return s->status();
    return {};
}

void SubsonicHub::sync()
{
    for (SubsonicSource *s : std::as_const(m_sources))
        s->sync();
}

void SubsonicHub::attach(Player &player)
{
    const auto feed = [this, &player] {
        const QString path = player.current().value(QStringLiteral("path")).toString();
        for (SubsonicSource *s : std::as_const(m_sources))
            s->noteProgress(path, player.position(), player.duration());
    };
    connect(&player, &Player::currentChanged, this, feed);
    connect(&player, &Player::positionChanged, this, feed);
}

QString SubsonicHub::streamUrl(const QString &path) const
{
    SubsonicSource *s = m_sources.value(Track::serverOf(path));
    return s ? s->streamUrl(Track::songIdOf(path)).toString(QUrl::FullyEncoded) : QString();
}

void SubsonicHub::toggleStar(const QString &path)
{
    if (SubsonicSource *s = m_sources.value(Track::serverOf(path)))
        s->toggleStar(path);
}

void SubsonicHub::testConnection(const QString &url, const QString &user, const QString &password)
{
    auto *probe = new SubsonicClient(this);
    probe->setServer(url, user, password);
    connect(probe, &SubsonicClient::pingDone, this, [this, probe](bool ok, const QString &message) {
        emit testResult(ok, ok ? tr("Connected") : message);
        probe->deleteLater();
    });
    probe->ping();
}

QString SubsonicHub::addServer(const QString &url, const QString &user, const QString &password, bool enabled)
{
    // Added switched off: the source gets its password first, so that its first sync does not wait for the keyring.
    const QString id = m_servers.add(url, user, false);
    if (id.isEmpty())
        return {};
    const ServerConfig c = m_servers.get(id);
    m_secrets.store(account(c), password);
    if (SubsonicSource *s = m_sources.value(id))
        s->setPassword(password, account(c));
    if (enabled)
        m_servers.update(id, c.url, c.user, true);
    return id;
}

bool SubsonicHub::updateServer(const QString &id, const QString &url, const QString &user,
                               const QString &password, bool enabled)
{
    const ServerConfig old = m_servers.get(id);
    if (old.id.isEmpty())
        return false;
    ServerConfig next = old;
    next.url = url.trimmed();
    while (next.url.endsWith(QLatin1Char('/')))
        next.url.chop(1);
    next.user = user.trimmed();
    const bool accountChanged = account(next) != account(old);
    if (!password.isEmpty()) {
        m_secrets.store(account(next), password);
        if (SubsonicSource *s = m_sources.value(id))
            s->setPassword(password, account(next));   // known to the source before the update triggers its sync
    } else if (accountChanged) {
        // No new password: carry the saved one over to the new account name, then apply the change.
        m_secrets.lookup(account(old), [this, id, url, user, enabled, next](const QString &saved, bool found) {
            if (found) {
                m_secrets.store(account(next), saved);
                if (SubsonicSource *s = m_sources.value(id))
                    s->setPassword(saved, account(next));   // so the sync after the change does not wait for the keyring
            }
            m_servers.update(id, url, user, enabled);
        });
        return true;
    }
    return m_servers.update(id, url, user, enabled);
}

void SubsonicHub::setServerEnabled(const QString &id, bool enabled)
{
    const ServerConfig c = m_servers.get(id);
    if (!c.id.isEmpty() && c.enabled != enabled)
        m_servers.update(id, c.url, c.user, enabled);
}

void SubsonicHub::removeServer(const QString &id)
{
    const ServerConfig c = m_servers.get(id);
    if (c.id.isEmpty())
        return;
    m_secrets.clear(account(c));
    if (SubsonicSource *s = m_sources.value(id))
        s->forget();
    m_servers.remove(id);   // the source is dropped with it
}
