#include <csignal>
#include <cstdio>

#include <sys/socket.h>
#include <unistd.h>

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSocketNotifier>

#include "config.h"
#include "instance.h"
#include "library.h"
#include "paths.h"
#include "player.h"
#include "settings.h"
#include "shell.h"
#include "theme.h"

namespace {
int g_signalFd[2] = {-1, -1};

void onSignal(int)
{
    const char b = 1;
    [[maybe_unused]] const ssize_t n = ::write(g_signalFd[0], &b, 1);
}

// Turns SIGTERM/SIGINT into QCoreApplication::quit() so destructors (and the
// mpv shutdown in ~Player) run.
void installQuitSignals()
{
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, g_signalFd) != 0)
        return;
    auto *notifier = new QSocketNotifier(g_signalFd[1], QSocketNotifier::Read, qApp);
    QObject::connect(notifier, &QSocketNotifier::activated, qApp, [] {
        char b;
        [[maybe_unused]] const ssize_t n = ::read(g_signalFd[1], &b, 1);
        QCoreApplication::quit();
    });
    struct sigaction sa {};
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    ::sigaction(SIGTERM, &sa, nullptr);
    ::sigaction(SIGINT, &sa, nullptr);
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("omamusic"));
    app.setDesktopFileName(QStringLiteral("omamusic"));   // Wayland app_id
    app.setQuitOnLastWindowClosed(false);
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    static const QStringList verbs{"show", "toggle", "panel", "quit"};
    const QString verb = app.arguments().value(1, QStringLiteral("show"));
    if (!verbs.contains(verb)) {
        std::fputs("usage: omamusic [show|toggle|panel|quit]\n", stderr);
        return 2;
    }

    Instance instance(Paths::runtimeDir() + QStringLiteral("/instance.sock"));
    if (!instance.claimOrSend(verb) || verb == QLatin1String("quit"))
        return 0;
    if (!instance.listening())
        std::fputs("omamusic: single-instance socket unavailable; later launches will not be forwarded\n", stderr);

    installQuitSignals();

    Theme theme;
    Settings settings;
    theme.setAccentChoice(settings.accent());
    QObject::connect(&settings, &Settings::changed, &theme,
                     [&settings, &theme] { theme.setAccentChoice(settings.accent()); });
    Player player;
    Library library(Config::folders(),
                    qEnvironmentVariable("OMAMUSIC_SCAN",
                                         QCoreApplication::applicationDirPath() + "/omamusic-scan"),
                    Paths::stateDir() + QStringLiteral("/index.jsonl"));

    QQmlApplicationEngine engine;
    Shell shell(engine, player);
    QQmlContext *ctx = engine.rootContext();
    ctx->setContextProperty("theme", &theme);
    ctx->setContextProperty("ui", &settings);
    ctx->setContextProperty("player", &player);
    ctx->setContextProperty("library", &library);
    ctx->setContextProperty("shell", &shell);

    engine.loadFromModule("Omamusic", "Main");
    engine.loadFromModule("Omamusic", "Panel");
    if (engine.rootObjects().size() < 2)
        return 1;

    QObject::connect(&instance, &Instance::verbReceived, &shell, &Shell::handleVerb);
    player.start();
    library.start();
    shell.handleVerb(verb);
    return app.exec();
}
