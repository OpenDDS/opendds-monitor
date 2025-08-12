#include "qos_dictionary.h"
#include "topic_table_model.h"
#include "editor_delegates.h"
#include "open_dynamic_data.h"
#include "dds_manager.h"
#include "dds_data.h"

#include <QIcon>

#include <iostream>
#include <cstdint>
#include <stdexcept>

//------------------------------------------------------------------------------
TopicTableModel::TopicTableModel(QTableView *parent, const QString &topicName)
    : QAbstractTableModel(parent), m_tableView(parent), m_sample(nullptr), m_topicName(topicName)
{
    m_columnHeaders << "Name" << "Type" << "Value";
    m_tableView->setItemDelegate(new LineEditDelegate(this));

    // Create a blank sample, so the user can publish an initial instance
    std::shared_ptr<TopicInfo> topicInfo = CommonData::getTopicInfo(m_topicName);
    if (!topicInfo)
    {
        throw std::runtime_error(std::string("TopicTableModel: No topic information found for topic \"") +
                                 topicName.toStdString() + "\"");
    }

    // TODO: Keep the call to setSample for blank OpenDynamicData sample and maybe
    // call setSample overloads conditionally depending on the monitor's settings, i.e.,
    // whether we want to use OpenDynamicData or DDS's DynamicData.

    if (topicInfo->typeMode() == TypeDiscoveryMode::TypeCode)
    {
        // This is not really a blank sample, but a null sample.
        std::shared_ptr<OpenDynamicData> blankSample = CreateOpenDynamicData(topicInfo->typeCode(),
                                                                             QosDictionary::getEncodingKind(), topicInfo->extensibility());
        setSample(blankSample);
    }
}

//------------------------------------------------------------------------------
TopicTableModel::~TopicTableModel()
{
    cleanupDataRow();
}

void TopicTableModel::cleanupDataRow()
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
        std::cerr << "TopicTableModel::setSample: Null OpenDynamicData sample" << std::endl;
        return;
    }

    if (sample->getLength() == 0)
    {
        std::cerr << "TopicTableModel::setSample: Sample has no children" << std::endl;
        return;
    }

    emit layoutAboutToBeChanged();

    cleanupDataRow();
    m_sample = sample;
    parseData(m_sample);

    emit layoutChanged();
}

//------------------------------------------------------------------------------
void TopicTableModel::setSample(DDS::DynamicData_var sample)
{
    if (!sample)
    {
        std::cerr << "TopicTableModel::setSample: Null DynamicData sample" << std::endl;
        return;
    }

    emit layoutAboutToBeChanged();

    cleanupDataRow();

    // Store sample for reverting any changes to it.
    m_dynamicSample = sample;
    parseData(m_dynamicSample, "");

    emit layoutChanged();
}

//------------------------------------------------------------------------------
const std::shared_ptr<OpenDynamicData> TopicTableModel::commitSample()
{
    // Reset the edited state
    for (size_t i = 0; i < m_data.size(); ++i)
    {
        m_data.at(i)->setEdited(false);
    }

    // Create a new sample
    std::shared_ptr<TopicInfo> topicInfo = CommonData::getTopicInfo(m_topicName);
    if (!topicInfo || topicInfo->typeMode() != TypeDiscoveryMode::TypeCode)
    {
        return nullptr;
    }

    // Create a copy of the current sample
    std::shared_ptr<OpenDynamicData> newSample = CreateOpenDynamicData(topicInfo->typeCode(),
                                                                       QosDictionary::getEncodingKind(),
                                                                       topicInfo->extensibility());
    *newSample = *m_sample;

    // Populate the new sample from the user-edited changes
    for (size_t i = 0; i < m_data.size(); i++)
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
    if (row < 0 || static_cast<size_t>(row) >= m_data.size() || !m_data.at(row))
    {
        return QVariant();
    }

    const DataRow *const member = m_data.at(row);

    if (column == VALUE_COLUMN && member->getEdited() == true)
    {
        if (role == Qt::BackgroundRole)
        {
            // Change the background color for edited rows
            return QColor(Qt::yellow);
        }
        else if (role == Qt::ForegroundRole)
        {
            // Change the text color for edited rows (black on yellow)
            return QColor(Qt::black);
        }
    }

    // Is this a request to display the text value?
    if (role == Qt::DisplayRole && column == NAME_COLUMN)
    {
        return member->getName();
    }
    else if (role == Qt::DisplayRole && column == VALUE_COLUMN)
    {
        return member->getDisplayedValue();
    }
    else if (role == Qt::DisplayRole && column == TYPE_COLUMN)
    {
        const CORBA::TCKind typeID = member->getType();
        switch (typeID)
        {
        case CORBA::tk_short:
            return "int16";
        case CORBA::tk_long:
            return "int32";
        case CORBA::tk_ushort:
            return "uint16";
        case CORBA::tk_ulong:
            return "uint32";
        case CORBA::tk_longlong:
            return "int64";
        case CORBA::tk_ulonglong:
            return "uint64";
        case CORBA::tk_float:
            return "float32";
        case CORBA::tk_double:
            return "float64";
        case CORBA::tk_boolean:
            return "boolean";
        case CORBA::tk_char:
            return "char";
        case CORBA::tk_wchar:
            return "wchar";
        case CORBA::tk_octet:
            return "uint8";
        case CORBA::tk_enum:
            return "enum";
        case CORBA::tk_string:
            return "string";
        default:
            std::cerr << "unknown typeID: " << typeID << std::endl;
            return "?";
        }
    }

    return QVariant();
}

//------------------------------------------------------------------------------
bool TopicTableModel::setData(const QModelIndex &index,
                              const QVariant &value,
                              int)
{
    const int row = index.row();

    // Make sure we're in bounds of the data array
    if (row < 0 || static_cast<size_t>(row) >= m_data.size())
    {
        return false;
    }

    // Make sure we found the data row
    DataRow *const data = m_data.at(row);
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
    if (data->getValue() == value)
    {
        return false;
    }

    if (!data->setValue(value))
    {
        return false;
    }

    data->setEdited(true);

    emit dataHasChanged();
    return true;
}

//------------------------------------------------------------------------------
void TopicTableModel::revertChanges()
{
    std::shared_ptr<TopicInfo> topicInfo = CommonData::getTopicInfo(m_topicName);
    if (!topicInfo)
    {
        return;
    }

    if (topicInfo->typeMode() == TypeDiscoveryMode::TypeCode)
    {
        setSample(m_sample);
    }
    else
    {
        setSample(m_dynamicSample);
    }
}

void TopicTableModel::updateDisplayHex(bool display_as_hex)
{
    if (m_dispHex == display_as_hex)
    {
        return;
    }

    emit layoutAboutToBeChanged();
    m_dispHex = display_as_hex;

    for (unsigned int i = 0; i < m_data.size(); ++i)
    {
        m_data[i]->updateDisplayedValue();
    }
    emit layoutChanged();
}

void TopicTableModel::updateDisplayAscii(bool display_as_ascii)
{
    if (m_dispAscii == display_as_ascii)
    {
        return;
    }

    emit layoutAboutToBeChanged();
    m_dispAscii = display_as_ascii;

    for (unsigned int i = 0; i < m_data.size(); ++i)
    {
        m_data[i]->updateDisplayedValue();
    }
    emit layoutChanged();
}

//------------------------------------------------------------------------------
void TopicTableModel::parseData(const std::shared_ptr<OpenDynamicData> data)
{
    const size_t childCount = data->getLength();
    for (size_t i = 0; i < childCount; ++i)
    {
        const std::shared_ptr<OpenDynamicData> child = data->getMember(i);
        if (!child)
        {
            continue;
        }

        // Handle child data if necessary
        if (child->isContainerType())
        {
            parseData(child);
            continue;
        }

        DataRow *dataRow = new DataRow(this,
                                       child->getFullName().c_str(),
                                       child->getKind(),
                                       false, // TODO
                                       false);

        // Update the current editor delegate
        const int thisRow = static_cast<int>(m_data.size());
        if (m_tableView->itemDelegateForRow(thisRow))
        {
            delete m_tableView->itemDelegateForRow(thisRow);
        }
        m_tableView->setItemDelegateForRow(thisRow, new LineEditDelegate(this));

        // Store the value into a QVariant
        // The tmpValue may seem redundant, but it's very helpful for debug
        switch (dataRow->getType())
        {
        case CORBA::tk_long:
        {
            const int32_t tmpValue = child->getValue<int32_t>();
            dataRow->setValue(tmpValue);
            break;
        }
        case CORBA::tk_short:
        {
            const int16_t tmpValue = child->getValue<int16_t>();
            dataRow->setValue(tmpValue);
            break;
        }
        case CORBA::tk_ushort:
        {
            const uint16_t tmpValue = child->getValue<uint16_t>();
            dataRow->setValue(tmpValue);
            break;
        }
        case CORBA::tk_ulong:
        {
            const uint32_t tmpValue = child->getValue<uint32_t>();
            dataRow->setValue(tmpValue);
            break;
        }
        case CORBA::tk_float:
        {
            const float tmpValue = child->getValue<float>();
            dataRow->setValue(tmpValue);
            break;
        }
        case CORBA::tk_double:
        {
            const double tmpValue = child->getValue<double>();
            dataRow->setValue(tmpValue);
            break;
        }
        case CORBA::tk_boolean:
        {
            const bool tmpValue = child->getValue<bool>();
            dataRow->setValue(tmpValue);
            break;
        }
        case CORBA::tk_char:
        {
            const char tmpValue = child->getValue<char>();
            dataRow->setValue(tmpValue);
            break;
        }
        case CORBA::tk_wchar:
        {
            wchar_t tmpValue[2] = {0, 0};
            tmpValue[0] = child->getValue<wchar_t>();
            dataRow->setValue(QString(ACE_Wide_To_Ascii(tmpValue).char_rep()));
            break;
        }
        case CORBA::tk_octet:
        {
            const uint8_t tmpValue = child->getValue<uint8_t>();
            dataRow->setValue(tmpValue);
            break;
        }
        case CORBA::tk_longlong:
        {
            const qint64 tmpValue = child->getValue<qint64>();
            dataRow->setValue(tmpValue);
            break;
        }
        case CORBA::tk_ulonglong:
        {
            const quint64 tmpValue = child->getValue<quint64>();
            dataRow->setValue(tmpValue);
            break;
        }
        case CORBA::tk_string:
        {
            const char *tmpValue = child->getStringValue();
            dataRow->setValue(tmpValue);
            break;
        }
        case CORBA::tk_enum:
        {
            // Make sure the int value is valid
            const CORBA::ULong enumValue = child->getValue<CORBA::ULong>();
            CORBA::TypeCode_var enumTypeCode = child->getTypeCode();
            const CORBA::ULong enumMemberCount = enumTypeCode->member_count();
            if (enumValue >= enumMemberCount)
            {
                dataRow->setValue("INVALID");
                break;
            }

            // Remove the old delegate
            if (m_tableView->itemDelegateForRow(thisRow))
            {
                delete m_tableView->itemDelegateForRow(thisRow);
            }

            // Install the new delegate
            ComboDelegate *enumDelegate = new ComboDelegate(this);
            for (CORBA::ULong enumIndex = 0; enumIndex < enumMemberCount; ++enumIndex)
            {
                QString stringValue = enumTypeCode->member_name(enumIndex);
                if (!stringValue.isEmpty())
                {
                    enumDelegate->addItem(stringValue, static_cast<int>(enumIndex));
                }
            }

            m_tableView->setItemDelegateForRow(thisRow, enumDelegate);
            dataRow->setValue(enumTypeCode->member_name(enumValue));
            break;
        }
        default:
            std::cerr << "TopicTableModel::parseData: Unknown kind: " << dataRow->getType() << std::endl;
            dataRow->setValue("NULL");
            break;
        }

        m_data.push_back(dataRow);
    }
}

//------------------------------------------------------------------------------
CORBA::TCKind TopicTableModel::typekind_to_tckind(DDS::TypeKind tk)
{
    using namespace OpenDDS::XTypes;
    switch (tk)
    {
    case TK_INT32:
        return CORBA::tk_long;
    case TK_UINT32:
        return CORBA::tk_ulong;
    case TK_INT8:
        return CORBA::tk_short;
    case TK_UINT8:
        return CORBA::tk_ushort;
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
    {
        std::cerr << "unknown TypeKind " << static_cast<uint16_t>(tk) << std::endl;
        // Use tk_void?
        return CORBA::tk_void;
    }
    }
}

//------------------------------------------------------------------------------
bool TopicTableModel::check_rc(DDS::ReturnCode_t rc, const char *what)
{
    if (rc != DDS::RETCODE_OK)
    {
        std::cerr << "WARNING: " << what << std::endl;
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------
void TopicTableModel::setDataRow(DataRow *const data_row,
                                 const DDS::DynamicData_var &data,
                                 DDS::MemberId id)
{
    // Helper function to check NO_DATA and set value
    auto setValueOrNotPresent = [&](DDS::ReturnCode_t ret, auto value, const char *errorMsg) -> bool
    {
        if (ret == DDS::RETCODE_NO_DATA && data_row->getIsOptional())
        {
            data_row->setValue("(not present)");
            return true;
        }
        else if (check_rc(ret, errorMsg))
        {
            data_row->setValue(value);
            return true;
        }
        return false;
    };

    switch (data_row->getType())
    {
    case CORBA::tk_long:
    {
        CORBA::Long value;
        DDS::ReturnCode_t ret = data->get_int32_value(value, id);
        setValueOrNotPresent(ret, static_cast<int32_t>(value), "get_int32_value failed");
        break;
    }
    case CORBA::tk_short:
    {
        CORBA::Short value;
        DDS::ReturnCode_t ret = data->get_int16_value(value, id);
        if (ret == DDS::RETCODE_NO_DATA && data_row->getIsOptional())
        {
            data_row->setValue("(not present)");
        }
        else if (ret != DDS::RETCODE_OK)
        {
            ACE_INT8 tmp;
            ret = data->get_int8_value(tmp, id);
            if (setValueOrNotPresent(ret, static_cast<int16_t>(static_cast<CORBA::Short>(tmp)), "get_int8_value failed"))
            {
                // Value was set, nothing more to do
            }
        }
        else if (check_rc(ret, "get_int16_value failed"))
        {
            data_row->setValue(static_cast<int16_t>(value));
        }
        break;
    }
    case CORBA::tk_ushort:
    {
        CORBA::UShort value;
        DDS::ReturnCode_t ret = data->get_uint16_value(value, id);
        if (ret == DDS::RETCODE_NO_DATA && data_row->getIsOptional())
        {
            data_row->setValue("(not present)");
        }
        else if (ret != DDS::RETCODE_OK)
        {
            ACE_UINT8 tmp;
            ret = data->get_uint8_value(tmp, id);
            if (setValueOrNotPresent(ret, static_cast<uint16_t>(static_cast<CORBA::UShort>(tmp)), "get_uint8_value failed"))
            {
                // Value was set, nothing more to do
            }
        }
        else if (check_rc(ret, "get_uint16_value failed"))
        {
            data_row->setValue(static_cast<uint16_t>(value));
        }
        break;
    }
    case CORBA::tk_ulong:
    {
        CORBA::ULong value;
        DDS::ReturnCode_t ret = data->get_uint32_value(value, id);
        setValueOrNotPresent(ret, static_cast<uint32_t>(value), "get_uint32_value failed");
        break;
    }
    case CORBA::tk_float:
    {
        CORBA::Float value;
        DDS::ReturnCode_t ret = data->get_float32_value(value, id);
        setValueOrNotPresent(ret, static_cast<float>(value), "get_float32_value failed");
        break;
    }
    case CORBA::tk_double:
    {
        CORBA::Double value;
        DDS::ReturnCode_t ret = data->get_float64_value(value, id);
        setValueOrNotPresent(ret, static_cast<double>(value), "get_float64_value failed");
        break;
    }
    case CORBA::tk_boolean:
    {
        CORBA::Boolean value;
        DDS::ReturnCode_t ret = data->get_boolean_value(value, id);
        setValueOrNotPresent(ret, static_cast<bool>(value), "get_boolean_value failed");
        break;
    }
    case CORBA::tk_char:
    {
        char tmp;
        DDS::ReturnCode_t ret = data->get_char8_value(tmp, id);
        setValueOrNotPresent(ret, tmp, "get_char8_value failed");
        break;
    }
    case CORBA::tk_wchar:
    {
        CORBA::WChar value[2] = {0, 0};
        DDS::ReturnCode_t ret = data->get_char16_value(value[0], id);
        setValueOrNotPresent(ret, QString(ACE_Wide_To_Ascii(value).char_rep()), "get_char16_value failed");
        break;
    }
    case CORBA::tk_octet:
    {
        CORBA::Octet value;
        DDS::ReturnCode_t ret = data->get_byte_value(value, id);
        setValueOrNotPresent(ret, static_cast<uint8_t>(value), "get_byte_value failed");
        break;
    }
    case CORBA::tk_longlong:
    {
        CORBA::LongLong value;
        DDS::ReturnCode_t ret = data->get_int64_value(value, id);
        setValueOrNotPresent(ret, static_cast<qint64>(value), "get_int64_value failed");
        break;
    }
    case CORBA::tk_ulonglong:
    {
        CORBA::ULongLong value;
        DDS::ReturnCode_t ret = data->get_uint64_value(value, id);
        setValueOrNotPresent(ret, static_cast<quint64>(value), "get_uint64_value failed");
        break;
    }
    case CORBA::tk_string:
    {
        CORBA::String_var value;
        DDS::ReturnCode_t ret = data->get_string_value(value, id);
        setValueOrNotPresent(ret, value.in(), "get_string_value failed");
        break;
    }
    case CORBA::tk_enum:
    {
        // When @bit_bound is supported, update this to call the right interface.
        CORBA::Long value;
        if (check_rc(data->get_int32_value(value, id), "get enum value failed"))
        {
            DDS::DynamicType_var type = data->type();
            const OpenDDS::XTypes::TypeKind tk = type->get_kind();
            DDS::DynamicType_var enum_dt;
            if (tk == OpenDDS::XTypes::TK_STRUCTURE || tk == OpenDDS::XTypes::TK_UNION)
            {
                DDS::DynamicTypeMember_var enum_dtm;
                if (type->get_member(enum_dtm, id) != DDS::RETCODE_OK)
                {
                    std::cerr << "get_member failed for enum member with Id "
                              << id << std::endl;
                    break;
                }
                DDS::MemberDescriptor_var enum_md;
                if (enum_dtm->get_descriptor(enum_md) != DDS::RETCODE_OK)
                {
                    std::cerr << "get_descriptor failed for enum member with Id "
                              << id << std::endl;
                    break;
                }
                enum_dt = DDS::DynamicType::_duplicate(enum_md->type());
            }
            else if (tk == OpenDDS::XTypes::TK_SEQUENCE || tk == OpenDDS::XTypes::TK_ARRAY)
            {
                DDS::TypeDescriptor_var td;
                if (type->get_descriptor(td) != DDS::RETCODE_OK)
                {
                    std::cerr << "get_descriptor failed" << std::endl;
                    break;
                }
                enum_dt = OpenDDS::XTypes::get_base_type(td->element_type());
            }

            DDS::DynamicTypeMember_var enum_lit_dtm;
            if (enum_dt->get_member(enum_lit_dtm, value) != DDS::RETCODE_OK)
            {
                std::cerr << "get_member failed for enum literal with value " << value << std::endl;
                break;
            }
            DDS::MemberDescriptor_var enum_lit_md;
            if (enum_lit_dtm->get_descriptor(enum_lit_md) != DDS::RETCODE_OK)
            {
                std::cerr << "get_descriptor failed for enum literal with value " << value << std::endl;
                break;
            }
            data_row->setValue(enum_lit_md->name());
        }
        break;
    }
    default:
        data_row->setValue("NULL");
        break;
    }
}

//------------------------------------------------------------------------------
void TopicTableModel::parseCollection(const DDS::DynamicData_var &data, const std::string &namePrefix)
{
    DDS::DynamicType_var type = data->type();
    DDS::TypeDescriptor_var td;
    if (type->get_descriptor(td) != DDS::RETCODE_OK)
    {
        std::cerr << "Failed to get TypeDescriptor for a "
                  << OpenDDS::XTypes::typekind_to_string(type->get_kind())
                  << " member" << std::endl;
        return;
    }
    const unsigned int count = data->get_item_count();
    DDS::DynamicType_var elem_type = OpenDDS::XTypes::get_base_type(td->element_type());
    const OpenDDS::XTypes::TypeKind elem_tk = elem_type->get_kind();

    for (unsigned int i = 0; i < count; ++i)
    {
        DDS::MemberId id = data->get_member_id_at_index(i);
        if (id == OpenDDS::XTypes::MEMBER_ID_INVALID)
        {
            std::cerr << "Failed to get MemberId for element at index " << i << std::endl;
            continue;
        }

        switch (elem_tk)
        {
        case OpenDDS::XTypes::TK_SEQUENCE:
        case OpenDDS::XTypes::TK_ARRAY:
        case OpenDDS::XTypes::TK_STRUCTURE:
        case OpenDDS::XTypes::TK_UNION:
        {
            DDS::DynamicData_var nested_data;
            DDS::ReturnCode_t ret = data->get_complex_value(nested_data, id);
            if (ret != DDS::RETCODE_OK)
            {
                std::cerr << "get_complex_value for element at index " << i << " failed" << std::endl;
            }
            else
            {
                std::string scoped_elem_name = namePrefix + "[" + std::to_string(i) + "]";
                parseData(nested_data, scoped_elem_name);
            }
            continue;
        }
        }

        const std::string scoped_elem_name = namePrefix + "[" + std::to_string(i) + "]";

        // DataRow for each element
        DataRow *data_row = new DataRow(this,
                                        scoped_elem_name.c_str(),
                                        typekind_to_tckind(elem_tk),
                                        false, // TODO: Get the right value from the containing type
                                        false  // TODO: Get the right value from the containing type
        );

        // Update the current editor delegate
        const int thisRow = static_cast<int>(m_data.size());
        if (m_tableView->itemDelegateForRow(thisRow))
        {
            delete m_tableView->itemDelegateForRow(thisRow);
        }
        m_tableView->setItemDelegateForRow(thisRow, new LineEditDelegate(this));

        setDataRow(data_row, data, id);
        m_data.push_back(data_row);
    }
}

//------------------------------------------------------------------------------
void TopicTableModel::parseAggregated(const DDS::DynamicData_var &data, const std::string &namePrefix)
{
    DDS::DynamicType_var type = data->type();
    const CORBA::ULong total_member_count = type->get_member_count(); // Get the whole member, even if it was optional and empty
    for (CORBA::ULong i = 0; i < total_member_count; ++i)
    {
        DDS::MemberId id = data->get_member_id_at_index(i);
        if (id == OpenDDS::XTypes::MEMBER_ID_INVALID)
        {
            std::cerr << "Failed to get MemberId at index " << i << std::endl;
            continue;
        }

        DDS::DynamicTypeMember_var dtm;
        DDS::ReturnCode_t ret = type->get_member(dtm, id);
        if (ret != DDS::RETCODE_OK)
        {
            std::cerr << "Failed to get DynamicTypeMember for member Id " << id << std::endl;
            continue;
        }
        DDS::MemberDescriptor_var md;
        ret = dtm->get_descriptor(md);
        if (ret != DDS::RETCODE_OK)
        {
            std::cerr << "Failed to get MemberDescriptor for member Id " << id << std::endl;
            continue;
        }

        const std::string scoped_member_name = namePrefix.empty() ? md->name() : namePrefix + "." + md->name();
        const DDS::DynamicType_var base_type = OpenDDS::XTypes::get_base_type(md->type());
        const OpenDDS::XTypes::TypeKind member_tk = base_type->get_kind();

        switch (member_tk)
        {
        case OpenDDS::XTypes::TK_SEQUENCE:
        case OpenDDS::XTypes::TK_ARRAY:
        case OpenDDS::XTypes::TK_STRUCTURE:
        case OpenDDS::XTypes::TK_UNION:
        {
            DDS::DynamicData_var nested_data;
            ret = data->get_complex_value(nested_data, id);
            if (ret != DDS::RETCODE_OK)
            {
                std::cerr << "get_complex_value for member Id " << id << " failed" << std::endl;
            }
            else
            {
                parseData(nested_data, scoped_member_name);
            }
            continue;
        }
        }

        // DataRow for each member
        DataRow *data_row = new DataRow(this,
                                        scoped_member_name.c_str(),
                                        typekind_to_tckind(member_tk),
                                        md->is_key(), // TODO: Handle implicit key case
                                        md->is_optional());

        // Update the current editor delegate
        const int thisRow = static_cast<int>(m_data.size());
        if (m_tableView->itemDelegateForRow(thisRow))
        {
            delete m_tableView->itemDelegateForRow(thisRow);
        }
        m_tableView->setItemDelegateForRow(thisRow, new LineEditDelegate(this));

        setDataRow(data_row, data, id);

        m_data.push_back(data_row);
    }
}

//------------------------------------------------------------------------------
void TopicTableModel::parseData(const DDS::DynamicData_var &data, const std::string &namePrefix)
{
    const OpenDDS::XTypes::TypeKind tk = data->type()->get_kind();
    switch (tk)
    {
    case OpenDDS::XTypes::TK_SEQUENCE:
    case OpenDDS::XTypes::TK_ARRAY:
        parseCollection(data, namePrefix);
        break;
    case OpenDDS::XTypes::TK_STRUCTURE:
    case OpenDDS::XTypes::TK_UNION:
        parseAggregated(data, namePrefix);
        break;
    default:
        std::cerr << "Attempted to parse a DynamicData object of unexpected type ("
                  << OpenDDS::XTypes::typekind_to_string(tk) << ")" << std::endl;
    }
}

//------------------------------------------------------------------------------
bool TopicTableModel::populateSample(std::shared_ptr<OpenDynamicData> const sample,
                                     DataRow *const dataInfo)
{
    bool pass = false;
    if (!sample || !dataInfo)
    {
        return false;
    }

    const std::string memberName = dataInfo->getName().toUtf8().data();
    std::shared_ptr<OpenDynamicData> memberData = sample->getMember(memberName);
    if (!memberData)
    {
        std::cerr << "TopicTableModel::populateSample: "
                  << "Unable to find member named "
                  << memberName
                  << std::endl;

        return false;
    }

    switch (dataInfo->getType())
    {
    case CORBA::tk_long:
    {
        const int32_t intVal = dataInfo->getValue().toInt(&pass);
        if (pass)
        {
            memberData->setValue(intVal);
        }
        break;
    }
    case CORBA::tk_ulong:
    {
        const uint32_t uintVal = dataInfo->getValue().toUInt(&pass);
        if (pass)
        {
            memberData->setValue(uintVal);
        }
        break;
    }
    case CORBA::tk_short:
    {
        const int32_t intVal = dataInfo->getValue().toInt(&pass);
        if (pass)
        {
            memberData->setValue(static_cast<int16_t>(intVal));
        }
        break;
    }
    case CORBA::tk_ushort:
    {
        const uint32_t uintVal = dataInfo->getValue().toUInt(&pass);
        if (pass)
        {
            memberData->setValue(static_cast<uint16_t>(uintVal));
        }
        break;
    }
    case CORBA::tk_float:
    {
        const float fltVal = dataInfo->getValue().toFloat(&pass);
        if (pass)
        {
            memberData->setValue(fltVal);
        }
        break;
    }
    case CORBA::tk_double:
    {
        const double dblVal = dataInfo->getValue().toDouble(&pass);
        if (pass)
        {
            memberData->setValue(dblVal);
        }
        break;
    }
    case CORBA::tk_boolean:
    {
        if (dataInfo->getValue().canConvert<bool>())
        {
            const bool val = dataInfo->getValue().toBool();
            memberData->setValue(val);
            pass = true;
        }
        break;
    }
    case CORBA::tk_char:
    {
        if (dataInfo->getValue().canConvert<char>())
        {
            const char val = dataInfo->getValue().toChar().toLatin1();
            memberData->setValue(val);
            pass = true;
        }
        break;
    }
    case CORBA::tk_wchar:
    {
        if (dataInfo->getValue().canConvert<QChar>())
        {
            const char16_t val = dataInfo->getValue().toChar().unicode();
            memberData->setValue(val);
            pass = true;
        }
        break;
    }
    case CORBA::tk_octet:
    {
        const uint32_t uintVal = dataInfo->getValue().toUInt(&pass);
        if (pass)
        {
            memberData->setValue(static_cast<uint8_t>(uintVal));
        }
        break;
    }
    case CORBA::tk_longlong:
    {
        const int64_t llVal = dataInfo->getValue().toLongLong(&pass);
        if (pass)
        {
            memberData->setValue(llVal);
        }
        break;
    }
    case CORBA::tk_ulonglong:
    {
        const uint64_t ullVal = dataInfo->getValue().toULongLong(&pass);
        if (pass)
        {
            memberData->setValue(ullVal);
        }
        break;
    }
    case CORBA::tk_string:
    {
        memberData->setStringValue(dataInfo->getValue().toString().toUtf8().data());
        pass = true;
        break;
    }
    case CORBA::tk_enum:
    {
        const QString enumStringValue = dataInfo->getValue().toString();
        CORBA::TypeCode_var enumTypeCode = memberData->getTypeCode();
        const CORBA::ULong enumMemberCount = enumTypeCode->member_count();

        // Find the enum int value of this enum string
        for (CORBA::ULong i = 0; i < enumMemberCount; ++i)
        {
            const QString searchValue = enumTypeCode->member_name(i);
            if (searchValue == enumStringValue)
            {
                memberData->setValue(i);
                pass = true;
                break;
            }
        }
        break;
    }
    default:
        std::cerr << "TopicTableModel::populateSample: Unsupported type for member \""
                  << memberName << "\"" << std::endl;
        break;
    }

    return pass;
}

//------------------------------------------------------------------------------
TopicTableModel::DataRow::DataRow(TopicTableModel *parent, QString name,
                                  CORBA::TCKind kind, bool isKey, bool isOptional)
    : m_parent(parent), m_name(name), m_type(kind), m_isKey(isKey), m_isOptional(isOptional), m_edited(false)
{
}

//------------------------------------------------------------------------------
TopicTableModel::DataRow::~DataRow()
{
}

//------------------------------------------------------------------------------
bool TopicTableModel::DataRow::setValue(const QVariant &newValue)
{
    QVariant origValue = newValue, dispValue;
    bool pass = false;

    if (newValue.toString() == "(not present)")
    {
        origValue = newValue.toString();
        dispValue = origValue;
        pass = true;
    }
    else
    {

        switch (m_type)
        {
        case CORBA::tk_long:
        {
            const int intVal = newValue.toInt(&pass);
            if (pass)
            {
                origValue = intVal;
                dispValue = m_parent->type_to_qvariant(intVal);
            }
            break;
        }
        case CORBA::tk_short:
        {
            const int intVal = newValue.toInt(&pass);
            if (pass)
            {
                // Truncate the input value to fit the type of the member.
                const qint16 shortVal = static_cast<qint16>(intVal);
                origValue = shortVal;
                dispValue = m_parent->type_to_qvariant(shortVal);
            }
            break;
        }
        case CORBA::tk_enum:
        {
            // Make sure the input value matches one of the enumerators.
            const QString strVal = newValue.toString();
            if (strVal.isEmpty())
            {
                break;
            }
            origValue = strVal;

            std::shared_ptr<TopicInfo> topicInfo = CommonData::getTopicInfo(m_parent->m_topicName);
            if (!topicInfo)
            {
                std::cerr << "Failed to get TopicInfo for topic\"" << m_parent->m_topicName.toStdString() << "\"" << std::endl;
                return false;
            }

            if (topicInfo->typeMode() == TypeDiscoveryMode::TypeCode)
            {
                CORBA::TypeCode_var enumTypeCode = m_parent->m_sample->getTypeCode();
                const CORBA::ULong memberCount = enumTypeCode->member_count();
                for (CORBA::ULong i = 0; i < memberCount; ++i)
                {
                    if (origValue == enumTypeCode->member_name(i))
                    {
                        pass = true;
                        dispValue = origValue;
                        break;
                    }
                }
            }
            else
            {
                // TODO: Verify that the input string matches one of the enumerators.
                pass = true;
                dispValue = origValue;
            }
            break;
        }
        case CORBA::tk_ushort:
        {
            const unsigned int uintVal = newValue.toUInt(&pass);
            if (pass)
            {
                const quint16 ushortVal = static_cast<quint16>(uintVal);
                origValue = ushortVal;
                dispValue = m_parent->type_to_qvariant(ushortVal);
            }
            break;
        }
        case CORBA::tk_ulong:
        {
            const unsigned int uintVal = newValue.toUInt(&pass);
            if (pass)
            {
                origValue = uintVal;
                dispValue = m_parent->type_to_qvariant(uintVal);
            }
            break;
        }
        case CORBA::tk_float:
            origValue = newValue.toFloat(&pass);
            dispValue = origValue;
            break;
        case CORBA::tk_double:
            origValue = newValue.toDouble(&pass);
            dispValue = origValue;
            break;
        case CORBA::tk_boolean:
            origValue = newValue.toBool();
            dispValue = origValue;
            pass = true;
            break;
        case CORBA::tk_char:
            pass = newValue.canConvert<QChar>();
            if (pass)
            {
                const QChar charVal = newValue.toChar();
                origValue = charVal;
                dispValue = m_parent->type_to_qvariant(charVal.toLatin1());
            }
            break;
        case CORBA::tk_octet:
        {
            const unsigned int uintVal = newValue.toUInt(&pass);
            if (pass)
            {
                const quint8 octetVal = static_cast<quint8>(uintVal);
                origValue = octetVal;
                dispValue = m_parent->type_to_qvariant(octetVal);
            }
            break;
        }
        case CORBA::tk_longlong:
        {
            const qlonglong llVal = newValue.toLongLong(&pass);
            if (pass)
            {
                origValue = llVal;
                dispValue = m_parent->type_to_qvariant(llVal);
            }
            break;
        }
        case CORBA::tk_ulonglong:
        {
            const qulonglong ullVal = newValue.toULongLong(&pass);
            if (pass)
            {
                origValue = ullVal;
                dispValue = m_parent->type_to_qvariant(ullVal);
            }
            break;
        }
        case CORBA::tk_string:
            pass = newValue.canConvert<QString>();
            if (pass)
            {
                origValue = newValue.toString();
                dispValue = origValue;
            }
            break;
        default:
            break;
        }
    }
    if (pass)
    {
        m_value = origValue;
        m_displayedValue = dispValue;
        return true;
    }

    return false;
}

void TopicTableModel::DataRow::updateDisplayedValue()
{
    switch (m_type)
    {
    case CORBA::tk_long:
    {
        // The type of the value stored in m_value should be consistent with
        // the CORBA::tk_* type. So this call to toInt() and the following calls
        // should succeed.
        const qint32 intVal = m_value.toInt();
        m_displayedValue = m_parent->type_to_qvariant(intVal);
        break;
    }
    case CORBA::tk_ulong:
    {
        const quint32 uintVal = m_value.toUInt();
        m_displayedValue = m_parent->type_to_qvariant(uintVal);
        break;
    }
    case CORBA::tk_short:
    {
        // Can't get the short value directly. Read it through toInt().
        // Similar for other types that can't be read directly from QVariant.
        const qint16 shortVal = static_cast<qint16>(m_value.toInt());
        m_displayedValue = m_parent->type_to_qvariant(shortVal);
        break;
    }
    case CORBA::tk_ushort:
    {
        const quint16 ushortVal = static_cast<quint16>(m_value.toUInt());
        m_displayedValue = m_parent->type_to_qvariant(ushortVal);
        break;
    }
    case CORBA::tk_char:
    {
        const char cVal = m_value.toChar().toLatin1();
        m_displayedValue = m_parent->type_to_qvariant(cVal);
        break;
    }
    case CORBA::tk_octet:
    {
        const quint8 octetVal = static_cast<quint8>(m_value.toUInt());
        m_displayedValue = m_parent->type_to_qvariant(octetVal);
        break;
    }
    case CORBA::tk_longlong:
    {
        const qint64 llVal = m_value.toLongLong();
        m_displayedValue = m_parent->type_to_qvariant(llVal);
        break;
    }
    case CORBA::tk_ulonglong:
    {
        const quint64 ullVal = m_value.toULongLong();
        m_displayedValue = m_parent->type_to_qvariant(ullVal);
        break;
    }
    default:
        break;
    }
}

/**
 * @}
 */
