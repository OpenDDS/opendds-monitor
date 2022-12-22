#include "participant_table_model.h"
#include "dds_data.h"

#include <iostream>
#include <algorithm>

ParticipantTableModel::ParticipantTableModel()
    : QAbstractTableModel(nullptr)
{
    qRegisterMetaType<QList<QPersistentModelIndex>>("QList<QPersistentModelIndex>");
    qRegisterMetaType<QAbstractItemModel::LayoutChangeHint>("QAbstractItemModel::LayoutChangeHint");

    m_columnHeaders
        << "GUID"
        << "IP:Port"
        << "Host"
        << "App"
        << "Instance"
        << "Discovered";
}

int ParticipantTableModel::rowCount(const QModelIndex& idx) const
{
    if (idx.parent().isValid()) return 0;

    return static_cast<int>(m_data.size());
}

int ParticipantTableModel::columnCount(const QModelIndex& /*index*/) const
{
    return static_cast<int>(m_columnHeaders.size());
}

QVariant ParticipantTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
    {
        return QVariant();
    }

    if (orientation == Qt::Vertical)
    {
        return QString::number(section);
    }
    else if (orientation == Qt::Horizontal && section < m_columnHeaders.count())
    {
        return m_columnHeaders.at(section);
    }

    return QVariant();
}

Qt::ItemFlags ParticipantTableModel::flags(const QModelIndex& /*index*/) const
{
    return Qt::ItemIsEnabled;
}

QVariant ParticipantTableModel::data(const QModelIndex& index, int role) const
{
    const int column = index.column();
    const int row = index.row();

    // Make sure the data row is valid
    if (row < 0 || row >= m_data.size())
    {
        return QVariant();
    }

    // Is this a request to display the text value?
    if (role == Qt::DisplayRole && column == COLUMN_GUID)
    {
        return m_data.at(row).guid.c_str();
    }
    else if (role == Qt::DisplayRole && column == COLUMN_LOCATION)
    {
        return m_data.at(row).location.c_str();
    }
    else if (role == Qt::DisplayRole && column == COLUMN_HOST)
    {
        return m_data.at(row).hostID.c_str();
    }
    else if (role == Qt::DisplayRole && column == COLUMN_APP)
    {
        return m_data.at(row).appID.c_str();
    }
    else if (role == Qt::DisplayRole && column == COLUMN_INSTANCE)
    {
        return m_data.at(row).instanceID.c_str();
    }
    else if (role == Qt::DisplayRole && column == COLUMN_DISCOVERED_TIMESTAMP)
    {
        return m_data.at(row).discovered_timestamp.c_str();
    }

    return QVariant();

}

void ParticipantTableModel::addParticipant(const ParticipantInfo& info)
{
    emit layoutAboutToBeChanged();

    auto iter = std::find_if(m_data.begin(), m_data.end(), [&](const ParticipantInfo& i) { return i.guid == info.guid; });
    if (iter != m_data.end()) {
        *iter = info;
    } else {
        m_data.push_back(info);
        std::sort(m_data.begin(), m_data.end());
    }

    emit layoutChanged();
}

void ParticipantTableModel::removeParticipant(const ParticipantInfo& info)
{
    emit layoutAboutToBeChanged();

    auto iter = std::find_if(m_data.begin(), m_data.end(), [&](const ParticipantInfo& i) { return i.guid == info.guid; });
    if (iter != m_data.end()) {
        m_data.erase(iter);
    }

    emit layoutChanged();
}
