#include "participant_monitor.h"

#pragma warning(push, 0)  //No DDS warnings
#include <dds/DCPS/BuiltInTopicUtils.h>
#include <dds/OpenddsDcpsExtTypeSupportImpl.h> //new in 3.19, defines ParticipantLocationBuiltinTopicDataSeq
#pragma warning(pop)

#include <iostream>
#include <chrono>

ParticipantMonitor::ParticipantMonitor(DDS::DomainParticipant* domain, ParticipantInfoCallback onAdd, ParticipantInfoCallback onRemove)
    : m_participant_datareader(nullptr)
    , m_participant_location_datareader(nullptr)
    , m_participant_listener(this)
    , m_participant_location_listener(this)
    , m_addParticipant(onAdd)
    , m_removeParticipant(onRemove)
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
    m_participant_datareader = subscriber->lookup_datareader(
        OpenDDS::DCPS::BUILT_IN_PARTICIPANT_TOPIC);

    if (!m_participant_datareader)
    {
        std::cerr
            << "ParticipantMonitor: "
            << "Unable to find BUILT_IN_PARTICIPANT_LOCATION_TOPIC topic reader"
            << std::endl;
        return;
    }

    m_participant_datareader->set_listener(&m_participant_listener, DDS::DATA_AVAILABLE_STATUS);

    m_participant_location_datareader = subscriber->lookup_datareader(
        OpenDDS::DCPS::BUILT_IN_PARTICIPANT_LOCATION_TOPIC);

    if (!m_participant_location_datareader)
    {
        std::cerr
            << "ParticipantMonitor: "
            << "Unable to find BUILT_IN_PARTICIPANT_LOCATION_TOPIC topic reader"
            << std::endl;
        return;
    }

    m_participant_location_datareader->set_listener(&m_participant_location_listener, DDS::DATA_AVAILABLE_STATUS);
}

ParticipantMonitor::~ParticipantMonitor()
{   
    m_participant_datareader = nullptr;  // Do not delete. We don't own it.
    m_participant_location_datareader = nullptr;  // Do not delete. We don't own it.
}

void ParticipantMonitor::on_participant_data_available(DDS::DataReader_ptr reader)
{
    DDS::SampleInfoSeq infoSeq;
    DDS::ParticipantBuiltinTopicDataSeq msgList;

    DDS::ParticipantBuiltinTopicDataDataReader_var dataReader =
        DDS::ParticipantBuiltinTopicDataDataReader::_narrow(reader);

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
    CORBA::ULong msgListSize = msgList.length();
    for (CORBA::ULong i = 0; i < msgListSize; i++)
    {
        const DDS::ParticipantBuiltinTopicData& sampleData = msgList[i];
        const DDS::SampleInfo& sampleInfo = infoSeq[i];

        OpenDDS::DCPS::GUID_t guid;
        memcpy(&guid, &sampleData.key.value[0], sizeof (guid));

        ParticipantInfo info;

        std::unique_lock<std::mutex> lock(m_info_map_mutex);

        const auto iter = m_info_map.find(guid);
        if (iter != m_info_map.end()) {
          info = iter->second;
        } else {
          // Get the GUID information into a string
          std::ostringstream guidStream;
          for (int j = 3; j >= 0; j--)
          {
              guidStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned short>(sampleData.key.value[j]);
          }
          info.hostID = guidStream.str();

          guidStream.str("");
          for (int j = 7; j >= 4; j--)
          {
              guidStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned short>(sampleData.key.value[j]);
          }
          info.appID = guidStream.str();

          guidStream.str("");
          for (int j = 11; j >= 8; j--)
          {
              guidStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned short>(sampleData.key.value[j]);
          }
          info.instanceID = guidStream.str();
          info.guid = info.hostID + "." + info.appID + "." + info.instanceID;
        }

        if (sampleInfo.valid_data) {

          const unsigned long long milliSecondsSinceEpoch = static_cast<unsigned long long>(sampleInfo.source_timestamp.sec) * 1000U + static_cast<unsigned long>(sampleInfo.source_timestamp.nanosec) / 1000000U;
          const auto durationSinceEpoch = std::chrono::milliseconds(milliSecondsSinceEpoch);
          const std::chrono::time_point<std::chrono::system_clock> afterDuration(durationSinceEpoch);
          time_t timeT = std::chrono::system_clock::to_time_t(afterDuration);
          unsigned long long remainderMS = milliSecondsSinceEpoch % 1000U;
          std::ostringstream dateStream;
          dateStream << std::dec << std::put_time(std::localtime(&timeT), "%F %T.") << remainderMS;
          info.discovered_timestamp = dateStream.str();

          m_info_map[guid] = info;
        } else if (iter != m_info_map.end()) {
          m_info_map.erase(iter);
        }

        lock.unlock();

        if (sampleInfo.valid_data)
        {
            if (m_addParticipant) m_addParticipant(info);
        }
        else
        {
            if (m_removeParticipant) m_removeParticipant(info);
        }
    }
}

void ParticipantMonitor::on_participant_location_data_available(DDS::DataReader_ptr reader)
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
    CORBA::ULong msgListSize = msgList.length();
    for (CORBA::ULong i = 0; i < msgListSize; i++)
    {
        const OpenDDS::DCPS::ParticipantLocationBuiltinTopicData& sampleData = msgList[i];
        const DDS::SampleInfo& sampleInfo = infoSeq[i];

        OpenDDS::DCPS::GUID_t guid;
        memcpy(&guid, &sampleData.guid[0], sizeof (guid));

        ParticipantInfo info;

        std::unique_lock<std::mutex> lock(m_info_map_mutex);

        const auto iter = m_info_map.find(guid);
        if (iter != m_info_map.end()) {
          info = iter->second;
        } else {
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
        }

        if (sampleInfo.valid_data) {

          if (sampleData.location & OpenDDS::DCPS::LOCATION_LOCAL) {
            info.location = sampleData.local_addr.in();
          } else if (sampleData.location & OpenDDS::DCPS::LOCATION_LOCAL6) {
            info.location = sampleData.local6_addr.in();
          }

          m_info_map[guid] = info;
        } else if (iter != m_info_map.end()) {
          m_info_map.erase(iter);
        }

        lock.unlock();

        if (sampleInfo.valid_data)
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
