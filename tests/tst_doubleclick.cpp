#include <QtTest>

#include "doubleclick.h"

class TstDoubleClick : public QObject
{
    Q_OBJECT
private slots:
    void twoClicksCloseTogetherAreADoubleClick()
    {
        DoubleClick d(400);
        QVERIFY(!d.click(1000));          // the first click
        QVERIFY(d.click(1300));           // 300 ms later: a double click
        QVERIFY(!d.click(1500));          // a third one starts over: it does not chain
        QVERIFY(d.click(1700));
    }

    void slowClicksAreSeparateSingleClicks()
    {
        DoubleClick d(400);
        QVERIFY(!d.click(1000));
        QVERIFY(!d.click(1401));          // just too slow
        QVERIFY(!d.click(5000));
    }

    void theIntervalIsInclusiveAtTheLimit()
    {
        DoubleClick d(400);
        QVERIFY(!d.click(0));
        QVERIFY(d.click(400));
    }
};

QTEST_MAIN(TstDoubleClick)
#include "tst_doubleclick.moc"
