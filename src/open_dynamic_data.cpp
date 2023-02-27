#include <tao/AnyTypeCode/Enum_TypeCode.h>
#include <iostream>
#include <sstream>

#include "open_dynamic_data.h"

std::shared_ptr<OpenDynamicData> CreateOpenDynamicData(const CORBA::TypeCode* typeCode,
    const OpenDDS::DCPS::Encoding::Kind encodingKind, 
    const OpenDDS::DCPS::Extensibility extensibility,
    const std::weak_ptr<OpenDynamicData> parent)
{
    std::shared_ptr<OpenDynamicData> temp = std::make_shared<OpenDynamicData>(typeCode, encodingKind, extensibility, parent);
    temp->populate(); //Can't call this in the constructor because shared_from_this isn't ready yet!
    return temp;
}

//------------------------------------------------------------------------------
OpenDynamicData::OpenDynamicData(const CORBA::TypeCode* typeCode,
                                 const OpenDDS::DCPS::Encoding::Kind encodingKind, 
                                 const OpenDDS::DCPS::Extensibility extensibility,
                                 const std::weak_ptr<OpenDynamicData> parent) :
                                 m_parent(parent),
                                 m_name("EMPTY_NAME"),
                                 m_typeCode(typeCode), 
                                 m_encodingKind(encodingKind),
                                 m_extensibility(extensibility),
                                 m_containsComplexTypes(false)
{
    if (!m_typeCode)
    {
        std::cerr << "Bad typecode received in OpenDynamicData()" << std::endl;
        return;
    }

    memset(&m_value, 0, sizeof(m_value));
    //Can't call this in the constructor because shared_from_this isn't ready yet!
    //populate();
}


//------------------------------------------------------------------------------
OpenDynamicData::~OpenDynamicData()
{

}


//------------------------------------------------------------------------------
OpenDynamicData& OpenDynamicData::operator=(const OpenDynamicData& other)
{
    //std::cout << "DEBUG: operator=" << std::endl;
    // Make sure the type codes match
    if (this->getTypeCode() != other.getTypeCode())
    {
        std::cerr << "OpenDynamicData::operator=: "
                  << "Type codes do not match"
                  << std::endl;
        return *this;
    }

    const size_t otherCount = other.getLength();
    const size_t childCount = m_children.size();

    if (childCount != otherCount)
    {
        std::cerr << "OpenDynamicData::operator=: "
                  << "Children sizes do not match"
                  << std::endl;
        return *this;
    }

    for (size_t i = 0; i < childCount; i++)
    {
        const std::shared_ptr<OpenDynamicData> otherChild = other.getMember(i);
        std::shared_ptr<OpenDynamicData> thisChild = m_children[i];

        const CORBA::TCKind kind = thisChild->getKind();
        switch (kind)
        {
        case CORBA::tk_long:
            thisChild->setValue(otherChild->m_value.int32);
            break;
        case CORBA::tk_short:
            thisChild->setValue(otherChild->m_value.int16);
            break;
        case CORBA::tk_ushort:
            thisChild->setValue(otherChild->m_value.uint16);
            break;
        case CORBA::tk_enum:
        case CORBA::tk_ulong:
            thisChild->setValue(otherChild->m_value.uint32);
            break;
        case CORBA::tk_float:
            thisChild->setValue(otherChild->m_value.float32);
            break;
        case CORBA::tk_double:
            thisChild->setValue(otherChild->m_value.float64);
            break;
        case CORBA::tk_boolean:
            thisChild->setValue(otherChild->m_value.boolean != 0);
            break;
        case CORBA::tk_char:
            thisChild->setValue(otherChild->m_value.char8);
            break;
        case CORBA::tk_wchar:
            thisChild->setValue(otherChild->m_value.char16);
            break;
        case CORBA::tk_octet:
            thisChild->setValue(otherChild->m_value.uint8);
            break;
        case CORBA::tk_longlong:
            thisChild->setValue(otherChild->m_value.int64);
            break;
        case CORBA::tk_ulonglong:
            thisChild->setValue(otherChild->m_value.uint64);
            break;
        case CORBA::tk_string:
            thisChild->setStringValue(otherChild->getStringValue());
            break;
        case CORBA::tk_sequence:
            thisChild->setLength(otherChild->getLength());
            *thisChild = *otherChild;
            break;
        case CORBA::tk_struct:
            *thisChild = *otherChild;
            break;
        case CORBA::tk_array:
            *thisChild = *otherChild;
            break;
        case CORBA::tk_union: // TODO?
        case CORBA::tk_wstring: // TODO?
        default:
            std::cerr << "OpenDynamicData::operator=: "
                      << "Unsupported type (" << kind << ")"
                      << std::endl;
            break;

        } // End child type switch
    }

    return *this;

} // End OpenDynamicData::operator=


//------------------------------------------------------------------------------
bool OpenDynamicData::operator==(const OpenDynamicData& other)
{
    // Make sure the type codes match
    if (this->getTypeCode() != other.getTypeCode())
    {
        //std::cerr << "OpenDynamicData::operator==: "
        //    << "Type codes do not match"
        //    << std::endl;
        return false;
    }

    const size_t otherCount = other.getLength();
    const size_t childCount = m_children.size();

    if (childCount != otherCount)
    {
        //std::cerr << "OpenDynamicData::operator==: "
        //    << "Children sizes do not match"
        //    << std::endl;
        return false;
    }

    for (size_t i = 0; i < childCount; i++)
    {
        const std::shared_ptr<OpenDynamicData> otherChild = other.getMember(i);
        std::shared_ptr<OpenDynamicData> thisChild = m_children[i];

        const CORBA::TCKind kind = thisChild->getKind();
        switch (kind)
        {
        case CORBA::tk_long:
            if (thisChild->getValue<int32_t>() != otherChild->getValue<int32_t>())
            {
                return false;
            }
            break;
        case CORBA::tk_short:
            if (thisChild->getValue<int16_t>() != otherChild->getValue<int16_t>())
            {
                return false;
            }
            break;
        case CORBA::tk_ushort:
            if (thisChild->getValue<uint16_t>() != otherChild->getValue<uint16_t>())
            {
                return false;
            }
            break;
        case CORBA::tk_enum:
        case CORBA::tk_ulong:
            if (thisChild->getValue<uint32_t>() != otherChild->getValue<uint32_t>())
            {
                return false;
            }
            break;
        case CORBA::tk_float:
            if (thisChild->getValue<float>() != otherChild->getValue<float>())
            {
                return false;
            }
            break;
        case CORBA::tk_double:
            if (thisChild->getValue<double>() != otherChild->getValue<double>())
            {
                return false;
            }
            break;
        case CORBA::tk_boolean:
            if (thisChild->getValue<bool>() != otherChild->getValue<bool>())
            {
                return false;
            }
            break;
        case CORBA::tk_char:
            if (thisChild->getValue<char>() != otherChild->getValue<char>())
            {
                return false;
            }
            break;
        case CORBA::tk_wchar:
            if (thisChild->getValue<CORBA::WChar>() != otherChild->getValue<CORBA::WChar>())
            {
                return false;
            }
            break;
        case CORBA::tk_octet:
            if (thisChild->getValue<uint8_t>() != otherChild->getValue<uint8_t>())
            {
                return false;
            }
            break;
        case CORBA::tk_longlong:
            if (thisChild->getValue<int64_t>() != otherChild->getValue<int64_t>())
            {
                return false;
            }
            break;
        case CORBA::tk_ulonglong:
            if (thisChild->getValue<uint64_t>() != otherChild->getValue<uint64_t>())
            {
                return false;
            }
            break;
        case CORBA::tk_string:
            if (0 != strcmp(thisChild->getStringValue(), otherChild->getStringValue()))
            {
                return false;
            }
            break;
        case CORBA::tk_sequence:
            if (thisChild->getLength() != otherChild->getLength())
            {
                return false;
            }
            //Recurse
            if (*thisChild != *otherChild)
            {
                return false;
            }
            break;
        case CORBA::tk_struct:
            //Recurse
            if (*thisChild != *otherChild)
            {
                return false;
            }
            break;
        case CORBA::tk_array:
            //Recurse
            if (*thisChild != *otherChild)
            {
                return false;
            }
            break;
        case CORBA::tk_wstring: // TODO - Add implementation - Robert Jefferies, 2018-08-02.
        default:
            //std::cerr << "OpenDynamicData::operator==: "
            //    << "Unsupported type (" << kind << ")"
            //    << std::endl;
            return false;

        } // End child type switch
    }

    //If we got here, everything should be equal.
    return true;

} // End OpenDynamicData::operator==


//------------------------------------------------------------------------------
void OpenDynamicData::dump() const
{
    for (const auto& child : m_children)
    {
        if (child->getKind() == CORBA::tk_sequence ||
            child->getKind() == CORBA::tk_array ||
            child->getKind() == CORBA::tk_struct)
        {
            //child->dump();
        }
        else // Simple type
        {
            const std::string fullName = child->getFullName();
            const CORBA::TCKind childKind = child->getKind();
            std::stringstream typeStream;
            typeStream << childKind;
            printf("%s (%s) = ", fullName.c_str(), typeStream.str().c_str());

            switch (childKind)
            {
            case CORBA::tk_long:
                printf("%d\n", child->getValue<int32_t>());
                break;
            case CORBA::tk_short:
                printf("%d\n", child->getValue<int16_t>());
                break;
            case CORBA::tk_ushort:
                printf("%u\n", child->getValue<uint16_t>());
                break;
            case CORBA::tk_ulong:
                printf("%u\n", child->getValue<uint32_t>());
                break;
            case CORBA::tk_float:
                printf("%f\n", child->getValue<float>());
                break;
            case CORBA::tk_double:
                printf("%f\n", child->getValue<double>());
                break;
            case CORBA::tk_boolean:
                printf("%d\n", child->getValue<bool>());
                break;
            case CORBA::tk_char:
                printf("%c\n", child->getValue<char>());
                break;
            case CORBA::tk_wchar:
                printf("%c\n", child->getValue<CORBA::WChar>());
                break;
            case CORBA::tk_octet:
                printf("%o\n", static_cast<uint32_t>(child->getValue<uint8_t>()));
                break;
            case CORBA::tk_longlong:
                printf(ACE_INT64_FORMAT_SPECIFIER_ASCII "\n", child->getValue<int64_t>());
                break;
            case CORBA::tk_ulonglong:
                printf(ACE_UINT64_FORMAT_SPECIFIER_ASCII "\n", child->getValue<uint64_t>());
                break;
            case CORBA::tk_string:
            {
                const char* stringValue = child->getStringValue();
                printf("'%s'\n", stringValue);
                break;
            }
            case CORBA::tk_enum:
            {
                // Make sure the int value is valid
                const uint32_t enumValue = child->getValue<uint32_t>();
                const CORBA::TypeCode* enumTypeCode = child->getTypeCode();
                if (enumValue >= enumTypeCode->member_count())
                {
                    printf("***INVALID_ENUM_VALUE***\n");
                    break;
                }

                // Get the string value of the enum
                printf("%s\n", enumTypeCode->member_name(enumValue));
                break;
            }
            default:
                printf("?\n");
                break;

            } // End child type switch

        } // End simple type block

    } // End Child loop

} // End OpenDynamicData::printStruct


//------------------------------------------------------------------------------
bool OpenDynamicData::operator>>(OpenDDS::DCPS::Serializer& stream) const
{
    //std::cout << "DEBUG OpenDynamicData::operator>>" << endl;
    bool pass = true;
    for (const std::shared_ptr<OpenDynamicData>& child : m_children)
    {
        switch (child->getKind())
        {
        case CORBA::tk_long:
            //std::cout << "DEBUG OpenDynamicData::operator>> kind is long, " << child->getEncapsulationLength() << " bytes" << std::endl;
            pass &= (stream << child->getValue<CORBA::Long>());
            break;
        case CORBA::tk_short:
            //std::cout << "DEBUG OpenDynamicData::operator>> kind is short, " << child->getEncapsulationLength() << " bytes" << std::endl;
            pass &= (stream << child->getValue<CORBA::Short>());
            break;
        case CORBA::tk_ushort:
            //std::cout << "DEBUG OpenDynamicData::operator>> kind is ushort, " << child->getEncapsulationLength() << " bytes" << std::endl;
            pass &= (stream << child->getValue<CORBA::UShort>());
            break;
        case CORBA::tk_enum:
        case CORBA::tk_ulong:
            //std::cout << "DEBUG OpenDynamicData::operator>> kind is enum or ulong, " << child->getEncapsulationLength() << " bytes" << std::endl;
            pass &= (stream << child->getValue<CORBA::ULong>());
            break;
        case CORBA::tk_float:
            //std::cout << "DEBUG OpenDynamicData::operator>> kind is float, " << child->getEncapsulationLength() << " bytes" << std::endl;
            pass &= (stream << child->getValue<CORBA::Float>());
            break;
        case CORBA::tk_double:
            //std::cout << "DEBUG OpenDynamicData::operator>> kind is double, " << child->getEncapsulationLength() << " bytes" << std::endl;
            pass &= (stream << child->getValue<CORBA::Double>());
            break;
        case CORBA::tk_char:
            //std::cout << "DEBUG OpenDynamicData::operator>> kind is char, " << child->getEncapsulationLength() << " bytes" << std::endl;
            pass &= (stream << ACE_OutputCDR::from_char(child->getValue<CORBA::Char>()));
            break;
        case CORBA::tk_wchar:
            //std::cout << "DEBUG OpenDynamicData::operator>> kind is wchar, " << child->getEncapsulationLength() << " bytes" << std::endl;
            pass &= (stream << ACE_OutputCDR::from_wchar(child->getValue<CORBA::WChar>()));
            break;
        case CORBA::tk_octet:
            //std::cout << "DEBUG OpenDynamicData::operator>> kind is octet, " << child->getEncapsulationLength() << " bytes" << std::endl;
            pass &= (stream << ACE_OutputCDR::from_octet(child->getValue<CORBA::Octet>()));
            break;
        case CORBA::tk_longlong:
            //std::cout << "DEBUG OpenDynamicData::operator>> kind is longlong, " << child->getEncapsulationLength() << " bytes" << std::endl;
            pass &= (stream << child->getValue<CORBA::LongLong>());
            break;
        case CORBA::tk_ulonglong:
            //std::cout << "DEBUG OpenDynamicData::operator>> kind is ulonglong, " << child->getEncapsulationLength() << " bytes" << std::endl;
            pass &= (stream << child->getValue<CORBA::ULongLong>());
            break;
        case CORBA::tk_boolean:
            //std::cout << "DEBUG OpenDynamicData::operator>> kind is bool, " << child->getEncapsulationLength() << " bytes" << std::endl;
            pass &= (stream << ACE_OutputCDR::from_boolean(child->getValue<CORBA::Boolean>()));
            break;
        case CORBA::tk_string:
        {
            //std::cout << "DEBUG OpenDynamicData::operator>> kind is string, " << child->getEncapsulationLength() << " bytes, pass is" << (pass ? " true" : " false") << std::endl;
            const char* value = child->getStringValue();
            TAO::String_Manager stringMan = CORBA::string_dup(value);
            pass &= (stream << stringMan.in());
            //std::cout << "DEBUG OpenDynamicData::operator>> kind is string, value is " << value << " pass is " << (pass ? " true" : " false") << std::endl;
            break;
        }
        case CORBA::tk_sequence:
        {
            //std::cout << "DEBUG OpenDynamicData::operator>> kind is sequence, " << child->getEncapsulationLength() << " bytes" << std::endl;
            //XCDR2 adds a delimiter header before every sequence of complex types. Try reading it from the typecode.
            if((m_encodingKind != OpenDDS::DCPS::Encoding::KIND_XCDR1) && child->containsComplexTypes())
            {
                CORBA::ULong delimiterHeader = static_cast<CORBA::ULong>(child->getEncapsulationLength());
                //std::cout << "DEBUG Creating sequence delim hdr: " << delimiterHeader << std::endl;
                pass &= (stream << delimiterHeader);
            }
            CORBA::ULong seqLength = static_cast<CORBA::ULong>(child->getLength());
            //std::cout << "DEBUG sequence length hdr: " <<  child->getLength() << std::endl;
            pass &= (stream << seqLength);
            if(seqLength > 0)
            {
                pass &= ((*child) >> stream);
            }
            break;
        }
        case CORBA::tk_struct:
            //std::cout << "DEBUG OpenDynamicData::operator>> kind is struct, " << child->getEncapsulationLength() << " bytes" << std::endl;
            //XCDR2 adds a delimiter header before every struct. Try reading it from the typecode.
            if(m_encodingKind != OpenDDS::DCPS::Encoding::KIND_XCDR1)
            {
                CORBA::ULong delimiterHeader = static_cast<CORBA::ULong>(child->getEncapsulationLength());
                //std::cout << "DEBUG Creating struct delim hdr: " << delimiterHeader << std::endl;
                pass &= (stream << delimiterHeader);
            }
            pass &= ((*child) >> stream);
            break;
        case CORBA::tk_array:
            //std::cout << "DEBUG OpenDynamicData::operator>> kind is array, " << child->getEncapsulationLength() << " bytes" << std::endl;
            //XCDR2 adds a delimiter header before every array of complex types. Try reading it from the typecode.
            if((m_encodingKind != OpenDDS::DCPS::Encoding::KIND_XCDR1) && child->containsComplexTypes())
            {
                CORBA::ULong delimiterHeader = static_cast<CORBA::ULong>(child->getEncapsulationLength());
                //std::cout << "DEBUG Creating array delim hdr: " << delimiterHeader << std::endl;
                pass &= (stream << delimiterHeader);
            }
            pass &= ((*child) >> stream);
            break;
        case CORBA::tk_wstring: // TODO?
        case CORBA::tk_union: // TODO?
        default:
            std::cerr << "OpenDynamicData::operator>>: "
                      << "Unsupported type (" << child->getKind() << ")"
                      << std::endl;
            pass = false;
            break;
        }

        if (!pass)
        {
            std::cerr << "Failed to serialize '"
                << child->getName()
                << "'"
                << std::endl;
        }

    } // End child loop

    return pass;

} // End OpenDynamicData::operator>>


//------------------------------------------------------------------------------
bool OpenDynamicData::operator<<(OpenDDS::DCPS::Serializer& stream)
{
    bool pass = true;
    SimpleTypeUnion tmpValue;
    memset(&tmpValue, 0, sizeof(tmpValue));

    for (std::shared_ptr<OpenDynamicData> child : m_children)
    {
        // Protection for inconsistent topics or junk data
        if (!stream.good_bit())
        {
            break;
        }

        switch (child->getKind())
        {
        case CORBA::tk_long:
            //std::cout << "DEBUG OpenDynamicData::operator<< kind is long " << std::endl;
            pass &= (stream >> tmpValue.int32);
            child->setValue(tmpValue.int32);
            break;
        case CORBA::tk_short:
            //std::cout << "DEBUG OpenDynamicData::operator<< kind is short " << std::endl;
            pass &= (stream >> tmpValue.int16);
            child->setValue(tmpValue.int16);
            break;
        case CORBA::tk_ushort:
            //std::cout << "DEBUG OpenDynamicData::operator<< kind is ushort " << std::endl;
            pass &= (stream >> tmpValue.uint16);
            child->setValue(tmpValue.uint16);
            break;
        case CORBA::tk_enum:
            //std::cout << "DEBUG OpenDynamicData::operator<< kind is enum " << std::endl;
            pass &= (stream >> tmpValue.uint32);
            child->setValue(tmpValue.uint32);
            break;
        case CORBA::tk_ulong:
            //std::cout << "DEBUG OpenDynamicData::operator<< kind is enum or ulong" << std::endl;
            pass &= (stream >> tmpValue.uint32);
            child->setValue(tmpValue.uint32);
            break;
        case CORBA::tk_float:
            //std::cout << "DEBUG OpenDynamicData::operator<< kind is float " << std::endl;
            pass &= (stream >> tmpValue.float32);
            child->setValue(tmpValue.float32);
            break;
        case CORBA::tk_double:
            //std::cout << "DEBUG OpenDynamicData::operator<< kind is dbl " << std::endl;
            pass &= (stream >> tmpValue.float64);
            child->setValue(tmpValue.float64);
            break;
        case CORBA::tk_char:
            //std::cout << "DEBUG OpenDynamicData::operator<< kind is chat " << std::endl;
            pass &= (stream >> ACE_InputCDR::to_char(tmpValue.char8));
            child->setValue(tmpValue.char8);
            break;
        case CORBA::tk_wchar:
            //std::cout << "DEBUG OpenDynamicData::operator<< kind is wchar " << std::endl;
            pass &= (stream >> ACE_InputCDR::to_wchar(tmpValue.char16));
            child->setValue(tmpValue.char16);
            break;
        case CORBA::tk_octet:
            //std::cout << "DEBUG OpenDynamicData::operator<< kind is octet " << std::endl;
            pass &= (stream >> ACE_InputCDR::to_octet(tmpValue.uint8));
            child->setValue(tmpValue.uint8);
            break;
        case CORBA::tk_longlong:
            //std::cout << "DEBUG OpenDynamicData::operator<< kind is ll " << std::endl;
            pass &= (stream >> tmpValue.int64);
            child->setValue(tmpValue.int64);
            break;
        case CORBA::tk_ulonglong:
            //std::cout << "DEBUG OpenDynamicData::operator<< kind is ull " << std::endl;
            pass &= (stream >> tmpValue.uint64);
            child->setValue(tmpValue.uint64);
            break;
        case CORBA::tk_boolean:
            //std::cout << "DEBUG OpenDynamicData::operator<< kind is bool " << std::endl;
            pass &= (stream >> ACE_InputCDR::to_boolean(tmpValue.boolean));
            // Boolean values are often int on the wire (Example: true == 42)
            // Force them to [0|1]
            tmpValue.boolean = (tmpValue.boolean != 0);
            child->setValue(tmpValue.boolean);
            break;
        case CORBA::tk_string:
        { //create scope for tao string manager
            //std::cout << "DEBUG OpenDynamicData::operator<< kind is str " << std::endl;        
            TAO::String_Manager stringMan;
            pass &= (stream >> stringMan.out());
            const char* value = stringMan;
            child->setStringValue(value);
            break;
        }
        //XCDR2 adds a delimiter header before every struct, and every sequence, and array of complex types and/or appendable extensibility
        case CORBA::tk_array:
            //std::cout << "DEBUG OpenDynamicData::operator<< kind is array " << std::endl;
            if((m_encodingKind != OpenDDS::DCPS::Encoding::KIND_XCDR1) && child->containsComplexTypes())
            {
                uint32_t delim_header=0;
                if (! (stream >> delim_header)) {
                    std::cerr << "OpenDynamicData::operator<<: "
                              << "Could not read stream delimiter"
                              << std::endl;
                    pass = false;
                    break;
                }
                //std::cout << "DEBUG eating array stream delim. Size returned: " << delim_header <<std::endl;    
            }
            pass &= ((*child) << stream);
            break;
        case CORBA::tk_sequence:
            //std::cout << "DEBUG OpenDynamicData::operator<< kind is seq " << std::endl;       
            if((m_encodingKind != OpenDDS::DCPS::Encoding::KIND_XCDR1) && child->containsComplexTypes())
            {
                uint32_t delim_header=0;
                if (! (stream >> delim_header)) {
                    std::cerr << "OpenDynamicData::operator<<: "
                              << "Could not read stream delimiter"
                              << std::endl;
                    pass = false;
                    break;
                }
                //std::cout << "DEBUG eating sequence stream delim. Size returned: " << delim_header <<std::endl;    
            } 
            pass &= (stream >> tmpValue.uint32);
            //std::cout << "DEBUG sequence length: " << tmpValue.uint32 << std::endl;
            child->setLength(tmpValue.uint32);   
            pass &= ((*child) << stream);
            break;
        case CORBA::tk_struct:
            //structs always have a delimiter if xcdr2
            if(m_encodingKind != OpenDDS::DCPS::Encoding::KIND_XCDR1)
            {
                uint32_t delim_header=0;
                if (! (stream >> delim_header)) {
                    std::cerr << "OpenDynamicData::operator<<: "
                              << "Could not read stream delimiter"
                              << std::endl;
                    pass = false;
                    break;
                }
                //std::cout << "DEBUG eating sequence stream delim. Size returned: " << delim_header <<std::endl;    
            }
            //std::cout << "DEBUG OpenDynamicData::operator<< kind is struct " << std::endl;
            pass &= ((*child) << stream);
            break;
        case CORBA::tk_wstring: // TODO?
        case CORBA::tk_union: // TODO?
        default:
            std::cerr << "OpenDynamicData::operator<<: "
                      << "Unsupported type (" << child->getKind() << ")"
                      << std::endl;
            pass = false;
            break;
        }

        if (!pass)
        {
            std::cerr << "Failed to deserialize '"
                      << child->getName()
                      << "'"
                      << std::endl;
        }

    } // End child loop

    return pass;

} // End OpenDynamicData::operator<<


//------------------------------------------------------------------------------
std::string OpenDynamicData::getName() const
{
    return m_name;
}


//------------------------------------------------------------------------------
CORBA::TCKind OpenDynamicData::getKind() const
{
    if (!m_typeCode)
    {
        return CORBA::tk_null;
    }

    return m_typeCode->kind();
}

//------------------------------------------------------------------------------
bool OpenDynamicData::isPrimitive() const
{
    return isPrimitiveImpl(m_typeCode->kind());
}

//------------------------------------------------------------------------------
bool OpenDynamicData::isContainerType() const
{
    return isContainerType(m_typeCode->kind());
}

//------------------------------------------------------------------------------
size_t OpenDynamicData::getEncapsulationLength()
{
    switch (m_typeCode->kind())
    {
        case CORBA::tk_long:
            return sizeof(CORBA::Long);
        case CORBA::tk_short:
            return sizeof(CORBA::Short);
        case CORBA::tk_ushort:
            return sizeof(CORBA::UShort);
        case CORBA::tk_enum:
            return sizeof(CORBA::ULong);
        case CORBA::tk_ulong:
            return sizeof(CORBA::ULong);
        case CORBA::tk_float:
            return sizeof(CORBA::Float);
        case CORBA::tk_double:
            return sizeof(CORBA::Double);
        case CORBA::tk_char:
            return sizeof(CORBA::Char);
        case CORBA::tk_wchar:
            return sizeof(CORBA::WChar);
        case CORBA::tk_octet:
            return sizeof(CORBA::Octet);
        case CORBA::tk_longlong:
            return sizeof(CORBA::LongLong);
        case CORBA::tk_ulonglong:
            return sizeof(CORBA::ULongLong);
        case CORBA::tk_boolean:
            return sizeof(CORBA::Boolean);
        case CORBA::tk_string:
            {
                std::string temp(m_stringValue);
                //Per xtypes spec, section 7.4.3.5.3 rules 3, this includes NUL, and 4 bytes for string length
                return temp.length() + 1 + sizeof(CORBA::ULong) ; 
            }
        case CORBA::tk_sequence:
            {
                //per xtypes spec, section 7.4.3.5.3 rule 12, delimiter header includes the length of the sequence of complex types.
                //so if the length is 0, delim header should still be 4..
                //all sequences first serialize the length
                size_t ret = sizeof(CORBA::ULong);
                
                if(getLength() > 0)
                {
                    ret +=  m_children[0]->getEncapsulationLength() * getLength();
                }
                if((m_encodingKind != OpenDDS::DCPS::Encoding::KIND_XCDR1) && containsComplexTypes())
                {
                    ret += sizeof(CORBA::ULong);
                }      
                return ret;
            }
        case CORBA::tk_struct:
            {
                size_t sum=0;
                for(const auto& child : m_children)
                {
                    sum += child->getEncapsulationLength();
                }
                //if xcdr2 and appendable or mutable, add delimiter header, per rule (30)
                if((m_encodingKind != OpenDDS::DCPS::Encoding::KIND_XCDR1) && (m_extensibility != OpenDDS::DCPS::Extensibility::FINAL))
                {
                    sum += sizeof(CORBA::ULong);
                }
                return sum;
            }
        case CORBA::tk_array:
            {
                size_t sum = 0;
                if(getLength() > 0)
                {
                    sum += m_children[0]->getEncapsulationLength() * getLength();
                }
                if((m_encodingKind != OpenDDS::DCPS::Encoding::KIND_XCDR1) && containsComplexTypes())
                {
                    sum += sizeof(CORBA::ULong);
                }
                return sum;
            }
        case CORBA::tk_union: // TODO?
        case CORBA::tk_wstring:  // TODO?
        default:
            return 0;
    }
}

//------------------------------------------------------------------------------
bool OpenDynamicData::containsComplexTypes() const
{
    return m_containsComplexTypes;
}

//------------------------------------------------------------------------------
const CORBA::TypeCode* OpenDynamicData::getTypeCode() const
{
    return m_typeCode;
}


//------------------------------------------------------------------------------
size_t OpenDynamicData::getLength() const
{
    return m_children.size();
}


//------------------------------------------------------------------------------
void OpenDynamicData::setLength(const size_t& length)
{
    //while (!m_children.empty())
    //{
    //    delete m_children.back();
    //    m_children.back() = nullptr;
    //    m_children.pop_back();
    //}
    m_children.clear();

    m_children.resize(length);
    for (size_t i = 0; i < length; i++)
    {
        std::string elementName;
        elementName += "[";
        elementName += std::to_string(i);
        elementName += "]";

        // Get the true type for this array/sequence
        CORBA::TypeCode_ptr contentType = m_typeCode->content_type();

        // Follow the alias trail until we have the true type
        while (contentType->kind() == CORBA::tk_alias)
        {
            contentType = TAO::unaliased_typecode(contentType);
        }
        //RJ 2022-01-21 I think older verisons of ddsman will default to topics being appendable, but structs within them being final. I think.
        std::shared_ptr<OpenDynamicData> elementMember = CreateOpenDynamicData(contentType, m_encodingKind, OpenDDS::DCPS::Extensibility::FINAL, weak_from_this());
        elementMember->setName(elementName);
        m_children[i] = elementMember;
    }

} // End OpenDynamicData::setLength

//------------------------------------------------------------------------------
std::string OpenDynamicData::getFullName() const
{
    // Are we the root struct?
    if (auto parentPtr = m_parent.lock())
    {
            if (getKind() == CORBA::tk_struct)
            {
                return (parentPtr->getFullName() + m_name + ".");
            }

            return (parentPtr->getFullName() + m_name);
    }
    else
    {
        return "";
    }
}


//------------------------------------------------------------------------------
std::shared_ptr<OpenDynamicData> OpenDynamicData::getMember(const std::string& fullName) const
{
    // If the name is empty, we can't do much.
    if (fullName.empty())
    {
        return nullptr;
    }

    // First, check for simple matches
    for (auto child : m_children)
    {
       if (child->getName() == fullName)
       {
           return child;
       }
    }

    // If we got here, search the children for the array or nested struct member
    const size_t childNameSpot = fullName.find_first_of("[.", 1);
    if (childNameSpot != std::string::npos)
    {
        std::string parentName = fullName.substr(0, childNameSpot);
        std::string childName = fullName.substr(childNameSpot);

        // Eat '.' from struct members
        if (childName.size() > 1 && childName[0] == '.')
        {
            childName = childName.substr(1);
        }

        auto parent = getMember(parentName);
        if (!parent)
        {
            return parent; // nullptr
        }

        // Search into the complex type
        return parent->getMember(childName);
    }

    // Not found
    return nullptr;

} // End OpenDynamicData::getMember


//------------------------------------------------------------------------------
std::shared_ptr<OpenDynamicData> OpenDynamicData::getMember(const size_t& index) const
{
    if (index >= m_children.size())
    {
        std::cerr << "OpenDynamicData::getMember: "
                  << "Index out of range  "
                  << m_name
                  << std::endl;

        return nullptr;
    }

    return m_children[index];
}


//------------------------------------------------------------------------------
const char* OpenDynamicData::getStringValue() const
{
    if (getKind() != CORBA::tk_string)
    {
        std::cerr << "Warning: Accessed a string value to a non-string member: "
            << getFullName()
            << std::endl;

    }
    const char* charValue = m_stringValue;
    return charValue;
}


//------------------------------------------------------------------------------
void OpenDynamicData::setStringValue(const char* value)
{
    if (getKind() != CORBA::tk_string)
    {
        std::cerr << "Warning: Set a string value to a non-string member: "
                  << getFullName()
                  << std::endl;
    }
    m_stringValue = CORBA::string_dup(value);
}


//------------------------------------------------------------------------------
void OpenDynamicData::setName(const std::string& name)
{
    m_name = name;
}

//------------------------------------------------------------------------------
bool OpenDynamicData::isPrimitiveImpl(const CORBA::TCKind tck) const
{
    switch (tck)
    {
        case CORBA::tk_long:
        case CORBA::tk_short:
        case CORBA::tk_ushort:
        case CORBA::tk_enum:
        case CORBA::tk_ulong:
        case CORBA::tk_float:
        case CORBA::tk_double:
        case CORBA::tk_char:
        case CORBA::tk_wchar:
        case CORBA::tk_octet:
        case CORBA::tk_longlong:
        case CORBA::tk_ulonglong:
        case CORBA::tk_boolean:
            return true;
        case CORBA::tk_union:
        case CORBA::tk_string:
        case CORBA::tk_wstring:
        case CORBA::tk_sequence:
        case CORBA::tk_struct:
        case CORBA::tk_array:
        default:
            return false;
    }
}

//------------------------------------------------------------------------------
bool OpenDynamicData::isContainerType(const CORBA::TCKind tck) const
{
    switch (tck)
    {
    case CORBA::tk_union:
    case CORBA::tk_sequence:
    case CORBA::tk_struct:
    case CORBA::tk_array:
        return true;
    case CORBA::tk_long:
    case CORBA::tk_short:
    case CORBA::tk_ushort:
    case CORBA::tk_enum:
    case CORBA::tk_ulong:
    case CORBA::tk_float:
    case CORBA::tk_double:
    case CORBA::tk_char:
    case CORBA::tk_wchar:
    case CORBA::tk_octet:
    case CORBA::tk_longlong:
    case CORBA::tk_ulonglong:
    case CORBA::tk_boolean:
    case CORBA::tk_string:
    case CORBA::tk_wstring:
    default:
        return false;
    }
}

//------------------------------------------------------------------------------
void OpenDynamicData::populate()
{
    if (!m_typeCode)
    {
        std::cerr << "OpenDynamicData::populate(): "
                  << "Invalid typecode"
                  << std::endl;
        return;
    }

    const CORBA::TCKind kind = m_typeCode->kind();

  
    if((kind == CORBA::tk_array) || (kind == CORBA::tk_sequence))
    {
        // If XCDR2 we need to know when deserializing a sequence or array if 
        // this is a sequence/array of primitive or complex types, so 
        // we can conditionally deserialize the delimiter header.
        
        // Get the true type for this array/sequence
        CORBA::TypeCode_ptr contentType = m_typeCode->content_type();

        // Follow the alias trail until we have the true type
        while (contentType->kind() == CORBA::tk_alias)
        {
            contentType = TAO::unaliased_typecode(contentType);
        }

        m_containsComplexTypes = !isPrimitiveImpl(contentType->kind());
    }
    else {
        m_containsComplexTypes = !isPrimitiveImpl(kind);
    }

    // Create child members for each element in array and sequence types
    if (kind == CORBA::tk_array)
    {
        const size_t arrayLength = m_typeCode->length();
        setLength(arrayLength); // Resizes m_children
    }
    else if (kind == CORBA::tk_sequence)
    {
        // Large sequences can cause the application to hang, so set the length
        // an empty size. It will be resized when data is received.
        setLength(0);
        
    }


    // Create child members for each member of struct types
    if (kind == CORBA::tk_struct)
    {
        const CORBA::ULong memberCount = m_typeCode->member_count();
        for (CORBA::ULong i = 0; i < memberCount; i++)
        {
            CORBA::TypeCode_ptr memberType = m_typeCode->member_type(i);
            if (!memberType)
            {
                std::cerr << "Invalid member type on ["
                          << i
                          << "] within "
                          << m_typeCode->name()
                          << std::endl;

                continue;
            }

            // Follow the alias trail until we have the true type
            while (memberType->kind() == CORBA::tk_alias)
            {
                memberType = TAO::unaliased_typecode(memberType);
            }
            //RJ 2022-01-21 I think older verisons of ddsman will default to topics being appendable, but structs within them being final. I think.
            const std::string memberName = m_typeCode->member_name(i);
            std::shared_ptr<OpenDynamicData> newMember = CreateOpenDynamicData(memberType, m_encodingKind,  OpenDDS::DCPS::Extensibility::FINAL, weak_from_this());
            newMember->setName(memberName);
            m_children.push_back(newMember);
        }

    } // End struct check

} // End OpenDynamicData::populate


/**
 * @}
 */
