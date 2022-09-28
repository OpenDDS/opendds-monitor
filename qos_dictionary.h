#ifndef __DDSMAN_QOS_DICTIONARY_H__
#define __DDSMAN_QOS_DICTIONARY_H__

#pragma warning(push, 0)  //No DDS warnings
#include "dds/DdsDcpsCoreC.h"
#include "dds/DCPS/Serializer.h"
#include "dds_manager_defs.h"
#pragma warning(pop)

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
	DLL_PUBLIC DDS::DestinationOrderQosPolicyKind getTimestampPolicy();

	DLL_PUBLIC DDS::DataRepresentationId_t getDataRepresentationType();

	DLL_PUBLIC OpenDDS::DCPS::Encoding::Kind getEncodingKind();

    namespace Topic
    {
        DLL_PUBLIC DDS::TopicQos bestEffort();
        DLL_PUBLIC DDS::TopicQos latestReliableTransient();
        DLL_PUBLIC DDS::TopicQos latestReliable();
        DLL_PUBLIC DDS::TopicQos strictReliable();
    }

    namespace Subscriber
    {
        DLL_PUBLIC DDS::SubscriberQos defaultQos();
    }

    namespace Publisher
    {
        DLL_PUBLIC DDS::PublisherQos defaultQos();
    }

    namespace DataReader
    {
        DLL_PUBLIC DDS::DataReaderQos bestEffort();
        DLL_PUBLIC DDS::DataReaderQos latestReliableTransient();
        DLL_PUBLIC DDS::DataReaderQos latestReliable();
        DLL_PUBLIC DDS::DataReaderQos strictReliable();
    }

    namespace DataWriter
    {
        DLL_PUBLIC DDS::DataWriterQos bestEffort();
        DLL_PUBLIC DDS::DataWriterQos latestReliableTransient();
        DLL_PUBLIC DDS::DataWriterQos latestReliable();
        DLL_PUBLIC DDS::DataWriterQos strictReliable();
    }

}

#endif

/**
 * @}
 */
