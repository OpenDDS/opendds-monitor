#include <dds/DCPS/BuiltInTopicUtils.h>
#include <iostream>

#include "dds_data.h"
#include "dds_manager.h"
#include "subscription_monitor.h"


//------------------------------------------------------------------------------
SubscriptionMonitor::SubscriptionMonitor() : m_dataReader(nullptr)
{
    DDS::DomainParticipant* domain = CommonData::m_ddsManager->getDomainParticipant();
    DDS::Subscriber_var subscriber = domain->get_builtin_subscriber() ;
    if (!subscriber)
    {
        std::cerr << "SubscriptionMonitor: "
                  << "Unable to find get_builtin_subscriber"
                  << std::endl;
        return;
    }

    // Find and store the built-in data reader
    m_dataReader = subscriber->lookup_datareader(
        OpenDDS::DCPS::BUILT_IN_SUBSCRIPTION_TOPIC);

    if (!m_dataReader)
    {
        std::cerr << "SubscriptionMonitor: "
                  << "Unable to find built in subscription topic reader"
                  << std::endl;
        return;
    }

    // Attach a listener which reports reads subscription information
    m_dataReader->set_listener(this, DDS::DATA_AVAILABLE_STATUS);
}


//------------------------------------------------------------------------------
SubscriptionMonitor::~SubscriptionMonitor()
{
    // Don't delete?
    m_dataReader = nullptr;
}


//------------------------------------------------------------------------------
void SubscriptionMonitor::on_data_available(DDS::DataReader_ptr reader)
{
    DDS::SampleInfoSeq infoSeq;
    DDS::SubscriptionBuiltinTopicDataSeq msgList;

    DDS::SubscriptionBuiltinTopicDataDataReader_var dataReader =
        DDS::SubscriptionBuiltinTopicDataDataReader::_narrow(reader);

    if (!dataReader)
    {
        std::cerr << "SubscriptionMonitor::on_data_available: "
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
        const DDS::SubscriptionBuiltinTopicData& sampleData = msgList[i];
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
                 << " subscription"
                 << std::endl;

        std::shared_ptr<TopicInfo> topicInfo = CommonData::getTopicInfo(topicName);

        // If we already know about this topic but don't have the typecode, add it
        if (topicInfo != nullptr &&
            topicInfo->typeCode() == nullptr &&
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
        topicInfo->topicName() = sampleData.topic_name;
        topicInfo->typeName() = sampleData.type_name;
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

        // Check for USR specific user data
        if (userDataSize > 0)
        {
            topicInfo->storeUserData(sampleData.topic_data.value);
        }

        CommonData::storeTopicInfo(topicName, topicInfo);

        emit newTopic(topicName);
    }

    dataReader->return_loan(msgList, infoSeq);
}


/**
 * @}
 */
