#include "shell.h"

#include <QCoreApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

#include "player.h"

Shell::Shell(QQmlApplicationEngine &engine, Player &player, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_player(player)
    , m_tray(QIcon(QStringLiteral(":/icons/omamusic-symbolic.svg")))
{
    connect(m_menu.addAction(tr("Show")), &QAction::triggered, this, &Shell::showMain);
    connect(m_menu.addAction(tr("Play/Pause")), &QAction::triggered, &m_player, &Player::togglePause);
    connect(m_menu.addAction(tr("Next")), &QAction::triggered, &m_player, &Player::next);
    connect(m_menu.addAction(tr("Previous")), &QAction::triggered, &m_player, &Player::previous);
    m_menu.addSeparator();
    connect(m_menu.addAction(tr("Quit")), &QAction::triggered, this, &Shell::quitApp);

    m_tray.setContextMenu(&m_menu);
    m_tray.setToolTip(QStringLiteral("omamusic"));
    connect(&m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger)
            togglePanel();
        else if (r == QSystemTrayIcon::MiddleClick)
            m_player.togglePause();
    });
    m_tray.show();
}

QQuickWindow *Shell::window(const QString &objectName) const
{
    for (QObject *root : m_engine.rootObjects()) {
        if (root->objectName() == objectName)
            return qobject_cast<QQuickWindow *>(root);
    }
    return nullptr;
}

void Shell::handleVerb(const QString &verb)
{
    if (verb == QLatin1String("show"))
        showMain();
    else if (verb == QLatin1String("toggle"))
        toggleMain();
    else if (verb == QLatin1String("panel"))
        togglePanel();
    else if (verb == QLatin1String("quit"))
        quitApp();
}

void Shell::showMain()
{
    if (QQuickWindow *w = window(QStringLiteral("mainWindow"))) {
        w->show();
        w->raise();
        w->requestActivate();
    }
}

void Shell::toggleMain()
{
    QQuickWindow *w = window(QStringLiteral("mainWindow"));
    if (w && w->isVisible())
        w->hide();
    else
        showMain();
}

void Shell::togglePanel()
{
    QQuickWindow *w = window(QStringLiteral("panelWindow"));
    if (!w)
        return;
    if (w->isVisible()) {
        w->hide();
    } else {
        w->show();
        w->requestActivate();
    }
}

void Shell::hidePanel()
{
    if (QQuickWindow *w = window(QStringLiteral("panelWindow")))
        w->hide();
}

void Shell::quitApp()
{
    QCoreApplication::quit();
}
