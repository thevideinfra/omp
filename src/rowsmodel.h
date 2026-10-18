#pragma once

#include <QAbstractListModel>
#include <QVariantMap>
#include <QVector>

class RowsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    explicit RowsModel(const QStringList &roles, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(const QVector<QVariantMap> &rows);
    void append(const QVariantMap &row);
    const QVector<QVariantMap> &rows() const { return m_rows; }

    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE QVariantList all() const;

signals:
    void countChanged();

private:
    QStringList m_roles;
    QVector<QVariantMap> m_rows;
};
