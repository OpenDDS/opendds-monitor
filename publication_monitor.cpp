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
    for (size_t i = 0; i < msgList.length(); i++)
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

        // If we already know about this topic but don't have the typecode, add it
        if (topicInfo != nullptr &&
            topicInfo->typeCode == nullptr &&
            userDataSize > 0)
        {
            topicInfo->storeUserData(sampleData.topic_data.value);
        }
        
        // If we already know about this topic, just store additional
        // partition information and move on
        if (topicInfo != nullptr)
        {
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


/**
 * @}
 */
