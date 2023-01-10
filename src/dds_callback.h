#ifndef __CALLBACK_H__
#define __CALLBACK_H__

#ifdef WIN32
#pragma warning(push, 0)  //No DDS warnings
#endif

#include <dds/DCPS/TypeSupportImpl.h>
#include <dds/DCPS/WaitSet.h>
#include <dds/DCPS/EventDispatcher.h>

#ifdef WIN32
#pragma warning(pop)
#endif

#include <iostream>
#include <typeinfo>
#include <thread>
#include <map>
#include <typeindex>
#include <future>
#include <functional>

//This implementation was taken from the answer to stackoverflow question 16883817
struct GenericCallback {
    virtual ~GenericCallback() { }
};

template <typename TopicType>
struct Callback : GenericCallback {
    std::function<void(const TopicType&)> function;
    Callback(std::function<void(const TopicType&)> fun) : function(fun) { }
};

typedef std::multimap<std::type_index, std::shared_ptr<GenericCallback> > Listeners;

class EmitterBase
{
public:

    /// Default constructor
    EmitterBase(OpenDDS::DCPS::RcHandle<OpenDDS::DCPS::EventDispatcher> ed) : m_running(false), m_dispatcher(ed)
    {}

    virtual ~EmitterBase();

    virtual void run() = 0;
    virtual void stop() = 0;
    virtual void readQueue() = 0;
    virtual void setReader(DDS::DataReader_var reader) = 0;
    void AddToThreadPool(std::function<void(void)> fn);

    bool isRunning() const
    {
        return m_running;
    }

    void setAsync(bool set)
    {
        m_asyncEmitter = set;
    }

    template <typename TopicType>
    void addCallback(std::function<void(const TopicType&)> func)
    {
        std::type_index index(typeid(TopicType));
        std::shared_ptr<GenericCallback> func_ptr(new Callback<TopicType>(std::function<void(const TopicType&)>(func)));
        m_callbacks.insert(Listeners::value_type(index, std::move(func_ptr)));
    }

    /// Sends a message out to anyone registered for that type
    template <typename TopicType>
    void emitMessage(TopicType& arg)
    {
        std::type_index index(typeid(TopicType));
        if (m_callbacks.count(index) > 0)
        {
            auto range = m_callbacks.equal_range(index);

            for (auto it = range.first; it != range.second; ++it)
            {
                //Cast the generic function to the topic specific function and call with args
                const GenericCallback &f = *it->second;
                std::function<void(const TopicType&)> func = static_cast<const Callback<TopicType> &>(f).function;
                if (m_asyncEmitter) {
                    //Future destructor holds up execution
                    //https://stackoverflow.com/questions/44654548/stdasync-doesnt-work-asynchronously
                    //fut = std::async(std::launch::async, func, arg);
                    AddToThreadPool([func, arg]() {func(arg);});
                }
                else {
                    func(arg);
                }
            }
        }
    }

protected:

    // List of callbacks
    std::multimap<std::type_index, std::shared_ptr<GenericCallback> > m_callbacks;

    /// Flag to stop the thread.
    bool m_running;

    bool m_asyncEmitter = false;

    OpenDDS::DCPS::WeakRcHandle<OpenDDS::DCPS::EventDispatcher> m_dispatcher;

    //std::future<void> fut;
};


template <typename TopicType>
class Emitter : public EmitterBase
{
public:

    Emitter(DDS::DataReader_var const reader, OpenDDS::DCPS::RcHandle<OpenDDS::DCPS::EventDispatcher> ed) :
            m_reader(reader), EmitterBase(ed)
    {
        if (!m_reader)
        {
            return;
        }

        DDS::TopicDescription_var topicDesc = m_reader->get_topicdescription();
        if (!topicDesc)
        {
            return;
        }

		CORBA::String_var tempstr; //This is used to prevent a memory leak
		tempstr = topicDesc->get_name();
        m_topicName = tempstr.in();
        tempstr = topicDesc->get_type_name();
        m_topicType = tempstr.in();
    }

    void run()
    {
        if (!m_running) {
            m_running = true;
            m_dataThread = std::thread(&Emitter::listen, this);
        }
    }

    void stop()
    {
        // Wait for the thread to finish or we won't shutdown clean
        m_running = false;
        if (m_dataThread.joinable())
        {
            m_dataThread.join();
        }
    }

    void readQueue()
    {
        using OpenDDS::DCPS::DDSTraits;

        DDS::ReturnCode_t status = DDS::RETCODE_OK;

        typename DDSTraits<TopicType>::DataReaderType::_var_type dataReader =
            DDSTraits<TopicType>::DataReaderType::_narrow(m_reader);

        if (!dataReader)
        {
            std::cerr << "Error calling "
                      << m_topicType
                      << "::narrow on '"
                      << m_topicName
                      << "' of type '"
                      << m_topicType
                      << "'"
                      << std::endl;
            return;
        }

        // Check for new data
        typename DDSTraits<TopicType>::MessageSequenceType msgList;
        DDS::SampleInfoSeq infoSeq;
        status = dataReader->take(
            msgList,
            infoSeq,
            DDS::LENGTH_UNLIMITED,
            DDS::ANY_SAMPLE_STATE,
            DDS::ANY_VIEW_STATE,
            DDS::ALIVE_INSTANCE_STATE);

        if (status != DDS::RETCODE_OK && status != DDS::RETCODE_NO_DATA)
        {
            std::cerr << "Error calling "
                        << m_topicType
                        << "::take on '"
                        << m_topicName
                        << "' of type '"
                        << m_topicType
                        << "'"
                        << std::endl;
            return;
        }

        // Invoke the callback method for each received message
        for (int i = 0; i < (int)msgList.length(); i++)
        {
            emitMessage(msgList[i]);
        }

        dataReader->return_loan(msgList, infoSeq);

    } // End readQueue

    void setReader(DDS::DataReader_var reader)
    {
        m_reader = reader;
    }

private:

    void listen()
    {
        using OpenDDS::DCPS::DDSTraits;

        DDS::Duration_t waitTimeout;
        waitTimeout.sec = 0;
        waitTimeout.nanosec = 10000000; // 10 millis

        DDS::ReturnCode_t status = DDS::RETCODE_OK;
        DDS::WaitSet waitset;

        typename DDSTraits<TopicType>::DataReaderType::_var_type dataReader =
            DDSTraits<TopicType>::DataReaderType::_narrow(m_reader);

        if (!dataReader)
        {
            std::cerr << "Error calling "
                      << m_topicType
                      << "::narrow on '"
                      << m_topicName
                      << "' of type '"
                      << m_topicType
                      << "'"
                      << std::endl;
            return;
        }

        DDS::StatusCondition_var statusCondition = m_reader->get_statuscondition();
        if (statusCondition == nullptr)
        {
            std::cerr << "Invalid status condition on '"
                      << m_topicName
                      << "' of type '"
                      << m_topicType
                      << "'"
                      << std::endl;
            return;
        }

        // Trigger the waitset when new data is available
        status = statusCondition->set_enabled_statuses(DDS::DATA_AVAILABLE_STATUS);
        if (status != DDS::RETCODE_OK)
        {
            std::cerr << "Unable to enable waitset status monitor on '"
                      << m_topicName
                      << "' of type '"
                      << m_topicType
                      << "'"
                      << std::endl;
            return;
        }

        status = waitset.attach_condition(statusCondition);
        if (status != DDS::RETCODE_OK)
        {
            std::cerr << "Unable to assign waitset status condition on '"
                      << m_topicName
                      << "' of type '"
                      << m_topicType
                      << "'"
                      << std::endl;
            return;
        }


        // Loop until the parent of this thread stops
        while (m_running)
        {
            // Block this thread for new data
            DDS::ConditionSeq activeConditions;
            status = waitset.wait(activeConditions, waitTimeout);
            if (status != DDS::RETCODE_OK)
            {
                continue;
            }

            readQueue();
        }

        if (m_reader) {
            m_reader = DDS::DataReader::_nil();
        }

    } // End listen

    /// Stores the name of the topic associated with this class.
    std::string m_topicName;

    /// Stores the topic type name of the topic associated with this class.
    std::string m_topicType;

    /// The data reader for the target DDS topic.
    DDS::DataReader_var m_reader;

    /// Is the thread running
    bool m_running = false;

    /// The data reader thread
    std::thread m_dataThread;

};


#endif // __CALLBACK_H__
