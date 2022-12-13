#ifndef __DDS_DATA_MONITOR_H__
#define __DDS_DATA_MONITOR_H__

#include "first_define.h"

#include <cstdint>


#pragma warning(push, 0)  //No DDS warnings
#include <dds/DCPS/Serializer.h>
#include <dds/DdsDcpsCoreC.h>
#include <dds/DdsDynamicDataC.h>
#pragma warning(pop)

#include <QStringList>
#include <QVariant>
#include <QString>
#include <QMutex>
#include <QList>
#include <QMap>

#include <memory>
#include <string>


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

    /// The name of the DDS topic.
    std::string name;

    /// The type of the DDS topic.
    std::string typeName;

    /// The topic QoS settings.
    DDS::TopicQos topicQos;

    /// The publisher QoS settings.
    DDS::PublisherQos pubQos;

    /// The data writer QoS settings.
    DDS::DataWriterQos writerQos;

    /// The subscriber QoS settings.
    DDS::SubscriberQos subQos;

    /// The data readerQoS settings.
    DDS::DataReaderQos readerQos;

    /// The topic extensibility
    OpenDDS::DCPS::Extensibility extensibility;

    /// Stores a list of partitions.
    QStringList partitions;

    /// Flag if this is a keyed topic. Set from user_data in the Topic Qos.
    bool hasKey;

    /// Type code length
    size_t typeCodeLength;

    /// Pointer to the type code information object. Set from user_data in the Topic Qos.
    const CORBA::TypeCode* typeCode;

    /// The type code information object. Set from user_data in the Topic Qos.
    std::unique_ptr<CORBA::Any> typeCodeObj;

    /// DynamicType of the topic's type
    DDS::DynamicType_var dynamic_type;

}; // End TopicInfo


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
                              const unsigned int& index = 0);

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
    static void storeDynamicSample(const QString& topic_name,
                                   const QString& sample_name,
                                   const DDS::DynamicData_var sample);

    /**
     * @brief Get a copy of a sample for a specified topic.
     * @remarks The caller is responsible for deleting the new sample.
     * @param[in] topicName Get a sample of this topic.
     * @param[in] index The Sample index position. The newest is on the front.
     * @return A copy of the data sample or NULL if the index wasn't found.
     */
    static std::shared_ptr<OpenDynamicData> copySample(const QString& topicName,
                                                       const unsigned int& index);

    static DDS::DynamicData_var copyDynamicSample(const QString& topic_name,
                                                  unsigned int index);
    /**
     * @brief Get a list of sample names (timestamps) for a given topic.
     * @param[in] topicName The name of the topic.
     * @return A stringlist of sample names.
     */
    static QStringList getSampleList(const QString& topicName);

    /**
     * @brief Clear the sample history for a given topic.
     * @param[in] topicName The name of the topic.
     */
    static void clearSamples(const QString& topicName);

private:

    /**
     * @brief Stores the data samples from DDS.
     * @details The key is the topic name and the value is the data sample. The
     *          first sample is always the latest and the last sample is last.
     */
    static QMap<QString, QList<std::shared_ptr<OpenDynamicData> > > m_samples;

    /**
     * @brief Stores the data sample times.
     * @details The key is the topic name and the value is the data time. The
     *          first sample is always the latest and the last sample is last.
     */
    static QMap<QString, QStringList> m_sampleTimes;

    /**
     * @brief Stores information about the topics on the bus.
     * @details The key is the topic name and the value is the topic
     *          information struct.
     */
    static QMap<QString, std::shared_ptr<TopicInfo>> m_topicInfo;

    /// Store list of DynamicData objects for each topic.
    /// We are storing the timestamps for these samples also in m_sampleTimes.
    static QMap<QString, QList<DDS::DynamicData_var> > m_dynamicSamples;

    /// Mutex for protecting access to m_samples.
    static QMutex m_sampleMutex;

    /// Mutex for protecting access to m_topicInfo.
    static QMutex m_topicMutex;

    /// Mutex for protecting access to m_dynamicSamples.
    static QMutex m_dynamicSamplesMutex;

    /**
     * @brief Constructor for the DDS Monitor data storage class.
     */
    CommonData();

    /**
     * @brief Destructor for the DDS Monitor data storage class.
     */
    ~CommonData();

};

#endif

/**
 * @}
 */
