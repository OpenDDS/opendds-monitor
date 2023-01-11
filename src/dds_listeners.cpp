#include "dds_listeners.h"

#include <iostream>


//------------------------------------------------------------------------------
// GenericTopicListener
//------------------------------------------------------------------------------
void GenericTopicListener::on_inconsistent_topic(
    DDS::Topic* topic,
    const DDS::InconsistentTopicStatus&)
{
    std::cerr << "Inconsistent DDS topic definition."
              << " Topic: "
              << topic->get_name()
              << ", Type: "
              << topic->get_type_name()
              << std::endl;
}


//------------------------------------------------------------------------------
// GenericReaderListener
//------------------------------------------------------------------------------
void GenericReaderListener::on_data_available(DDS::DataReader*)
{}


//------------------------------------------------------------------------------
void GenericReaderListener::on_requested_deadline_missed(
    DDS::DataReader* reader,
    const DDS::RequestedDeadlineMissedStatus& status)
{
    DDS::TopicDescription* topicDesc = reader->get_topicdescription();
    std::cerr << "DDS deadline missed."
              << " Topic: "
              << topicDesc->get_name()
              << ", Type: "
              << topicDesc->get_type_name()
              << std::endl;

    if (m_handler != nullptr) {
        m_handler->on_requested_deadline_missed(reader, status);
    }
}


//------------------------------------------------------------------------------
void GenericReaderListener::on_requested_incompatible_qos(
    DDS::DataReader* reader,
    const DDS::RequestedIncompatibleQosStatus& status)
{
    DDS::TopicDescription* topicDesc = reader->get_topicdescription();
    std::cerr << "Incompatible DDS QoS."
              << " Topic: "
              << topicDesc->get_name()
              << ", Type: "
              << topicDesc->get_type_name()
              << std::endl;

    if (m_handler != nullptr) {
        m_handler->on_requested_incompatible_qos(reader, status);
    }
}


//------------------------------------------------------------------------------
void GenericReaderListener::on_sample_rejected(
    DDS::DataReader* reader,
    const DDS::SampleRejectedStatus& status)
{
    DDS::TopicDescription* topicDesc = reader->get_topicdescription();
    std::cerr << "DDS sample rejected."
              << " Topic: "
              << topicDesc->get_name()
              << ", Type: "
              << topicDesc->get_type_name()
              << std::endl;

    if (m_handler != nullptr) {
        m_handler->on_sample_rejected(reader, status);
    }
}


//------------------------------------------------------------------------------
void GenericReaderListener::on_liveliness_changed(
    DDS::DataReader* reader,
    const DDS::LivelinessChangedStatus& status)
{
    if (m_handler != nullptr) {
        m_handler->on_liveliness_changed(reader, status);
    }
}


//------------------------------------------------------------------------------
void GenericReaderListener::on_subscription_matched(
    DDS::DataReader* reader,
    const DDS::SubscriptionMatchedStatus& status)
{
    if (m_handler != nullptr) {
        m_handler->on_subscription_matched(reader, status);
    }
}


//------------------------------------------------------------------------------
void GenericReaderListener::on_sample_lost(
    DDS::DataReader* reader,
    const DDS::SampleLostStatus& status)
{
    if (m_handler != nullptr) {
        m_handler->on_sample_lost(reader, status);
    }
}


//------------------------------------------------------------------------------
// GenericSubscriberListener
//------------------------------------------------------------------------------
// void GenericSubscriberListener::on_data_on_readers(DDS::Subscriber*)
// {}


//------------------------------------------------------------------------------
// GenericWriterListener
//------------------------------------------------------------------------------
void GenericWriterListener::on_offered_deadline_missed(
    DDS::DataWriter* writer,
    const DDS::OfferedDeadlineMissedStatus& status)
{
    DDS::Topic* topicDesc = writer->get_topic();
    std::cerr << "DDS deadline missed."
              << " Topic: "
              << topicDesc->get_name()
              << ", Type: "
              << topicDesc->get_type_name()
              << std::endl;

    if (m_handler != nullptr) {
        m_handler->on_offered_deadline_missed(writer, status);
    }
}


//------------------------------------------------------------------------------
void GenericWriterListener::on_liveliness_lost(
    DDS::DataWriter* writer,
    const DDS::LivelinessLostStatus& status)
{
    if (m_handler != nullptr) {
        m_handler->on_liveliness_lost(writer, status);
    }
}


//------------------------------------------------------------------------------
void GenericWriterListener::on_offered_incompatible_qos(
    DDS::DataWriter* writer,
    const DDS::OfferedIncompatibleQosStatus& status)
{
    DDS::Topic* topic = writer->get_topic();
    std::cerr << "Incompatible topic QoS settings\n"
             << "  Topic: "
             << topic->get_name()
             << "\n  Type: "
             << topic->get_type_name()
             << std::endl;

    if (m_handler != nullptr) {
        m_handler->on_offered_incompatible_qos(writer, status);
    }
}


//------------------------------------------------------------------------------
void GenericWriterListener::on_publication_matched(
    DDS::DataWriter* writer,
    const DDS::PublicationMatchedStatus& status)
{
    if (m_handler != nullptr) {
        m_handler->on_publication_matched(writer, status);
    }
}


//------------------------------------------------------------------------------
void GenericWriterListener::on_instance_replaced(
    DDS::DataWriter*writer,
    const DDS::InstanceHandle_t& status)
{
    if (m_handler != nullptr) {
        m_handler->on_instance_replaced(writer, status);
    }
}


/**
 * @}
 */
