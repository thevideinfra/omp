#include <QtTest>

#include "config.h"

class TstConfig : public QObject
{
    Q_OBJECT
private slots:
    void parsesFoldersAndExpandsHome()
    {
        const QStringList got = Config::parseFolders("folders = [\"~/Music\", \"/mnt/nas/\"]\n");
        QCOMPARE(got, QStringList({QDir::homePath() + "/Music", "/mnt/nas"}));
    }

    void defaultsToHomeMusic()
    {
        QCOMPARE(Config::parseFolders(""), QStringList({QDir::homePath() + "/Music"}));
    }

    void ignoresOtherKeys()
    {
        QCOMPARE(Config::parseFolders("other = [\"/x\"]\n"),
                 QStringList({QDir::homePath() + "/Music"}));
    }
};

QTEST_GUILESS_MAIN(TstConfig)
#include "tst_config.moc"
