#ifndef DDS_PARTICIPANT_TABLE_MODEL_H
#define DDS_PARTICIPANT_TABLE_MODEL_H

#include "first_define.h"
#include "participant_monitor.h"

#include <QAbstractTableModel>
#include <QStringList>
#include <QHeaderView>
#include <QTableView>
#include <vector>


/**
 * @brief Table model for DDS participants
 */
class ParticipantTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:

    /**
     * @brief Constructor for the DDS participants model.
     * @param[in] parent The parent for this data model.
     */
    ParticipantTableModel();

    /**
     * @brief Destructor for the DDS participants model.
     */
    virtual ~ParticipantTableModel() = default;

    /**
     * @brief Standard row count for table.
     * @param[in] parent The parent model index.
     * @return Total rows in the table.
     */
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    /**
     * @brief Standard column count for table.
     * @param[in] parent When the parent of the table, returns 1 columns (value)
     * @return The total number of columns.
     */
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    /**
     * @brief Method to obtain row and column headers (row = vertical,
     *        col = horizontal)
     *
     * @param[in] section The row or column number to return the header for
     * @param[in] orientation The Qt::Horizontal and Qt::Vertical orientation
     *            specifying query for columns and rows (respectively)
     *
     * @return Returns the string for the row or column header
     */
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    /**
     * @brief Method to obtain edit/selection flags for table cell
     *
     * @param[in] index The model index of interest
     *
     * @return Qt::ItemFlags Returns the flags stating whether or not a
     *         parameter can be edited
     */
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    /**
     * @brief Return the textual data for a specific table cell.
     *
     * @param[in] index Obtain data for this table index.
     * @param[in] role The item data role that specifies what exactly to
     *            return about a table cell.
     *
     * @return QVariant Returns text representation in Qt::DisplayRole and
     *         QColor for Qt:DecorationRole (for activity highlights)
     */
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    /// Table column IDs for this data model
    enum eColumnIds
    {
        COLUMN_GUID,
        COLUMN_LOCATION,
        COLUMN_HOST,
        COLUMN_APP,
        COLUMN_INSTANCE,
        COLUMN_DISCOVERED_TIMESTAMP,
        MAX_eColumnIds_VALUE
    };

    /**
     * @brief Add a new participant to the model.
     */
    void addParticipant(const ParticipantInfo& info);

    /**
     * @brief Remove a participant from the model.
     */
    void removeParticipant(const ParticipantInfo& info);

private:

    /// Stores the names of all column titles
    QStringList m_columnHeaders;

    /// Stores the data values for this model
    std::vector<ParticipantInfo> m_data;

};

#endif

/**
 * @}
 */
