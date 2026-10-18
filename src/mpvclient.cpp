#include "mpvclient.h"

#include <QJsonDocument>
#include <QJsonObject>

MpvClient::MpvClient(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, &QLocalSocket::connected, this, &MpvClient::connected);
    connect(&m_socket, &QLocalSocket::disconnected, this, &MpvClient::disconnected);
    connect(&m_socket, &QLocalSocket::readyRead, this, &MpvClient::onReadyRead);
}

void MpvClient::connectTo(const QString &socketPath)
{
    m_socket.abort();
    m_buffer.clear();
    m_nextObserveId = 1;
    m_socket.connectToServer(socketPath);
}

void MpvClient::command(const QJsonArray &args)
{
    if (!isConnected())
        return;
    const QJsonObject message{{"command", args}};
    m_socket.write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
}

void MpvClient::observe(const QString &property)
{
    command({QStringLiteral("observe_property"), m_nextObserveId++, property});
}

void MpvClient::setProperty(const QString &name, const QJsonValue &value)
{
    command({QStringLiteral("set_property"), name, value});
}

void MpvClient::onReadyRead()
{
    m_buffer += m_socket.readAll();
    int nl;
    while ((nl = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(nl);
        m_buffer.remove(0, nl + 1);
        const QJsonObject o = QJsonDocument::fromJson(line).object();
        if (o.value("event").toString() == QLatin1String("property-change"))
            emit propertyChanged(o.value("name").toString(), o.value("data").toVariant());
    }
}
