#ifdef WIN32
#pragma warning(push, 0)  //No DDS warnings
#endif

#include <dds/DdsDcpsInfrastructureC.h>
#include <dds/DCPS/Marked_Default_Qos.h>

#ifdef WIN32
#pragma warning(pop)
#endif

#include "qos_dictionary.h"

#include <iostream>
#include <string>

namespace 
{

    /**
     * @brief Populate a topic QoS with valid initial data.
     * @param[out] qos Populate this QoS object.
     */
    void init(DDS::TopicQos& qos)
    {
        qos.durability.kind = DDS::VOLATILE_DURABILITY_QOS;
        qos.durability_service.service_cleanup_delay.sec = DDS::DURATION_ZERO_SEC;
        qos.durability_service.service_cleanup_delay.nanosec = DDS::DURATION_ZERO_NSEC;
        qos.durability_service.history_kind = DDS::KEEP_LAST_HISTORY_QOS;
        qos.durability_service.history_depth = 1;
        qos.durability_service.max_samples = DDS::LENGTH_UNLIMITED;
        qos.durability_service.max_instances = DDS::LENGTH_UNLIMITED;
        qos.durability_service.max_samples_per_instance = DDS::LENGTH_UNLIMITED;
        qos.deadline.period.sec = DDS::DURATION_INFINITE_SEC;
        qos.deadline.period.nanosec = DDS::DURATION_INFINITE_NSEC;
        qos.latency_budget.duration.sec = DDS::DURATION_ZERO_SEC;
        qos.latency_budget.duration.nanosec = DDS::DURATION_ZERO_NSEC;
        qos.liveliness.kind = DDS::AUTOMATIC_LIVELINESS_QOS;
        qos.liveliness.lease_duration.sec = DDS::DURATION_INFINITE_SEC;
        qos.liveliness.lease_duration.nanosec = DDS::DURATION_INFINITE_NSEC;
        qos.reliability.kind = DDS::BEST_EFFORT_RELIABILITY_QOS;
        qos.reliability.max_blocking_time.sec = DDS::DURATION_INFINITE_SEC;
        qos.reliability.max_blocking_time.nanosec = DDS::DURATION_INFINITE_NSEC;
        qos.destination_order.kind = QosDictionary::getTimestampPolicy();
        qos.history.kind = DDS::KEEP_LAST_HISTORY_QOS;
        qos.history.depth = 1;
        qos.resource_limits.max_samples = DDS::LENGTH_UNLIMITED;
        qos.resource_limits.max_instances = DDS::LENGTH_UNLIMITED;
        qos.resource_limits.max_samples_per_instance = DDS::LENGTH_UNLIMITED;
        qos.transport_priority.value = 0;
        qos.lifespan.duration.sec = DDS::DURATION_INFINITE_SEC;
        qos.lifespan.duration.nanosec = DDS::DURATION_INFINITE_NSEC;
        qos.ownership.kind = DDS::SHARED_OWNERSHIP_QOS;
        qos.representation.value.length(1);
        qos.representation.value[0] = QosDictionary::getDataRepresentationType();

    } // End init DDS::TopicQos


    /**
     * @brief Populate a reader QoS with valid initial data.
     * @param[out] qos Populate this QoS object.
     */
    void init(DDS::DataReaderQos& qos)
    {
        qos.type_consistency.kind = DDS::ALLOW_TYPE_COERCION;
        qos.type_consistency.prevent_type_widening = false;
        qos.type_consistency.ignore_sequence_bounds = true;
        qos.type_consistency.ignore_string_bounds = true;
        qos.type_consistency.ignore_member_names = false;
        qos.type_consistency.force_type_validation = false;
        qos.durability.kind = DDS::VOLATILE_DURABILITY_QOS;
        qos.deadline.period.sec = DDS::DURATION_INFINITE_SEC;
        qos.deadline.period.nanosec = DDS::DURATION_INFINITE_NSEC;
        qos.latency_budget.duration.sec = DDS::DURATION_ZERO_SEC;
        qos.latency_budget.duration.nanosec = DDS::DURATION_ZERO_NSEC;
        qos.liveliness.kind = DDS::AUTOMATIC_LIVELINESS_QOS;
        qos.liveliness.lease_duration.sec = DDS::DURATION_INFINITE_SEC;
        qos.liveliness.lease_duration.nanosec = DDS::DURATION_INFINITE_NSEC;
        qos.reliability.kind = DDS::BEST_EFFORT_RELIABILITY_QOS;
        qos.reliability.max_blocking_time.sec = DDS::DURATION_INFINITE_SEC;
        qos.reliability.max_blocking_time.nanosec = DDS::DURATION_INFINITE_NSEC;
        qos.destination_order.kind = QosDictionary::getTimestampPolicy();
        qos.history.kind = DDS::KEEP_LAST_HISTORY_QOS;
        qos.history.depth = 1;
        qos.resource_limits.max_samples = DDS::LENGTH_UNLIMITED;
        qos.resource_limits.max_instances = DDS::LENGTH_UNLIMITED;
        qos.resource_limits.max_samples_per_instance = DDS::LENGTH_UNLIMITED;
        qos.ownership.kind = DDS::SHARED_OWNERSHIP_QOS;
        qos.time_based_filter.minimum_separation.sec = DDS::DURATION_ZERO_SEC;
        qos.time_based_filter.minimum_separation.nanosec = DDS::DURATION_ZERO_NSEC;
        qos.reader_data_lifecycle.autopurge_nowriter_samples_delay.sec = 5;
        qos.reader_data_lifecycle.autopurge_nowriter_samples_delay.nanosec = 0;
        qos.reader_data_lifecycle.autopurge_disposed_samples_delay.sec = 5;
        qos.reader_data_lifecycle.autopurge_disposed_samples_delay.nanosec = 0;
        qos.representation.value.length(2);
        qos.representation.value[0] = DDS::XCDR_DATA_REPRESENTATION;
        qos.representation.value[1] = DDS::XCDR2_DATA_REPRESENTATION;
    } // End init DDS::DataReaderQos


    /**
     * @brief Populate a writer QoS with valid initial data.
     * @param[out] qos Populate this QoS object.
     */
    void init(DDS::DataWriterQos& qos)
    {
        qos.durability.kind = DDS::VOLATILE_DURABILITY_QOS;
        qos.durability_service.service_cleanup_delay.sec = DDS::DURATION_ZERO_SEC;
        qos.durability_service.service_cleanup_delay.nanosec = DDS::DURATION_ZERO_NSEC;
        qos.durability_service.history_kind = DDS::KEEP_LAST_HISTORY_QOS;
        qos.durability_service.history_depth = 1;
        qos.durability_service.max_samples = DDS::LENGTH_UNLIMITED;
        qos.durability_service.max_instances = DDS::LENGTH_UNLIMITED;
        qos.durability_service.max_samples_per_instance = DDS::LENGTH_UNLIMITED;
        qos.deadline.period.sec = DDS::DURATION_INFINITE_SEC;
        qos.deadline.period.nanosec = DDS::DURATION_INFINITE_NSEC;
        qos.latency_budget.duration.sec = DDS::DURATION_ZERO_SEC;
        qos.latency_budget.duration.nanosec = DDS::DURATION_ZERO_NSEC;
        qos.liveliness.kind = DDS::AUTOMATIC_LIVELINESS_QOS;
        qos.liveliness.lease_duration.sec = DDS::DURATION_INFINITE_SEC;
        qos.liveliness.lease_duration.nanosec = DDS::DURATION_INFINITE_NSEC;
        qos.reliability.kind = DDS::BEST_EFFORT_RELIABILITY_QOS; // Diff
        qos.reliability.max_blocking_time.sec = DDS::DURATION_INFINITE_SEC;
        qos.reliability.max_blocking_time.nanosec = 100000000; // 100ms
        qos.destination_order.kind = QosDictionary::getTimestampPolicy();
        qos.history.kind = DDS::KEEP_LAST_HISTORY_QOS;
        qos.history.depth = 1;
        qos.resource_limits.max_samples = DDS::LENGTH_UNLIMITED;
        qos.resource_limits.max_instances = DDS::LENGTH_UNLIMITED;
        qos.resource_limits.max_samples_per_instance = DDS::LENGTH_UNLIMITED;
        qos.transport_priority.value = 0;
        qos.lifespan.duration.sec = DDS::DURATION_INFINITE_SEC;
        qos.lifespan.duration.nanosec = DDS::DURATION_INFINITE_NSEC;
        qos.ownership.kind = DDS::SHARED_OWNERSHIP_QOS;
        qos.ownership_strength.value = 0;
        qos.writer_data_lifecycle.autodispose_unregistered_instances = 1;
        qos.representation.value.length(1);
        qos.representation.value[0] = QosDictionary::getDataRepresentationType();

    } // End init DDS::DataWriterQos

} // End anonymous namespace

/** 
 * @brief Get the destination order QoS policy
 * @details defaults to by source timestamp, but can be overridden with $DDS_DISTRUST_TIMESTAMPSS
 * @return QosPolicy for destination order.
 */
DDS::DestinationOrderQosPolicyKind QosDictionary::getTimestampPolicy()
{
	//lets use some static bools so we don't getenv() and transform strings every time we register a new topic.
	//TODO: Determine if this needs thread safety. I think it doesn't?
	static bool first_time = true;
	static DDS::DestinationOrderQosPolicyKind retval = DDS::BY_RECEPTION_TIMESTAMP_DESTINATIONORDER_QOS;
	if (!first_time)
	{
		//if (retval == DDS::BY_SOURCE_TIMESTAMP_DESTINATIONORDER_QOS)
		//{
		//	std::cout << "[DDSMAN DEBUG] Using source timestamp" << std::endl;
		//}
		//else
		//{
		//	std::cout << "[DDSMAN DEBUG] Using receipt timestamp" << std::endl;
		//}
		return retval;
	}
	char* DDS_DISTRUST_TIMESTAMPS = getenv("DDS_DISTRUST_TIMESTAMPS");
	bool distrust_timestamps = false;
	if (DDS_DISTRUST_TIMESTAMPS != NULL)
	{
		std::string distrust_val(DDS_DISTRUST_TIMESTAMPS);
		std::transform(distrust_val.begin(), distrust_val.end(), distrust_val.begin(), ::tolower); 
		if ((distrust_val != "") && (distrust_val != "0") && (distrust_val != "false"))
		{
			distrust_timestamps = true;
		}
	}
	if (!distrust_timestamps)
	{
		std::cout << "The 'DDS_DISTRUST_TIMESTAMPS' environment variable was not set. "
	    	<< "Using source timestamps."
	        << std::endl;
		retval = DDS::BY_SOURCE_TIMESTAMP_DESTINATIONORDER_QOS;
	}
	else
	{
		std::cout << "The 'DDS_DISTRUST_TIMESTAMPS' environment variable was set. "
	    	<< "Using receipt timestamps."
	        << std::endl;
		retval = DDS::BY_RECEPTION_TIMESTAMP_DESTINATIONORDER_QOS;
	}
	first_time = false;
	return retval;
}

/** 
 * @brief Get the encoding for DataWriters
 * @details Snippet from the OpenDDS 3.16 developers guide:
 * For the rtps_udp transport, the default encoding of DataWriters changed from classic CDR to XCDR2. To maintain interoperability with pre-3.16 OpenDDS and other DDS implementations, the first element of representation.value of DataWriterQos must be set to DDS::XCDR_DATA_REPRESENTATION or the non-OpenDDS 3.16 DataReader must be set up with DDS::XCDR2_DATA_REPRESENTATION if supported. DataReaders will continue to be interoperable by default
 * We default to XCDR2, but if $DDS_USE_OLD_CDR is set, CDR is used for compatilility with versions before 3.16.0
 * @return QosPolicy for destination order.
 */
DDS::DataRepresentationId_t QosDictionary::getDataRepresentationType()
{
	//lets use some static bools so we don't getenv() and transform strings every time we register a new topic.
	//TODO: Determine if this needs thread safety. I think it doesn't?
	static bool first_time = true;
	static DDS::DataRepresentationId_t retval = DDS::XCDR2_DATA_REPRESENTATION;
	if (!first_time)
	{
		//if (retval == DDS::XCDR2_DATA_REPRESENTATION)
		//{
		//	std::cout << "[DDSMAN DEBUG] Using XCDR2" << std::endl;
		//}
		//else
		//{
		//	std::cout << "[DDSMAN DEBUG] Using Classic CDR" << std::endl;
		//}
		return retval;
	}
	char* DDS_USE_OLD_CDR = getenv("DDS_USE_OLD_CDR");
	bool old_cdr = false;
	if (DDS_USE_OLD_CDR != NULL)
	{
		std::string old_cdr_val(DDS_USE_OLD_CDR);
		std::transform(old_cdr_val.begin(), old_cdr_val.end(), old_cdr_val.begin(), ::tolower); 
		if ((old_cdr_val != "") && (old_cdr_val != "0") && (old_cdr_val != "false"))
		{
			old_cdr = true;
		}
	}
	if (!old_cdr)
	{
		std::cout << "The 'DDS_USE_OLD_CDR' environment variable was not set. "
	    	<< "Using XCDR2 encoding for data writers. This will enable xtypes functionality with OpenDDS 3.16.0 and later"
	        << std::endl;
		retval = DDS::XCDR2_DATA_REPRESENTATION;
	}
	else
	{
		std::cout << "The 'DDS_USE_OLD_CDR' environment variable was set. "
	    	<< "Using 'classic' CDR encoding for data writers. This will enable interoperability with OpenDDS versions before 3.16.0"
	        << std::endl;
		retval = DDS::XCDR_DATA_REPRESENTATION;
	}
	first_time = false;
	return retval;
}

OpenDDS::DCPS::Encoding::Kind QosDictionary::getEncodingKind()
{
	if (QosDictionary::getDataRepresentationType() == DDS::XCDR_DATA_REPRESENTATION)
	{
		return OpenDDS::DCPS::Encoding::KIND_XCDR1;
	}
	else
	{
		return OpenDDS::DCPS::Encoding::KIND_XCDR2;
	}
}

//------------------------------------------------------------------------------
DDS::TopicQos QosDictionary::Topic::bestEffort()
{
    DDS::TopicQos qos;

    init(qos);
    qos.durability.kind = DDS::VOLATILE_DURABILITY_QOS;
    qos.reliability.kind = DDS::BEST_EFFORT_RELIABILITY_QOS;

    return qos;
}


//------------------------------------------------------------------------------
DDS::TopicQos QosDictionary::Topic::latestReliableTransient()
{
    DDS::TopicQos qos;

    init(qos);
    qos.durability.kind = DDS::TRANSIENT_LOCAL_DURABILITY_QOS;
    qos.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;

    return qos;
}


//------------------------------------------------------------------------------
DDS::TopicQos QosDictionary::Topic::latestReliable()
{
    DDS::TopicQos qos;

    init(qos);
    qos.durability.kind = DDS::VOLATILE_DURABILITY_QOS;
    qos.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;

    return qos;
}


//------------------------------------------------------------------------------
DDS::TopicQos QosDictionary::Topic::strictReliable()
{
    DDS::TopicQos qos;

    init(qos);
    qos.durability.kind = DDS::VOLATILE_DURABILITY_QOS;
    qos.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;
	//RJ 2021-12-03 do not change this without also changing ddsmon.  see TopicInfo::fixHistory()
    qos.history.kind = DDS::KEEP_ALL_HISTORY_QOS;

    return qos;
}


//------------------------------------------------------------------------------
DDS::SubscriberQos QosDictionary::Subscriber::defaultQos()
{
    DDS::SubscriberQos qos;

    qos.presentation.access_scope = DDS::INSTANCE_PRESENTATION_QOS;
    qos.presentation.coherent_access = 0;
    qos.presentation.ordered_access = 0;
    qos.partition.name.length(0);
    qos.group_data.value.length(0);
    qos.entity_factory.autoenable_created_entities = true;

    return qos;
}


//------------------------------------------------------------------------------
DDS::PublisherQos QosDictionary::Publisher::defaultQos()
{
    DDS::PublisherQos qos;

    qos.presentation.access_scope = DDS::INSTANCE_PRESENTATION_QOS;
    qos.presentation.coherent_access = 0;
    qos.presentation.ordered_access = 0;
    qos.partition.name.length(0);
    qos.group_data.value.length(0);
    qos.entity_factory.autoenable_created_entities = true;

    return qos;
}


//------------------------------------------------------------------------------
DDS::DataReaderQos QosDictionary::DataReader::bestEffort()
{
    DDS::DataReaderQos qos;

    init(qos);
    qos.durability.kind = DDS::VOLATILE_DURABILITY_QOS;
    qos.reliability.kind = DDS::BEST_EFFORT_RELIABILITY_QOS;

    return qos;
}


//------------------------------------------------------------------------------
DDS::DataReaderQos QosDictionary::DataReader::latestReliableTransient()
{
    DDS::DataReaderQos qos;

    init(qos);
    qos.durability.kind = DDS::TRANSIENT_LOCAL_DURABILITY_QOS;
    qos.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;

    return qos;
}


//------------------------------------------------------------------------------
DDS::DataReaderQos QosDictionary::DataReader::latestReliable()
{
    DDS::DataReaderQos qos;

    init(qos);
    qos.durability.kind = DDS::VOLATILE_DURABILITY_QOS;
    qos.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;

    return qos;
}


//------------------------------------------------------------------------------
DDS::DataReaderQos QosDictionary::DataReader::strictReliable()
{
    DDS::DataReaderQos qos;

    init(qos);
    qos.durability.kind = DDS::VOLATILE_DURABILITY_QOS;
    qos.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;
	//RJ 2021-12-03 do not change this without also changing ddsmon.  see TopicInfo::fixHistory()
    qos.history.kind = DDS::KEEP_ALL_HISTORY_QOS;

    return qos;
}


//------------------------------------------------------------------------------
DDS::DataWriterQos QosDictionary::DataWriter::bestEffort()
{
    DDS::DataWriterQos qos;

    init(qos);
    qos.durability.kind = DDS::VOLATILE_DURABILITY_QOS;
    qos.reliability.kind = DDS::BEST_EFFORT_RELIABILITY_QOS;

    return qos;
}


//------------------------------------------------------------------------------
DDS::DataWriterQos QosDictionary::DataWriter::latestReliableTransient()
{
    DDS::DataWriterQos qos;

    init(qos);
    qos.durability.kind = DDS::TRANSIENT_LOCAL_DURABILITY_QOS;
    qos.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;

    return qos;
}


//------------------------------------------------------------------------------
DDS::DataWriterQos QosDictionary::DataWriter::latestReliable()
{
    DDS::DataWriterQos qos;

    init(qos);
    qos.durability.kind = DDS::VOLATILE_DURABILITY_QOS;
    qos.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;

    return qos;
}


//------------------------------------------------------------------------------
DDS::DataWriterQos QosDictionary::DataWriter::strictReliable()
{
    DDS::DataWriterQos qos;

    init(qos);
    qos.durability.kind = DDS::VOLATILE_DURABILITY_QOS;
    qos.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;
	//RJ 2021-12-03 do not change this without also changing ddsmon.  see TopicInfo::fixHistory()
    qos.history.kind = DDS::KEEP_ALL_HISTORY_QOS;

    return qos;
}


/**
 * @}
 */
