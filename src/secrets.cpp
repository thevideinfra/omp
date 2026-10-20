#include "secrets.h"

#include <QTimer>

namespace {
const QString kService = QStringLiteral("omp");
const QString kOldService = QStringLiteral("omamusic");   // before the rename
}

Secrets::Secrets(const QString &program, QObject *parent)
    : QObject(parent)
    , m_program(program)
{
}

void Secrets::store(const QString &account, const QString &password)
{
    run(Op::Store, kService, account, password, [this](bool ok, const QString &) { emit stored(ok); });
}

void Secrets::lookup(const QString &account)
{
    run(Op::Lookup, kService, account, {}, [this, account](bool ok, const QString &out) {
        if (ok) {
            emit looked(out, true);
            return;
        }
        // Not there under the new name: a password saved before the rename moves over.
        run(Op::Lookup, kOldService, account, {}, [this, account](bool oldOk, const QString &oldOut) {
            if (oldOk) {
                run(Op::Store, kService, account, oldOut, [](bool, const QString &) {});
                run(Op::Clear, kOldService, account, {}, [](bool, const QString &) {});
            }
            emit looked(oldOk ? oldOut : QString(), oldOk);
        });
    });
}

void Secrets::clear(const QString &account)
{
    run(Op::Clear, kService, account, {}, [this](bool ok, const QString &) { emit cleared(ok); });
    run(Op::Clear, kOldService, account, {}, [](bool, const QString &) {});   // and a leftover old entry
}

void Secrets::run(Op op, const QString &service, const QString &account, const QString &password,
                  std::function<void(bool, const QString &)> done)
{
    auto *proc = new QProcess(this);
    QStringList args;
    switch (op) {
    case Op::Store: args = {QStringLiteral("store"), QStringLiteral("--label=OMP")}; break;
    case Op::Lookup: args = {QStringLiteral("lookup")}; break;
    case Op::Clear: args = {QStringLiteral("clear")}; break;
    }
    args << QStringLiteral("service") << service << QStringLiteral("account") << account;

    auto *timeout = new QTimer(proc);
    timeout->setSingleShot(true);
    timeout->setInterval(5000);
    connect(timeout, &QTimer::timeout, proc, &QProcess::kill);
    connect(proc, &QProcess::errorOccurred, this, [this, proc, done](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            emit failed(tr("secret-tool is not installed (libsecret); the password cannot be kept in the keyring"));
            done(false, {});
            proc->deleteLater();
        }
    });
    connect(proc, &QProcess::finished, this, [proc, done, op](int code, QProcess::ExitStatus status) {
        const bool ok = status == QProcess::NormalExit && code == 0;
        const QString out = QString::fromUtf8(proc->readAllStandardOutput());
        done(ok && (op != Op::Lookup || !out.isEmpty()), out);
        proc->deleteLater();
    });
    proc->start(m_program, args);
    timeout->start();
    if (op == Op::Store) {
        proc->write(password.toUtf8());
        proc->closeWriteChannel();
    }
}
