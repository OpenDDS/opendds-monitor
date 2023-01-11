#include "dds_manager.h"
#include "dds_listeners.h"
#include "qos_dictionary.h"

#ifdef WIN32
#pragma warning(push, 0)  //No DDS warnings
#endif

#include <tao/AnyTypeCode/TypeCode.h> // For ddsEnumToString
#include <ace/Configuration_Import_Export.h> //for ace config parsing
#include <ace/Init_ACE.h>
#include <dds/DCPS/transport/framework/TransportRegistry.h>
#include <dds/DCPS/transport/rtps_udp/RtpsUdpInst.h>
#include <dds/DCPS/transport/rtps_udp/RtpsUdpInst_rch.h>
#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/RTPS/RtpsDiscovery.h>
#include <dds/DCPS/ServiceEventDispatcher.h>

#ifdef WIN32
#pragma warning(pop)
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <future>

#include "platformIndependent.h"
#include "std_qosC.h"

std::map<int, int> g_transportInstances;

//------------------------------------------------------------------------------
DDSManager::DDSManager(std::function<void(LogMessageType mt, const std::string& message)> messageHandler, int threadPoolSize) :
    m_messageHandler(messageHandler), m_domainParticipant(nullptr), m_autoConfig(false), m_iniCustomization(false)
{

    if (m_messageHandler == nullptr) {
        m_messageHandler = [](LogMessageType mt, const std::string& message) {
            if (mt == LogMessageType::DDS_INFO) {
                std::cout << "DDS Manager: " << message << std::endl;
            }
            else {
                std::cerr << "DDS Manager: " << message << std::endl;
            }
        };
    }

    //Register to get ace messages
    ACE::init();
    m_dispatcher = OpenDDS::DCPS::make_rch<OpenDDS::DCPS::ServiceEventDispatcher>(threadPoolSize);

    QosDictionary::getDataRepresentationType();
    QosDictionary::getTimestampPolicy();

    std::cout << std::endl;

    //m_thisCount++;
}

void DDSManager::SetReaderListenerHandler(DDSReaderListenerStatusHandler* rlHandler)
{
    m_rlHandler = rlHandler;
    for (auto& topicGroup : m_topics) {
        for (auto& rl : topicGroup.second->m_readerListeners) {
            rl.second->SetHandler(m_rlHandler);
        }
    }
}

void DDSManager::SetWriterListenerHandler(DDSWriterListenerStatusHandler* wlHandler)
{
    m_wlHandler = wlHandler;
    for (auto& topicGroup : m_topics) {
        topicGroup.second->m_writerListener->SetHandler(m_wlHandler);
    }
}

bool DDSManager::cleanUpTopicsForOneManager()
{
    decltype(m_uniqueLock) lock(m_topicMutex);
    std::list<std::shared_future<bool>> asyncFutures;

    for (auto& iter : m_topics)
    {
        if (iter.second != nullptr)
        {
            //Deleting topics can take some time if there are subscribers.
            //This really speeds things up if you have a lot of publishers to clean up.
            asyncFutures.push_back(std::async(std::launch::async, &DDSManager::unregisterTopic, this, iter.first));
        }
    }

    lock.unlock();

    bool allClear = true;
    for (const auto& f : asyncFutures)
    {
        f.wait();
        allClear = allClear && f.get();
    }

    lock.lock();
    m_topics.clear();  //This is a fallback, topics should already be cleared by the unregisterTopic() calls.

    return allClear;
}

//------------------------------------------------------------------------------
DDSManager::~DDSManager()
{
    DDS::ReturnCode_t status = DDS::RETCODE_OK;

    cleanUpTopicsForOneManager();
    //if(!allClear)
    //{
    //    std::cout << "cleanUpTopicsForOneManager() returned false." << std::endl;
    //}

    m_messageHandler(LogMessageType::DDS_INFO, "Deleting DDSManagerImpl");

    if (!CORBA::is_nil(m_domainParticipant.in()))
    {
        status = m_domainParticipant->delete_contained_entities();
        checkStatus(status, "DDS::DomainParticipant::delete_contained_entities");
    }

    DDS::DomainParticipantFactory_var dpf = TheParticipantFactory;
    if (!CORBA::is_nil(dpf.in()))
    {
        status = dpf->delete_participant(m_domainParticipant);
        checkStatus(status, "DDS::DomainParticipant::delete_participant");
    }

    m_domainParticipant = nullptr;

    m_dispatcher->shutdown();
    m_dispatcher.reset();

} // End DDSManager::~DDSManager


//------------------------------------------------------------------------------
bool DDSManager::joinDomain(const int& domainID, const std::string& config, std::function<void(const ParticipantInfo&)> onAdd, std::function<void(const ParticipantInfo&)> onRemove)
{
    // If the domain participant has already been instantiated and it's
    // connected to a different domain than the request, report an error
    if (!CORBA::is_nil(m_domainParticipant.in()) &&
        domainID != m_domainParticipant->get_domain_id())
    {
        return false;
    }

    // If the domain participant has already been instantiated, we're done
    if (!CORBA::is_nil(m_domainParticipant.in()))
    {
        return true;
    }

    m_domainID = domainID;
    m_config = config;

    // If the user set the path to the DDS config file, use it
    std::string dds_config_file_str = pi::GetEnvVar("DDS_CONFIG_FILE");
    if (dds_config_file_str.empty())
    {
        // Use the current directory of the executable as the default
        auto ddsConfigPath = pi::GetExecutableDirectory();
        ddsConfigPath /= "opendds.ini";
        if (!std::filesystem::exists(ddsConfigPath)) {
          ddsConfigPath = pi::GetExecutableDirectory();
          ddsConfigPath /= "..";
          ddsConfigPath /= "opendds.ini";
        }
        dds_config_file_str = ddsConfigPath.string();
    }

    std::stringstream sstr;
    sstr << "Joining domain: " << GetDomainID() << " using config file: \"" << dds_config_file_str << "\".";
    m_messageHandler(LogMessageType::DDS_INFO, sstr.str());

    // Make sure we're able to open the config file

    ACE_Configuration_Heap heap;
    if (0 != heap.open()) {
        std::cerr << "Unable to open() configuration heap" << std::endl;
        exit(1);
    }

    ACE_Ini_ImpExp import( heap);
    if (0 != import.import_config( dds_config_file_str.c_str())) {
        std::cerr << "Unable to open " << dds_config_file_str.c_str()<< ". "
                  << "Specify the correct path in DDSManager::joinDomain, "
                  << "set the 'DDS_CONFIG_FILE' environment variable, or "
                  << "copy the DDS configuration file into the working dir."
                  << "\n\nThe application will now exit :("
                  << std::endl;

        // Allow some time for the user to see the message before exit
        std::this_thread::sleep_for(std::chrono::seconds(5));
        exit(1);
    }

    // Process common (no section) data here.
    const ACE_Configuration_Section_Key& root = heap.root_section();
    ACE_Configuration_Section_Key sect;

    if (heap.open_section(root, ACE_TEXT("common"), 0, sect) != 0) {
        m_messageHandler(LogMessageType::DDS_INFO, "Failed to open [common] section of opendds.ini");
    }

    ACE_TString globalTransportString;
    const ACE_TCHAR* CUSTOMIZATION_SECTION_NAME = ACE_TEXT("Customization");
    const ACE_TCHAR* GLOBAL_TRANSPORT_KEY_NAME = ACE_TEXT("DCPSGlobalTransportConfig");
    const ACE_TCHAR* AUTO_CONFIG_SECTION_NAME = ACE_TEXT("auto_config_transport_options");
    const ACE_TCHAR* nak_depth_section = ACE_TEXT("nak_depth");
    const ACE_TCHAR* nak_response_delay_section = ACE_TEXT("nak_response_delay");
    const ACE_TCHAR* optimum_packet_size_section = ACE_TEXT("optimum_packet_size");
    const ACE_TCHAR* queue_initial_pools_section = ACE_TEXT("queue_initial_pools");
    const ACE_TCHAR* queue_messages_per_pool_section = ACE_TEXT("queue_messages_per_pool");
    const ACE_TCHAR* rcv_buffer_size_section = ACE_TEXT("rcv_buffer_size");
    const ACE_TCHAR* send_buffer_size_section = ACE_TEXT("send_buffer_size");
    const ACE_TCHAR* thread_per_connection_section = ACE_TEXT("thread_per_connection");
    const ACE_TCHAR* ttl_section = ACE_TEXT("ttl");


    ACE_Configuration_Section_Key customizationKey;
    if (0 == heap.open_section( root, CUSTOMIZATION_SECTION_NAME, 0, customizationKey))
    {
        m_messageHandler(LogMessageType::DDS_INFO, "Customization section found, setting INI Customization to true.");
        m_iniCustomization = true;
    }
    else if (-1 == heap.get_string_value( sect, GLOBAL_TRANSPORT_KEY_NAME, globalTransportString))
    {
        m_messageHandler(LogMessageType::DDS_INFO, "No global transport found, setting autoConfig");
        m_autoConfig = true;
    }


    // Force loading the opendds.ini configuration file
    int argc = 3;


    char* argv[5] =
    {
        (char*)"dds_manager",
        (char*)"-DCPSConfigFile",
        const_cast<char*>(dds_config_file_str.c_str()),
        (char*)"-DCPSDefaultAddress",
        (char*)"xxx.xxx.xxx.xxx" // Replaced later
    };

    // Force using the specified NIC
    ddsIP = pi::GetEnvVar("DDS_IP");
    if (ddsIP.empty())
    {
        m_messageHandler(LogMessageType::DDS_INFO, "The 'DDS_IP' environment variable was not set.  Using default NIC for DDS.");
    }
    else
    {
        argc = 5;
        argv[4] = &ddsIP[0];
    }


    DDS::DomainParticipantFactory_var domainFactory =
        TheParticipantFactoryWithArgs(argc, argv);

    if (!domainFactory)
    {
        std::cerr << "Error accessing domain factory '"
            << domainID
            << "'. This can happen if you have deleted and recreated a manager."
            << std::endl;

        return false;
    }

#ifdef WIN32
    //Must be done before we get the domain participant.
    if (ddsIP == "127.0.0.1")
    {
        m_messageHandler(LogMessageType::DDS_INFO, "OpenDDS 3.15.0pre10 workaround: Stuffing DDS_IP into multicast_interface for rtps discovery");
        OpenDDS::DCPS::Discovery_rch discovery = TheServiceParticipant->get_discovery(domainID);
        OpenDDS::RTPS::RtpsDiscovery_rch rd = OpenDDS::DCPS::dynamic_rchandle_cast<OpenDDS::RTPS::RtpsDiscovery>(discovery);
        rd->multicast_interface(ddsIP);
    }
#endif

    // Set the default domain QoS
    DDS::DomainParticipantQos domainQos;
    DDS::ReturnCode_t status = domainFactory->get_default_participant_qos(domainQos);
    DDSManager::checkStatus(status, "DDS::DomainParticipant::get_default_participant_qos");

    m_domainParticipant = domainFactory->create_participant(
        domainID,
        domainQos,
        nullptr,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (CORBA::is_nil(m_domainParticipant.in()))
    {
        std::cerr << "Error creating participant for domain '"
            << domainID
            << "'"
            << std::endl;

        return false;
    }

    // Add the monitor only if there is an add or remove participant function for it to call
    if (onAdd || onRemove) {
        m_monitor = std::make_unique<ParticipantMonitor>(m_domainParticipant, onAdd, onRemove);
    }

    if ((m_iniCustomization))
    {
        //if we aren't using fancy auto config logic, we are done.
        return true;
    }

    OpenDDS::DCPS::TransportRegistry* transportReg = TheTransportRegistry;


    //As of DDS 3.13, we can delete managers and rejoin domains within the same program
    //BUT we need to use unique transports as they are unique participants. So we not only
    //need to have unique transports for domain, but each instance of a domain

    //We only want to be creating/initializing a transport one at a time
    std::unique_lock<std::mutex> transportMapLock (m_transportMapMutex);

    // If the transport already exists... (it would have been created below this function first)
    if (g_transportInstances.find(domainID) != g_transportInstances.end()) {
        g_transportInstances[domainID] = g_transportInstances[domainID] + 1;
    }
    else {
        g_transportInstances[domainID] = 1;
    }

    // If the user set a config section of the INI file, use it and we're done
    //NOTE:: This will not implement the RTPS domain segregation logic (for transport only)
    if (!config.empty())
    {

        // Make sure a config with this name exists
        OpenDDS::DCPS::TransportConfig_rch configTest = transportReg->get_config(config);
        if (configTest.is_nil())
        {
            std::cerr << "\nUnable to find the configuration section named '"
                << config
                << "' in the OpenDDS INI file."
                << std::endl;

            return false;
        }

        transportReg->bind_config(config, m_domainParticipant);
        return true;
    }

    // If we got here, we must create a new config for this domain participant
    // which is based off of the default from the INI file. See note #2 in
    // section 7.4.5.5 of the OpenDDS Developers Guide for why this is required.
    // ("RTPS transport instances can not be shared by different Domain Participants.")
    const std::string configName = "config-" + std::to_string(domainID) + "-" + std::to_string(g_transportInstances[domainID]);

    // Use the existing config if it has already been created
    OpenDDS::DCPS::TransportConfig_rch existingConfig = transportReg->get_config(configName);
    if (!existingConfig.is_nil())
    {
        sstr.str(std::string());
        sstr << "Binding transport registry to existing config: " << configName << dds_config_file_str;
        m_messageHandler(LogMessageType::DDS_INFO, sstr.str());
        return true;
    }

    //This simplified version was supplied by OCI. I assume Brenneman has a reason for the more complicated version below
    //const std::string& transportType = "rtps_udp";
    //const std::string transportName =
    //    transportType + "-" + std::to_string(domainID) + "-" + std::to_string(g_transportInstances[domainID]);

    //OpenDDS::DCPS::TransportInst_rch newTransport =
    //    transportReg->create_inst(transportName, transportType);

    //OpenDDS::DCPS::TransportConfig_rch newConfig = transportReg->create_config(transportName);
    //newConfig->instances_.push_back(newTransport);

    // Set the correct port and multicast address to match the RTPS
    // standard. See 9.6.1.3 in the RTPS 2.2 protocol specification.
    const uint16_t PB = 7400;
    const uint16_t DG = 250;
    //const uint16_t PG = 2;
    //const uint16_t D0 = 0;
    //const uint16_t D1 = 10;
    const uint16_t D2 = 1;
    //const uint16_t D3 = 111;
    uint16_t rtpsPort = PB + DG * static_cast<uint16_t>(domainID) + D2;
    //if(!newRtpsTransport->use_multicast_)
    //{
    //    rtpsPort = PB + DG * domainID + D3 + PG * participantId; //what is participant ID? We seem to be doing this with configs instead.
    //}

    OpenDDS::DCPS::TransportConfig_rch newConfig = transportReg->create_config(configName);
    if (m_autoConfig)
    {
        // Loop through all the default transports and add them into a new config
        const std::string transportName =
            "rtps_udp-" + std::to_string(domainID) + "-" + std::to_string(g_transportInstances[domainID]);

        sstr.str(std::string());
        sstr << "Creating a transport named " << transportName;
        m_messageHandler(LogMessageType::DDS_INFO, sstr.str());

        OpenDDS::DCPS::TransportInst_rch newTransport =
            transportReg->create_inst(transportName, "rtps_udp");

        OpenDDS::DCPS::RtpsUdpInst_rch newRtpsTransport =
            OpenDDS::DCPS::static_rchandle_cast
            <OpenDDS::DCPS::RtpsUdpInst>(newTransport);


        //BEGIN CONFIG PARSING
        // Find all of the [auto_config_transport_options] section.
        ACE_Configuration_Section_Key autoconfigKey;
        if (0 == heap.open_section( root, AUTO_CONFIG_SECTION_NAME, 0, autoconfigKey))
        {
            m_messageHandler(LogMessageType::DDS_INFO, "Found autoconfig section");
        }
        ACE_TString argValString;
        if (0 == heap.get_string_value( autoconfigKey, nak_depth_section, argValString))
        {
            sstr.str(std::string());
            sstr << "Found nak_depth: " << argValString;
            m_messageHandler(LogMessageType::DDS_INFO, sstr.str());
            newRtpsTransport->nak_depth_ = ACE_OS::atoi( argValString.c_str());
        }
        if (0 == heap.get_string_value( autoconfigKey, nak_response_delay_section, argValString))
        {
            sstr.str(std::string());
            sstr << "Found nak_response_delay: " <<  argValString << std::endl;
            m_messageHandler(LogMessageType::DDS_INFO, sstr.str());
            newRtpsTransport->nak_response_delay_ = ACE_OS::atoi( argValString.c_str());
        }
        if (0 == heap.get_string_value( autoconfigKey, nak_depth_section, argValString))
        {
            sstr.str(std::string());
            sstr << "Found nak_depth: " <<  argValString << std::endl;
            m_messageHandler(LogMessageType::DDS_INFO, sstr.str());
            newRtpsTransport->nak_depth_ = ACE_OS::atoi( argValString.c_str());
        }
        if (0 == heap.get_string_value( autoconfigKey, optimum_packet_size_section, argValString))
        {
            sstr.str(std::string());
            sstr << "Found optimum packet size: " <<  argValString << std::endl;
            m_messageHandler(LogMessageType::DDS_INFO, sstr.str());
            newRtpsTransport->optimum_packet_size_ = ACE_OS::atoi( argValString.c_str());
        }
        if (0 == heap.get_string_value( autoconfigKey, queue_initial_pools_section, argValString))
        {
            sstr.str(std::string());
            sstr << "Found queue_initial_pools: " <<  argValString << std::endl;
            m_messageHandler(LogMessageType::DDS_INFO, sstr.str());
            newRtpsTransport->queue_initial_pools_ = ACE_OS::atoi( argValString.c_str());
        }
        if (0 == heap.get_string_value( autoconfigKey, queue_messages_per_pool_section, argValString))
        {
            sstr.str(std::string());
            sstr << "Found queue_messages_per_pool: " <<  argValString << std::endl;
            m_messageHandler(LogMessageType::DDS_INFO, sstr.str());
            newRtpsTransport->queue_messages_per_pool_ = ACE_OS::atoi( argValString.c_str());
        }
        if (0 == heap.get_string_value( autoconfigKey, rcv_buffer_size_section, argValString))
        {
            sstr.str(std::string());
            sstr << "Found rcv_buffer_size: " <<  argValString << std::endl;
            m_messageHandler(LogMessageType::DDS_INFO, sstr.str());
            newRtpsTransport->rcv_buffer_size_ = ACE_OS::atoi( argValString.c_str());
        }
        if (0 == heap.get_string_value( autoconfigKey, send_buffer_size_section, argValString))
        {
            sstr.str(std::string());
            sstr << "Found send_buffer_size: " <<  argValString << std::endl;
            m_messageHandler(LogMessageType::DDS_INFO, sstr.str());
            newRtpsTransport->send_buffer_size_ = ACE_OS::atoi( argValString.c_str());
        }
        if (0 == heap.get_string_value( autoconfigKey, thread_per_connection_section, argValString))
        {
            sstr.str(std::string());
            sstr << "Found thread_per_connection: " <<  argValString << std::endl;
            m_messageHandler(LogMessageType::DDS_INFO, sstr.str());
            newRtpsTransport->thread_per_connection_ = ACE_OS::atoi( argValString.c_str());
        }
        if (0 == heap.get_string_value( autoconfigKey, ttl_section, argValString))
        {
            sstr.str(std::string());
            sstr << "Found ttl: " <<  argValString << std::endl;
            m_messageHandler(LogMessageType::DDS_INFO, sstr.str());
            newRtpsTransport->ttl_ = static_cast<unsigned char>(ACE_OS::atoi( argValString.c_str()));
        }
#ifdef WIN32
        if (ddsIP == "127.0.0.1")
        {
            m_messageHandler(LogMessageType::DDS_INFO, "OpenDDS 3.15.0preXX workaround: Stuffing DDS_IP into multicast_interface for rtps transport");
            newRtpsTransport->multicast_interface_ = ddsIP;
        }
#endif



///END CONFIG PARSING

        // newRtpsTransport->datalink_control_chunks_ = defaultRtpsTransport->datalink_control_chunks_;
        // newRtpsTransport->datalink_release_delay_ = defaultRtpsTransport->datalink_release_delay_;
        // newRtpsTransport->durable_data_timeout_ = defaultRtpsTransport->durable_data_timeout_;
        // newRtpsTransport->handshake_timeout_ = defaultRtpsTransport->handshake_timeout_;
        // newRtpsTransport->heartbeat_period_ = defaultRtpsTransport->heartbeat_period_;
        // newRtpsTransport->heartbeat_response_delay_ = defaultRtpsTransport->heartbeat_response_delay_;
        // newRtpsTransport->max_packet_size_ = defaultRtpsTransport->max_packet_size_;
        // newRtpsTransport->max_samples_per_packet_ = defaultRtpsTransport->max_samples_per_packet_;
        // newRtpsTransport->multicast_group_address_ = defaultRtpsTransport->multicast_group_address_;
        // newRtpsTransport->multicast_interface_ = defaultRtpsTransport->multicast_interface_;
        //settings below here have been reimplemented above.
        // newRtpsTransport->nak_depth_ = defaultRtpsTransport->nak_depth_;
        // newRtpsTransport->nak_response_delay_ = defaultRtpsTransport->nak_response_delay_;
        // newRtpsTransport->optimum_packet_size_ = defaultRtpsTransport->optimum_packet_size_;
        // newRtpsTransport->queue_initial_pools_ = defaultRtpsTransport->queue_initial_pools_;
        // newRtpsTransport->queue_messages_per_pool_ = defaultRtpsTransport->queue_messages_per_pool_;
        // newRtpsTransport->rcv_buffer_size_ = defaultRtpsTransport->rcv_buffer_size_;
        // newRtpsTransport->send_buffer_size_ = defaultRtpsTransport->send_buffer_size_;
        // newRtpsTransport->thread_per_connection_ = defaultRtpsTransport->thread_per_connection_;

        std::string multicastAddr = "239.255.2." + std::to_string(domainID);

        // Create a full multicast address + port (239.255.2.X:ABCD). NetworkAddress() is just a wrapper for the ACE_INET crap we were doing before.
        OpenDDS::DCPS::NetworkAddress multicast_addr = OpenDDS::DCPS::NetworkAddress(rtpsPort, multicastAddr.c_str());
        newRtpsTransport->multicast_group_address(multicast_addr);

        newConfig->sorted_insert(newRtpsTransport);
        m_messageHandler(LogMessageType::DDS_INFO, newTransport->dump_to_str());

        static bool alreadySetGlobal = false;
        if (alreadySetGlobal)
        {
            m_messageHandler(LogMessageType::DDS_INFO,"Global config has already been set. Will not set again.");
        }
        else
        {
            m_messageHandler(LogMessageType::DDS_INFO, "Set global config to new config");
            //Set global config to new config
            transportReg->global_config(newConfig);
            alreadySetGlobal = true;
        }
    }

    else
    {
        const OpenDDS::DCPS::TransportConfig_rch globalConfig = transportReg->global_config();
        const size_t transportConfigCount = globalConfig->instances_.size();
        //std::cout << "Num transports existing: " << transportConfigCount << std::endl;
        for (size_t i = 0; i < transportConfigCount; i++)
        {
            const OpenDDS::DCPS::TransportInst_rch transportInstance = globalConfig->instances_[i];
            const std::string& transportType = transportInstance.in()->transport_type_;

            // The default configuration for the rtps_udp transport does not conform
            // to the standard. We will make one which conforms.
            if (transportType == "rtps_udp")
            {
                const std::string transportName =
                    transportType + "-" + std::to_string(domainID) + "-" + std::to_string(g_transportInstances[domainID]);
                sstr.str(std::string());
                sstr << "Creating a transport named " << transportName.c_str() << std::endl;
                m_messageHandler(LogMessageType::DDS_INFO, sstr.str());

                OpenDDS::DCPS::TransportInst_rch newTransport =
                    transportReg->create_inst(transportName, transportType);

                OpenDDS::DCPS::RtpsUdpInst_rch newRtpsTransport =
                    OpenDDS::DCPS::static_rchandle_cast
                    <OpenDDS::DCPS::RtpsUdpInst>(newTransport);

                OpenDDS::DCPS::RtpsUdpInst_rch defaultRtpsTransport =
                    OpenDDS::DCPS::static_rchandle_cast
                    <OpenDDS::DCPS::RtpsUdpInst>(transportInstance);

                // Use settings from the config file as a starting point
                newRtpsTransport->anticipated_fragments_ = defaultRtpsTransport->anticipated_fragments_;
                newRtpsTransport->datalink_control_chunks_ = defaultRtpsTransport->datalink_control_chunks_;
                newRtpsTransport->datalink_release_delay_ = defaultRtpsTransport->datalink_release_delay_;
                newRtpsTransport->fragment_reassembly_timeout_ = defaultRtpsTransport->fragment_reassembly_timeout_;

                //newRtpsTransport->handshake_timeout_ = defaultRtpsTransport->handshake_timeout_;
                newRtpsTransport->heartbeat_period_ = defaultRtpsTransport->heartbeat_period_;
                //newRtpsTransport->heartbeat_response_delay_ = defaultRtpsTransport->heartbeat_response_delay_; //removed in 3.19
                newRtpsTransport->max_message_size_ = defaultRtpsTransport->max_message_size_;
                newRtpsTransport->max_packet_size_ = defaultRtpsTransport->max_packet_size_;
                newRtpsTransport->max_samples_per_packet_ = defaultRtpsTransport->max_samples_per_packet_;
                newRtpsTransport->multicast_group_address(defaultRtpsTransport->multicast_group_address());
                newRtpsTransport->multicast_interface_ = (defaultRtpsTransport->multicast_interface_);
                newRtpsTransport->nak_depth_ = defaultRtpsTransport->nak_depth_;
                newRtpsTransport->nak_response_delay_ = defaultRtpsTransport->nak_response_delay_;
                newRtpsTransport->optimum_packet_size_ = defaultRtpsTransport->optimum_packet_size_;
                newRtpsTransport->queue_initial_pools_ = defaultRtpsTransport->queue_initial_pools_;
                newRtpsTransport->queue_messages_per_pool_ = defaultRtpsTransport->queue_messages_per_pool_;
                newRtpsTransport->rcv_buffer_size_ = defaultRtpsTransport->rcv_buffer_size_;
                newRtpsTransport->receive_address_duration_ = defaultRtpsTransport->receive_address_duration_;
                newRtpsTransport->responsive_mode_ = defaultRtpsTransport->responsive_mode_;
                newRtpsTransport->send_buffer_size_ = defaultRtpsTransport->send_buffer_size_;
                newRtpsTransport->send_delay_ = defaultRtpsTransport->send_delay_;
                newRtpsTransport->thread_per_connection_ = defaultRtpsTransport->thread_per_connection_;
                newRtpsTransport->ttl_ = defaultRtpsTransport->ttl_;
                newRtpsTransport->use_multicast_ = defaultRtpsTransport->use_multicast_;

                OpenDDS::DCPS::NetworkAddress rtps_multicast_addr = OpenDDS::DCPS::NetworkAddress(rtpsPort, "239.255.0.2");
                newRtpsTransport->multicast_group_address(rtps_multicast_addr);
                newConfig->sorted_insert(newRtpsTransport);
                //std::cout << "Transport config: " << std::endl;
                //std::cout << newTransport->dump_to_str() << std::endl;
            }
            else // Not rtps_udp transport, so just add the existing config
            {
                //std::cout << "Not rtps_udp transport, so adding existing config" << std::endl;
                newConfig->sorted_insert(transportInstance);
            }

        } // End global config transport loop
    } //end if auto config

    // Force this domain participant to use the new config
    transportReg->bind_config(newConfig, m_domainParticipant);

    return true;

} // End DDSManager::joinDomain

//------------------------------------------------------------------------------
bool DDSManager::enableDomain()
{
    if (CORBA::is_nil(m_domainParticipant.in()))
    {
        return false;
    }

    DDS::ReturnCode_t status = m_domainParticipant->enable();
    return (status == DDS::RETCODE_OK);
}


//------------------------------------------------------------------------------
bool DDSManager::registerQos(const std::string& topicName, const STD_QOS::QosType qosType)
{
    decltype(m_sharedLock) lock(m_topicMutex);

    // Make sure the topic has been created
    if (m_topics.find(topicName) == m_topics.end())
    {
        std::cerr << "Unable to register the QoS for "
            << topicName
            << ". The topic has not been created"
            << std::endl;

        return false;
    }

    std::shared_ptr<TopicGroup> topicGroup = m_topics[topicName];
    lock.unlock();

    // If the QoS is already registered, we're done
    if (topicGroup->qosPreset != -1)
    {
        return true;
    }

    // Apply the QoS preset (referencing std_qos.idl)
    switch (qosType)
    {
        case STD_QOS::QosType::LATEST_RELIABLE_TRANSIENT:
            setTopicQos(topicName, QosDictionary::Topic::latestReliableTransient());
            setReaderQos(topicName, QosDictionary::DataReader::latestReliableTransient());
            setWriterQos(topicName, QosDictionary::DataWriter::latestReliableTransient());
            break;
        case STD_QOS::QosType::LATEST_RELIABLE:
            setTopicQos(topicName, QosDictionary::Topic::latestReliable());
            setReaderQos(topicName, QosDictionary::DataReader::latestReliable());
            setWriterQos(topicName, QosDictionary::DataWriter::latestReliable());
            break;
        case STD_QOS::QosType::STRICT_RELIABLE:
            setTopicQos(topicName, QosDictionary::Topic::strictReliable());
            setReaderQos(topicName, QosDictionary::DataReader::strictReliable());
            setWriterQos(topicName, QosDictionary::DataWriter::strictReliable());
            break;
        case STD_QOS::QosType::BEST_EFFORT:
            setTopicQos(topicName, QosDictionary::Topic::bestEffort());
            setReaderQos(topicName, QosDictionary::DataReader::bestEffort());
            setWriterQos(topicName, QosDictionary::DataWriter::bestEffort());
            break;
        default:
            std::cerr << "Invalid QoS type of '"
                << qosType
                << "' for "
                << topicName
                << std::endl;

            return false;
            break;
    }

    lock.lock();
    topicGroup->qosPreset = qosType;

    return true;

} // End DDSManager::registerQos


//------------------------------------------------------------------------------
bool DDSManager::unregisterTopic(const std::string& topicName)
{
    decltype(m_uniqueLock) lock_unique(m_topicMutex);

    // Make sure this topic has been registered
    auto iter = m_topics.find(topicName);
    if (iter == m_topics.end() ||
        m_topics[topicName] == nullptr)
    {
        return false;
    }

    //We have to delete outside of a mutex lock so we don't deadlock with OpenDDS mutexes
    //Save a pointer so when we erase from m_topics, the topic won't get deleted
    auto savePtrToDelete = iter->second;

    //Erase from map within a unique_lock, so we are thread safe
    m_topics.erase(m_topics.find(topicName));
    lock_unique.unlock();

    //Now when we return, we will delete the savePtrToDelete, which will delete the topic outside of our map mutex
    return true;

    //m_topicCounts[topicName] = m_topicCounts[topicName] - count;

    //if (m_topicCounts[topicName] <= 0)
    //{
    //    //delete m_topics[topicName];
    //    m_topics.erase(m_topics.find(topicName));
    //    m_topicCounts.erase(m_topicCounts.find(topicName));
    //}

    //return true;
}


//------------------------------------------------------------------------------
bool DDSManager::addPartition(const std::string& topicName,
                              const std::string& partitionName)
{
    decltype(m_sharedLock) lock(m_topicMutex);

    // Make sure this topic has been registered
    if (m_topics.find(topicName) == m_topics.end() ||
        m_topics[topicName] == nullptr)
    {
        std::cerr << "Error adding a partition to '"
            << topicName
            << "'. The topic has not been registered."
            << std::endl;

        return false;
    }

    std::shared_ptr<TopicGroup> topicGroup = m_topics[topicName];
    int partitionCount = 0;
    CORBA::String_var partitionNameVar = partitionName.c_str();

    // Add the new partition to the subscriber QoS
    partitionCount = topicGroup->subQos.partition.name.length();
    topicGroup->subQos.partition.name.length(partitionCount + 1);
    topicGroup->subQos.partition.name[partitionCount] = partitionNameVar;

    // Add the new partition to the publisher QoS
    partitionCount = topicGroup->pubQos.partition.name.length();
    topicGroup->pubQos.partition.name.length(partitionCount + 1);
    topicGroup->pubQos.partition.name[partitionCount] = partitionNameVar;

    return true;
}


//------------------------------------------------------------------------------
bool DDSManager::createSubscriber(const std::string& topicName,
    const std::string& readerName,
    const std::string& filter)
{
    // Make sure the data reader name is valid
    if (readerName.empty())
    {
        std::cerr << "Error creating subscriber for '"
            << topicName
            << "'. The reader name must not be empty."
            << std::endl;

        return false;
    }

    decltype(m_sharedLock) lock(m_topicMutex);

    // Make sure this topic has been registered
    if (m_topics.find(topicName) == m_topics.end() ||
        m_topics[topicName] == nullptr ||
        m_topics[topicName]->topic == nullptr)
    {
        std::cerr << "Error creating subscriber for '"
            << topicName
            << "'. The topic has not been registered."
            << std::endl;

        return false;
    }

    std::shared_ptr<TopicGroup> topicGroup = m_topics[topicName];

    if (topicGroup->m_readerListeners.find(readerName) != topicGroup->m_readerListeners.end()) {
        std::cerr << "Error in createSubscriber:  Reader listener '" << readerName
            << "' already registered for topic '"
            << topicName
            << "'." << std::endl;
        return false;
    }

    // Create the subscriber if we don't already have one
    if (!topicGroup->subscriber)
    {
        topicGroup->subscriber = m_domainParticipant->create_subscriber(
            topicGroup->subQos,
            nullptr,
            OpenDDS::DCPS::NO_STATUS_MASK);

        if (!topicGroup->subscriber)
        {
            std::cerr << "Error creating subscriber for '"
                << topicName
                << "'"
                << std::endl;

            return false;
        }
    }

    auto readerListener = std::make_unique<GenericReaderListener>();
    readerListener->SetHandler(m_rlHandler);
    DDS::DataReader_var reader;

    // Create a new filtered topic if requested
    if (!filter.empty())
    {
        const DDS::StringSeq noParams;
        const std::string filterName = topicName + "_" + readerName;
        DDS::ContentFilteredTopic_var filteredTopic =
            m_domainParticipant->create_contentfilteredtopic(
                filterName.c_str(),
                topicGroup->topic,
                filter.c_str(),
                noParams);

        if (!filteredTopic)
        {
            std::cerr << "Error creating new content filtered topic '"
                << topicName
                << "' with the filter ["
                << filter
                << "]"
                << std::endl;

            return false;
        }

        topicGroup->filteredTopics[filterName] = filteredTopic;

        reader = topicGroup->subscriber->create_datareader(
            filteredTopic,
            topicGroup->dataReaderQos,
            readerListener.get(),
            DDS::INCONSISTENT_TOPIC_STATUS |
            DDS::REQUESTED_INCOMPATIBLE_QOS_STATUS |
            DDS::SUBSCRIPTION_MATCHED_STATUS |
            DDS::SAMPLE_LOST_STATUS);
    }
    else
    {
        reader = topicGroup->subscriber->create_datareader(
            topicGroup->topic,
            topicGroup->dataReaderQos,
            readerListener.get(),
            DDS::INCONSISTENT_TOPIC_STATUS |
            DDS::REQUESTED_INCOMPATIBLE_QOS_STATUS |
            DDS::SUBSCRIPTION_MATCHED_STATUS |
            DDS::SAMPLE_LOST_STATUS);
    }

    if (!reader)
    {
        std::cerr << "Error creating data reader for '"
            << topicName
            << "'"
            << std::endl;
        return false;
    }


    // Store the data reader with the reference name
    topicGroup->readers[readerName] = reader;
    topicGroup->m_readerListeners.emplace(readerName, std::move(readerListener));

    return true;

} // End DDSManager::createSubscriber


//------------------------------------------------------------------------------
bool DDSManager::createPublisher(const std::string& topicName)
{
    decltype(m_uniqueLock) lock(m_topicMutex);

    // Make sure this topic has been registered
    if (m_topics.find(topicName) == m_topics.end() ||
        m_topics[topicName] == nullptr ||
        m_topics[topicName]->topic == nullptr)
    {
        std::cerr << "Error creating publisher for '"
            << topicName
            << "'. The topic has not been registered."
            << std::endl;

        return false;
    }

    std::shared_ptr<TopicGroup> topicGroup = m_topics[topicName];

    // Create the publisher if one does not already exist
    if (!topicGroup->publisher) {
        topicGroup->publisher = m_domainParticipant->create_publisher(
            topicGroup->pubQos,
            nullptr,
            OpenDDS::DCPS::NO_STATUS_MASK);

        if (!topicGroup->publisher)
        {
            std::cerr << "Error creating publisher for '"
                << topicName
                << "'"
                << std::endl;

            return false;
        }


        // Create the data writer
        auto writerListener = std::make_unique<GenericWriterListener>();
        topicGroup->writer = topicGroup->publisher->create_datawriter(
            topicGroup->topic,
            topicGroup->dataWriterQos,
            writerListener.get(),
            DDS::INCONSISTENT_TOPIC_STATUS |
            DDS::OFFERED_INCOMPATIBLE_QOS_STATUS |
            DDS::SAMPLE_LOST_STATUS |
            DDS::SAMPLE_REJECTED_STATUS |
            DDS::PUBLICATION_MATCHED_STATUS);
        writerListener->SetHandler(m_wlHandler);

        if (!topicGroup->writer)
        {
            std::cerr << "Error creating data writer for '"
                << topicName
                << "'"
                << std::endl;

            return false;
        }

        topicGroup->m_writerListener = std::move(writerListener);
    }

    //std::cout << "Successfully created writer for topic '"
    //    << topicName
    //    << "' for handle: "
    //    << topicGroup->topic->get_instance_handle()
    //    << " with writer handle: "
    //    << topicGroup->writer->get_instance_handle()
    //    << std::endl;

    return true;

} // End DDSManager::createPublisher


//------------------------------------------------------------------------------
bool DDSManager::createPublisherSubscriber(const std::string& topicName,
                                           const std::string& readerName,
                                           const std::string& filter)
{
    // TODO: Remove this function (No longer used).
    bool pass = false;

    pass = createPublisher(topicName);
    if (!pass)
    {
        return false;
    }

    pass = createSubscriber(topicName, readerName, filter);
    if (!pass)
    {
        return false;
    }

    return true;
}


//------------------------------------------------------------------------------
bool DDSManager::readCallbacks(const std::string& topicName,
                               const std::string& readerName)
{
    decltype(m_sharedLock) lock(m_topicMutex);

    // Make sure the data reader name is valid
    if (readerName.empty())
    {
        std::cerr << "Error reading callback data for '"
            << topicName
            << "'. The reader name must not be empty."
            << std::endl;

        return false;
    }

    auto iter = m_topics.find(topicName);

    if (iter == m_topics.end())
    {
        return false;
    }

    std::shared_ptr<TopicGroup> topicGroup = iter->second;
    if (!topicGroup)
    {
        return false;
    }

    if (topicGroup->emitters.find(readerName) == topicGroup->emitters.end())
    {
        return false;
    }

    auto& emitter = topicGroup->emitters[readerName];
    if (!emitter)
    {
        return false;
    }

    emitter->readQueue();

    return true;
}


//------------------------------------------------------------------------------
void DDSManager::addDataListener(const std::string& topicName,
                                 const std::string& readerName,
                                 DDS::DataReaderListener_var listener,
                                 const int& mask)
{
    // Make sure the data reader name is valid
    if (readerName.empty())
    {
        std::cerr << "Error adding listener for '"
            << topicName
            << "'. The reader name must not be empty."
            << std::endl;

        return;
    }

    DDS::DataReader_var reader = getReader(topicName, readerName);
    if (!reader)
    {
        std::cerr << "No data reader available for "
            << topicName
            << std::endl;

        return;
    }

    reader->set_listener(listener, mask);

} // End DDSManager::addDataListener


//------------------------------------------------------------------------------
bool DDSManager::replaceFilter(const std::string& topicName,
                               const std::string& readerName,
                               const std::string& filter)
{
    // Make sure the data reader name is valid
    if (readerName.empty())
    {
        std::cerr << "Error replacing topic filter for '"
            << topicName
            << "'. The reader name must not be empty."
            << std::endl;

        return false;
    }

    // Has the subscriber already been registered?
    DDS::Subscriber_var subscriber = getSubscriber(topicName);
    if (!subscriber)
    {
        std::cerr << "Error replacing topic filter for '"
            << topicName
            << "'. The subscriber has not been created."
            << std::endl;

        return false;
    }

    // Make sure this data reader exists
    DDS::DataReader_var dataReader = getReader(topicName, readerName);
    if (!dataReader)
    {
        std::cerr << "Error replacing topic filter for '"
            << topicName
            << "'. The data reader named '"
            << readerName
            << "' does not exist."
            << std::endl;

        return false;
    }

    decltype(m_sharedLock) lock(m_topicMutex);
    std::shared_ptr<TopicGroup> topicGroup = m_topics[topicName];

    if (topicGroup->m_readerListeners.find(readerName) != topicGroup->m_readerListeners.end()) {
        std::cerr << "Error in replaceFilter:  Reader listener '" << readerName
            << "' already registered for topic '"
            << topicName
            << "'." << std::endl;
        return false;
    }

    // Stop the emitter if it exists
    EmitterBase* emitter = nullptr;
    if (topicGroup->emitters.find(readerName) != topicGroup->emitters.end())
    {
        emitter = topicGroup->emitters[readerName].get();
        if (emitter->isRunning())
        {
            emitter->stop();
        }
    }


    DDS::ContentFilteredTopic* topicDesc =
        dynamic_cast<DDS::ContentFilteredTopic*>
        (dataReader->get_topicdescription());

    // Remove this content filtered topic from the domain if it exists
    if (topicDesc && m_domainParticipant)
    {
        for (auto iter = topicGroup->filteredTopics.begin();
            iter != topicGroup->filteredTopics.end();
            ++iter)
        {
            if (topicDesc != iter->second)
            {
                continue;
            }

            m_domainParticipant->delete_contentfilteredtopic(topicDesc);
            iter->second = nullptr;
            topicDesc = nullptr;
            topicGroup->filteredTopics.erase(iter);
            break;
        }
    }


    // We have to destroy the current data reader before building a new one
    dataReader->delete_contained_entities();
    subscriber->delete_datareader(dataReader);
    DDS::TopicDescription* targetTopic = nullptr;


    // Create a new filtered topic if requested
    if (!filter.empty())
    {
        // The topic filter name must be unique or it will fail on the
        // second time it's created
        static int counter = 0;
        ++counter;
        const std::string filterName =
            topicName + "_" +
            readerName + "_" +
            std::to_string(counter);

        const DDS::StringSeq noParams;
        DDS::ContentFilteredTopic_var filteredTopic =
            m_domainParticipant->create_contentfilteredtopic(
                filterName.c_str(),
                topicGroup->topic,
                filter.c_str(),
                noParams);

        if (!filteredTopic)
        {
            std::cerr << "Error updating content filtered topic '"
                << topicName
                << "' with the filter ["
                << filter
                << "]"
                << std::endl;

            return false;
        }

        topicGroup->filteredTopics[filterName] = filteredTopic;
        targetTopic = filteredTopic;
    }


    // If we're not creating a filtered topic, use the non-filtered topic
    if (!targetTopic && topicGroup->topic)
    {
        targetTopic = topicGroup->topic;
    }

    // Create the new data reader
    auto readerListener = std::make_unique<GenericReaderListener>();
    readerListener->SetHandler(m_rlHandler);
    dataReader = topicGroup->subscriber->create_datareader(
        targetTopic,
        topicGroup->dataReaderQos,
        readerListener.get(),
        DDS::REQUESTED_INCOMPATIBLE_QOS_STATUS |
        DDS::SUBSCRIPTION_MATCHED_STATUS |
        DDS::SAMPLE_LOST_STATUS);

    if (!dataReader)
    {
        std::cerr << "Error creating data reader for '"
            << topicName
            << "'"
            << std::endl;

        return false;
    }

    // Store the data reader with the reference name
    topicGroup->readers[readerName] = dataReader;
    topicGroup->m_readerListeners.emplace(readerName, std::move(readerListener));
    lock.unlock();

    // Restart the emitter thread with the new reader if it existed
    if (emitter != nullptr)
    {
        emitter->setReader(dataReader);
        emitter->run();
    }

    return true;

} // End DDSManager::replaceFilter


//------------------------------------------------------------------------------
bool DDSManager::setMaxDataRate(const std::string& topicName,
                                const std::string& readerName,
                                const int& rate)
{
    if (rate < 1)
    {
        std::cerr << "Invalid data receive rate of '"
            << rate
            << "' for the topic '"
            << topicName
            << "' data reader name '"
            << readerName
            << "'"
            << std::endl;

        return false;
    }

    // Make sure the data reader was created
    DDS::DataReader_var reader = getReader(topicName, readerName);
    if (!reader)
    {
        std::cerr << "Error setting the max data receive rate for the topic '"
            << topicName
            << "' with the data reader named '"
            << readerName
            << "'. The topic subscriber '"
            << readerName
            << "' has not been created."
            << std::endl;

        return false;
    }

    // Create a time based filter for this data reader
    DDS::DataReaderQos qos = getReaderQos(topicName);
    qos.time_based_filter.minimum_separation.sec = DDS::DURATION_ZERO_SEC;
    qos.time_based_filter.minimum_separation.nanosec = rate * 1000000; // ms to ns

    // Apply the time based filter ONLY to the specified data reader
    reader->set_qos(qos);

    return true;

} // End DDSManager::setMaxDataRate


//------------------------------------------------------------------------------
DDS::DomainParticipant_var DDSManager::getDomainParticipant() const
{
    return m_domainParticipant;
}


//------------------------------------------------------------------------------
DDS::Topic_var DDSManager::getTopic(const std::string& topicName) const
{
    decltype(m_sharedLock) lock(m_topicMutex);
    auto iter = m_topics.find(topicName);

    if (iter == m_topics.end())
    {
        return nullptr;
    }

    return iter->second->topic;
}


//------------------------------------------------------------------------------
DDS::DataReader_var DDSManager::getReader(const std::string& topicName,
                                       const std::string& readerName) const
{
    if (readerName.empty())
    {
        return nullptr;
    }

    decltype(m_sharedLock) lock(m_topicMutex);

    auto iter = m_topics.find(topicName);

    if (iter == m_topics.end())
    {
        return nullptr;
    }

    if (iter->second == nullptr)
    {
        return nullptr;
    }

    if (iter->second->readers.empty())
    {
        return nullptr;
    }

    if (iter->second->readers.find(readerName) != iter->second->readers.end())
    {
        return iter->second->readers[readerName];
    }

    return nullptr;
}


//------------------------------------------------------------------------------
DDS::DataWriter_var DDSManager::getWriter(const std::string& topicName) const
{
    decltype(m_sharedLock) lock(m_topicMutex);
    auto iter = m_topics.find(topicName);

    if (iter == m_topics.end())
    {
        return nullptr;
    }

    if (iter->second == nullptr)
    {
        return nullptr;
    }

    //std::cout << "Successfully found writer for topic '"
    //    << topicName
    //    << "' for handle: "
    //    << iter->second->topic->get_instance_handle()
    //    << " with writer handle: "
    //    << iter->second->writer->get_instance_handle()
    //    << std::endl;


    return iter->second->writer;
}


//------------------------------------------------------------------------------
DDS::Publisher_var DDSManager::getPublisher(const std::string& topicName) const
{
    decltype(m_sharedLock) lock(m_topicMutex);
    auto iter = m_topics.find(topicName);

    if (iter == m_topics.end())
    {
        return nullptr;
    }

    return iter->second->publisher;
}


//------------------------------------------------------------------------------
DDS::Subscriber_var DDSManager::getSubscriber(const std::string& topicName) const
{
    decltype(m_sharedLock) lock(m_topicMutex);
    auto iter = m_topics.find(topicName);

    if (iter == m_topics.end())
    {
        return nullptr;
    }

    return iter->second->subscriber;
}


//------------------------------------------------------------------------------
DDS::TopicQos DDSManager::getTopicQos(const std::string& topicName) const
{
    decltype(m_sharedLock) lock(m_topicMutex);
    auto iter = m_topics.find(topicName);

    if (iter != m_topics.end())
    {
        return iter->second->topicQos;
    }

    return QosDictionary::Topic::latestReliableTransient();
}


//------------------------------------------------------------------------------
void DDSManager::setTopicQos(const std::string& topicName,
                             const DDS::TopicQos& qos)
{
    decltype(m_sharedLock) lock(m_topicMutex);
    if (m_topics.find(topicName) == m_topics.end())
    {
        m_topics[topicName] = std::make_shared<TopicGroup>();
    }

    m_topics[topicName]->topicQos = qos;
}


//------------------------------------------------------------------------------
DDS::PublisherQos DDSManager::getPublisherQos(const std::string& topicName) const
{
    decltype(m_sharedLock) lock(m_topicMutex);
    auto iter = m_topics.find(topicName);

    if (iter != m_topics.end())
    {
        return iter->second->pubQos;
    }

    return QosDictionary::Publisher::defaultQos();
}


//------------------------------------------------------------------------------
void DDSManager::setPublisherQos(const std::string& topicName,
                                 const DDS::PublisherQos& qos)
{
    decltype(m_sharedLock) lock(m_topicMutex);
    if (m_topics.find(topicName) == m_topics.end())
    {
        m_topics[topicName] = std::make_shared<TopicGroup>();
    }

    if (m_topics[topicName]->publisher)
    {
        m_topics[topicName]->publisher->set_qos(qos);
    }

    m_topics[topicName]->pubQos = qos;
}


//------------------------------------------------------------------------------
DDS::SubscriberQos DDSManager::getSubscriberQos(const std::string& topicName) const
{
    decltype(m_sharedLock) lock(m_topicMutex);
    auto iter = m_topics.find(topicName);

    if (iter != m_topics.end())
    {
        return iter->second->subQos;
    }

    return QosDictionary::Subscriber::defaultQos();
}


//------------------------------------------------------------------------------
void DDSManager::setSubscriberQos(const std::string& topicName,
                                  const DDS::SubscriberQos& qos)
{
    decltype(m_sharedLock) lock(m_topicMutex);
    if (m_topics.find(topicName) == m_topics.end())
    {
        m_topics[topicName] = std::make_shared<TopicGroup>();
    }

    if (m_topics[topicName]->subscriber)
    {
        m_topics[topicName]->subscriber->set_qos(qos);
    }

    m_topics[topicName]->subQos = qos;
}


//------------------------------------------------------------------------------
DDS::DataWriterQos DDSManager::getWriterQos(const std::string& topicName) const
{
    decltype(m_sharedLock) lock(m_topicMutex);
    auto iter = m_topics.find(topicName);

    if (iter != m_topics.end())
    {
        return iter->second->dataWriterQos;
    }

    return QosDictionary::DataWriter::latestReliableTransient();
}


//------------------------------------------------------------------------------
void DDSManager::setWriterQos(const std::string& topicName,
                              const DDS::DataWriterQos& qos)
{
    decltype(m_sharedLock) lock(m_topicMutex);
    if (m_topics.find(topicName) == m_topics.end())
    {
        m_topics[topicName] = std::make_shared<TopicGroup>();
    }

    if (m_topics[topicName]->writer)
    {
        m_topics[topicName]->writer->set_qos(qos);
    }

    m_topics[topicName]->dataWriterQos = qos;
}


//------------------------------------------------------------------------------
DDS::DataReaderQos DDSManager::getReaderQos(const std::string& topicName) const
{
    decltype(m_sharedLock) lock(m_topicMutex);
    auto iter = m_topics.find(topicName);

    if (iter != m_topics.end())
    {
        return iter->second->dataReaderQos;
    }

    return QosDictionary::DataReader::latestReliableTransient();
}


//------------------------------------------------------------------------------
void DDSManager::setReaderQos(const std::string& topicName,
                              const DDS::DataReaderQos& qos)
{
    decltype(m_sharedLock) lock(m_topicMutex);
    if (m_topics.find(topicName) == m_topics.end())
    {
        m_topics[topicName] = std::make_shared<TopicGroup>();
    }

    for (auto iter = m_topics[topicName]->readers.begin();
        iter != m_topics[topicName]->readers.end();
        ++iter)
    {
        iter->second->set_qos(qos);
    }

    m_topics[topicName]->dataReaderQos = qos;
}


//------------------------------------------------------------------------------
DDSManager::TopicGroup::TopicGroup() :
    domain(nullptr),
    topic(nullptr),
    publisher(nullptr),
    subscriber(nullptr),
    writer(nullptr),
    qosPreset(-1)
{
    topicQos = QosDictionary::Topic::latestReliableTransient();
    dataReaderQos = QosDictionary::DataReader::latestReliableTransient();
    dataWriterQos = QosDictionary::DataWriter::latestReliableTransient();
    pubQos = QosDictionary::Publisher::defaultQos();
    subQos = QosDictionary::Subscriber::defaultQos();
}

//------------------------------------------------------------------------------
DDSManager::TopicGroup::~TopicGroup()
{
    int tempRet;
    if (subscriber && !readers.empty())
    {
        for (auto iter = readers.begin(); iter != readers.end(); ++iter)
        {
            tempRet = subscriber->delete_datareader(iter->second);
            if (tempRet != DDS::RETCODE_OK) {
                std::cerr << "Error in delete_datareader: "
                    << iter->first
                    << " : "
                    << getErrorName(tempRet)
                    << std::endl;
            }
            iter->second = DDS::DataReader::_nil();
        }
        readers.clear();
    }

    if (publisher && writer)
    {
        tempRet = publisher->delete_datawriter(writer);
        if (tempRet != DDS::RETCODE_OK) {
            std::cerr << "Error in delete_datawriter "
                << topic->get_name() << " : "
                << getErrorName(tempRet) << std::endl;
        }
        writer = nullptr;
    }

    if (domain && publisher)
    {
        tempRet = domain->delete_publisher(publisher);
        if (tempRet != DDS::RETCODE_OK) {
            std::cerr << "Error in delete_publisher "
                << topic->get_name() << " : "
                << getErrorName(tempRet) << std::endl;
        }
        publisher = nullptr;
    }

    if (domain && subscriber)
    {
        tempRet = domain->delete_subscriber(subscriber);
        if (tempRet != DDS::RETCODE_OK) {
            std::cerr << "Error in delete_subscriber "
                << topic->get_name() << " : "
                << getErrorName(tempRet) << std::endl;
        }
        subscriber = nullptr;
    }

    //Moved this up because I'm not sure if it was causing delete_contentfilteredtopic and delete_topic
    //to fail sometimes. -MM
    for (auto& emiter : emitters)
    {
        emiter.second->stop();
    }
    emitters.clear();

    if (domain && !filteredTopics.empty())
    {
        for (auto& iter : filteredTopics)
        {
            tempRet = domain->delete_contentfilteredtopic(iter.second);
            if (tempRet != DDS::RETCODE_OK) {
                //if (tempRet == DDS::RETCODE_PRECONDITION_NOT_MET) {
                //    std::cerr << "ERROR: delete_contentfilteredtopic failed: " << iter.first << std::endl <<
                //        "RETCODE_PRECONDITION_NOT_MET returned when a datareader still exists" << std::endl;
                //}
                //else {
                    std::cerr << "Error in delete_contentfilteredtopic: "
                        << iter.first << " return value: " << getErrorName(tempRet)
                        << std::endl;
                //}
            }
            else {
                iter.second = nullptr;
            }
        }
        filteredTopics.clear();
    }

    if (domain && topic)
    {
        tempRet = domain->delete_topic(topic);
        if (tempRet != DDS::RETCODE_OK) {
            std::cerr << "Error in delete_topic: "
                << topic->get_name() << " : "
                << getErrorName(tempRet) << std::endl;
        }
        topic = nullptr;
    }

    // Don't delete the domain. We don't own it.
    domain = nullptr;

} // End DDSManager::TopicGroup::~TopicGroup

//------------------------------------------------------------------------------
std::string DDSManager::getErrorName(const DDS::ReturnCode_t& status)
{
    switch (status)
    {
    case DDS::RETCODE_OK: return "OK";
    case DDS::RETCODE_ERROR: return "ERROR";
    case DDS::RETCODE_UNSUPPORTED: return "UNSUPPORTED";
    case DDS::RETCODE_BAD_PARAMETER: return "BAD PARAMETER";
    case DDS::RETCODE_PRECONDITION_NOT_MET: return "PRECONDITION NOT MET";
    case DDS::RETCODE_OUT_OF_RESOURCES: return "OUT OF RESOURCES";
    case DDS::RETCODE_NOT_ENABLED: return "NOT ENABLED";
    case DDS::RETCODE_IMMUTABLE_POLICY: return "IMMUTABLE POLICY";
    case DDS::RETCODE_INCONSISTENT_POLICY: return "INCONSISTENT POLICY";
    case DDS::RETCODE_ALREADY_DELETED: return "ALREADY DELETED";
    case DDS::RETCODE_TIMEOUT: return "TIMEOUT";
    case DDS::RETCODE_NO_DATA: return "NO DATA";
    case DDS::RETCODE_ILLEGAL_OPERATION: return "ILLEGAL OPERATION";
    }
    return "Unknown";
}


//------------------------------------------------------------------------------
void DDSManager::checkStatus(const DDS::ReturnCode_t& status, const char* info)
{
    if (status != DDS::RETCODE_OK && status != DDS::RETCODE_NO_DATA)
    {
        std::cerr << "Error in "
            << info
            << ": "
            << getErrorName(status)
            << std::endl;
    }
}

//------------------------------------------------------------------------------
std::string ddsEnumToString(const CORBA::TypeCode* enumTypeCode,
                            const unsigned int& enumValue)
{
    // Make sure this is an enum or we'll likely crash
    if (enumTypeCode->kind() != CORBA::tk_enum)
    {
        std::cerr << "ddsEnumToString: "
                  << "Typecode parameter is not an enum."
                  << std::endl;
        return "";
    }


    // Make sure the enum value is in bounds
    const size_t enumMemberCount = enumTypeCode->member_count();
    if (enumValue >= enumMemberCount)
    {
        std::cerr << "ddsEnumToString: "
                  << "Invalid enum value of "
                  << enumValue
                  << " for "
                  << enumTypeCode->name();

        return "";
    }

    // Looks good
    return enumTypeCode->member_name(enumValue);
}


/**
 * @}
 */
