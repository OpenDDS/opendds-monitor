#ifndef __DDS_TOPIC_REPLAYER_H__
#define __DDS_TOPIC_REPLAYER_H__

#include "first_define.h"

#include <dds/DCPS/TopicDescriptionImpl.h>
#include <dds/DCPS/OwnershipManager.h>
#include <dds/DCPS/EntityImpl.h>
#include <dds/DCPS/RecorderImpl.h>
#include <dds/DdsDcpsCoreC.h>

#include <QString>

#include <memory>

class OpenDynamicData;


/**
 * @brief Topic replayer for sending raw DDS data samples.
 */
class TopicReplayer
{
public:

    /**
     * @brief Constructor for the DDS topic replayer.
     * @param[in] topicName The name of the topic to replay.
     */
    TopicReplayer(const QString& topicName);

    /**
     * @brief Publish a data sample to the bus.
     * @param[in] sample Publish this data sample.
     */
    void publishSample(const std::shared_ptr<OpenDynamicData> sample);
    void publishSample(const DDS::DynamicData_var sample);

    /**
    * @brief Destructor for the DDS topic replayer.
    */
    virtual ~TopicReplayer();

private:

    /// Stores the name of the topic.
    QString m_topicName;

    /// Stores the typecode for this topic.
    const CORBA::TypeCode* m_typeCode;

    /// Stores the topic object for this replayer.
    DDS::Topic* m_topic;

    /// Stores the replayer object for this class.
    OpenDDS::DCPS::Replayer* m_replayer;

    /// The topic extensibility
    OpenDDS::DCPS::Extensibility m_extensibility;

    /// A dynamic data writer for this topic
    DDS::DataWriter_var m_dw;
};

#endif

/**
 * @}
 */
