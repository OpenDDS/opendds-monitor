#include "qos_dictionary.h"
#include "topic_table_model.h"
#include "editor_delegates.h"
#include "open_dynamic_data.h"
#include "dds_manager.h"
#include "dds_data.h"

#include <QIcon>
#include <iostream>
#include <cstdint>


//------------------------------------------------------------------------------
TopicTableModel::TopicTableModel(
    QTableView *parent,
    const QString& topicName) :
    QAbstractTableModel(parent),
    m_tableView(parent),
    m_sample(nullptr),
    m_topicName(topicName)
{
    m_columnHeaders /*<< ""*/ << "Name" << "Type" << "Value";
    m_tableView->setItemDelegate(new LineEditDelegate(this));

    // Create a blank sample, so the user can publish an initial instance
    std::shared_ptr<TopicInfo> topicInfo = CommonData::getTopicInfo(m_topicName);
    if (!topicInfo || !topicInfo->typeCode)
    {
        return;
    }

    std::shared_ptr<OpenDynamicData> blankSample = CreateOpenDynamicData(topicInfo->typeCode, QosDictionary::getEncodingKind(), topicInfo->extensibility);
    setSample(blankSample);
}


//------------------------------------------------------------------------------
TopicTableModel::~TopicTableModel()
{
    while (!m_data.empty())
    {
        delete m_data.back();
        m_data.pop_back();
    }
}


//------------------------------------------------------------------------------
void TopicTableModel::setSample(std::shared_ptr<OpenDynamicData> sample)
{
    if (!sample)
    {
        std::cerr << "TopicTableModel::setSample:"
                  << "Null sample"
                  << std::endl;
        return;
    }

    if (sample->getLength() == 0)
    {
        std::cerr << "TopicTableModel::setSample:"
                  << "Sample has no children"
                   << std::endl;
        return;
    }

    emit layoutAboutToBeChanged();

    while (!m_data.empty())
    {
       delete m_data.back();
       m_data.pop_back();
    }

    m_sample = sample;
    parseData(m_sample);

    emit layoutChanged();

} // End TopicTableModel::updateSample


//------------------------------------------------------------------------------
const std::shared_ptr<OpenDynamicData> TopicTableModel::commitSample()
{
    // Reset the edited state
    for (int i = 0; i < m_data.size(); i++)
    {
        m_data.at(i)->edited = false;
    }

    // Create a new sample
    std::shared_ptr<TopicInfo> topicInfo = CommonData::getTopicInfo(m_topicName);
    if (!topicInfo || !topicInfo->typeCode)
    {
        return nullptr;
    }

    // Create a copy of the current sample
    std::shared_ptr<OpenDynamicData> newSample = CreateOpenDynamicData(topicInfo->typeCode, QosDictionary::getEncodingKind(), topicInfo->extensibility);
    *newSample = *m_sample;

    // Populate the new sample from the user-edited changes
    for (int i = 0; i < m_data.size(); i++)
    {
        populateSample(newSample, m_data.at(i));
    }

    // Replace and delete the old sample
    setSample(newSample);

    return newSample;
}


//------------------------------------------------------------------------------
int TopicTableModel::rowCount(const QModelIndex &) const
{
    return static_cast<int>(m_data.size());
}


//------------------------------------------------------------------------------
int TopicTableModel::columnCount(const QModelIndex &) const
{
    return MAX_eColumnIds_VALUE;
}


//------------------------------------------------------------------------------
QVariant TopicTableModel::headerData(int section,
                                     Qt::Orientation orientation,
                                     int role) const
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


//------------------------------------------------------------------------------
Qt::ItemFlags TopicTableModel::flags(const QModelIndex &index) const
{
    // Is this data editable?
    if (index.column() == VALUE_COLUMN)
    {
        return Qt::ItemIsEnabled | Qt::ItemIsEditable;
    }

    // Is this items selectable?
    if (index.column() == NAME_COLUMN)
    {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }

    // If we got here, the column must be read-only
    return Qt::ItemIsEnabled;
}


//------------------------------------------------------------------------------
QVariant TopicTableModel::data(const QModelIndex &index, int role) const
{
    const int column = index.column();
    const int row = index.row();

    // Make sure the data row is valid
    if (row < 0 || row >= m_data.size() || !m_data.at(row))
    {
        return QVariant();
    }

    // Change the background color for edited rows
    if (role == Qt::BackgroundRole &&
        column == VALUE_COLUMN &&
        m_data.at(row)->edited == true)
    {
        return QColor(Qt::yellow);
    }

    // Change the text color for edited rows (black on yellow)
    if (role == Qt::ForegroundRole &&
        column == VALUE_COLUMN &&
        m_data.at(row)->edited == true)
    {
        return QColor(Qt::black);
    }

    // Is this a request to display the text value?
    if (role == Qt::DisplayRole && column == NAME_COLUMN)
    {
        return m_data.at(row)->name;
    }
    else if (role == Qt::DisplayRole && column == VALUE_COLUMN)
    {
        return m_data.at(row)->value;
    }
    else if (role == Qt::DisplayRole && column == TYPE_COLUMN)
    {
        CORBA::TCKind typeID = m_data.at(row)->type;
        switch (typeID)
        {
            case CORBA::tk_short: return "int16";
            case CORBA::tk_long: return "int32";
            case CORBA::tk_ushort: return "uint16";
            case CORBA::tk_ulong: return "uint32";
            case CORBA::tk_longlong: return "int64";
            case CORBA::tk_ulonglong: return "uint64";
            case CORBA::tk_float: return "float32";
            case CORBA::tk_double: return "float64";
            case CORBA::tk_boolean: return "boolean";
            case CORBA::tk_char: return "char";
            case CORBA::tk_wchar: return "wchar";
            case CORBA::tk_octet: return "uint8";
            case CORBA::tk_enum: return "enum";
            case CORBA::tk_string: return "string";
            default: return "?";
        }
    }

    return QVariant();

} // End TopicTableModel::data


//------------------------------------------------------------------------------
bool TopicTableModel::setData(const QModelIndex &index,
                              const QVariant &value,
                              int)
{
    const int row = index.row();
    DataRow* data = nullptr;

    // Make sure we're in bounds of the data array
    if (row < 0 || row >= m_data.size())
    {
        return false;
    }

    // Make sure we found the data row
    data = m_data.at(row);
    if (!data)
    {
        return false;
    }

    // Make sure the data isn't invalid
    if (value.toString() == "-")
    {
        return false;
    }

    // If the user entered the same value, don't do anything
    if (data->value == value)
    {
        return false;
    }

    if (!data->setValue(value))
    {
        return false;
    }

    emit dataChanged();
    return true;

} // End TopicTableModel::setData


//------------------------------------------------------------------------------
void TopicTableModel::revertChanges()
{
    setSample(m_sample);
}


//------------------------------------------------------------------------------
void TopicTableModel::parseData(const std::shared_ptr<OpenDynamicData> data)
{
    const size_t childCount = data->getLength();
    for (size_t i = 0; i < childCount; i++)
    {
        const std::shared_ptr<OpenDynamicData> child = data->getMember(i);
        if (!child)
        {
            continue;
        }

        // Handle child data if necessary
        if(child->isContainerType())
        {
            parseData(child);
            continue;
        }


        DataRow* dataRow = new DataRow;
        dataRow->type = child->getKind();
        dataRow->isOptional = false;
        dataRow->name = child->getFullName().c_str();
        dataRow->isKey = false; // TODO?


        // Update the current editor delegate
        const int thisRow = static_cast<int>(m_data.size());
        if (m_tableView->itemDelegateForRow(thisRow))
        {
            delete m_tableView->itemDelegateForRow(thisRow);
        }
        m_tableView->setItemDelegateForRow(thisRow, new LineEditDelegate(this));


        // Store the value into a QVariant
        // The tmpValue may seem redundant, but it's very helpful for debug
        switch (dataRow->type)
        {
        case CORBA::tk_long:
        {
            int32_t tmpValue = child->getValue<int32_t>();
            dataRow->value = tmpValue;
            break;
        }
        case CORBA::tk_short:
        {
            int16_t tmpValue = child->getValue<int16_t>();
            dataRow->value = tmpValue;
            break;
        }
        case CORBA::tk_ushort:
        {
            uint16_t tmpValue = child->getValue<uint16_t>();
            dataRow->value = tmpValue;
            break;
        }
        case CORBA::tk_ulong:
        {
            uint32_t tmpValue = child->getValue<uint32_t>();
            dataRow->value = tmpValue;
            break;
        }
        case CORBA::tk_float:
        {
            float tmpValue = child->getValue<float>();
            dataRow->value = tmpValue;
            break;
        }
        case CORBA::tk_double:
        {
            double tmpValue = child->getValue<double>();
            dataRow->value = tmpValue;
            break;
        }
        case CORBA::tk_boolean:
        {
            uint32_t tmpValue = child->getValue<uint32_t>();
            dataRow->value = tmpValue;
            break;
        }
        case CORBA::tk_char:
        {
            char tmpValue = child->getValue<char>();
            dataRow->value = tmpValue;
            break;
        }
        case CORBA::tk_wchar:
        {
            dataRow->value = ""; // FIXME?
            break;
        }
        case CORBA::tk_octet:
        {
            uint8_t tmpValue = child->getValue<uint8_t>();
            dataRow->value = tmpValue;
            break;
        }
        case CORBA::tk_longlong:
        {
            qint64 tmpValue = child->getValue<qint64>();
            dataRow->value = tmpValue;
            break;
        }
        case CORBA::tk_ulonglong:
        {
            quint64 tmpValue = child->getValue<quint64>();
            dataRow->value = tmpValue;
            break;
        }
        case CORBA::tk_string:
        {
            const char* tmpValue = child->getStringValue();
            dataRow->value = tmpValue;
            break;
        }
        case CORBA::tk_enum:
        {
            // Make sure the int value is valid
            const CORBA::ULong enumValue = child->getValue<CORBA::ULong>();
            const CORBA::TypeCode* enumTypeCode = child->getTypeCode();
            const CORBA::ULong enumMemberCount = enumTypeCode->member_count();
            if (enumValue >= enumMemberCount)
            {
                dataRow->value = "INVALID";
                break;
            }

            // Remove the old delegate
            if (m_tableView->itemDelegateForRow(thisRow))
            {
                delete m_tableView->itemDelegateForRow(thisRow);
            }

            // Install the new delegate
            ComboDelegate *enumDelegate = new ComboDelegate(this);
            for (CORBA::ULong enumIndex = 0; enumIndex < enumMemberCount; enumIndex++)
            {
                QString stringValue = enumTypeCode->member_name(enumIndex);
                if (!stringValue.isEmpty())
                {
                    enumDelegate->addItem(stringValue, (int)enumIndex);
                }
            }

            m_tableView->setItemDelegateForRow(thisRow, enumDelegate);
            dataRow->value = enumTypeCode->member_name(enumValue);

            break;
        } // End enum block
        default:
            dataRow->value = "NULL";
            break;

        } // End child type switch

        m_data.push_back(dataRow);

    } // End member loop

} // End TopicTableModel::parseData


//------------------------------------------------------------------------------
bool TopicTableModel::populateSample(std::shared_ptr<OpenDynamicData> const sample,
                                     DataRow* const dataInfo)
{
    bool pass = false;
    if (!sample || !dataInfo)
    {
        return false;
    }

    const std::string memberName = dataInfo->name.toUtf8().data();
    std::shared_ptr<OpenDynamicData> memberData = sample->getMember(memberName);
    if (!memberData)
    {
        std::cerr << "TopicTableModel::populateSample: "
                  << "Unable to find member named "
                  << memberName
                  << std::endl;

        return false;
    }

    switch (dataInfo->type)
    {
    case CORBA::tk_long:
    {
        int32_t tmpValue = dataInfo->value.toInt(&pass);
        if (pass)
        {
            memberData->setValue(tmpValue);
        }
        break;
    }
    case CORBA::tk_short:
    {
        int16_t tmpValue = dataInfo->value.toInt(&pass);
        if (pass)
        {
            memberData->setValue(tmpValue);
        }
        break;
    }
    case CORBA::tk_ushort:
    {
        uint16_t tmpValue = dataInfo->value.toUInt(&pass);
        if (pass)
        {
            memberData->setValue(tmpValue);
        }
        break;
    }
    case CORBA::tk_ulong:
    {
        uint32_t tmpValue = dataInfo->value.toUInt(&pass);
        if (pass)
        {
            memberData->setValue(tmpValue);
        }
        break;
    }
    case CORBA::tk_float:
    {
        float tmpValue = dataInfo->value.toFloat(&pass);
        if (pass)
        {
            memberData->setValue(tmpValue);
        }
        break;
    }
    case CORBA::tk_double:
    {
        double tmpValue = dataInfo->value.toDouble(&pass);
        if (pass)
        {
            memberData->setValue(tmpValue);
        }
        break;
    }
    case CORBA::tk_boolean:
    {
        if (dataInfo->value.canConvert(QMetaType::Bool))
        {
            bool tmpValue = dataInfo->value.toBool();
            memberData->setValue(tmpValue);
            pass = true;
        }
        break;
    }
    case CORBA::tk_char:
    {
        if (dataInfo->value.canConvert(QMetaType::Char))
        {
            char tmpValue = dataInfo->value.toChar().toLatin1();
            memberData->setValue(tmpValue);
            pass = true;
        }
        break;
    }
    case CORBA::tk_wchar:
    {
        // FIXME?
        break;
    }
    case CORBA::tk_octet:
    {
        bool tmpPass = false;
        int32_t octetTestVar = dataInfo->value.toInt(&tmpPass);
        if (tmpPass && octetTestVar >= 0 && octetTestVar <= 255)
        {
            uint8_t tmpValue = static_cast<uint8_t>(octetTestVar);
            memberData->setValue(tmpValue);
            pass = true;
        }
        break;
    }
    case CORBA::tk_longlong:
    {
        int64_t tmpValue = dataInfo->value.toLongLong(&pass);
        if (pass)
        {
            memberData->setValue(tmpValue);
        }
        break;
    }
    case CORBA::tk_ulonglong:
    {
        uint64_t tmpValue = dataInfo->value.toULongLong(&pass);
        if (pass)
        {
            memberData->setValue(tmpValue);
        }
        break;
    }
    case CORBA::tk_string:
    {
        memberData->setStringValue(dataInfo->value.toString().toUtf8().data());
        pass = true;
        break;
    }
    case CORBA::tk_enum:
    {
        const QString enumStringValue = dataInfo->value.toString();
        const CORBA::TypeCode* enumTypeCode = memberData->getTypeCode();
        const size_t enumMemberCount = enumTypeCode->member_count();

        // Find the enum int value of this enum string
        for (size_t enumIndex = 0; enumIndex < enumMemberCount; enumIndex++)
        {
            QString searchValue = enumTypeCode->member_name(enumIndex);
            if (searchValue == enumStringValue)
            {
                memberData->setValue(enumIndex);
                pass = true;
                break;
            }
        }
        break;

    } // End enum block

    default:
        std::cerr << "TopicTableModel::populateSample "
                  << "Skipped "
                  << memberName
                  << std::endl;
        break;

    } // End child type switch

    return pass;

} // End TopicTableModel::populateSample


//------------------------------------------------------------------------------
TopicTableModel::DataRow::DataRow() : type(CORBA::tk_null),
                                      isKey(false),
                                      isOptional(false),
                                      edited(false)
{}


//------------------------------------------------------------------------------
TopicTableModel::DataRow::~DataRow()
{}


//------------------------------------------------------------------------------
bool TopicTableModel::DataRow::setValue(const QVariant& newValue)
{
    bool pass = false;
    switch (type)
    {
        case CORBA::tk_long:
            newValue.toInt(&pass);
            break;
        case CORBA::tk_short:
            newValue.toInt(&pass);
            break;
        case CORBA::tk_enum:
            pass = true;
            break;
        case CORBA::tk_ushort:
            newValue.toUInt(&pass);
            break;
        case CORBA::tk_ulong:
            newValue.toUInt(&pass);
            break;
        case CORBA::tk_float:
            newValue.toFloat(&pass);
            break;
        case CORBA::tk_double:
            newValue.toDouble(&pass);
            break;
        case CORBA::tk_boolean:
        if (newValue.toString() == "0" || newValue.toString() == "1")
        {
            pass = true;
            break;
        }
        case CORBA::tk_char:
        if (newValue.canConvert(QMetaType::Char))
        {
            pass = true;
            break;
        }
        case CORBA::tk_octet:
        {
            bool tmpPass = false;
            int32_t octetTestVar = newValue.toInt(&tmpPass);
            if (tmpPass && octetTestVar >= 0 && octetTestVar <= 255)
            {
                pass = true;
            }
            break;
        }
        case CORBA::tk_longlong:
            newValue.toLongLong(&pass);
            break;
        case CORBA::tk_ulonglong:
            newValue.toULongLong(&pass);
            break;
        case CORBA::tk_string:
            pass = true;
            break;
        default:
            pass = false;
            break;
    }

    if (pass && value != newValue)
    {
        value = newValue;
        edited = true;

        return true;
    }

    return false;

} // End TopicTableModel::DataRow::setValue


/**
 * @}
 */
