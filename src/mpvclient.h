#pragma once

#include <QJsonArray>
#include <QJsonValue>
#include <QLocalSocket>
#include <QObject>
#include <QVariant>

class MpvClient : public QObject
{
    Q_OBJECT
public:
    explicit MpvClient(QObject *parent = nullptr);

    void connectTo(const QString &socketPath);
    bool isConnected() const { return m_socket.state() == QLocalSocket::ConnectedState; }
    void command(const QJsonArray &args);
    void observe(const QString &property);
    void setProperty(const QString &name, const QJsonValue &value);

signals:
    void connected();
    void disconnected();
    void propertyChanged(const QString &name, const QVariant &value);

private:
    void onReadyRead();

    QLocalSocket m_socket;
    QByteArray m_buffer;
    int m_nextObserveId = 1;
};
