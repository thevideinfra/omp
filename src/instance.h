#pragma once

#include <QLocalServer>
#include <QObject>

class Instance : public QObject
{
    Q_OBJECT
public:
    explicit Instance(const QString &socketPath, QObject *parent = nullptr);

    // false: a running instance received the verb; the caller should exit.
    // true: no other instance is running; this process should run. Check
    // listening(): it is false when the single-instance socket could not be
    // created, in which case later launches will not be forwarded here.
    bool claimOrSend(const QString &verb);

    bool listening() const { return m_server.isListening(); }

signals:
    void verbReceived(const QString &verb);

private:
    QString m_path;
    QLocalServer m_server;
};
