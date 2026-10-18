#pragma once

#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QSystemTrayIcon>

class Player;
class QQmlApplicationEngine;

class Shell : public QObject
{
    Q_OBJECT
public:
    Shell(QQmlApplicationEngine &engine, Player &player, QObject *parent = nullptr);

    void handleVerb(const QString &verb);

    Q_INVOKABLE void showMain();
    Q_INVOKABLE void toggleMain();
    Q_INVOKABLE void togglePanel();
    Q_INVOKABLE void hidePanel();
    Q_INVOKABLE void quitApp();
    Q_INVOKABLE void showSettings();
    void setPanelWindow(QQuickWindow *panel) { m_panel = panel; }

private:
    QQuickWindow *window(const QString &objectName) const;

    QQmlApplicationEngine &m_engine;
    Player &m_player;
    QSystemTrayIcon m_tray;
    QMenu m_menu;
    QPointer<QQuickWindow> m_panel;
};
