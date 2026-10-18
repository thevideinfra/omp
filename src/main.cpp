#include <cstdio>

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "config.h"
#include "instance.h"
#include "library.h"
#include "paths.h"
#include "player.h"
#include "shell.h"
#include "theme.h"

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

    Theme theme;
    Player player;
    Library library(Config::folders(),
                    qEnvironmentVariable("OMAMUSIC_SCAN",
                                         QCoreApplication::applicationDirPath() + "/omamusic-scan"),
                    Paths::stateDir() + QStringLiteral("/index.jsonl"));

    QQmlApplicationEngine engine;
    Shell shell(engine, player);
    QQmlContext *ctx = engine.rootContext();
    ctx->setContextProperty("theme", &theme);
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
