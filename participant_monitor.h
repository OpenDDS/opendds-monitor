#ifndef __DDS_PARTICIPANT_MONITOR_H__
#define __DDS_PARTICIPANT_MONITOR_H__

#include <string>
#include <functional>
#include "dds_listeners.h"

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
    std::string timestamp; ///< Discovered timestamp

}; // End ParticipantInfo


/**
 * @brief Receives information about participants on the bus.
 * @details 
 */
class ParticipantMonitor : public virtual GenericReaderListener
{
public:

    /**
     * @brief Constructor for the DDS participant monitor class.
     */
    ParticipantMonitor(DDS::DomainParticipant* domain, std::function<void(const ParticipantInfo&)> onAdd = nullptr, std::function<void(const ParticipantInfo&)> onRemove = nullptr);
    ParticipantMonitor() = delete;

    /**
     * @brief Destructor for the DDS participant monitor class.
     */
    ~ParticipantMonitor();

    /**
     * @brief Callback method to handle the DDS::DATA_AVAILABLE_STATUS message.
     * @param[in] reader The data reader containing the new message.
     */
    void on_data_available(DDS::DataReader_ptr reader) override;

private:

    /// Stores the built-in data reader for the participant topic
    DDS::DataReader_ptr m_dataReader;
    std::function<void(const ParticipantInfo&)> m_addParticipant;
    std::function<void(const ParticipantInfo&)> m_removeParticipant;
	
	/// Callbacks for adding and removing participants

}; // End ParticipantMonitor


#endif

/**
 * @}
 */
