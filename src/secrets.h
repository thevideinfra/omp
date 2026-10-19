#pragma once

#include <QObject>
#include <QProcess>

// The server password lives in the system keyring (Secret Service), reached
// through the `secret-tool` program from libsecret. Calls are asynchronous and
// time out after five seconds.
class Secrets : public QObject
{
    Q_OBJECT
public:
    explicit Secrets(const QString &program = QStringLiteral("secret-tool"), QObject *parent = nullptr);

    // `account` is "<user>@<server url>".
    void store(const QString &account, const QString &password);
    void lookup(const QString &account);
    void clear(const QString &account);

signals:
    void stored(bool ok);
    void looked(const QString &password, bool found);
    void cleared(bool ok);
    void failed(const QString &message);   // the tool is missing, or it hung

private:
    enum class Op { Store, Lookup, Clear };
    void run(Op op, const QString &account, const QString &password);

    QString m_program;
};
