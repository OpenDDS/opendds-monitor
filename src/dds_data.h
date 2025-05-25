#ifndef __DDS_DATA_MONITOR_H__
#define __DDS_DATA_MONITOR_H__

#include "first_define.h"

#ifdef WIN32
#pragma warning(push, 0)  //No DDS warnings
#endif

#include <dds/DCPS/Serializer.h>
#include <dds/DdsDcpsCoreC.h>
#include <dds/DdsDynamicDataC.h>

#ifdef WIN32
#pragma warning(pop)
#endif

#include <QStringList>
#include <QVariant>
#include <QString>
#include <QMutex>
#include <QList>
#include <QMap>

#include <memory>
#include <string>
#include <cstdint>


class DDSManager;
class OpenDynamicData;
class TopicSampleTableModel;

const std::string DATA_READER_NAME = "DDSMon";
const QString SETTINGS_APP_NAME = "DDS Monitor";
const QString SETTINGS_ORG_NAME = "DDS";

const QString DURABILITY_QOS_POLICY_STRINGS[] =
{
    "VOLATILE",
    "TRANSIENT_LOCAL",
    "TRANSIENT",
    "PERSISTENT"
};

const QString RELIABILITY_QOS_POLICY_STRINGS[] =
{
    "BEST_EFFORT",
    "RELIABLE"
};

const QString OWNERSHIP_QOS_POLICY_STRINGS[] =
{
    "SHARED",
    "EXCLUSIVE"
};

const QString DESTINATION_ORDER_QOS_POLICY_STRING[] =
{
    "BY_RECEPTION",
    "BY_SOURCE"
};

const QString DESTINATION_SCOPE_QOS_POLICY_STRING[] =
{
    "BY_INSTANCE",
    "BY_TOPIC"
};

/// The monitor supports two modes for discovering remote types. It uses either
/// CORBA TypeCode encoded in the user data QoS or the XTypes dynamic type mechanism.
/// The mode is set per topic and writers to the same topic should assume the same
/// method of exchanging type information.
enum class TypeDiscoveryMode
{
    TypeCode,
    DynamicType
};

/**
 * @brief Stores information on discovered DDS topics.
 */
class TopicInfo
{
public:

    /// Constructor for the DDS topic information class.
    TopicInfo();

    /// Destructor for the DDS topic information class.
    ~TopicInfo();

    /**
     * @brief Add any new partition QoS information to this topic
     * @param[in] partitionQos The new partition information.
     */
    void addPartitions(const DDS::PartitionQosPolicy& partitionQos);

    /**
     * @brief Set the durability policy for the appropriate QoS members.
     * @param[in] policy The new policy information.
     */
    void setDurabilityPolicy(const DDS::DurabilityQosPolicy& policy);

    /**
     * @brief Set the durability service policy for the appropriate QoS members.
     * @param[in] policy The new policy information.
     */
    void setDurabilityServicePolicy(const DDS::DurabilityServiceQosPolicy& policy);

    /**
     * @brief Set the deadline policy for the appropriate QoS members.
     * @param[in] policy The new policy information.
     */
    void setDeadlinePolicy(const DDS::DeadlineQosPolicy& policy);

    /**
     * @brief Set the latency budget policy for the appropriate QoS members.
     * @param[in] policy The new policy information.
     */
    void setLatencyBudgePolicy(const DDS::LatencyBudgetQosPolicy& policy);

    /**
     * @brief Set the lifespan policy for the appropriate QoS members.
     * @param[in] policy The new policy information.
     */
    void setLifespanPolicy(const DDS::LifespanQosPolicy& policy);

    /**
     * @brief Set the liveliness policy for the appropriate QoS members.
     * @param[in] policy The new policy information.
     */
    void setLivelinessPolicy(const DDS::LivelinessQosPolicy& policy);

    /**
     * @brief Set the reliability policy for the appropriate QoS members.
     * @param[in] policy The new policy information.
     */
    void setReliabilityPolicy(const DDS::ReliabilityQosPolicy& policy);

    /**
     * @brief Set the ownership policy for the appropriate QoS members.
     * @param[in] policy The new policy information.
     */
    void setOwnershipPolicy(const DDS::OwnershipQosPolicy& policy);

    /**
     * @brief Set the destination order policy for the appropriate QoS members.
     * @param[in] policy The new policy information.
     */
    void setDestinationOrderPolicy(const DDS::DestinationOrderQosPolicy& policy);

    /**
     * @brief Set the presentation policy for the appropriate QoS members.
     * @param[in] policy The new policy information.
     */
    void setPresentationPolicy(const DDS::PresentationQosPolicy& policy);

    /**
     * @brief Fixes history for strict reliable topics. see QosDictionary::Topic::strictReliable()
     */
    void fixHistory();

    /**
     * @brief Store the Topic QoS user_data information.
     * @param[in] userData The Topic QoS user_data information.
     */
    void storeUserData(const DDS::OctetSeq& userData);

    /**
     * Accessors for the data members
     * @{
     */
    const std::string& topicName() const
    {
        return m_topicName;
    }

    std::string& topicName()
    {
        return m_topicName;
    }

    const std::string& typeName() const
    {
        return m_typeName;
    }

    std::string& typeName()
    {
        return m_typeName;
    }

    const DDS::TopicQos& topicQos() const
    {
        return m_topicQos;
    }

    DDS::PublisherQos& pubQos()
    {
        return m_pubQos;
    }

    const DDS::DataWriterQos& writerQos() const
    {
        return m_writerQos;
    }

    DDS::SubscriberQos& subQos()
    {
        return m_subQos;
    }

    const DDS::DataReaderQos& readerQos() const
    {
        return m_readerQos;
    }

    OpenDDS::DCPS::Extensibility extensibility() const
    {
        return m_extensibility;
    }

    TypeDiscoveryMode typeMode() const
    {
        return m_typeMode;
    }

    void typeMode(TypeDiscoveryMode mode)
    {
        m_typeMode = mode;
    }

    const QStringList& partitions() const
    {
        return m_partitions;
    }

    bool hasKey() const
    {
        return m_hasKey;
    }

    const CORBA::TypeCode* typeCode() const
    {
        return m_typeCode;
    }

    DDS::DynamicType_var dynamicType() const
    {
        return m_dynamicType;
    }

    DDS::DynamicType_var& dynamicType()
    {
        return m_dynamicType;
    }

    /**
     * @}
     */

private:

    void dumpTypeCode(const char* cdrBuffer, size_t typeCodeSize) const;

    /// The name of the DDS topic.
    std::string m_topicName;

    /// The type of the DDS topic.
    std::string m_typeName;

    /// The topic QoS settings.
    DDS::TopicQos m_topicQos;

    /// The publisher QoS settings.
    DDS::PublisherQos m_pubQos;

    /// The data writer QoS settings.
    DDS::DataWriterQos m_writerQos;

    /// The subscriber QoS settings.
    DDS::SubscriberQos m_subQos;

    /// The data readerQoS settings.
    DDS::DataReaderQos m_readerQos;

    /// Stores a list of partitions.
    QStringList m_partitions;

    /// The topic extensibility
    OpenDDS::DCPS::Extensibility m_extensibility;

    /// Flag if this is a keyed topic. Set from user_data in the Topic Qos.
    bool m_hasKey;

    /// Type exchange mode.
    TypeDiscoveryMode m_typeMode;

    /// Type code length
    size_t m_typeCodeLength;

    /// Pointer to the type code information object. Set from user_data in the Topic Qos.
    const CORBA::TypeCode* m_typeCode;

    /// The type code information object. Set from user_data in the Topic Qos.
    std::unique_ptr<CORBA::Any> m_typeCodeObj;

    /// DynamicType of the topic's type
    DDS::DynamicType_var m_dynamicType;

};


/**
 * @brief Stores the shared data for the DDS Monitor application
 */
class CommonData
{
public:

    /// The maximum number of samples to store in the history.
    static const int MAX_SAMPLES = 500;

    /// The shared DDS manager object.
    static std::unique_ptr<DDSManager> m_ddsManager;

    /// Delete all data objects before closing.
    static void cleanup();

    /**
     * @brief Store the information on a discovered topic.
     * @param[in] topicName The name of the discovered topic.
     * @param[in] info The topic information struct.
     */
    static void storeTopicInfo(const QString& topicName, std::shared_ptr<TopicInfo> info);

    /**
     * @brief Get the information on a discovered topic.
     * @param[in] topicName The name of the discovered topic.
     * @return The topic information struct or NULL if not found.
     */
    static std::shared_ptr<TopicInfo> getTopicInfo(const QString& topicName);

    /**
     * @brief Read the value of a DDS sample.
     * @param[in] topicName The name of the topic.
     * @param[in] memberName The name of the topic member.
     * @param[in] index The sample index. 0 is the newest.
     * @return A QVariant containing the sample value.
     */
    static QVariant readValue(const QString& topicName,
                              const QString& memberName,
                              unsigned int index = 0);

    /**
     * @brief Delete all data samples for a specified topic.
     * @param[in] topicName The name of the topic.
     */
    static void flushSamples(const QString& topicName);

    /**
     * @brief Store a new data sample for a specified topic
     * @param[in] topicName The name of the topic.
     * @param[in] sampleName The name (timestamp) of the data sample.
     * @param[in] sample The data sample of the topic.
     */
    static void storeSample(const QString& topicName,
                            const QString& sampleName,
                            const std::shared_ptr<OpenDynamicData> sample);

    /// Store a new sample represented by a DynamicData object.
    static void storeDynamicSample(const QString& topicName,
                                   const QString& sampleName,
                                   DDS::DynamicData_var sample);

    /**
     * @brief Get a copy of a sample for a specified topic.
     * @remarks The caller is responsible for deleting the new sample.
     * @param[in] topicName Get a sample of this topic.
     * @param[in] index The sample index position. The newest is on the front.
     * @return A copy of the data sample or NULL if the index wasn't found.
     */
    static std::shared_ptr<OpenDynamicData> copySample(const QString& topicName,
                                                       int index);

    static DDS::DynamicData_var copyDynamicSample(const QString& topicName,
                                                  int index);
    /**
     * @brief Get a list of sample names (timestamps) for a given topic.
     * @param[in] topicName The name of the topic.
     * @return A stringlist of sample names.
     */
    static QStringList getSampleList(const QString& topicName);

private:

    static QVariant readMember(const QString& topicName,
                               const QString& memberName,
                               unsigned int index = 0);

    static QVariant readDynamicMember(const QString& topicName,
                                      const QString& memberName,
                                      unsigned int index = 0);

    /// Called by flushSamples() depending on whether a recorder or a dynamic data reader is used.
    static void flushStaticSamples(const QString& topicName);
    static void flushDynamicSamples(const QString& topicName);

    /**
     * @brief Stores the data samples from DDS.
     * @details The key is the topic name and the value is the data sample. The
     *          first sample is always the latest and the last sample is last.
     */
    using SampleMap = QMap<QString, QList<std::shared_ptr<OpenDynamicData>>>;
    static SampleMap m_samples;

    /**
     * @brief Store list of DynamicData objects for each topic.
     * The timestamps for the samples are also stored in m_sampleTimes.
     */
    using DynamicSampleMap = QMap<QString, QList<DDS::DynamicData_var>>;
    static DynamicSampleMap m_dynamicSamples;

    /**
     * @brief Stores the data sample times.
     * @details The key is the topic name and the value is the data time. The
     *          first sample is always the latest and the last sample is last.
     */
    using SampleTimeMap = QMap<QString, QStringList>;
    static SampleTimeMap m_sampleTimes;

    /**
     * @brief Stores information about the topics on the bus.
     * @details The key is the topic name and the value is the topic
     *          information struct.
     */
    static QMap<QString, std::shared_ptr<TopicInfo>> m_topicInfo;

    /// Mutex for protecting access to m_samples.
    static QMutex m_sampleMutex;

    /// Mutex for protecting access to m_topicInfo.
    static QMutex m_topicMutex;

    /// Mutex for protecting access to m_dynamicSamples.
    static QMutex m_dynamicSamplesMutex;

};

#endif

/**
 * @}
 */
