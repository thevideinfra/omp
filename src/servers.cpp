#include "servers.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include "paths.h"
#include "settings.h"

Servers::Servers(const QString &file, QObject *parent)
    : QObject(parent)
    , m_file(file)
{
    QFile f(m_file);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    m_nextNumber = qMax(1, root.value("next").toInt(1));
    for (const QJsonValue &v : root.value("servers").toArray()) {
        const QJsonObject o = v.toObject();
        ServerConfig c;
        c.id = o.value("id").toString();
        c.url = o.value("url").toString();
        c.user = o.value("user").toString();
        c.type = o.value("type").toString();
        c.enabled = o.value("enabled").toBool();
        if (c.id.isEmpty())
            continue;
        m_servers.append(c);
        const int n = c.id.mid(1).toInt();
        m_nextNumber = qMax(m_nextNumber, n + 1);
    }
}

QString Servers::defaultPath()
{
    return Paths::configDir() + QStringLiteral("/servers.json");
}

ServerConfig Servers::get(const QString &id) const
{
    for (const ServerConfig &c : m_servers)
        if (c.id == id)
            return c;
    return {};
}

QString Servers::cleanUrl(const QString &url)
{
    QString u = url.trimmed();
    if (!u.startsWith(QLatin1String("http://")) && !u.startsWith(QLatin1String("https://")))
        return {};
    while (u.endsWith(QLatin1Char('/')))
        u.chop(1);
    return u;
}

QString Servers::add(const QString &url, const QString &user, bool enabled)
{
    const QString u = cleanUrl(url), name = user.trimmed();
    if (u.isEmpty() || name.isEmpty())
        return {};
    ServerConfig c;
    c.id = QStringLiteral("s%1").arg(m_nextNumber++);
    c.url = u;
    c.user = name;
    c.enabled = enabled;
    m_servers.append(c);
    save();
    emit changed();
    return c.id;
}

bool Servers::update(const QString &id, const QString &url, const QString &user, bool enabled)
{
    const QString u = cleanUrl(url), name = user.trimmed();
    if (u.isEmpty() || name.isEmpty())
        return false;
    for (ServerConfig &c : m_servers) {
        if (c.id != id)
            continue;
        c.url = u;
        c.user = name;
        c.enabled = enabled;
        save();
        emit changed();
        return true;
    }
    return false;
}

void Servers::setType(const QString &id, const QString &type)
{
    for (ServerConfig &c : m_servers) {
        if (c.id == id && c.type != type) {
            c.type = type;
            save();
            emit changed();
            return;
        }
    }
}

bool Servers::remove(const QString &id)
{
    for (int i = 0; i < m_servers.size(); ++i) {
        if (m_servers.at(i).id == id) {
            m_servers.removeAt(i);
            save();
            emit changed();
            return true;
        }
    }
    return false;
}

bool Servers::migrateFrom(Settings &settings)
{
    if (!m_servers.isEmpty() || settings.serverUrl().isEmpty() || settings.serverUser().isEmpty())
        return false;
    const QString type = settings.serverType();
    const QString id = add(settings.serverUrl(), settings.serverUser(), settings.serverEnabled());
    if (id.isEmpty())
        return false;
    setType(id, type);
    settings.set("serverUrl", QString());
    settings.set("serverUser", QString());
    settings.set("serverType", QString());
    settings.set("serverEnabled", false);
    return true;
}

void Servers::save() const
{
    QJsonArray list;
    for (const ServerConfig &c : m_servers) {
        list.append(QJsonObject{{"id", c.id}, {"url", c.url}, {"user", c.user}, {"type", c.type}, {"enabled", c.enabled}});
    }
    const QJsonObject root{{"next", m_nextNumber}, {"servers", list}};
    if (!QDir().mkpath(QFileInfo(m_file).absolutePath())) {
        qWarning() << "cannot create the folder for" << m_file;
        return;
    }
    QSaveFile f(m_file);
    if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(root).toJson()) < 0 || !f.commit())
        qWarning() << "cannot write" << m_file;
}
