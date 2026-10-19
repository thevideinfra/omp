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

    void parsesMultiLineArray()
    {
        const QStringList got = Config::parseFolders(
            "folders = [\n  \"~/Music\",\n  \"/mnt/nas\",\n]\n");
        QCOMPARE(got, QStringList({QDir::homePath() + "/Music", "/mnt/nas"}));
    }

    void skipsCommentLinesInsideArray()
    {
        const QStringList got = Config::parseFolders(
            "folders = [\n  # density = \"normal\"\n  \"/mnt/nas\",\n  # \"/old\"\n]\n");
        QCOMPARE(got, QStringList({"/mnt/nas"}));
    }

    void ignoresKeysStartingWithFolders()
    {
        QCOMPARE(Config::parseFolders("foldersX = [\"/x\"]\n"),
                 QStringList({QDir::homePath() + "/Music"}));
        QCOMPARE(Config::parseFolders("folders_old = [\"/x\"]\n"),
                 QStringList({QDir::homePath() + "/Music"}));
    }

    void aStringValueIsNotAnArrayAndDoesNotSwallowLaterLines()
    {
        const QByteArray text = "folders = \"/mnt/music\"\nfont_family = \"Foo\"\n";
        QCOMPARE(Config::parseFolders(text), QStringList({QDir::homePath() + "/Music"}));
        QVERIFY(!Config::startsArray("\"/mnt/music\""));
        QVERIFY(Config::startsArray(" [\"/a\","));
    }

    void bracketsInsideQuotesAndCommentsDoNotCloseTheArray()
    {
        QVERIFY(!Config::closesArray("\"/music/[live]\","));
        QVERIFY(Config::closesArray("\"/music/[live]\"]"));
        QVERIFY(!Config::closesArray("# see [x]"));
        QCOMPARE(Config::parseFolders("folders = [\n  \"/a/[b]\",\n  # see [x]\n  \"/c\",\n]\n"),
                 QStringList({"/a/[b]", "/c"}));
    }
};

QTEST_GUILESS_MAIN(TstConfig)
#include "tst_config.moc"
