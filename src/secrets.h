#pragma once

#include <functional>

#include <QObject>
#include <QProcess>

// The server password lives in the system keyring (Secret Service), reached
// through the `secret-tool` program from libsecret. Calls are asynchronous and
// time out after five seconds. Passwords saved under the old name (omamusic) are
// found, and moved to the new one, on the first lookup.
class Secrets : public QObject
{
    Q_OBJECT
public:
    explicit Secrets(const QString &program = QStringLiteral("secret-tool"), QObject *parent = nullptr);

    // `account` is "<user>@<server url>".
    void store(const QString &account, const QString &password);
    void lookup(const QString &account);
    // The same, answered to `done(password, found)` instead of the `looked` signal, so that several
    // lookups at once (one per server) do not mix up their answers.
    void lookup(const QString &account, std::function<void(const QString &, bool)> done);
    void clear(const QString &account);

signals:
    void stored(bool ok);
    void looked(const QString &password, bool found);
    void cleared(bool ok);
    void failed(const QString &message);   // the tool is missing, or it hung

private:
    enum class Op { Store, Lookup, Clear };
    // Runs secret-tool for one service; `done(ok, output)`.
    void run(Op op, const QString &service, const QString &account, const QString &password,
             std::function<void(bool, const QString &)> done);

    QString m_program;
};
