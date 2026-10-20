#include <QtTest>

#include "servers.h"
#include "settings.h"

class TstServers : public QObject
{
    Q_OBJECT
private slots:
    void addsUpdatesAndRemovesServersAndRemembersThem()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("servers.json");
        QString first, second;
        {
            Servers s(file);
            QVERIFY(s.list().isEmpty());
            QSignalSpy changed(&s, &Servers::changed);
            first = s.add("https://music.example.com/", " amy ", true);
            second = s.add("http://192.168.1.5:4533", "bob", false);
            QCOMPARE(first, QString("s1"));
            QCOMPARE(second, QString("s2"));
            QCOMPARE(changed.count(), 2);
            QCOMPARE(s.get("s1").url, QString("https://music.example.com"));      // no trailing slash
            QCOMPARE(s.get("s1").user, QString("amy"));                           // trimmed
            QVERIFY(s.get("s1").enabled);
            QVERIFY(!s.get("s2").enabled);

            QVERIFY(s.update("s2", "http://192.168.1.6:4533", "bob", true));
            QCOMPARE(s.get("s2").url, QString("http://192.168.1.6:4533"));
            QVERIFY(s.get("s2").enabled);
            s.setType("s1", "navidrome");
            QVERIFY(!s.update("s9", "http://x", "u", true));                      // no such server
            QCOMPARE(s.add("ftp://nope", "u", true), QString());                  // only http(s)
            QCOMPARE(s.add("http://x", "", true), QString());                     // a user is needed
            QCOMPARE(s.list().size(), 2);
        }
        Servers again(file);                                                      // it was saved
        QCOMPARE(again.list().size(), 2);
        QCOMPARE(again.get("s1").type, QString("navidrome"));
        QCOMPARE(again.get("s2").user, QString("bob"));

        QVERIFY(again.remove("s1"));
        QVERIFY(!again.remove("s1"));
        QCOMPARE(again.list().size(), 1);
        QCOMPARE(again.add("http://third", "c", true), QString("s3"));            // a removed id is never reused
        QVERIFY(Servers(file).get("s1").id.isEmpty());
    }

    void theOneServerOfTheOldSettingsBecomesTheFirstServerOnce()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath("config.toml"));
        settings.set("serverUrl", "http://192.168.10.104:4533");
        settings.set("serverUser", "videinfra");
        settings.set("serverEnabled", true);
        settings.set("serverType", "navidrome");

        Servers s(dir.filePath("servers.json"));
        QVERIFY(s.migrateFrom(settings));
        QCOMPARE(s.list().size(), 1);
        const ServerConfig c = s.get("s1");
        QCOMPARE(c.url, QString("http://192.168.10.104:4533"));
        QCOMPARE(c.user, QString("videinfra"));
        QCOMPARE(c.type, QString("navidrome"));
        QVERIFY(c.enabled);
        QCOMPARE(settings.serverUrl(), QString());                                // the old keys are cleared
        QVERIFY(!settings.serverEnabled());

        QVERIFY(!s.migrateFrom(settings));                                        // and it does not run again
        settings.set("serverUrl", "http://other");
        settings.set("serverUser", "x");
        QVERIFY(!s.migrateFrom(settings));                                        // not once servers exist
        QCOMPARE(s.list().size(), 1);
    }

    void aSettingsFileWithoutAServerMigratesNothing()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath("config.toml"));
        Servers s(dir.filePath("servers.json"));
        QVERIFY(!s.migrateFrom(settings));
        QVERIFY(s.list().isEmpty());
    }
};

QTEST_MAIN(TstServers)
#include "tst_servers.moc"
