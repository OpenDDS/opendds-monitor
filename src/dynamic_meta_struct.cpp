#include "dynamic_meta_struct.h"
#include "open_dynamic_data.h"
#include <cctype>


//------------------------------------------------------------------------------
DynamicMetaStruct::DynamicMetaStruct(const std::shared_ptr<OpenDynamicData> oddInfo)
    : m_sample(oddInfo)
{}


//------------------------------------------------------------------------------
DynamicMetaStruct::~DynamicMetaStruct()
{
}


//------------------------------------------------------------------------------
OpenDDS::DCPS::Value DynamicMetaStruct::getValue(const void*, DDS::MemberId) const
{
    std::cout << "DynamicMetaStruct::getValue (by MemberId)" << std::endl;
    return 0;
}


//------------------------------------------------------------------------------
OpenDDS::DCPS::Value DynamicMetaStruct::getValue(const void*, const char*) const
{
    std::cout << "DynamicMetaStruct::getValue" << std::endl;
    return 0;
}


//------------------------------------------------------------------------------
OpenDDS::DCPS::Value DynamicMetaStruct::getValue(
    OpenDDS::DCPS::Serializer&, const char* fieldSpec, const OpenDDS::DCPS::TypeSupportImpl*) const
{
    if (!m_sample)
    {
        return 0;
    }

    const std::shared_ptr<OpenDynamicData> member = m_sample->getMember(fieldSpec);
    if (!member)
    {
        std::cerr << "Filter error: "
                  << "Unable to find member named '"
                  << fieldSpec
                  << "'" << std::endl;

        return 0;
    }

    OpenDDS::DCPS::Value newValue = 0;

    switch (member->getKind())
    {
        case CORBA::tk_longlong: newValue = member->getValue<CORBA::LongLong>(); break;
        case CORBA::tk_ulonglong: newValue = member->getValue<CORBA::ULongLong>(); break;
        case CORBA::tk_long: newValue = member->getValue<CORBA::Long>(); break;
        case CORBA::tk_ulong: newValue = member->getValue<CORBA::ULong>(); break;
        case CORBA::tk_boolean: newValue = member->getValue<CORBA::ULong>(); break;
        case CORBA::tk_short: newValue = member->getValue<CORBA::Short>(); break;
        case CORBA::tk_ushort: newValue = member->getValue<CORBA::UShort>(); break;
        case CORBA::tk_octet: newValue = member->getValue<CORBA::Octet>(); break;
        case CORBA::tk_char: newValue = member->getValue<CORBA::Char>(); break;
        case CORBA::tk_wchar: newValue = member->getValue<CORBA::WChar>(); break;
        case CORBA::tk_float: newValue = member->getValue<CORBA::Float>(); break;
        case CORBA::tk_double: newValue = member->getValue<CORBA::Double>(); break;
        case CORBA::tk_string: newValue = member->getStringValue(); break;

        // Use the string value for enums
        case CORBA::tk_enum:
        {
            // Make sure the int value is valid
            const uint32_t enumValue = member->getValue<uint32_t>();
            const CORBA::TypeCode* enumTypeCode = member->getTypeCode();
            if (enumValue >= enumTypeCode->member_count())
            {
                newValue = "***INVALID_ENUM_VALUE***\n";
                break;
            }

            // Get the string value of the enum
            newValue = enumTypeCode->member_name(enumValue);
            break;
        }

        default: break;

    } // End member type switch

    return newValue;

} // End DynamicMetaStruct::getValue


//------------------------------------------------------------------------------
OpenDDS::DCPS::ComparatorBase::Ptr DynamicMetaStruct::create_qc_comparator(
    const char*, OpenDDS::DCPS::ComparatorBase::Ptr) const
{
    std::cout << "DynamicMetaStruct::create_qc_comparator" << std::endl;
    OpenDDS::DCPS::ComparatorBase::Ptr unused;
    return unused;
}


//------------------------------------------------------------------------------
OpenDDS::DCPS::ComparatorBase::Ptr DynamicMetaStruct::create_qc_comparator(const char*) const
{
    std::cout << "DynamicMetaStruct::create_qc_comparator" << std::endl;
    OpenDDS::DCPS::ComparatorBase::Ptr unused;
    return unused;
}


//------------------------------------------------------------------------------
bool DynamicMetaStruct::compare(const void*, const void*, const char*) const
{
    std::cout << "DynamicMetaStruct::compare" << std::endl;
    return false;
}


//------------------------------------------------------------------------------
bool DynamicMetaStruct::isDcpsKey(const char* /*field*/) const
{
    std::cout << "DynamicMetaStruct::isDcpsKey" << std::endl;
    return false;
}


//------------------------------------------------------------------------------
size_t DynamicMetaStruct::numDcpsKeys() const
{
    std::cout << "DynamicMetaStruct::numDcpsKeys" << std::endl;
    return 0;
}


//------------------------------------------------------------------------------
const char** DynamicMetaStruct::getFieldNames() const
{
    std::cout << "DynamicMetaStruct::getFieldNames" << std::endl;
    return 0;
}


//------------------------------------------------------------------------------
void DynamicMetaStruct::assign(void*, const char*, const void*, const char*, const MetaStruct&) const
{
    std::cout << "DynamicMetaStruct::assign" << std::endl;
}


//------------------------------------------------------------------------------
const void* DynamicMetaStruct::getRawField(const void*, const char*) const
{
    std::cout << "DynamicMetaStruct::getRawField" << std::endl;
    return 0;
}


//------------------------------------------------------------------------------
void* DynamicMetaStruct::allocate() const
{
    std::cout << "DynamicMetaStruct::allocate" << std::endl;
    return 0;
}


//------------------------------------------------------------------------------
void DynamicMetaStruct::deallocate(void*) const
{
    std::cout << "DynamicMetaStruct::deallocate" << std::endl;
}


/**
 * @}
 */
