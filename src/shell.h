#pragma once

#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QSystemTrayIcon>

#include "doubleclick.h"

class Player;
class Theme;
class QQmlApplicationEngine;

class Shell : public QObject
{
    Q_OBJECT
public:
    Shell(QQmlApplicationEngine &engine, Player &player, const Theme &theme, QObject *parent = nullptr);

    void handleVerb(const QString &verb);

    Q_INVOKABLE void showMain();
    Q_INVOKABLE void toggleMain();
    Q_INVOKABLE void togglePanel();
    Q_INVOKABLE void hidePanel();
    Q_INVOKABLE void quitApp();
    // Whether a tray host is there to bring the window back; without one, closing the window quits.
    Q_INVOKABLE bool trayAvailable() const;
    Q_INVOKABLE void showSettings();
    void setPanelWindow(QQuickWindow *panel) { m_panel = panel; }

private:
    QQuickWindow *window(const QString &objectName) const;
    void updateIcon();

    QQmlApplicationEngine &m_engine;
    Player &m_player;
    const Theme &m_theme;
    QSystemTrayIcon m_tray;
    DoubleClick m_trayClicks;
    QMenu m_menu;
    QPointer<QQuickWindow> m_panel;
};
