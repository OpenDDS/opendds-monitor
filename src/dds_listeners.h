#ifndef __DDS_LISTENERS__
#define __DDS_LISTENERS__

#ifdef WIN32
#pragma warning(push, 0)  //No DDS warnings
#endif

#include <dds/DdsDcpsTopicC.h>
#include <dds/DdsDcpsPublicationC.h>
#include <dds/DdsDcpsSubscriptionC.h>

#ifdef WIN32
#pragma warning(pop)
#endif

class DDSWriterListenerStatusHandler {
public:
    // Handles the DDS::OFFERED_DEADLINE_MISSED_STATUS status.
    virtual void on_offered_deadline_missed(
        DDS::DataWriter* /*writer*/ ,
        const DDS::OfferedDeadlineMissedStatus& /*status*/) {}

    // Handles the DDS::LIVELINESS_LOST_STATUS status.
    virtual void on_liveliness_lost(
        DDS::DataWriter* /*writer*/,
        const DDS::LivelinessLostStatus& /*status*/) {}

    // Handles the DDS::OFFERED_INCOMPATIBLE_QOS_STATUS status.
    virtual void on_offered_incompatible_qos(
        DDS::DataWriter* /*writer*/,
        const DDS::OfferedIncompatibleQosStatus& /*status*/) {}

    // Handles the DDS::PUBLICATION_MATCHED_STATUS status.
    virtual void on_publication_matched(
        DDS::DataWriter* /*writer*/,
        const DDS::PublicationMatchedStatus& /*status*/) {}

    // Notifies when an instance is replaced in DataWriter queue.
    virtual void on_instance_replaced(
        DDS::DataWriter* /*writer*/,
        const DDS::InstanceHandle_t& /*handle*/) {}
};

class DDSReaderListenerStatusHandler {
public:
    // Handles the DDS::REQUESTED_DEADLINE_MISSED_STATUS communication status.
    virtual void on_requested_deadline_missed(
        DDS::DataReader* /*reader*/,
        const DDS::RequestedDeadlineMissedStatus& /*status*/) {}

    // Handles the DDS::REQUESTED_INCOMPATIBLE_QOS_STATUS communication status.
    virtual void on_requested_incompatible_qos(
        DDS::DataReader* /*reader*/,
        const DDS::RequestedIncompatibleQosStatus& /*status*/) {}

    // Handles the DDS::SAMPLE_REJECTED_STATUS communication status.
    virtual void on_sample_rejected(
        DDS::DataReader* /*reader*/,
        const DDS::SampleRejectedStatus& /*status*/) {}

    // Handles the DDS::LIVELINESS_CHANGED_STATUS communication status.
    virtual void on_liveliness_changed(
        DDS::DataReader* /*reader*/,
        const DDS::LivelinessChangedStatus& /*status*/) {}

    // Handles the DDS::SUBSCRIPTION_MATCHED_STATUS communication status.
    virtual void on_subscription_matched(
        DDS::DataReader* /*reader*/,
        const DDS::SubscriptionMatchedStatus& /*status*/) {}

    // Handles the DDS::SAMPLE_LOST_STATUS communication status.
    virtual void on_sample_lost(
        DDS::DataReader* /*reader*/,
        const DDS::SampleLostStatus& /*status*/) {}
};

/**
 * @brief Generic listener class for topic data.
 */
class GenericTopicListener: public DDS::TopicListener
{
public:

    // Handle the DDS::INCONSISTENT_TOPIC_STATUS status.
    void on_inconsistent_topic(
        DDS::Topic* topic,
        const DDS::InconsistentTopicStatus& status);
};


/**
 * @brief Generic listener class for data writer objects.
 */
class GenericWriterListener: public DDS::DataWriterListener
{
public:
    GenericWriterListener() = default;

    void SetHandler(DDSWriterListenerStatusHandler* handler) { m_handler = handler; }

    // Handles the DDS::OFFERED_DEADLINE_MISSED_STATUS status.
    void on_offered_deadline_missed(
        DDS::DataWriter* writer,
        const DDS::OfferedDeadlineMissedStatus& status);

    // Handles the DDS::LIVELINESS_LOST_STATUS status.
    void on_liveliness_lost(
        DDS::DataWriter* writer,
        const DDS::LivelinessLostStatus& status);

    // Handles the DDS::OFFERED_INCOMPATIBLE_QOS_STATUS status.
    void on_offered_incompatible_qos(
        DDS::DataWriter* writer,
        const DDS::OfferedIncompatibleQosStatus& status);

    // Handles the DDS::PUBLICATION_MATCHED_STATUS status.
    void on_publication_matched(
        DDS::DataWriter* writer,
        const DDS::PublicationMatchedStatus& status);

    // Notifies when an instance is replaced in DataWriter queue.
    void on_instance_replaced(
        DDS::DataWriter* writer,
        const DDS::InstanceHandle_t& handle);

private:
    DDSWriterListenerStatusHandler* m_handler = nullptr;
};


/**
 * @brief Generic listener class for data reader objects.
 */
class GenericReaderListener: public DDS::DataReaderListener
{
public:
    GenericReaderListener() = default;

    void SetHandler(DDSReaderListenerStatusHandler* handler) { m_handler = handler; }

    // Handle the DDS::DATA_AVAILABLE_STATUS communication status.
    void on_data_available(
        DDS::DataReader* reader);

    // Handles the DDS::REQUESTED_DEADLINE_MISSED_STATUS communication status.
    void on_requested_deadline_missed(
        DDS::DataReader* reader,
        const DDS::RequestedDeadlineMissedStatus& status);

    // Handles the DDS::REQUESTED_INCOMPATIBLE_QOS_STATUS communication status.
    void on_requested_incompatible_qos(
        DDS::DataReader* reader,
        const DDS::RequestedIncompatibleQosStatus& status);

    // Handles the DDS::SAMPLE_REJECTED_STATUS communication status.
    void on_sample_rejected(
        DDS::DataReader* reader,
        const DDS::SampleRejectedStatus& status);

    // Handles the DDS::LIVELINESS_CHANGED_STATUS communication status.
    void on_liveliness_changed(
        DDS::DataReader* reader,
        const DDS::LivelinessChangedStatus& status);

    // Handles the DDS::SUBSCRIPTION_MATCHED_STATUS communication status.
    void on_subscription_matched(
        DDS::DataReader* reader,
        const DDS::SubscriptionMatchedStatus& status);

    // Handles the DDS::SAMPLE_LOST_STATUS communication status.
    void on_sample_lost(
        DDS::DataReader* reader,
        const DDS::SampleLostStatus& status);

private:
    DDSReaderListenerStatusHandler* m_handler = nullptr;
};

#endif

/**
 * @}
 */
