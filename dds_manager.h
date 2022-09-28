#ifndef __DDS_MANAGER_H__
#define __DDS_MANAGER_H__

#pragma warning(push, 0)  //No DDS warnings
#include <dds/DdsDcpsCoreC.h>
#include <dds/DdsDcpsDomainC.h>
#pragma warning(pop)

#include <vector>
#include <string>
#include <mutex>
#include <map>
#include <memory>
#ifndef CANT_SUPPORT_SHARED_MUTEX
#include <shared_mutex>
#endif

#include "dds_callback.h"
#include "dds_listeners.h"
#include "dds_logging.h"
#include "dds_manager_defs.h"
#include "dds_listeners.h"
#include "participant_monitor.h"

//User must supply this by compiling std_qos.idl.
#include "std_qosC.h"

class ThreadPool;

/**
 * @brief Main interface into the DDS global data space.
 *
 * @details This class provides methods to prevent copy-paste code. The entire
 *          DDS API is still exposed if the user needs to do more than the
 *          basics. The order of operations is as follows:
 *
 * - Join the domain with the joinDomain method.
 *
 * - Register topics for this application using the registerTopic method.
 *
 * - Optionally assign a topic to DDS partitions via the addPartition method.
 *
 * - Optionally assign any non-default QoS settings to topics, publishers,
 *   subscribers, data readers, and data writers.
 *
 * - Create publisher, subscriber, data readers, and data writers from the
 *   createPublisher, createSubscriber, and createPublisherSubscriber methods.
 *
 * - Optionally create data listeners and bind callback methods for any
 *   subscribed topics using the addDataListener and addCallback methods.
 *
 * - Enable the domain by calling the enableDomain method.
 *
 * - Read data samples with the takeSample and takeAllSamples methods.
 *
 * - Write new data samples with the writeSample method.
 */

class DLL_PUBLIC DDSManager
{
public:

    static constexpr int DefaultThreadPoolSize = 5;

    /**
     * @brief Constructor for the DDS manager class.
     */
    DDSManager(std::function<void(LogMessageType mt, const std::string& message)> messageHandler = nullptr, int threadPoolSize = DefaultThreadPoolSize);

    /**
     * @brief Destructor for the DDS manager class.
     */
    virtual ~DDSManager();

    /**
     * @brief Sets the handler for the reader listeners.  This allows special handling when certain events happen for the reader listener.
     * Set to nullptr to remove the handler.
     */
    void SetReaderListenerHandler(DDSReaderListenerStatusHandler* rlHandler);

    /**
     * @brief Sets the handler for the writer listeners.  This allows special handling when certain events happen for the writer listener.
     * Set to nullptr to remove the handler.
    */
    void SetWriterListenerHandler(DDSWriterListenerStatusHandler* wlHandler);

    /**
     * @brief Join the DDS domain.
     *
     * @details An INI file is used to configure the DDS library. See chapter 7
     *          of the OpenDDS developer guide for available options. The 
     *          DDS_CONFIG environment variable can be used to specify the path
     *          to this file. If DDS_CONFIG is not set, the opendds.ini file
     *          from the working directory will be used.
     *
     * @param[in] domainID Join the DDS domain with this ID.
     * @param[in] config Optionally specify the config section from the INI
     *            file for this domain. The config section format is
     *            [config/NAME] where NAME is the input parameter for this
     *            method.
     * @param[in] onAdd Optional function called when a new participant is added to the domain.  
     *            The function argument is a structure with information about the new 
     *            participant.
     * @param[in] onRemove Optional function called when a participant leaves the domain.
     *            The function argument is a structure with information about the 
     *            participant which left.
     * @return True if the operation was successful; false otherwise.
     */
    bool joinDomain(const int& domainID, const std::string& config = "", 
        std::function<void(const ParticipantInfo&)> onAdd = nullptr, 
        std::function<void(const ParticipantInfo&)> onRemove = nullptr);

    /**
     * @brief Enable the DDS domain.
     * @return True if the operation was successful; false otherwise.
     */
    bool enableDomain();

    /**
     * @brief Register new topic in the global data space.
     * @param[in] topicName The name of the topic.
     * @param[in] qosType The QoS type for this topic (STD_QOS::QosType).
     * @return True if the operation was successful; false otherwise.
     */
    template <typename TopicType>
    bool registerTopic(const std::string& topicName, const STD_QOS::QosType qosType);

    /**
     * @brief Register the QoS settings for a topic.
     * @remarks This method is usually only called from register*Topic.
     * @param[in] topicName The name of the topic.
     * @param[in] qosType The QoS type for this topic (STD_QOS::QosType).
     * @return True if the operation was successful; false otherwise.
     */
    bool registerQos(const std::string& topicName, const STD_QOS::QosType qosType);

    /**
     * @brief Unregister all publisher and subscriber objects for a topic.
     * @param[in] topicName The name of the topic.
     * @return True if the operation was successful; false otherwise.
     */
    bool unregisterTopic(const std::string& topicName);

    /**
     * @brief Convenience method to add a topic to a DDS partition.
     * @remarks Call this method before creating a publisher or subscriber.
     * @param[in] topicName The name of the topic.
     * @param[in] partitionName The name of the topic.
     * @return True if the operation was successful; false otherwise.
     */
    bool addPartition(const std::string& topicName,
                      const std::string& partitionName);

    /**
     * @brief Create a new topic subscriber.
     * @param[in] topicName The name of the topic.
     * @param[in] readerName Unique data reader name per topic.
     * @param[in] filter Apply this data filter if not empty.
     * @return True if the operation was successful; false otherwise.
     */
    bool createSubscriber(const std::string& topicName,
                          const std::string& readerName,
                          const std::string& filter = "");

    /**
     * @brief Create a new topic publisher.
     * @param[in] topicName The name of the topic.
     * @return True if the operation was successful; false otherwise.
     */
    bool createPublisher(const std::string& topicName);

    /**
     * @brief Create a new topic publisher/subscriber.
     * @param[in] topicName The name of the topic.
     * @param[in] readerName Unique data reader name per topic.
     * @param[in] filter Apply this data filter if not empty.
     * @return True if the operation was successful; false otherwise.
     */
    bool createPublisherSubscriber(const std::string& topicName,
                                   const std::string& readerName,
                                   const std::string& filter = "");

    /**
     * @brief Read a single data sample for a given topic.
     * @param[out] sample Populate this object from the received sample.
     * @param[in] topicName The name of the topic.
     * @param[in] readerName Unique data reader name.
     * @param[in] filter Take a sample matching this optional filter.
     * @return True if new data was read; false otherwise.
     */
    template <typename TopicType>
    bool takeSample(TopicType& sample,
                    const std::string& topicName,
                    const std::string& readerName,
                    const std::string& filter = "");

    /**
     * @brief Read all data samples for a given topic.
     * @param[out] samples Populate this vector from the received samples.
     * @param[in] topicName The name of the topic.
     * @param[in] readerName Unique data reader name per topic.
     * @param[in] filter Take all samples matching this optional filter.
     * @param[in] readOnly If true, the samples will not be removed from
     *            the data reader after reading.
     * @return True if new data was read; false otherwise.
     */
    template <typename TopicType>
    bool takeAllSamples(std::vector<TopicType>& samples,
                        const std::string& topicName,
                        const std::string& readerName,
                        const std::string& filter = "",
                        const bool& readOnly = false);

    /**
     * @brief Write a data sample for a given topic.
     * @param[in] topicInstance Write this topic instance as a data sample.
     * @param[in] topicName The name of the topic.
     * @return True if new data was written; false otherwise.
     */
    template <typename TopicType>
    bool writeSample(const TopicType& topicInstance,
                     const std::string& topicName);

    /**
     * @brief Dispose of a data sample for a given topic. Useful for transient messages.
     * @param[in] topicInstance Dispose of this topic instance as a data sample.
     * @param[in] topicName The name of the topic.
     * @return True if new data was disposed; false otherwise.
     */
    template <typename TopicType>
    bool disposeSample(const TopicType& topicInstance,
        const std::string& topicName);

    /**
     * @brief Add a data callback to a specified data reader.
     * @param[in] topicName The name of the topic.
     * @param[in] readerName Unique data reader name per topic.
     * @param[in] func std::function which will be callback.
     * @param[in] queueMessages If true, callback methods will only be invoked
     *            when the readCallbacks function is called. When false,
     *            callbacks are invoked immediately after data is received.
     * @return True if the operation was successful; false otherwise.
     */
    template <typename TopicType>
    bool addCallback(const std::string& topicName,
                     const std::string& readerName,
                     std::function<void(const TopicType&)> func,
                     const bool& queueMessages = false,
                     const bool& asyncHandling = false);

    /**
     * @brief Invoke callback methods for each message in the middleware.
     * @remarks This method should only be used if the queueMessages parameter
     *          was set to 'true' when binding a callback.
     * @param[in] topicName The name of the topic.
     * @param[in] readerName Unique data reader name per topic.
     * @return True if the operation was successful; false otherwise.
     */
    bool readCallbacks(const std::string& topicName,
                       const std::string& readerName);

    /**
     * @brief Add a data listener to a specified data reader.
     * @param[in] topicName The name of the topic.
     * @param[in] readerName Unique data reader name per topic.
     * @param[in] listener Attach this listener to the target data reader.
     * @param[in] mask Bind the listener to this status mask. Usually
     *            DDS::STATUS_MASK_ALL or DDS::DATA_AVAILABLE_STATUS.
     */
    void addDataListener(const std::string& topicName,
                         const std::string& readerName,
                         DDS::DataReaderListener_var listener,
                         const int& mask);

    /**
     * @brief Replace the content filter for a given topic.
     * @remarks This method is slow, so it should be used sparingly. Use the
     *          filter parameter in takeSample or takeAllSamples when the
     *          filter must change often.
     * @param[in] topicName The name of the topic.
     * @param[in] readerName Unique data reader name per topic.
     * @param[in] filter Apply this data filter. An empty string will remove an
     *            existing filter.
     * @return True if the operation was successful; false otherwise.
     */
    bool replaceFilter(const std::string& topicName,
                       const std::string& readerName,
                       const std::string& filter = "");

    /**
     * @brief Set the maximum data receive rate for a data reader.
     * @remarks This method must be called after createSubscriber.
     * @param[in] topicName The name of the topic.
     * @param[in] readerName Unique data reader name per topic.
     * @param[in] rate The maximum data receive rate in millisecs.
     * @return True if the operation was successful; false otherwise.
     */
    bool setMaxDataRate(const std::string& topicName,
                        const std::string& readerName,
                        const int& rate);

    /**
     * @brief Return the domain participant object.
     * @return The domain participant object if it was found; otherwise nullptr.
     */
    DDS::DomainParticipant_var getDomainParticipant() const;

    /**
     * @brief Get the topic associated with a topic.
     * @param[in] topicName The name of the topic.
     * @return The topic object if it was found; otherwise nullptr.
     */
    DDS::Topic_var getTopic(const std::string& topicName) const;

    /**
     * @brief Get the most recently created data reader associated with a topic.
     * @param[in] topicName The name of the topic.
     * @param[in] readerName Unique data reader name.
     * @return The data reader object if it was found; otherwise nullptr.
     */
    DDS::DataReader_var getReader(const std::string& topicName,
                               const std::string& readerName) const;

    /**
     * @brief Get the data writer associated with a topic.
     * @param[in] topicName The name of the topic.
     * @return The data writer object if it was found; otherwise nullptr.
     */
    DDS::DataWriter_var getWriter(const std::string& topicName) const;

    /**
     * @brief Get the data publisher associated with a topic.
     * @param[in] topicName The name of the topic.
     * @return The data publisher object if it was found; otherwise nullptr.
     */
    DDS::Publisher_var getPublisher(const std::string& topicName) const;

    /**
     * @brief Get the data subscriber associated with a topic.
     * @param[in] topicName The name of the topic.
     * @return The data subscriber object if it was found; otherwise nullptr.
     */
    DDS::Subscriber_var getSubscriber(const std::string& topicName) const;

    /**
     * @brief Get the topic QoS for a topic.
     * @param[in] topicName The name of the topic.
     * @return The topic QoS or the default QoS if not found.
     */
    DDS::TopicQos getTopicQos(const std::string& topicName) const;

    /**
     * @brief Set a new topic QoS for topic.
     * @remarks This should be called before creating the topic.
     * @param[in] topicName The name of the topic.
     * @param[in] qos Set the QoS to this value.
     */
    void setTopicQos(const std::string& topicName,
                     const DDS::TopicQos& qos);

    /**
     * @brief Get the publisher QoS for a topic.
     * @param[in] topicName The name of the topic.
     * @return The publisher QoS or the default QoS if not found.
     */
    DDS::PublisherQos getPublisherQos(const std::string& topicName) const;

    /**
     * @brief Set a new publisher QoS for a topic.
     * @remarks This should be called before creating the topic.
     * @param[in] topicName The name of the topic.
     * @param[in] qos Set the QoS to this value.
     */
    void setPublisherQos(const std::string& topicName,
                         const DDS::PublisherQos& qos);

    /**
     * @brief Get the subscriber QoS for a topic.
     * @param[in] topicName The name of the topic.
     * @return The subscriber QoS or the default QoS if not found.
     */
    DDS::SubscriberQos getSubscriberQos(const std::string& topicName) const;

    /**
     * @brief Set a new subscriber QoS for a topic.
     * @remarks This should be called before creating the topic.
     * @param[in] topicName The name of the topic.
     * @param[in] qos Set the QoS to this value.
     */
    void setSubscriberQos(const std::string& topicName,
                          const DDS::SubscriberQos& qos);

    /**
     * @brief Get the data writer QoS for a topic.
     * @param[in] topicName The name of the topic.
     * @return The data writer QoS or the default QoS if not found.
     */
    DDS::DataWriterQos getWriterQos(const std::string& topicName) const;

    /**
     * @brief Set a new data writer QoS for a topic.
     * @remarks This should be called before creating the topic.
     * @param[in] topicName The name of the topic.
     * @param[in] qos Set the QoS to this value.
     */
    void setWriterQos(const std::string& topicName,
                      const DDS::DataWriterQos& qos);

    /**
     * @brief Get the data reader QoS for a topic.
     * @param[in] topicName The name of the topic.
     * @return The data reader QoS or the default QoS if not found.
     */
    DDS::DataReaderQos getReaderQos(const std::string& topicName) const;

    /**
     * @brief Set a new data reader QoS for a topic.
     * @remarks This should be called before creating the topic.
     * @param[in] topicName The name of the topic.
     * @param[in] qos Set the QoS to this value.
     */
    void setReaderQos(const std::string& topicName,
                      const DDS::DataReaderQos& qos);

    /**
    * @brief Gets the domain id for the manager.
    * @return The domain id for the manager.
    */
    int GetDomainID() { return m_domainID; }

    /**
     * @brief Returns the human readable string of an error code.
     * @param[in] status Return the error string for this return code.
     */
    static std::string getErrorName(const DDS::ReturnCode_t& status);

    /**
     * @brief Check the return status code for errors.
     * @param[in] status Check the status for this return code.
     * @param[in] info Append this message to std::cerr if an error was found.
     */
    static void checkStatus(const DDS::ReturnCode_t& status, const char* info);

    bool cleanUpTopicsForOneManager();

protected:
    std::function<void(LogMessageType mt, const std::string& message)> m_messageHandler;

private:
    /**
    * @brief Stores all data objects for a topic.
    */
    class DLL_PUBLIC TopicGroup
    {
        public:

        TopicGroup();
        ~TopicGroup();

        CORBA::String_var typeName;
        DDS::DomainParticipant_var domain;
        DDS::Topic_var topic;
        DDS::Publisher_var publisher;
        DDS::Subscriber_var subscriber;
        DDS::DataWriter_var writer;
        DDS::TopicQos topicQos;
        DDS::PublisherQos pubQos;
        DDS::SubscriberQos subQos;
        DDS::DataWriterQos dataWriterQos;
        DDS::DataReaderQos dataReaderQos;

        std::map<const std::string, std::unique_ptr<GenericReaderListener>> m_readerListeners;
        std::unique_ptr<GenericTopicListener> m_listener;
        std::unique_ptr<GenericWriterListener> m_writerListener;

        /// The QoS type for this topic (STD_DOC::QosType).
        int qosPreset;

        /**
        * @brief Stores the filtered topic objects.
        * @details The key is the data reader name and the value is the topic.
        */
        std::map<const std::string, DDS::ContentFilteredTopic_var> filteredTopics;

        /**
        * @brief Stores the data reader objects.
        * @details The key is the data reader name and the value is the reader.
        */
        std::map<const std::string, DDS::DataReader_var> readers;

        /**
        * @brief Stores the callback emitter objects.
        * @details The key is the data reader name and the value is the emitter.
        */
        std::map<const std::string, std::shared_ptr<EmitterBase>> emitters;
    };

    std::shared_ptr<ThreadPool> m_threadPool;

    std::string ddsIP;

    /**
    * @brief Stores all the DDS entity objects managed by this class.
    * @details The key is the topic name and the value contains
    *          all the DDS entity objects associated with a topic.
    */
    std::map<const std::string, std::shared_ptr<TopicGroup> > m_topics;

    /// The DDS domain participation object for this manager.
    DDS::DomainParticipant_var m_domainParticipant;

    int m_domainID;

	bool m_autoConfig;
	bool m_iniCustomization;

    std::string m_config;

    //Thread safety mutex.
#ifndef CANT_SUPPORT_SHARED_MUTEX
    mutable std::shared_mutex m_topicMutex;
    mutable std::shared_lock<decltype(m_topicMutex)> m_sharedLock;
#else
    mutable std::mutex m_topicMutex;
    //No such thing as shared locks. Less efficient. Hopefully no deadlocks!	
    mutable std::unique_lock<decltype(m_topicMutex)> m_sharedLock;
#endif
    mutable std::unique_lock<decltype(m_topicMutex)> m_uniqueLock;

    /**
    * @brief Mutex to prevent multiple threads from trying to initialize transports at the same time
    * @details We keep a map of transport instances, g_transportInstances, so we can make a unique
    *           transport for each participant/manager
    */
    std::mutex m_transportMapMutex;

    std::unique_ptr<ParticipantMonitor> m_monitor;

    DDSReaderListenerStatusHandler *m_rlHandler = nullptr;
    DDSWriterListenerStatusHandler *m_wlHandler = nullptr;

}; // End class DDSManager


/**
 * @brief Get the string for a given enum value.
 * @remarks If we ever have a DDS utility class, this should go in there.
 * @remarks This is typically used when creating filters with enum members.
 * @param[in] enumTypeCode Get the string value from this typecode object.
 * @param[in] enumValue The target enum ordinal value.
 * @return The string for a target enum value or empty if an error was found.
 */
std::string DLL_PUBLIC ddsEnumToString(const CORBA::TypeCode* enumTypeCode,
                                       const unsigned int& enumValue);

/**
 * @brief Return true if the samples are equal; otherwise false.
 * @remarks If we ever have a DDS utility class, this should go in there.
 * @param[in] lhs Left sample in compare.
 * @param[in] rhs Right sample in compare.
 * @return True if the samples are equal; otherwise false.
 */
template <typename TopicType>
bool ddsSampleEquals(const TopicType& lhs, const TopicType& rhs);

/**
 * @brief Initialize a DDS topic to 0.
 * @remarks If we ever have a DDS utility class, this should go in there.
 * @param[out] topicInstance Set all members of this topic instance to 0.
 * @return True if the operation was successful; false otherwise.
 */
template <typename TopicType>
bool ddsInit(TopicType& topicInstance);

void ShutdownDDS();

/**
* @brief Counter of transport instances for each domain since program began
* @details We need to use unique RTPS transports as they are unique participants. So we not only
    need to have unique transports for domain, but each instance of a domain. We possible
    could just use an int here rather than a map, but I'm not sure how shared memory comes into play.
*/
extern std::map<int, int> g_transportInstances;

// Include the template implementation file
#include "dds_manager.hpp"

#endif

/**
 * @}
 */
