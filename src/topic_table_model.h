#ifndef DDS_DATA_SAMPLE_TABLE_MODEL_H
#define DDS_DATA_SAMPLE_TABLE_MODEL_H

#include "first_define.h"

#include <dds/DCPS/XTypes/DynamicTypeSupport.h>

#include <tao/Typecode_typesC.h>

#include <QAbstractTableModel>
#include <QStringList>
#include <QHeaderView>
#include <QTableView>

#include <memory>

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

    void setSample(DDS::DynamicData_var sample);

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
    
    void updateDisplayHexAndAscii(bool new_hex, bool new_ascii);

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
    
    /**
     * @brief controls the display mode of integer & ascii types.
     */
    bool disp_hex = false;
    bool disp_ascii = true;
    
signals:

    /**
     * @brief Sends a notification that the user modified data.
     */
    void dataHasChanged();

protected:

    template<typename T>
    QVariant type_to_qvariant(T convertMe) {
        QVariant retMe;

        if (disp_ascii && sizeof(T) == 1) {// looks stupid to have a redundant check, but the QChar wont compile with a larger type, so it is eliminated by the if constexpr in that case. Still need the original && sizeof(T)==1 so that no other type enters this path, display breaks if you don't have it.
            if constexpr (sizeof(T) == 1) retMe = QChar(convertMe);
        }
        else if (disp_hex) {
            std::stringstream stream;
            for (int i = sizeof(T) - 1; i >= 0; i--) {
                stream << std::hex << (((convertMe) >> (8 * i + 4)) & 0x0F);
                stream << std::hex << (((convertMe) >> (8 * i + 0)) & 0x0F);
            }
            std::string tempStr = stream.str();
            for (std::string::size_type i = 0; i < tempStr.size(); i++)
                tempStr[i] = std::toupper(tempStr[i]);
            retMe = QString(tempStr.c_str());
        }
        else {
            if constexpr (sizeof(T) == 1)
                retMe = convertMe;
            else
                retMe = convertMe;
        }
        return retMe;
    }

    template<typename T>
    T qvariant_to_type(QVariant convertMe) {
        T retMe{};

        if (disp_ascii && sizeof(T) == 1) {
            retMe = convertMe.toString()[0].unicode() & 0xFF;
        }
        else if (disp_hex) {
            try { retMe = std::stoll(convertMe.toString().toStdString(), nullptr, 16); }
            catch (...) { retMe = 0; };
        }
        else {
            try { retMe = std::stoi(convertMe.toString().toStdString()); }
            catch (...) { retMe = 0; };
        }
        return retMe;
    }

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
        bool setValue(TopicTableModel* parent, const QVariant& newValue);

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

    /// Convert from DDS::TypeKind to CORBA::TCKind. Needed to parse DynamicData.
    CORBA::TCKind typekind_to_tckind(DDS::TypeKind tk);

    bool check_rc(DDS::ReturnCode_t rc, const char* what);

    void setDataRow(DataRow* row, const DDS::DynamicData_var& data, DDS::MemberId id);

    void parseCollection(const DDS::DynamicData_var& data, const std::string& namePrefix);
    void parseAggregated(const DDS::DynamicData_var& data, const std::string& namePrefix);

    /// Parse a DynamicData object into m_data
    void parseData(const DDS::DynamicData_var& data, const std::string& namePrefix);

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

    /// For reverting.
    DDS::DynamicData_var m_dynamicsample;

    /// The name of the topic for this data model
    QString m_topicName;

};

#endif

/**
 * @}
 */
