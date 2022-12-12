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

    emit dataHasChanged();
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
CORBA::TCKind typekind_to_tckind(DDS::TypeKind tk)
{
    switch (tk) {
    case TK_INT32:
      return CORBA::tk_long;
    case TK_UINT32:
      return CORBA::tk_ulong;
    case TK_INT16:
      return CORBA::tk_short;
    case TK_UINT16:
      return CORBA::tk_ushort;
    case TK_INT64:
      return CORBA::tk_longlong;
    case TK_UINT64:
      return CORBA::tk_ulonglong;
    case TK_FLOAT32:
      return CORBA::tk_float;
    case TK_FLOAT64:
      return CORBA::tk_double;
    case TK_FLOAT128:
      return CORBA::tk_longdouble;
    case TK_CHAR8:
      return CORBA::tk_char;
    case TK_CHAR16:
      return CORBA::tk_wchar;
    case TK_BYTE:
      return CORBA::tk_octet;
    case TK_BOOLEAN:
      return CORBA::tk_boolean;
    case TK_STRING8:
      return CORBA::tk_string;
    case TK_STRING16:
      return CORBA::tk_wstring;
    case TK_ENUM:
      return CORBA::tk_enum;
    case TK_SEQUENCE:
      return CORBA::tk_sequence;
    case TK_ARRAY:
      return CORBA::tk_array;
    case TK_STRUCTURE:
      return CORBA::tk_struct;
    case TK_UNION:
      return CORBA::tk_union;
    default:
      // Use tk_void?
      return CORBA::tk_void;
    }
}

//------------------------------------------------------------------------------
bool TopicTableModel::check_rc(DDS::ReturnCode_t rc, const char* what)
{
  if (rc != DDS::RETCODE_OK) {
    std::cerr << "WARNING: " << what << std::endl;
    return false;
  }
  return true;
}

//------------------------------------------------------------------------------
void TopicTableModel::parseData(const DDS::DynamicData_var data)
{
    DDS::DynamicType_var type = data->type();
    const unsigned int count = data->get_item_count();
    for (unsigned int i = 0; i < count; ++i) {
        DDS::MemberId id = data->get_member_id_at_index(i);
        if (id == OpenDDS::XTypes::MEMBER_ID_INVALID) {
            std::cerr << "Failed to get MemberId at index " << i << std::endl;
            continue;
        }

        DDS::DynamicTypeMember_var dtm;
        DDS::ReturnCode_t ret = type->get_member(dtm, id);
        if (ret != DDS::RETCODE_OK) {
            std::cerr << "Failed to get DynamicTypeMember for member Id " << id << std::endl;
            continue;
        }
        DDS::MemberDescriptor_var md;
        ret = dtm->get_descriptor(md);
        if (ret != DDS::RETCODE_OK) {
            std::cerr << "Failed to get MemberDescriptor for member Id " << id << std::endl;
            continue;
        }

        // DataRow for each member
        DataRow* data_row = new DataRow;
        data_row->type = typekind_to_tckind(md->type()->get_kind());
        data_row->isOptional = false; // TODO: set to the right value
        data_row->name = md->name();
        data_row->isKey = false; // TODO: set to the right value

        // Update the current editor delegate
        const int thisRow = static_cast<int>(m_data.size());
        if (m_tableView->itemDelegateForRow(thisRow))
        {
            delete m_tableView->itemDelegateForRow(thisRow);
        }
        m_tableView->setItemDelegateForRow(thisRow, new LineEditDelegate(this));

        // Store the value into a QVariant
        // TODO: This only supports members of basic types. Need updates to handle
        // more complex members (sequence, array, struct, union, etc).
        switch (data_row->type) {
        case CORBA::tk_long: {
            CORBA::Long value;
            if (check_rc(data->get_int32_value(value, id), "get_int32_value failed")) {
                data_row->value = static_cast<int32_t>value;
            }
            break;
        }
        case CORBA::tk_short: {
            CORBA::Short value;
            if (check_rc(data->get_int16_value(value, id), "get_int16_value failed")) {
                data_row->value = static_cast<int16_t>(value);
            }
            break;
        }
        case CORBA::tk_ushort: {
            CORBA::UShort value;
            if (check_rc(data->get_uint16_value(value, id), "get_uint16_value failed")) {
                data_row->value = static_cast<uint16_t>(value);
            }
            break;
        }
        case CORBA::tk_ulong: {
            CORBA::ULong value;
            if (check_rc(data->get_uint32_value(value, id), "get_uint32_value failed")) {
                data_row->value = static_cast<uint32_t>(value);
            }
            break;
        }
        case CORBA::tk_float: {
            CORBA::Float value;
            if (check_rc(data->get_float32_value(value, id), "get_float32_value failed")) {
                data_row->value = static_cast<float>(value);
            }
            break;
        }
        case CORBA::tk_double: {
            CORBA::Double value;
            if (check_rc(data->get_float64_value(value, id), "get_float64_value failed")) {
                data_row->value = static_cast<double>(value);
            }
            break;
        }
        case CORBA::tk_boolean: {
            CORBA::Boolean value;
            if (check_rc(data->get_boolean_value(value, id), "get_boolean_value failed")) {
                data_row->value = static_cast<uint32_t>(value);
            }
            break;
        }
        case CORBA::tk_char: {
            CORBA::Char value;
            if (check_rc(data->get_char8_value(value, id), "get_char8_value failed")) {
                data_row->value = value;
            }
            break;
        }
        case CORBA::tk_wchar: {
            CORBA::WChar value;
            if (check_rc(data->get_char16_value(value, id), "get_char16_value failed")) {
                // TODO: Set to data_row?
                // data_row->value = value;
            }
            break;
        }
        case CORBA::tk_octet: {
            CORBA::Octet value;
            if (check_rc(data->get_byte_value(value, id), "get_byte_value failed")) {
                data_row->value = static_cast<uint8_t>(value);
            }
            break;
        }
        case CORBA::tk_longlong: {
            CORBA::LongLong value;
            if (check_rc(data->get_int64_value(value, id), "get_int64_value failed")) {
                data_row->value = static_cast<qint64>(value);
            }
            break;
        }
        case CORBA::tk_ulonglong: {
            CORBA::ULongLong value;
            if (check_rc(data->get_uint64_value(value, id), "get_uint64_value failed")) {
                data_row->value = static_cast<quint64>(value);
            }
            break;
        }
        case CORBA::tk_string: {
            char* value;
            if (check_rc(data->get_string_value(value, id), "get_string_value failed")) {
                data_row->value = value;
            }
            break;
        }
        default:
            data_row->value = "NULL";
            break;
        }

        m_data.push_back(data_row);
    }
}

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
        const CORBA::ULong enumMemberCount = enumTypeCode->member_count();

        // Find the enum int value of this enum string
        for (CORBA::ULong enumIndex = 0; enumIndex < enumMemberCount; enumIndex++)
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
