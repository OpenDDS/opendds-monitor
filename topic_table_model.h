#ifndef DDS_DATA_SAMPLE_TABLE_MODEL_H
#define DDS_DATA_SAMPLE_TABLE_MODEL_H

#include "first_define.h"

#include <tao/Typecode_typesC.h>

#include <QAbstractTableModel>
#include <QStringList>
#include <QHeaderView>
#include <QTableView>

class OpenDynamicData;


/**
 * @brief Table model for DDS topic samples
 */
class TopicTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:

    /**
     * @brief Constructor for the DDS sample data model.
     * @param[in] parent The parent for this data model.
     */
    TopicTableModel(QTableView *parent, const QString& topicName);

    /**
     * @brief Destructor for the DDS sample data model.
     */
    ~TopicTableModel();

    /**
     * @brief Update the model to use a new DDS data sample.
     * @param[in] sample The new DDS data sample.
     */
    void setSample(std::shared_ptr<OpenDynamicData> sample);

    /**
     * @brief Commit changes to the current sample.
     * @return The commited sample.
     * @remarks The returned value is the user-edited sample data to publish.
     */
    const std::shared_ptr<OpenDynamicData> commitSample();

    /**
     * @brief Standard row count for table.
     * @param[in] parent The parent model index.
     * @return Total rows in the table.
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    /**
     * @brief Standard column count for table.
     * @param[in] parent When the parent of the table, returns 1 columns (value)
     * @return The total number of columns.
     */
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

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
                        int role = Qt::DisplayRole) const;

    /**
     * @brief Method to obtain edit/selection flags for table cell
     *
     * @param[in] index The model index of interest
     *
     * @return Qt::ItemFlags Returns the flags stating whether or not a
     *         parameter can be edited
     */
    Qt::ItemFlags flags(const QModelIndex &index) const;

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
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    /**
     * @brief Write a new sample to the DDS bus with the new value
     *
     * @param[in] index The table index
     * @param[in] value The new edited value
     * @param[in] role The item data role that specifies what exactly to
     *                 return about a table cell.
     *
     * @return Returns true if the operation was successful; false otherwise
     */
    bool setData(const QModelIndex &index,
                 const QVariant &value,
                 int role = Qt::EditRole);

    /**
     * @brief Revert all changes to the original sample.
     */
    void revertChanges();

    /// Table column IDs for this data model
    enum eColumnIds
    {
        //STATUS_COLUMN,
        NAME_COLUMN,
        TYPE_COLUMN,
        VALUE_COLUMN,
        MAX_eColumnIds_VALUE
    };

signals:

    /**
     * @brief Sends a notification that the user modified data.
     */
    void dataChanged();

private:

    /**
     * @brief Stores information for a table row.
     */
    class DataRow
    {
    public:

        /**
         * @brief The table row information constructor.
         */
        DataRow();

        /**
         * @brief The table row information destructor.
         */
        ~DataRow();

        /**
         * @brief Set the value of the data row to a new value.
         * @param[in] newValue Attempt to set the value to this value.
         * @return Returns true if the operation was successful; false otherwise.
         */
        bool setValue(const QVariant& newValue);

        QString name; ///< The topic member name.
        QVariant value; ///< The topic member value.
        CORBA::TCKind type; ///< The topic member type.
        bool isKey; ///< The topic member key flag.
        bool isOptional; ///< The topic member is optional flag.
        bool edited; ///< The topic member edited flag.
    };

    /**
     * @brief Recursively parse a dynamic data structure and copy the contents
     *        into m_data;
     * @param[in] data The DDS dynamic data to parse.
     */
    void parseData(const std::shared_ptr<OpenDynamicData> data);

    /**
     * @brief Populate a DDS sample member.
     * @param[out] sample Populate this DDS sample.
     * @param[in] dataInfo Populate the DDS sample member from this data.
     * @return Returns true if the operation was successful; false otherwise.
     */
    bool populateSample(std::shared_ptr<OpenDynamicData> const sample,
                        DataRow *dataInfo);

    /// The table view using this model
    QTableView* m_tableView;

    /// Stores the names of all column titles
    QStringList m_columnHeaders;

    /// Stores the data values for this model
    std::vector<DataRow*> m_data;

    /// Stores the data sample for reverting.
    std::shared_ptr<OpenDynamicData> m_sample;

    /// The name of the topic for this data model
    QString m_topicName;

};

#endif

/**
 * @}
 */
