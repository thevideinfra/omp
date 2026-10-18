#include <QtTest>

#include "rowsmodel.h"

class TstRowsModel : public QObject
{
    Q_OBJECT
private slots:
    void exposesRolesAndCount()
    {
        RowsModel m({"name", "path"});
        QSignalSpy count(&m, &RowsModel::countChanged);
        m.setRows({{{"name", "a"}, {"path", "/a"}}, {{"name", "b"}, {"path", "/b"}}});
        QCOMPARE(m.rowCount(), 2);
        QCOMPARE(count.count(), 1);
        const int nameRole = m.roleNames().key("name");
        QCOMPARE(m.data(m.index(1), nameRole).toString(), QString("b"));
        QCOMPARE(m.get(0).value("path").toString(), QString("/a"));
        QCOMPARE(m.all().size(), 2);
    }

    void appendAddsRow()
    {
        RowsModel m({"name"});
        m.append({{"name", "x"}});
        m.append({{"name", "y"}});
        QCOMPARE(m.rowCount(), 2);
        QCOMPARE(m.get(1).value("name").toString(), QString("y"));
    }

    void getOutOfRangeIsEmpty()
    {
        RowsModel m({"name"});
        QVERIFY(m.get(3).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstRowsModel)
#include "tst_rowsmodel.moc"
