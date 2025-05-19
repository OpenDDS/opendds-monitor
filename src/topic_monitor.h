#ifndef __DDS_TOPIC_MONITOR_H__
#define __DDS_TOPIC_MONITOR_H__

#include "first_define.h"

#include <dds/DCPS/TopicDescriptionImpl.h>
#include <dds/DCPS/OwnershipManager.h>
#include <dds/DCPS/EntityImpl.h>
#include <dds/DCPS/RecorderImpl.h>
#include <dds/DdsDcpsCoreC.h>
#include <dds/DCPS/Serializer.h>

#include <QString>

#include <memory>

class DynamicMetaStruct;

/**
 * @brief Topic monitor for receiving raw DDS data samples.
 */
class TopicMonitor
{
public:

    /**
     * @brief Constructor for the DDS topic monitor.
     * @param[in] topicName The name of the topic to monitor.
     */
    TopicMonitor(const QString& topicName);

    /**
     * @brief Apply a filter to this topic.
     * @param[in] filter The SQL filter string for this topic.
     */
    void setFilter(const QString& filter);

    /**
    * @brief Get the current filter for this topic.
    * @return The current filter for this topic.
    */
    QString getFilter() const;

    /**
     * @brief Close the topic monitor for this topic.
     * @details This object doesn't delete properly from the
     *          OpenDDS::DCPS::Service_Participant::delete_recorder
     *          method, so we're forced to simply stop publishing
     *          updates and live with a memory leak.
     */
    void close();

    /**
     * @brief Read all the samples from the data reader.
     * @param[in] recorder The recorder object with the data.
     * @param[in] rawSample The new data sample for this topic.
     */
    void on_sample_data_received(OpenDDS::DCPS::Recorder* recorder,
                                 const OpenDDS::DCPS::RawDataSample& rawSample);

    class RecorderListener : public OpenDDS::DCPS::RecorderListener
    {
    public:
        RecorderListener(TopicMonitor& monitor) : m_monitor(monitor) {}

        void on_sample_data_received(OpenDDS::DCPS::Recorder* recorder,
                                     const OpenDDS::DCPS::RawDataSample& rawSample) override
        {
            m_monitor.on_sample_data_received(recorder, rawSample);
        }

        void on_recorder_matched(OpenDDS::DCPS::Recorder* /*recorder*/,
                                 const DDS::SubscriptionMatchedStatus& /*status*/) override {}
    private:
        TopicMonitor& m_monitor;
    };

    void on_data_available(DDS::DataReader_ptr);

    class DataReaderListenerImpl : public virtual OpenDDS::DCPS::LocalObject<DDS::DataReaderListener>
    {
    public:
      DataReaderListenerImpl(TopicMonitor& monitor) : m_monitor(monitor) {}
      virtual void on_requested_deadline_missed(DDS::DataReader_ptr,
                                                const DDS::RequestedDeadlineMissedStatus&) {}

      virtual void on_requested_incompatible_qos(DDS::DataReader_ptr,
                                                 const DDS::RequestedIncompatibleQosStatus&) {}

      virtual void on_liveliness_changed(DDS::DataReader_ptr,
                                         const DDS::LivelinessChangedStatus&) {}

      virtual void on_subscription_matched(DDS::DataReader_ptr,
                                           const DDS::SubscriptionMatchedStatus&) {}

      virtual void on_sample_rejected(DDS::DataReader_ptr,
                                      const DDS::SampleRejectedStatus&) {}

      virtual void on_data_available(DDS::DataReader_ptr reader)
      {
        m_monitor.on_data_available(reader);
      }

      virtual void on_sample_lost(DDS::DataReader_ptr,
                                  const DDS::SampleLostStatus&) {}
    private:
      TopicMonitor& m_monitor;
    };

    /**
     * @brief Stop receiving samples.
     */
    void pause();

    /**
     * @brief Resume receiving samples.
     */
    void unpause();

    /**
     * @brief Destructor for the DDS topic monitor.
     */
    virtual ~TopicMonitor();

private:

    /// Stores the name of the topic.
    QString m_topicName;

    /// Stores the SQL filter if specified by the user.
    QString m_filter;

    /// Stores the typecode for this topic.
    const CORBA::TypeCode* m_typeCode;

    /// Listener for the recorder, calls back into this object
    OpenDDS::DCPS::RcHandle<RecorderListener> m_recorder_listener;

    /// Stores the recorder object for this monitor.
    OpenDDS::DCPS::Recorder* m_recorder;

    /// Listener for a dynamic reader
    DDS::DataReaderListener_var m_dr_listener;

    /// A dynamic data reader for this topic
    DDS::DataReader_var m_dr;

    /// Stores the topic object for this monitor.
    DDS::Topic* m_topic;

    /// The paused status of the data reader.
    bool m_paused;

    /// The topic extensibility
    OpenDDS::DCPS::Extensibility m_extensibility;

#if OPENDDS_MAJOR_VERSION == 3 && OPENDDS_MINOR_VERSION >= 24
    struct FilterTypeSupport : OpenDDS::DCPS::TypeSupportImpl {
      FilterTypeSupport(const DynamicMetaStruct& metastruct, OpenDDS::DCPS::Extensibility exten);

      // DDS::TypeSupport
      DDS::ReturnCode_t register_type(DDS::DomainParticipant*, const char*) { return DDS::RETCODE_UNSUPPORTED; }
      char* get_type_name() { return CORBA::string_dup(""); }
      DDS::DynamicType* get_type() { return 0; }

      // OpenDDS::DCPS::TypeSupport
      DDS::DataWriter* create_datawriter() { return 0; }
      DDS::DataReader* create_datareader() { return 0; }
      DDS::DataReader* create_multitopic_datareader() { return 0; }
      bool has_dcps_key() { return false; }
      DDS::ReturnCode_t unregister_type(DDS::DomainParticipant*, const char*) { return DDS::RETCODE_UNSUPPORTED; }
      void representations_allowed_by_type(DDS::DataRepresentationIdSeq&) {}

      // TypeSupportImpl
      const OpenDDS::DCPS::MetaStruct& getMetaStructForType() const;
      const char* name() const { return ""; }
      DDS::DynamicType* get_type() const { return 0; }
      size_t key_count() const { return 0; }
      bool is_dcps_key(const char*) const { return false; }
      OpenDDS::DCPS::SerializedSizeBound serialized_size_bound(const OpenDDS::DCPS::Encoding& encoding) const;
      OpenDDS::DCPS::SerializedSizeBound key_only_serialized_size_bound(const OpenDDS::DCPS::Encoding& encoding) const;
      OpenDDS::DCPS::Extensibility base_extensibility() const { return ext_; }
      OpenDDS::DCPS::Extensibility max_extensibility() const;
      OpenDDS::XTypes::TypeIdentifier& getMinimalTypeIdentifier() const;
      const OpenDDS::XTypes::TypeMap& getMinimalTypeMap() const;
      const OpenDDS::XTypes::TypeIdentifier& getCompleteTypeIdentifier() const;
      const OpenDDS::XTypes::TypeMap& getCompleteTypeMap() const;

      const DynamicMetaStruct& meta_;
      OpenDDS::DCPS::Extensibility ext_;
    };
#endif
}; // End TopicMonitor

#endif

/**
 * @}
 */
