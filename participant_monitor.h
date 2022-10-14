#pragma once

#include "dds_listeners.h"

#include <dds/DCPS/GuidConverter.h>

#include <functional>
#include <map>
#include <mutex>
#include <string>

/**
 * @brief Stores information on a DDS participant.
 */
class ParticipantInfo
{
public:

    ParticipantInfo() = default;
    ~ParticipantInfo() = default;

    /**
     * @brief Required operator for stl::sort.
     * @param[in] rhs Compare the current object with this object.
     * @return True if this < rhs; otherwise false.
     */
    inline bool operator<(const ParticipantInfo& rhs) const {
        return this->guid < rhs.guid;
    };

    /**
     * @brief Useful operator for find.
     * @param[in] rhs Compare the current object with this object.
     * @return True if this == rhs; otherwise false.
     */
    inline bool operator==(const ParticipantInfo& rhs) const {
        return this->guid == rhs.guid;
    };

    std::string guid; ///< Original complete GUID
    std::string location; ///< IP:Port
    std::string hostID; ///< GUID host ID
    std::string appID; ///< GUID app ID
    std::string instanceID; ///< GUID instance ID
    std::string discovered_timestamp; ///< Created timestamp
};

typedef std::function<void(const ParticipantInfo&)> ParticipantInfoCallback;

/**
 * @brief Receives information about participants on the bus.
 * @details 
 */
class ParticipantMonitor
{
public:

    /**
     * @brief Constructor for the DDS participant monitor class.
     */
    ParticipantMonitor(DDS::DomainParticipant* domain, ParticipantInfoCallback onAdd = nullptr, ParticipantInfoCallback onRemove = nullptr);
    ParticipantMonitor() = delete;

    /**
     * @brief Destructor for the DDS participant monitor class.
     */
    ~ParticipantMonitor();

    struct DcpsParticipantListener : public GenericReaderListener {
        DcpsParticipantListener(ParticipantMonitor* monitor) : m_monitor(monitor) {}
        void on_data_available(DDS::DataReader_ptr reader) { if (m_monitor) { m_monitor->on_participant_data_available(reader); } }
        ParticipantMonitor* m_monitor;
    };

    struct DcpsParticipantLocationListener : public GenericReaderListener {
        DcpsParticipantLocationListener(ParticipantMonitor* monitor) : m_monitor(monitor) {}
        void on_data_available(DDS::DataReader_ptr reader) { if (m_monitor) { m_monitor->on_participant_location_data_available(reader); } }
        ParticipantMonitor* m_monitor;
    };

private:

    void on_participant_data_available(DDS::DataReader_ptr reader);
    void on_participant_location_data_available(DDS::DataReader_ptr reader);

    /// Stores the built-in data reader for the participant topic
    DDS::DataReader_ptr m_participant_datareader;
    DDS::DataReader_ptr m_participant_location_datareader;
    DcpsParticipantListener m_participant_listener;
    DcpsParticipantLocationListener m_participant_location_listener;
    ParticipantInfoCallback m_addParticipant;
    ParticipantInfoCallback m_removeParticipant;

    std::mutex m_info_map_mutex;
    std::map<OpenDDS::DCPS::GUID_t, ParticipantInfo> m_info_map;
};
