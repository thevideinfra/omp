#include "instance.h"

#include <QDebug>
#include <QLocalSocket>

Instance::Instance(const QString &socketPath, QObject *parent)
    : QObject(parent)
    , m_path(socketPath)
{
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *s = m_server.nextPendingConnection()) {
            auto drain = [this, s] {
                while (s->canReadLine())
                    emit verbReceived(QString::fromUtf8(s->readLine()).trimmed());
            };
            connect(s, &QLocalSocket::readyRead, this, drain);
            connect(s, &QLocalSocket::disconnected, this, [drain, s] {
                drain();
                s->deleteLater();
            });
            drain();
        }
    });
}

bool Instance::claimOrSend(const QString &verb)
{
    QLocalSocket probe;
    probe.connectToServer(m_path);
    if (probe.waitForConnected(300)) {
        probe.write(verb.toUtf8() + '\n');
        probe.waitForBytesWritten(500);
        probe.disconnectFromServer();
        return false;
    }
    QLocalServer::removeServer(m_path);   // stale socket left by a crash
    if (!m_server.listen(m_path))
        qWarning() << "single-instance socket failed:" << m_server.errorString();
    return true;
}
