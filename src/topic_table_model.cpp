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

    // TODO: Keep the call to setSample for blank OpenDynamicData sample and maybe
    // call setSample overloads conditionally depending on the monitor's settings, i.e.,
    // whether we want to use OpenDynamicData or DDS's DynamicData.

    // This is not really a blank sample, but a null sample.
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
void TopicTableModel::setSample(DDS::DynamicData_var sample)
{
    if (!sample) {
        std::cerr << "TopicTableModel::setSample: Sample is null" << std::endl;
        return;
    }

    emit layoutAboutToBeChanged();

    while (!m_data.empty()) {
        delete m_data.back();
        m_data.pop_back();
    }

    // Store sample for reverting any changes to it.
    m_dynamicsample = sample;
    parseData(m_dynamicsample, "");

    emit layoutChanged();
}

//------------------------------------------------------------------------------
const std::shared_ptr<OpenDynamicData> TopicTableModel::commitSample()
{
    // Reset the edited state
    for (size_t i = 0; i < m_data.size(); i++)
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
            default: {
                std::cerr << "unknown typeID: " << typeID << std::endl;
                return "?";
            }
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
    if (row < 0 || static_cast<size_t>(row) >= m_data.size())
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
            wchar_t tmpValue[2] = { 0, 0 };
            tmpValue[0] = child->getValue<wchar_t>();
            dataRow->value = ACE_Wide_To_Ascii(tmpValue).char_rep();
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
            std::cerr << "unknown kind: " << dataRow->type << std::endl;
            dataRow->value = "NULL";
            break;

        } // End child type switch

        m_data.push_back(dataRow);

    } // End member loop

} // End TopicTableModel::parseData

//------------------------------------------------------------------------------
CORBA::TCKind TopicTableModel::typekind_to_tckind(DDS::TypeKind tk)
{
    using namespace OpenDDS::XTypes;
    switch (tk) {
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
bool TopicTableModel::check_rc(DDS::ReturnCode_t rc, const char* what)
{
    if (rc != DDS::RETCODE_OK) {
        std::cerr << "WARNING: " << what << std::endl;
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------
void TopicTableModel::setDataRow(DataRow* const data_row,
                                 const DDS::DynamicData_var& data,
                                 DDS::MemberId id)
{
    switch (data_row->type) {
    case CORBA::tk_long: {
        CORBA::Long value;
        if (check_rc(data->get_int32_value(value, id), "get_int32_value failed")) {
            data_row->value = static_cast<int32_t>(value);
        }
        break;
    }
    case CORBA::tk_short: {
        CORBA::Short value;
        DDS::ReturnCode_t ret = data->get_int16_value(value, id);
        if (ret != DDS::RETCODE_OK) {
          ACE_INT8 tmp;
          ret = data->get_int8_value(tmp, id);
          value = static_cast<CORBA::Short>(tmp);
        }
        if (check_rc(ret, "get_int16_value failed")) {
            data_row->value = static_cast<int16_t>(value);
        }
        break;
    }
    case CORBA::tk_ushort: {
        CORBA::UShort value;
        DDS::ReturnCode_t ret = data->get_uint16_value(value, id);
        if (ret != DDS::RETCODE_OK) {
          ACE_UINT8 tmp;
          ret = data->get_uint8_value(tmp, id);
          value = static_cast<CORBA::UShort>(tmp);
        }
        if (check_rc(ret, "get_uint16_value failed")) {
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
        CORBA::Char value[2] = { 0, 0 };
        if (check_rc(data->get_char8_value(value[0], id), "get_char8_value failed")) {
            data_row->value = value;
        }
        break;
    }
    case CORBA::tk_wchar: {
        CORBA::WChar value[2] = { 0, 0 };
        if (check_rc(data->get_char16_value(value[0], id), "get_char16_value failed")) {
            data_row->value = ACE_Wide_To_Ascii(value).char_rep();
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
        CORBA::String_var value;
        if (check_rc(data->get_string_value(value, id), "get_string_value failed")) {
            data_row->value = value.in();
        }
        break;
    }
    case CORBA::tk_enum: {
        // When @bit_bound is supported, update this to call the right interface.
        CORBA::Long value;
        if (check_rc(data->get_int32_value(value, id), "get enum value failed")) {
            DDS::DynamicType_var type = data->type();
            const OpenDDS::XTypes::TypeKind tk = type->get_kind();
            DDS::DynamicType_var enum_dt;
            if (tk == OpenDDS::XTypes::TK_STRUCTURE || tk == OpenDDS::XTypes::TK_UNION) {
                DDS::DynamicTypeMember_var enum_dtm;
                if (type->get_member(enum_dtm, id) != DDS::RETCODE_OK) {
                    std::cerr << "get_member failed for enum member with Id "
                          << id << std::endl;
                    break;
                }
                DDS::MemberDescriptor_var enum_md;
                if (enum_dtm->get_descriptor(enum_md) != DDS::RETCODE_OK) {
                    std::cerr << "get_descriptor failed for enum member with Id "
                          << id << std::endl;
                    break;
                }
                enum_dt = DDS::DynamicType::_duplicate(enum_md->type());
            } else if (tk == OpenDDS::XTypes::TK_SEQUENCE || tk == OpenDDS::XTypes::TK_ARRAY) {
                DDS::TypeDescriptor_var td;
                if (type->get_descriptor(td) != DDS::RETCODE_OK) {
                    std::cerr << "get_descriptor failed" << std::endl;
                    break;
                }
                enum_dt = OpenDDS::XTypes::get_base_type(td->element_type());
            }

            DDS::DynamicTypeMember_var enum_lit_dtm;
            if (enum_dt->get_member(enum_lit_dtm, value) != DDS::RETCODE_OK) {
                std::cerr << "get_member failed for enum literal with value " << value << std::endl;
                break;
            }
            DDS::MemberDescriptor_var enum_lit_md;
            if (enum_lit_dtm->get_descriptor(enum_lit_md) != DDS::RETCODE_OK) {
                std::cerr << "get_descriptor failed for enum literal with value " << value << std::endl;
                break;
            }
            data_row->value = enum_lit_md->name();
        }
        break;
    }
    default:
        data_row->value = "NULL";
        break;
    }
}

//------------------------------------------------------------------------------
void TopicTableModel::parseCollection(const DDS::DynamicData_var& data, const std::string& namePrefix)
{
    DDS::DynamicType_var type = data->type();
    DDS::TypeDescriptor_var td;
    if (type->get_descriptor(td) != DDS::RETCODE_OK) {
        std::cerr << "Failed to get TypeDescriptor for a "
                  << OpenDDS::XTypes::typekind_to_string(type->get_kind())
                  << " member" << std::endl;
        return;
    }
    const unsigned int count = data->get_item_count();
    DDS::DynamicType_var elem_type = OpenDDS::XTypes::get_base_type(td->element_type());
    const OpenDDS::XTypes::TypeKind elem_tk = elem_type->get_kind();

    for (unsigned int i = 0; i < count; ++i) {
        DDS::MemberId id = data->get_member_id_at_index(i);
        if (id == OpenDDS::XTypes::MEMBER_ID_INVALID) {
            std::cerr << "Failed to get MemberId for element at index " << i << std::endl;
            continue;
        }

        switch (elem_tk) {
        case OpenDDS::XTypes::TK_SEQUENCE:
        case OpenDDS::XTypes::TK_ARRAY:
        case OpenDDS::XTypes::TK_STRUCTURE:
        case OpenDDS::XTypes::TK_UNION: {
            DDS::DynamicData_var nested_data;
            DDS::ReturnCode_t ret = data->get_complex_value(nested_data, id);
            if (ret != DDS::RETCODE_OK) {
                std::cerr << "get_complex_value for element at index " << i << " failed" << std::endl;
            } else {
                std::string scoped_elem_name = namePrefix + "[" + std::to_string(i) + "]";
                parseData(nested_data, scoped_elem_name);
            }
            continue;
        }
        }

        // DataRow for each element
        DataRow* data_row = new DataRow;
        data_row->type = typekind_to_tckind(elem_tk);
        data_row->isOptional = false; // TODO: Get the right value from the containing type
        std::string scoped_elem_name = namePrefix + "[" + std::to_string(i) + "]";
        data_row->name = scoped_elem_name.c_str();
        data_row->isKey = false; // TODO: Get the right value from the containing type

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
void TopicTableModel::parseAggregated(const DDS::DynamicData_var& data, const std::string& namePrefix)
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

        std::string scoped_member_name = namePrefix.empty() ? md->name() : namePrefix + "." + md->name();
        const DDS::DynamicType_var type = md->type();
        const DDS::DynamicType_var base_type = OpenDDS::XTypes::get_base_type(type.in());
        const OpenDDS::XTypes::TypeKind member_tk = base_type->get_kind();
        switch (member_tk) {
        case OpenDDS::XTypes::TK_SEQUENCE:
        case OpenDDS::XTypes::TK_ARRAY:
        case OpenDDS::XTypes::TK_STRUCTURE:
        case OpenDDS::XTypes::TK_UNION: {
            DDS::DynamicData_var nested_data;
            DDS::ReturnCode_t ret = data->get_complex_value(nested_data, id);
            if (ret != DDS::RETCODE_OK) {
                std::cerr << "get_complex_value for member Id " << id << " failed" << std::endl;
            } else {
                parseData(nested_data, scoped_member_name);
            }
            continue;
        }
        }

        // DataRow for each member
        DataRow* data_row = new DataRow;
        data_row->type = typekind_to_tckind(member_tk);
        data_row->isOptional = md->is_optional();
        data_row->name = scoped_member_name.c_str();
        data_row->isKey = md->is_key(); // TODO: Handle implicit key case

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
void TopicTableModel::parseData(const DDS::DynamicData_var& data, const std::string& namePrefix)
{
    const OpenDDS::XTypes::TypeKind tk = data->type()->get_kind();
    switch (tk) {
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
        if (dataInfo->value.canConvert(QMetaType::Char))
        {
            char tmpValue = dataInfo->value.toChar().toLatin1();
            memberData->setValue(tmpValue);
            pass = true;
        }
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
            }
            break;
        case CORBA::tk_char:
            if (newValue.canConvert(QMetaType::Char))
            {
                pass = true;
            }
            break;
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
