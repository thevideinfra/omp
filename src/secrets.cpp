#include "secrets.h"

#include <QTimer>

Secrets::Secrets(const QString &program, QObject *parent)
    : QObject(parent)
    , m_program(program)
{
}

void Secrets::store(const QString &account, const QString &password) { run(Op::Store, account, password); }
void Secrets::lookup(const QString &account) { run(Op::Lookup, account, {}); }
void Secrets::clear(const QString &account) { run(Op::Clear, account, {}); }

void Secrets::run(Op op, const QString &account, const QString &password)
{
    auto *proc = new QProcess(this);
    QStringList args;
    switch (op) {
    case Op::Store: args = {QStringLiteral("store"), QStringLiteral("--label=omamusic")}; break;
    case Op::Lookup: args = {QStringLiteral("lookup")}; break;
    case Op::Clear: args = {QStringLiteral("clear")}; break;
    }
    args << QStringLiteral("service") << QStringLiteral("omamusic") << QStringLiteral("account") << account;

    auto report = [this, op](bool ok, const QString &out) {
        switch (op) {
        case Op::Store: emit stored(ok); break;
        case Op::Lookup: emit looked(out, ok); break;
        case Op::Clear: emit cleared(ok); break;
        }
    };
    auto *timeout = new QTimer(proc);
    timeout->setSingleShot(true);
    timeout->setInterval(5000);
    connect(timeout, &QTimer::timeout, proc, &QProcess::kill);
    connect(proc, &QProcess::errorOccurred, this, [this, proc, report](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            emit failed(tr("secret-tool is not installed (libsecret); the password cannot be kept in the keyring"));
            report(false, {});
            proc->deleteLater();
        }
    });
    connect(proc, &QProcess::finished, this, [proc, report, op](int code, QProcess::ExitStatus status) {
        const bool ok = status == QProcess::NormalExit && code == 0;
        const QString out = QString::fromUtf8(proc->readAllStandardOutput());
        report(ok && (op != Op::Lookup || !out.isEmpty()), out);
        proc->deleteLater();
    });
    proc->start(m_program, args);
    timeout->start();
    if (op == Op::Store) {
        proc->write(password.toUtf8());
        proc->closeWriteChannel();
    }
}
