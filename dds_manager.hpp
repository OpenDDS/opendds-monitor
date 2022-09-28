#ifndef __DDS_MANAGER_HPP__
#define __DDS_MANAGER_HPP__

#include <exception>
#include <iostream>
#include <vector>
#include <string>

#pragma warning(push, 0)  //No DDS warnings
#include <dds/DCPS/TypeSupportImpl.h>
#include <dds/DCPS/Serializer.h>
#include <tao/AnyTypeCode/Any.h>
#include "dds_listeners.h"
#include <dds/DCPS/Service_Participant.h>
#pragma warning(pop)

//User must supply this by compiling std_qos.idl.
#include "std_qosC.h"

 //As of OpenDDS 3.13, we can rejoin domains after calling the destructor, but only if we don't call this function
 //until the end of the program
inline void ShutdownDDS()
{
    // Calling shutdown should be done as the last thing after all dds calls are done.
    OpenDDS::DCPS::Service_Participant* service = TheServiceParticipant;
    if (service)
    {
        service->shutdown();
    }
}

//------------------------------------------------------------------------------
template <typename TopicType>
bool DDSManager::registerTopic(const std::string& topicName, const STD_QOS::QosType qosType)
{
    std::shared_ptr<TopicGroup> topicGroup = nullptr;
    DDS::ReturnCode_t status = DDS::RETCODE_OK;
    decltype(m_sharedLock) shared_lock(m_topicMutex);

    // If nothing has been created for this topic yet, construct it
    auto iter = m_topics.find(topicName);
    if (iter == m_topics.end())
    {
        topicGroup = std::make_shared<TopicGroup>();
    }
    // If a topic group exists, use it and create the topic object later
    else if (m_topics[topicName] != nullptr &&
             m_topics[topicName]->topic == nullptr)
    {
        topicGroup = m_topics[topicName];
    }
    // The topic already exists, so we're done
    else
    {
        return true;
    }
    shared_lock.unlock();

    typename OpenDDS::DCPS::DDSTraits<TopicType>::TypeSupportType::_var_type ts =
        new (typename OpenDDS::DCPS::DDSTraits<TopicType>::TypeSupportImplType);


    //std::cout << "Registering topic '"
    //            << topicName
    //            << "' of type '"
    //            << ts->get_type_name()
    //            << "'"
    //            << std::endl;

	//Get the marshal traits. New in 3.18
    typedef OpenDDS::DCPS::MarshalTraits<TopicType> marshalTraits;

    // Register the topic type
    CORBA::String_var tn = ts->get_type_name();
    status = ts->register_type(m_domainParticipant, tn.in());
    checkStatus(status, "register_type");
    topicGroup->typeName = tn;



    // Stuff the USR topic info into the user_data member of the topic QoS
    // Format:
    // char[0-3] - USR user_data header
    // char[5]   - Topic has a key flag
    // char[6]   - Topic Type extensibility and mutability flags
    //           - 0: APPENDABLE (Default if not specified, and in old implementations)
    //           - 1: FINAL
    //           - 2: MUTABLE
    // char[7]   - Padding
    // char[8-N] - Serialized CDR topic typecode

    // Get the topic typecode and stuff it into a CDR object
    typename OpenDDS::DCPS::DDSTraits<TopicType>::MessageType topicMessageType;
    ddsInit(topicMessageType);
    CORBA::Any topicTypeAny;
    topicTypeAny <<= topicMessageType; // It's a CORBA::TypeCode

    TAO_OutputCDR topicTypeCDR;
    bool pass = (topicTypeCDR << topicTypeAny);
    if (!pass)
    {
        std::cerr << "Failed to marshal type object for "
            << topicName
            << std::endl;
    }
    else // Add the typecode into the topic QoS
    {
        topicTypeCDR.consolidate();

        const size_t headerSize = 8;
        const size_t typeCodeSize = topicTypeCDR.total_length();
        const size_t totalDataSize = headerSize + typeCodeSize;
        topicGroup->topicQos.topic_data.value.length(static_cast<CORBA::ULong>(totalDataSize));

        // Create a header for USR specific topic info
        char header[headerSize];
        memset(header, 0, headerSize);

        strcpy(header, "USR");
        (ts->has_dcps_key()) ? header[5] = 1 : header[5] = 0;
		switch(marshalTraits::extensibility())
		{
			case OpenDDS::DCPS::Extensibility::FINAL:
				header[6] = 1;
				break;
			case OpenDDS::DCPS::Extensibility::MUTABLE:
				header[6] = 2;
				break;
			default:
				header[6] = 0;
				break;
		}

        memcpy(&topicGroup->topicQos.topic_data.value[0],
            (unsigned char*)header,
            headerSize);

        // Copy the type code CDR after the header
        memcpy(&topicGroup->topicQos.topic_data.value[headerSize],
            (unsigned char*)topicTypeCDR.buffer(),
            typeCodeSize);
    }


    // Create the topic
    auto listener = std::make_unique<GenericTopicListener>();
    topicGroup->topic = m_domainParticipant->create_topic(
        topicName.c_str(),
        topicGroup->typeName.in(),
        topicGroup->topicQos,
        listener.get(),
        DDS::INCONSISTENT_TOPIC_STATUS);

    if (!topicGroup->topic)
    {
        std::cerr << "Error creating new topic '"
            << topicName
            << "'"
            << std::endl;

        return false;
    }
    //else {
    //    std::cout << "Successfully registered new topic '"
    //        << topicName
    //        << "' for handle: "
    //        << topicGroup->topic->get_instance_handle()
    //        << std::endl;
    //}

    // If we got here, everything looks good. Store for future use.
    topicGroup->domain = m_domainParticipant;
    topicGroup->m_listener = std::move(listener);
    decltype(m_uniqueLock) lock(m_topicMutex);
    m_topics[topicName] = topicGroup;
    lock.unlock();

    return registerQos(topicName, qosType);
} // End DDSManager::registerTopic

//------------------------------------------------------------------------------
template <typename TopicType>
bool DDSManager::takeSample(TopicType& sample,
                            const std::string& topicName,
                            const std::string& readerName,
                            const std::string& filter)
{
    DDS::DataReader_var dataReader = getReader(topicName, readerName);
    if (!dataReader)
    {
        return false;
    }

    DDS::ReturnCode_t status = DDS::RETCODE_OK;
    DDS::SampleInfoSeq infoSeq;

    typename OpenDDS::DCPS::DDSTraits<TopicType>::MessageSequenceType msgList;

    typename OpenDDS::DCPS::DDSTraits<TopicType>::DataReaderType::_var_type topicReader =
        OpenDDS::DCPS::DDSTraits<TopicType>::DataReaderType::_narrow(dataReader);

    if (!topicReader)
    {
        std::cerr << "Unable to cast '"
            << topicName
            << "' to data reader type"
            << std::endl;

        return false;
    }

    // Did the user specify a read condition?
    if (filter != "")
    {
        DDS::QueryCondition_var condition = topicReader->create_querycondition(
            DDS::ANY_SAMPLE_STATE,
            DDS::ANY_VIEW_STATE,
            DDS::ALIVE_INSTANCE_STATE,
            filter.c_str(),
            DDS::StringSeq());

        // Take a single ALIVE sample with the condition parameter
        status = topicReader->take_w_condition(
            msgList,
            infoSeq,
            1,
            condition);

        topicReader->delete_readcondition(condition);
    }
    else // No read condition
    {
        // Take a single ALIVE sample
        status = topicReader->take(
            msgList,
            infoSeq,
            1,
            DDS::ANY_SAMPLE_STATE,
            DDS::ANY_VIEW_STATE,
            DDS::ALIVE_INSTANCE_STATE);
    }

    // If we don't have any data, we're done
    checkStatus(status, "DDSManager::takeSample::take");
    if (status != DDS::RETCODE_OK)
    {
        return false;
    }

    sample = msgList[0];

    status = topicReader->return_loan(msgList, infoSeq);
    checkStatus(status, "DDSManager::takeSample::return_loan");

    // Report that we have new data by return true
    return true;

} // End DDSManager::takeSample


//------------------------------------------------------------------------------
template <typename TopicType>
bool DDSManager::takeAllSamples(std::vector<TopicType>& samples,
                                const std::string& topicName,
                                const std::string& readerName,
                                const std::string& filter,
                                const bool& readOnly)
{
    DDS::DataReader_var dataReader = getReader(topicName, readerName);
    if (!dataReader)
    {
        return false;
    }

    DDS::ReturnCode_t status = DDS::RETCODE_OK;
    DDS::SampleInfoSeq infoSeq;

    typename OpenDDS::DCPS::DDSTraits<TopicType>::MessageSequenceType msgList;

    typename OpenDDS::DCPS::DDSTraits<TopicType>::DataReaderType::_var_type topicReader =
        OpenDDS::DCPS::DDSTraits<TopicType>::DataReaderType::_narrow(dataReader);

    if (!topicReader)
    {
        std::cerr << "Unable to cast '"
            << topicName
            << "' to data reader type"
            << std::endl;

        return false;
    }


    // Did the user specify a read/take condition?
    if (filter != "")
    {
        // Prepare the read/take condition
        DDS::QueryCondition_var condition = topicReader->create_querycondition(
            DDS::ANY_SAMPLE_STATE,
            DDS::ANY_VIEW_STATE,
            DDS::ALIVE_INSTANCE_STATE,
            filter.c_str(),
            DDS::StringSeq());

        if (readOnly)
        {
            // Read all ALIVE samples (with condition) and leave samples in
            // the data reader
            status = topicReader->read_w_condition(
                msgList,
                infoSeq,
                DDS::LENGTH_UNLIMITED,
                condition);
        }
        else
        {
            // Take ALL the ALIVE samples (with condition)
            status = topicReader->take_w_condition(
                msgList,
                infoSeq,
                DDS::LENGTH_UNLIMITED,
                condition);
        }

        topicReader->delete_readcondition(condition);
    }
    else // Not using a read/take condition
    {
        if (readOnly)
        {
            // Read all ALIVE samples and leave the samples in the data reader
            status = topicReader->read(
                msgList,
                infoSeq,
                DDS::LENGTH_UNLIMITED,
                DDS::ANY_SAMPLE_STATE,
                DDS::ANY_VIEW_STATE,
                DDS::ALIVE_INSTANCE_STATE);
        }
        else // Take ALL the ALIVE samples
        {
            status = topicReader->take(
                msgList,
                infoSeq,
                DDS::LENGTH_UNLIMITED,
                DDS::ANY_SAMPLE_STATE,
                DDS::ANY_VIEW_STATE,
                DDS::ALIVE_INSTANCE_STATE);
        }
    }


    // If we don't have any data, we're done
    checkStatus(status, "DDSManager::takeAllSamples::take");
    if (status != DDS::RETCODE_OK)
    {
        return false;
    }

    samples.resize(msgList.length());
    for (int i = 0; i < (int) msgList.length(); i++)
    {
        samples[i] = msgList[i];
    }

    status = topicReader->return_loan(msgList, infoSeq);
    checkStatus(status, "DDSManager::takeAllSamples::return_loan");

    // Report that we have new data by return true
    return true;

} // End DDSManager::takeAllSamples


//------------------------------------------------------------------------------
template <typename TopicType>
bool DDSManager::writeSample(const TopicType& topicInstance,
                             const std::string& topicName)
{
    DDS::ReturnCode_t status = DDS::RETCODE_OK;
    DDS::DataWriter_var writer = getWriter(topicName);
    if (!writer)
    {
        std::cerr << "Unable to find writer for '"
            << topicName
            << "'"
            << std::endl;
        return false;
    }

    typename OpenDDS::DCPS::DDSTraits<TopicType>::DataWriterType::_var_type topicWriter =
        OpenDDS::DCPS::DDSTraits<TopicType>::DataWriterType::_narrow(writer);

    if (!topicWriter)
    {
        std::cerr << "Unable to cast '"
            << topicName
            << "' to data writer type"
            << std::endl;

        return false;
    }

    try
    {
        //I believe OpenDDS has mutex protection. I don't think we need to add to it.
        status = topicWriter->write(topicInstance, DDS::HANDLE_NIL);
    }
    catch (const std::runtime_error& error)
    {
        std::cerr << "\n!!! Caught exception in DDSManager::writeSample !!!"
            << "\n!!! An invalid data reader filter is most likely the issue. !!!"
            << "\n!!! Error: " << error.what() << " !!!\n"
            << std::endl;
    }

    if (status != DDS::RETCODE_OK)
    {
        checkStatus(status, "DDSManager::writeSample::write");
        return false;
    }

    return true;

} // End DDSManager::writeSample


//------------------------------------------------------------------------------
template <typename TopicType>
bool DDSManager::disposeSample(const TopicType& topicInstance,
    const std::string& topicName)
{
    DDS::ReturnCode_t status = DDS::RETCODE_OK;
    DDS::DataWriter_var writer = getWriter(topicName);
    if (!writer)
    {
        std::cerr << "Unable to find writer for '"
            << topicName
            << "'"
            << std::endl;
        return false;
    }

    typename OpenDDS::DCPS::DDSTraits<TopicType>::DataWriterType::_var_type topicWriter =
        OpenDDS::DCPS::DDSTraits<TopicType>::DataWriterType::_narrow(writer);

    if (!topicWriter)
    {
        std::cerr << "Unable to cast '"
            << topicName
            << "' to data writer type"
            << std::endl;

        return false;
    }

    try
    {
        //I believe OpenDDS has mutex protection. I don't think we need to add to it.
        status = topicWriter->dispose(topicInstance, DDS::HANDLE_NIL);
    }
    catch (const std::runtime_error& error)
    {
        std::cerr << "\n!!! Caught exception in DDSManager::disposeSample !!!"
            << "\n!!! An invalid data reader filter is most likely the issue. !!!"
            << "\n!!! Error: " << error.what() << " !!!\n"
            << std::endl;
    }

    if (status != DDS::RETCODE_OK)
    {
        checkStatus(status, "DDSManager::disposeSample::dispose");
        return false;
    }

    return true;

} // End DDSManager::disposeSample

//------------------------------------------------------------------------------
template <typename TopicType>
bool ddsSampleEquals(const TopicType& lhs, const TopicType& rhs)
{
    const size_t maxSize = sizeof(TopicType);
    ACE_Message_Block blockA(maxSize);
    ACE_Message_Block blockB(maxSize);

    OpenDDS::DCPS::Serializer serialA(&blockA, OpenDDS::DCPS::Encoding::Kind::KIND_UNALIGNED_CDR, false);
    OpenDDS::DCPS::Serializer serialB(&blockB, OpenDDS::DCPS::Encoding::Kind::KIND_UNALIGNED_CDR, false);

    serialA << lhs;
    serialB << rhs;

    if (blockA.length() != blockB.length())
    {
        return false;
    }

    return (memcmp(blockA.base(), blockB.base(), blockA.length()) == 0);
}


//------------------------------------------------------------------------------
template <typename TopicType>
bool ddsInit(TopicType& topicInstance)
{
    const size_t maxSize = sizeof(topicInstance);
    char* cdrBuffer = new char[maxSize];
    memset(cdrBuffer, 0, maxSize);
    TAO_InputCDR cdrInput(cdrBuffer, maxSize);

    bool pass = (cdrInput >> topicInstance);
    if (!pass)
    {
        std::cerr << "Failed to initialize topic" << std::endl;
    }

    delete[] cdrBuffer;
    return pass;
}


//------------------------------------------------------------------------------
template <typename TopicType>
bool DDSManager::addCallback(const std::string& topicName,
                             const std::string& readerName,
                             std::function<void(const TopicType&)> func,
                             const bool& queueMessages,
                             const bool& asyncHandling)
{
    decltype(m_sharedLock) lock(m_topicMutex);
    auto iter = m_topics.find(topicName);

    if (iter == m_topics.end())
    {
        return false;
    }

    DDS::DataReader_var reader = getReader(topicName, readerName);
    if (!reader)
    {
        return false;
    }

    std::shared_ptr<TopicGroup> topicGroup = iter->second;
    if (!topicGroup)
    {
        return false;
    }

    Emitter<TopicType>* emitter = nullptr;

    if (topicGroup->emitters.find(readerName) != topicGroup->emitters.end())
    {
        emitter = static_cast<Emitter<TopicType>*>(topicGroup->emitters[readerName].get());
    }
    else
    {
        emitter = new Emitter<TopicType>(reader, m_threadPool);
        topicGroup->emitters.emplace(readerName, emitter);
    }
    emitter->addCallback(func);
    emitter->setAsync(asyncHandling);
    lock.unlock();

    // If we're not queuing messages in the middleware, start a waitset
    // thread which waits for data and invokes callback methods immediately
    // after a message is received.
    if (!queueMessages)
    {
        emitter->run();
    }

    return true;
}

#endif

/**
 * @}
 */
