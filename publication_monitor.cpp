#include <dds/DCPS/BuiltInTopicUtils.h>
#include <iostream>

#include "dds_data.h"
#include "dds_manager.h"
#include "publication_monitor.h"


//------------------------------------------------------------------------------
PublicationMonitor::PublicationMonitor() : m_dataReader(nullptr)
{
    DDS::DomainParticipant* domain = CommonData::m_ddsManager->getDomainParticipant();
    DDS::Subscriber_var subscriber = domain->get_builtin_subscriber() ;
    if (!subscriber)
    {
        std::cerr << "PublicationMonitor: "
                  << "Unable to find get_builtin_subscriber"
                  << std::endl;
        return;
    }

    // Find and store the built-in data reader
    m_dataReader = subscriber->lookup_datareader(
        OpenDDS::DCPS::BUILT_IN_PUBLICATION_TOPIC);

    if (!m_dataReader)
    {
        std::cerr << "PublicationMonitor: "
                  << "Unable to find built in publication topic reader"
                  << std::endl;
        return;
    }

    // Attach a listener which reports reads publication information
    m_dataReader->set_listener(this, DDS::DATA_AVAILABLE_STATUS);
}


//------------------------------------------------------------------------------
PublicationMonitor::~PublicationMonitor()
{
    // Don't delete?
    m_dataReader = nullptr;
}


//------------------------------------------------------------------------------
void PublicationMonitor::on_data_available(DDS::DataReader_ptr reader)
{
    //std::cout << "DEBUG PublicationMonitor::on_data_available" << std::endl;
    DDS::SampleInfoSeq infoSeq;
    DDS::PublicationBuiltinTopicDataSeq msgList;

    DDS::PublicationBuiltinTopicDataDataReader_var dataReader =
        DDS::PublicationBuiltinTopicDataDataReader::_narrow(reader);

    if (!dataReader)
    {
        std::cerr << "PublicationMonitor::on_data_available: "
                  << "Error calling _narrow"
                  << std::endl;
        return;
    }

    dataReader->take(
        msgList,
        infoSeq,
        DDS::LENGTH_UNLIMITED,
        DDS::ANY_SAMPLE_STATE,
        DDS::ANY_VIEW_STATE,
        DDS::ALIVE_INSTANCE_STATE);

    // Iterate through all received samples
    for (CORBA::ULong i = 0; i < msgList.length(); i++)
    {
        const DDS::PublicationBuiltinTopicData& sampleData = msgList[i];
        const DDS::SampleInfo& sampleInfo = infoSeq[i];

        // Skip bogus data
        if (!sampleInfo.valid_data)
        {
            continue;
        }

        const char* topicNameC = sampleData.topic_name;
        const QString topicName = topicNameC;
        const size_t userDataSize = sampleData.topic_data.value.length();
        std::cout << "Discovered "
                 << sampleData.topic_name
                 << " publication"
                 << std::endl;

        std::shared_ptr<TopicInfo> topicInfo = CommonData::getTopicInfo(topicName);

        if (topicInfo != nullptr) {
            if (topicInfo->typeCode == nullptr && userDataSize > 0) {
                // If we already know about this topic but don't have the typecode, add it
                topicInfo->storeUserData(sampleData.topic_data.value);
            }
            if (!topicInfo->dynamic_type) {
                // Try getting DynamicType of this topic again if it failed before
                DDS::DynamicType_var dt;
                if (get_dynamic_type(dt, sampleData.key, sampleData.topic_name, sampleData.type_name)) {
                    topicInfo->dynamic_type = dt;
                }
            }

            // If we already know about this topic, just store additional
            // partition information and move on
            topicInfo->addPartitions(sampleData.partition);
            continue;
        }

        // If we got here, we have new topic data
        topicInfo = std::make_shared<TopicInfo>();
        topicInfo->name = sampleData.topic_name;
        topicInfo->typeName = sampleData.type_name;
        //topicInfo->typeCode = Only exists in RTI implementation. We use user_data.

        topicInfo->setDurabilityPolicy(sampleData.durability);
        //topicInfo->setDurabilityServicePolicy(sampleData.durability_service);
        topicInfo->setDeadlinePolicy(sampleData.deadline);
        topicInfo->setLatencyBudgePolicy(sampleData.latency_budget);
        //topicInfo->setLifespanPolicy(sampleData.lifespan);
        topicInfo->setLivelinessPolicy(sampleData.liveliness);
        topicInfo->setReliabilityPolicy(sampleData.reliability);
        topicInfo->setOwnershipPolicy(sampleData.ownership);
        topicInfo->setDestinationOrderPolicy(sampleData.destination_order);
        topicInfo->setPresentationPolicy(sampleData.presentation);
        topicInfo->addPartitions(sampleData.partition);
        topicInfo->fixHistory();
        // Get DynamicType of the topic
        DDS::DynamicType_var dt;
        if (get_dynamic_type(dt, sampleData.key, sampleData.topic_name, sampleData.type_name)) {
            topicInfo->dynamic_type = dt;
        }
        CommonData::storeTopicInfo(topicName, topicInfo);

        // Check for USR specific user data
        if (userDataSize > 0)
        {
            topicInfo->storeUserData(sampleData.topic_data.value);
        }

        emit newTopic(topicName);

    } // End message iteration loop

    dataReader->return_loan(msgList, infoSeq);

} // End PublicationMonitor::on_data_available

bool PublicationMonitor::get_dynamic_type(DDS::DynamicType_var& type, const DDS::BuiltinTopicKey_t& key,
                                          const char* topic_name, const char* type_name)
{
    DDS::DomainParticipant* participant = CommonData::m_ddsManager->getDomainParticipant();
    DDS::ReturnCode_t ret = participant->get_dynamic_type(type, key);
    if (ret != DDS::RETCODE_OK) {
        std::cout << "WARNING: get_dynamic_type for topic " << topic_name
                  << " returned " << OpenDDS::DCPS::retcode_to_string(ret) << std::endl;
        return false;
    }
    DDS::TypeSupport_var dts = new DDS::DynamicTypeSupport(type);
    dts->register_type(participant, type_name);
    return true;
}

/**
 * @}
 */
