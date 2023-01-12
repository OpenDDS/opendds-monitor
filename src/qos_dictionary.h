#ifndef __DDSMAN_QOS_DICTIONARY_H__
#define __DDSMAN_QOS_DICTIONARY_H__

#ifdef WIN32
#pragma warning(push, 0)  //No DDS warnings
#endif

#include "dds/DdsDcpsCoreC.h"
#include "dds/DCPS/Serializer.h"

#ifdef WIN32
#pragma warning(pop)
#endif

/**
 * @brief Methods for accessing shared QoS settings.
 *
 * @details bestEffort: No effort or resources are spent to track whether
 *          or not sent samples are received. Minimal resources are used
 *          and data samples may be lost. This setting is good for periodic
 *          data.
 *
 *          latestReliableTransient: Guarantees delivery of the latest
 *          published sample. Samples may be overwritten, lost samples are
 *          retransmitted, and late-joining participants will receive
 *          previous samples. This is the default QoS used in the
 *          DDSManager class.
 *
 *          latestReliable: Guarantees delivery of the latest published sample.
 *          Samples may be overwritten and lost samples are retransmitted.
 *
 *          strictReliable: Guarantees delivery of every published sample.
 *          Samples will not be overwritten and lost samples are retransmitted.
 */
namespace QosDictionary
{
	DDS::DestinationOrderQosPolicyKind getTimestampPolicy();

	DDS::DataRepresentationId_t getDataRepresentationType();

	OpenDDS::DCPS::Encoding::Kind getEncodingKind();

    namespace Topic
    {
        DDS::TopicQos bestEffort();
        DDS::TopicQos latestReliableTransient();
        DDS::TopicQos latestReliable();
        DDS::TopicQos strictReliable();
    }

    namespace Subscriber
    {
        DDS::SubscriberQos defaultQos();
    }

    namespace Publisher
    {
        DDS::PublisherQos defaultQos();
    }

    namespace DataReader
    {
        DDS::DataReaderQos bestEffort();
        DDS::DataReaderQos latestReliableTransient();
        DDS::DataReaderQos latestReliable();
        DDS::DataReaderQos strictReliable();
    }

    namespace DataWriter
    {
        DDS::DataWriterQos bestEffort();
        DDS::DataWriterQos latestReliableTransient();
        DDS::DataWriterQos latestReliable();
        DDS::DataWriterQos strictReliable();
    }

}

#endif

/**
 * @}
 */
