#pragma once

#include <QLocalServer>
#include <QObject>

class Instance : public QObject
{
    Q_OBJECT
public:
    explicit Instance(const QString &socketPath, QObject *parent = nullptr);

    // true: this process is the primary instance and now listens.
    // false: a running instance received the verb; the caller should exit.
    bool claimOrSend(const QString &verb);

signals:
    void verbReceived(const QString &verb);

private:
    QString m_path;
    QLocalServer m_server;
};
