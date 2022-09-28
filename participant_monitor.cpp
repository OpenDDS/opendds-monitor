#include "participant_monitor.h"

#pragma warning(push, 0)  //No DDS warnings
#include <dds/DCPS/BuiltInTopicUtils.h>
#include <dds/OpenddsDcpsExtTypeSupportImpl.h> //new in 3.19, defines ParticipantLocationBuiltinTopicDataSeq
#pragma warning(pop)

#include <iostream>
#include <chrono>


//------------------------------------------------------------------------------
ParticipantMonitor::ParticipantMonitor(DDS::DomainParticipant* domain, std::function<void(const ParticipantInfo&)> onAdd, std::function<void(const ParticipantInfo&)> onRemove) : m_dataReader(nullptr), m_addParticipant(onAdd), m_removeParticipant(onRemove)
{
    DDS::Subscriber_var subscriber = domain->get_builtin_subscriber();
    if (!subscriber)
    {
        std::cerr
            << "ParticipantMonitor: "
            << "Unable to find get_builtin_subscriber"
            << std::endl;
        return;
    }

    // Find and store the built-in data reader
    m_dataReader = subscriber->lookup_datareader(
        OpenDDS::DCPS::BUILT_IN_PARTICIPANT_LOCATION_TOPIC);

    if (!m_dataReader)
    {
        std::cerr
            << "ParticipantMonitor: "
            << "Unable to find BUILT_IN_PARTICIPANT_LOCATION_TOPIC topic reader"
            << std::endl;
        return;
    }

    m_dataReader->set_listener(this, DDS::DATA_AVAILABLE_STATUS);
}


//------------------------------------------------------------------------------
ParticipantMonitor::~ParticipantMonitor()
{   
    m_dataReader = nullptr;  // Do not delete. We don't own it.
}


//------------------------------------------------------------------------------
void ParticipantMonitor::on_data_available(DDS::DataReader_ptr reader)
{
    DDS::SampleInfoSeq infoSeq;
    OpenDDS::DCPS::ParticipantLocationBuiltinTopicDataSeq msgList;

    OpenDDS::DCPS::ParticipantLocationBuiltinTopicDataDataReader_var dataReader =
        OpenDDS::DCPS::ParticipantLocationBuiltinTopicDataDataReader::_narrow(reader);

    if (!dataReader)
    {
        std::cerr
            << "ParticipantMonitor::on_data_available: "
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
        DDS::ANY_INSTANCE_STATE);

    // Iterate through all received samples
    unsigned long msgListSize = msgList.length();
    for (unsigned long i = 0; i < msgListSize; i++)
    {
        const OpenDDS::DCPS::ParticipantLocationBuiltinTopicData& sampleData = msgList[i];
        const DDS::SampleInfo& sampleInfo = infoSeq[i];
        ParticipantInfo info;

        // Get the GUID information into a string
        std::ostringstream guidStream;
        for (int j = 3; j >= 0; j--)
        {
            guidStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned short>(sampleData.guid[j]);
        }
        info.hostID = guidStream.str();

        guidStream.str("");
        for (int j = 7; j >= 4; j--)
        {
            guidStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned short>(sampleData.guid[j]);
        }
        info.appID = guidStream.str();

        guidStream.str("");
        for (int j = 11; j >= 8; j--)
        {
            guidStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned short>(sampleData.guid[j]);
        }
        info.instanceID = guidStream.str();

        info.guid = info.hostID + "." + info.appID + "." + info.instanceID;
        info.location = sampleData.local_addr.in();

        // Convert the timestamp from DDS into a nice string
        const unsigned long long milliSecondsSinceEpoch = static_cast<unsigned long long>(sampleData.local_timestamp.sec) * 1000U + static_cast<unsigned long>(sampleData.local_timestamp.nanosec) / 1000000U;
        const auto durationSinceEpoch = std::chrono::milliseconds(milliSecondsSinceEpoch);
        const std::chrono::time_point<std::chrono::system_clock> afterDuration(durationSinceEpoch);
        time_t timeT = std::chrono::system_clock::to_time_t(afterDuration);
        unsigned long long remainderMS = milliSecondsSinceEpoch % 1000U;
        std::ostringstream dateStream;
        dateStream << std::dec << std::put_time(std::localtime(&timeT), "%H:%M:%S.") << remainderMS;
        info.timestamp = dateStream.str();

        if (sampleInfo.instance_state == DDS::ALIVE_INSTANCE_STATE)
        {
            if (m_addParticipant) m_addParticipant(info);
        }
        else
        {
            if (m_removeParticipant) m_removeParticipant(info);
        }

    }

    dataReader->return_loan(msgList, infoSeq);

} 


/**
 * @}
 */
