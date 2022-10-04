#ifndef __OPEN_DYNAMIC_DATA_H__
#define __OPEN_DYNAMIC_DATA_H__


#include <dds/DCPS/Serializer.h>
#include <dds/DCPS/TypeSupportImpl.h>
#include <tao/AnyTypeCode/TypeCode.h>
#include <cctype>
#include <string>
#include <vector>

/// Simplified noncopyable without Boost
class noncopyable
{
public:
    //As name implies. Can't copy this base class
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
    noncopyable() = default;
    ~noncopyable() = default;
};

/**
 * @brief Non-standard dynamic data class with similar features to XTypes
 *        with OpenDDS.
 *
 * @remarks This class will not be required when OpenDDS implements
 *          dds::core::xtypes::DynamicData.
 */
class OpenDynamicData
    : public std::enable_shared_from_this<OpenDynamicData>,
    private noncopyable
{
public:

    /**
     * @brief Constructor for the flexible DDS sample class.
     * @param[in] typeCode The type definition pointer for this type.
     * @param[in] encodingKind the encoding kind for this type. XCDR1 or XCDR2
     * @param[in] extensibility the extensibility for this type. final/appendable/mutable.
     * @param[in] parent The parent of this member or nullptr if it's the root.
     */
    OpenDynamicData(const CORBA::TypeCode* typeCode,
                    const OpenDDS::DCPS::Encoding::Kind encodingKind, 
                    const OpenDDS::DCPS::Extensibility extensibility,
                    const std::weak_ptr<OpenDynamicData> parent = std::weak_ptr<OpenDynamicData>());

    /**
     * @brief Destructor for the flexible DDS sample class.
     */
    ~OpenDynamicData();

    /**
     * @brief Create a deep copy of another OpenDynamicData.
     * @remarks The type codes of the two objects must match.
     * @param[in] other Create a deep copy of this OpenDynamicData.
     * @return The newly created OpenDynamicData
     */
    OpenDynamicData& operator=(const OpenDynamicData& other);

    /**
     * @brief Comparator (equal) operator for two OpenDynamicData objects.
     * @remarks The type codes of the two objects must match.
     * @param[in] const reference to OpenDynamicData object to compare.
     * @return true if identical.
     */
    bool operator==(const OpenDynamicData& other);
    
    /**
     * @brief Comparator (inequal) operator for two OpenDynamicData objects.
     * @remarks The type codes of the two objects must match.
     * @param[in] const reference to OpenDynamicData object to compare.
     * @return true if not identical.
     */
    bool operator!=(const OpenDynamicData& other) {return !(*this == other);}
     

    /**
     * @brief Dump the type information and values to the terminal.
     */
    void dump() const;

    /**
     * @brief Populate the passed in Serializer from member values.
     * @remarks The Serializer object MUST be in the CDR format.
     * @param[out] stream Populate this Serializer object.
     * @return True if the operation was successful; false otherwise.
     */
    bool operator>>(OpenDDS::DCPS::Serializer& stream) const;

    /**
     * @brief Populate member values from a passed in Serializer.
     * @remarks The Serializer object MUST be in the CDR format.
     * @param[in] stream Populate member values from this Serializer object.
     * @return True if the operation was successful; false otherwise.
     */
    bool operator<<(OpenDDS::DCPS::Serializer& stream);

    /**
     * @brief Get the name of this member.
     * @return The name of this member.
     */
    std::string getName() const;

    /**
     * @brief Get the type kind for this member.
     * @return The type kind for the child member or tk_null if not found.
     */
    CORBA::TCKind getKind() const;

    /**
     * @brief Get whether this member is a primive kind. Required for serializing xcdr2 properly.
     * @return true if primitive type. false if not. 
     */
    bool isPrimitive() const;

    /**
     * @brief Does this type contain child data.  Used to determine if the type needs recursive handling.
     * @return true if the type contains other data, false if not. 
     */
    bool isContainerType() const;

    /**
     * @brief Get whether this member is an array or sequence of complex types. Required for serializing xcdr2 properly.
     * @return true if contains complex types. false if not. 
     */
    bool containsComplexTypes() const;

    /**
     * @brief Get the type code for this member.
     * @return The type code for this member or nullptr if not found.
     */
    const CORBA::TypeCode* getTypeCode() const;

    /**
     * @brief Get the length of a sequence/array member or the number of
     *        struct members for a string member.
     * @return The length of the sequence/array or struct member count.
     */
    size_t getLength() const;

    /**
     * @brief Set the length of a sequence member.
     * @remarks All children will be deleted and recreated to the new length.
     * @param[in] length The new sequence length.
     */
    void setLength(const size_t& length);

    /**
     * @brief Get the full name of this member.
     * @return The full member name of this member.
     */
    std::string getFullName() const;

    /**
     * @brief Get the child with the matching name.
     * @param[in] fullName Get the child with this full name.
     * @return The target child or nullptr if not found.
     */
    std::shared_ptr<OpenDynamicData> getMember(const std::string& fullName) const;

    /**
     * @brief Get the child with the matching index.
     * @param[in] index Get the child with this index.
     * @return The target child or nullptr if not found.
     */
    std::shared_ptr<OpenDynamicData> getMember(const size_t& index) const;

    /**
     * @brief Get the value for string members.
     * @return The string value for this member.
     */
    const char* getStringValue() const;

    /**
     * @brief Set the value for string members.
     * @param[in] value Set the string member to this value.
     */
    void setStringValue(const char* value);

    /**
     * @brief Get the value for this member.
     * @return The value for this member or 0 if we found an unsupported type.
     */
    template<class T>
    T getValue() const
    {
        switch (m_typeCode->kind())
        {
        case CORBA::tk_long: return static_cast<T>(m_value.int32);
        case CORBA::tk_short: return static_cast<T>(m_value.int16);
        case CORBA::tk_ushort: return static_cast<T>(m_value.uint16);
        case CORBA::tk_enum:
        case CORBA::tk_ulong: return static_cast<T>(m_value.uint32);
        case CORBA::tk_float: return static_cast<T>(m_value.float32);
        case CORBA::tk_double: return static_cast<T>(m_value.float64);
        case CORBA::tk_boolean: return static_cast<T>(m_value.boolean);
        case CORBA::tk_char: return static_cast<T>(m_value.char8);
        case CORBA::tk_wchar: return static_cast<T>(m_value.char16);
        case CORBA::tk_octet: return static_cast<T>(m_value.uint8);
        case CORBA::tk_longlong: return static_cast<T>(m_value.int64);
        case CORBA::tk_ulonglong: return static_cast<T>(m_value.uint64);
        default:
            std::cerr << "OpenDynamicData::getValue: "
                      << "Unsupported type (" << m_typeCode->kind() << ")"
                      << std::endl;
            break;
        }

        return static_cast<T>(0);

    } // End OpenDynamicData::getValue

    /**
     * @brief Set the value for this member.
     * @param[in] value Set the member to this value.
     */
    template<class T>
    void setValue(const T& value)
    {
        switch (m_typeCode->kind())
        {
        case CORBA::tk_long: m_value.int32 = static_cast<CORBA::Long>(value); break;
        case CORBA::tk_short: m_value.int16  = static_cast<CORBA::Short>(value); break;
        case CORBA::tk_ushort: m_value.uint16  = static_cast<CORBA::UShort>(value); break;
        case CORBA::tk_enum:
        case CORBA::tk_ulong: m_value.uint32  = static_cast<CORBA::ULong>(value); break;
        case CORBA::tk_float: m_value.float32  = static_cast<CORBA::Float>(value); break;
        case CORBA::tk_double: m_value.float64  = static_cast<CORBA::Double>(value); break;
        case CORBA::tk_boolean: m_value.boolean  = static_cast<CORBA::Boolean>(value); break;
        case CORBA::tk_char: m_value.char8  = static_cast<CORBA::Char>(value); break;
        case CORBA::tk_wchar: m_value.char16  = static_cast<CORBA::WChar>(value); break;
        case CORBA::tk_octet: m_value.uint8  = static_cast<CORBA::Octet>(value); break;
        case CORBA::tk_longlong: m_value.int64  = static_cast<CORBA::LongLong>(value); break;
        case CORBA::tk_ulonglong: m_value.uint64  = static_cast<CORBA::ULongLong>(value); break;
        default:
            std::cerr << "OpenDynamicData::setValue: "
                      << "Unsupported type (" << m_typeCode->kind() << ")"
                      << std::endl;
            break;
        }

    } // End OpenDynamicData::setValue

        /**
     * @brief Recursively populate the internal data structure.
     */
    void populate();

    size_t getEncapsulationLength();

private:

    /**
     * @brief Set the name of a child member.
     * @param[in] name The name of the child member.
     */
    void setName(const std::string& name);

    /**
     * @brief Get whether a type is a primitive kind. Required for serializing xcdr2 properly.
     * @return true if primitive type. false if not. 
     */
    bool isPrimitiveImpl(const CORBA::TCKind tck) const;

    /**
     * @brief Does this type contain child data.  Used to determine if the type needs recursive handling.
     * @return true if the type contains data. false if not.
     */
    bool isContainerType(const CORBA::TCKind tck) const;


    /// Stores all primitive data types.
    union SimpleTypeUnion
    {
        ACE_CDR::ULongLong uint64;
        ACE_CDR::LongLong int64;
        ACE_CDR::ULong uint32;
        ACE_CDR::Long int32;
        ACE_CDR::UShort uint16;
        ACE_CDR::Short int16;
        ACE_CDR::Octet uint8;
        ACE_CDR::Char char8;
        ACE_CDR::WChar char16;
        ACE_CDR::Double float64;
        ACE_CDR::Float float32;
        ACE_CDR::Boolean boolean;
    };

    /// Stores the child members.
    std::vector<std::shared_ptr<OpenDynamicData>> m_children;

    /// Stores the parent of this member or nullptr if it's the root.
    const std::weak_ptr<OpenDynamicData> m_parent;

    /// The name of this member.
    std::string m_name;

    /// The value of this member for primitive types.
    SimpleTypeUnion m_value;

    /// The value of this member for the string type.
    TAO::String_Manager m_stringValue;

    /// The type code for this member.
    const CORBA::TypeCode* m_typeCode;

    /// The encoding kind used for this type. Required by the Serializer
    const OpenDDS::DCPS::Encoding::Kind m_encodingKind;

    /// The extensibility of this type. Required by Serializer.
    const OpenDDS::DCPS::Extensibility m_extensibility;

    /// Boolean indicating whether this type contains complex types. Only valid if this type is sequence or array.
    bool m_containsComplexTypes;

}; // End class OpenDynamicData

std::shared_ptr<OpenDynamicData> CreateOpenDynamicData(const CORBA::TypeCode* typeCode,
    const OpenDDS::DCPS::Encoding::Kind encodingKind, 
    const OpenDDS::DCPS::Extensibility extensibility,
    const std::weak_ptr<OpenDynamicData> parent = std::weak_ptr<OpenDynamicData>());

#endif

/**
 * @}
 */
