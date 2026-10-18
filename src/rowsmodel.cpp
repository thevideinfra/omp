#include "rowsmodel.h"

RowsModel::RowsModel(const QStringList &roles, QObject *parent)
    : QAbstractListModel(parent)
    , m_roles(roles)
{
}

int RowsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant RowsModel::data(const QModelIndex &index, int role) const
{
    const int i = role - Qt::UserRole - 1;
    if (!index.isValid() || index.row() >= m_rows.size() || i < 0 || i >= m_roles.size())
        return {};
    return m_rows.at(index.row()).value(m_roles.at(i));
}

QHash<int, QByteArray> RowsModel::roleNames() const
{
    QHash<int, QByteArray> names;
    for (int i = 0; i < m_roles.size(); ++i)
        names.insert(Qt::UserRole + 1 + i, m_roles.at(i).toUtf8());
    return names;
}

void RowsModel::setRows(const QVector<QVariantMap> &rows)
{
    const int before = m_rows.size();
    beginResetModel();
    m_rows = rows;
    endResetModel();
    if (before != m_rows.size())
        emit countChanged();
}

void RowsModel::append(const QVariantMap &row)
{
    beginInsertRows({}, m_rows.size(), m_rows.size());
    m_rows.append(row);
    endInsertRows();
    emit countChanged();
}

QVariantMap RowsModel::get(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row) : QVariantMap();
}

QVariantList RowsModel::all() const
{
    QVariantList out;
    out.reserve(m_rows.size());
    for (const QVariantMap &r : m_rows)
        out << r;
    return out;
}
