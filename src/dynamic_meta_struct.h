#ifndef __DYNAMIC_META_STRUCT_H__
#define __DYNAMIC_META_STRUCT_H__

#include <dds/DCPS/FilterEvaluator.h> // For OpenDDS::DCPS::MetaStruct
#include <dds/DCPS/TypeSupportImpl.h>

class OpenDynamicData;


/**
 * @brief MetaStruct class for OpenDynamicData.
 * @details The FilterEvaluator class from OpenDDS requires an implementation of
 *          MetaStruct to work properly. Normally this is generated from the
 *          IDL compiler, but since this application does not use generated code,
 *          this class must exist. The only class that does any real work is
 *          getValue. The other functions exist to satisfy MetaStruct.
 * @remarks If any DynamicMetaStruct:: messages appear to the log window, the
 *          function is used and should be implemented.
 */
class DynamicMetaStruct : public OpenDDS::DCPS::MetaStruct
{
public:

    /**
     * @brief Constructor for the MetaStruct implementation of OpenDynamicData.
     * @param[in] sample Create the MetaStruct for this sample.
     */
    DynamicMetaStruct(const std::shared_ptr<OpenDynamicData> sample);

    /**
     * @brief Destructor for the MetaStruct implementation of OpenDynamicData.
     */
    ~DynamicMetaStruct();

    /**
     * @brief This function does nothing, but is required by MetaStruct.
     */
    OpenDDS::DCPS::Value getValue(const void*, DDS::MemberId) const;

    /**
     * @brief This function does nothing, but is required by MetaStruct.
     */
    OpenDDS::DCPS::Value getValue(const void*, const char*) const;

    /**
     * @brief Get the value of a topic member.
     * @remarks The value is pulled from m_sample rather than the Serializer parameter.
     * @param[in] unusedSerializer Unused but required by MetaStruct.
     * @param[in] fieldSpec The topic member name.
     */
    OpenDDS::DCPS::Value getValue(OpenDDS::DCPS::Serializer& unusedSerializer, const char* fieldSpec,
                                  OpenDDS::DCPS::TypeSupportImpl*) const;

    /**
     * @brief OpenDDS before version 3.24 uses a 2-argument getValue()
     *
     */
    OpenDDS::DCPS::Value getValue(OpenDDS::DCPS::Serializer& unusedSerializer, const char* fieldSpec) const
    {
      return getValue(unusedSerializer, fieldSpec, 0);
    }

    /**
     * @brief This function does nothing, but is required by MetaStruct.
     */
    OpenDDS::DCPS::ComparatorBase::Ptr create_qc_comparator(const char*, OpenDDS::DCPS::ComparatorBase::Ptr) const;

    /**
     * @brief This function does nothing, but is required by MetaStruct.
     */
    OpenDDS::DCPS::ComparatorBase::Ptr create_qc_comparator(const char*) const;

    /**
     * @brief This function does nothing, but is required by MetaStruct.
     * @return false.
     */
    bool compare(const void*, const void*, const char*) const;

	/**
     * @brief This function does nothing, but is required by MetaStruct.
     * @return false.
     */
    bool isDcpsKey(const char* field) const;

    /**
     * @brief This function does nothing, but is required by MetaStruct.
     * @return 0
     */
    size_t numDcpsKeys() const;

    /**
     * @brief This function does nothing, but is required by MetaStruct.
     * @return 0.
     */
    const char** getFieldNames() const;

    /**
     * @brief This function does nothing, but is required by MetaStruct.
     */
    void assign(void*, const char*, const void*, const char*, const MetaStruct&) const;

    /**
     * @brief This function does nothing, but is required by MetaStruct.
     * @return 0.
     */
    const void* getRawField(const void*, const char*) const;

    /**
     * @brief This function does nothing, but is required by MetaStruct.
     * @return 0.
     */
    void* allocate() const;

    /**
     * @brief This function does nothing, but is required by MetaStruct.
     */
    void deallocate(void*) const;



private:

    /// Stores the sample type information and values.
    const std::shared_ptr<OpenDynamicData> m_sample;

};


#endif

/**
 * @}
 */
