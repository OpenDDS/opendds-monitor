#include "qos_dictionary.h"
#include "open_dynamic_data.h"
#include "topic_monitor.h"
#include "dynamic_meta_struct.h"
#include "dds_manager.h"
#include "dds_data.h"
#include "qos_dictionary.h"

#include <dds/DCPS/EncapsulationHeader.h>
#include <dds/DCPS/Message_Block_Ptr.h>
#include <dds/DCPS/XTypes/DynamicTypeSupport.h>

#include <QDateTime>

#include <iostream>
#include <stdexcept>


//------------------------------------------------------------------------------
TopicMonitor::TopicMonitor(const QString& topicName)
    : m_topicName(topicName)
    , m_filter("")
    , m_recorder_listener(OpenDDS::DCPS::make_rch<RecorderListener>(OpenDDS::DCPS::ref(*this)))
    , m_recorder(nullptr)
    , m_dr_listener(new DataReaderListenerImpl(*this))
    , m_topic(nullptr)
    , m_paused(false)
{
    // Make sure we have an information object for this topic
    std::shared_ptr<TopicInfo> topicInfo = CommonData::getTopicInfo(topicName);
    if (topicInfo == nullptr)
    {
        throw std::runtime_error(std::string("Unable to find topic information for topic \"") + topicName.toStdString() + "\"");
    }

    // Store extensibility
    m_extensibility = topicInfo->extensibility();
    OpenDDS::DCPS::Service_Participant* service = TheServiceParticipant;
    DDS::DomainParticipant_var participant;
    if (CommonData::m_ddsManager)
    {
        participant = CommonData::m_ddsManager->getDomainParticipant();
    }

    if (!participant)
    {
        throw std::runtime_error("No domain participant");
    }

    // The operation mode is determined by whether the TypeCode for the topic is available
    // at the time the TopicMonitor is instantiated.
    if (topicInfo->typeCode())
    {
        // Use the existing mechanism based on TypeCode.
        m_typeCode = topicInfo->typeCode();
        m_topic = service->create_typeless_topic(participant,
                                                 topicInfo->topicName().c_str(),
                                                 topicInfo->typeName().c_str(),
                                                 topicInfo->hasKey(),
                                                 topicInfo->topicQos(),
                                                 new GenericTopicListener,
                                                 DDS::INCONSISTENT_TOPIC_STATUS);
        if (!m_topic)
        {
            throw std::runtime_error(std::string("Failed to create typeless topic \"") + topicInfo->topicName() + "\"");
        }

        m_recorder = service->create_recorder(participant,
                                              m_topic,
                                              topicInfo->subQos(),
                                              topicInfo->readerQos(),
                                              m_recorder_listener);
        if (!m_recorder)
        {
            throw std::runtime_error(std::string("Failed to create recorder for topic \"") + topicInfo->topicName() + "\"");
        }
        topicInfo->typeMode(TypeDiscoveryMode::TypeCode);
    }
    else
    {
        // Use DynamicDataReader instead.
        // When this is called, the information about this topic including its
        // DynamicType should be already obtained. The topic's type should also be
        // registered with the local domain participant.
        m_topic = participant->create_topic(topicInfo->topicName().c_str(),
                                            topicInfo->typeName().c_str(),
                                            topicInfo->topicQos(),
                                            0,
                                            0);
        if (!m_topic)
        {
            throw std::runtime_error(std::string("Failed to create topic \"") + topicInfo->topicName() + "\"");
        }

        DDS::Subscriber_var subscriber = participant->create_subscriber(topicInfo->subQos(),
                                                                        0,
                                                                        0);
        if (!subscriber)
        {
            throw std::runtime_error(std::string("Failed to create subscriber for topic \"") + topicInfo->topicName() + "\"");
        }

        m_dr = subscriber->create_datareader(m_topic,
                                             topicInfo->readerQos(),
                                             m_dr_listener,
                                             OpenDDS::DCPS::DEFAULT_STATUS_MASK);
        if (!m_dr)
        {
            throw std::runtime_error(std::string("Failed to create data reader for topic \"") + topicInfo->topicName() + "\"");
        }
        topicInfo->typeMode(TypeDiscoveryMode::DynamicType);
    }
}


//------------------------------------------------------------------------------
TopicMonitor::~TopicMonitor()
{
    close();
}


//------------------------------------------------------------------------------
void TopicMonitor::setFilter(const QString& filter)
{
    m_filter = filter;
}


//------------------------------------------------------------------------------
QString TopicMonitor::getFilter() const
{
    return m_filter;
}


//------------------------------------------------------------------------------
void TopicMonitor::close()
{
    // The set_deleted method became protected in v3.12, so this object can no
    // destroy itself. We're forced to simply stop updating, and leave this
    // object as a memory leak
    m_paused = true; // Bad hack

    if (m_recorder)
    {
        // The set_deleted is not visible in >=3.12
        //OpenDDS::DCPS::RecorderImpl* readerImpl =
        //    dynamic_cast<OpenDDS::DCPS::RecorderImpl*>(m_recorder);
        //if (readerImpl)
        //{
        //    readerImpl->remove_all_associations();
        //    readerImpl->set_deleted(true);
        //}

        OpenDDS::DCPS::Service_Participant* service = TheServiceParticipant;
        service->delete_recorder(m_recorder);
        m_recorder = nullptr;
    }

    DDS::DomainParticipant* domain = CommonData::m_ddsManager ? CommonData::m_ddsManager->getDomainParticipant() : nullptr;
    if (domain)
    {
        domain->delete_topic(m_topic);
    }
    m_topic = nullptr;
}


//------------------------------------------------------------------------------
void TopicMonitor::on_sample_data_received(OpenDDS::DCPS::Recorder*,
                                           const OpenDDS::DCPS::RawDataSample& rawSample)
{
    if (m_paused)
    {
        return;
    }

    OpenDDS::DCPS::Encoding::Kind globalEncoding = QosDictionary::getEncodingKind();

    if (rawSample.header_.message_id_ != OpenDDS::DCPS::SAMPLE_DATA)
    {
        std::cerr << "\nSkipping message that is not SAMPLE_DATA. This should not be possible! "
                  << "Check for compatibility with RecorderImpl::data_received()."
                  << std::endl;
        return;
    }

    if (globalEncoding != rawSample.encoding_kind_)
    {
        std::cerr << "Skipping message with encoding kind that does not match our encoding kind.\n"
                  << "Global Encoding Kind: " << OpenDDS::DCPS::Encoding::kind_to_string(globalEncoding) << "\n"
                  << "Raw Sample Encoding Kind: " << OpenDDS::DCPS::Encoding::kind_to_string(rawSample.encoding_kind_) << "\n"
                  << "There probably is a mismatch between the configuration of ddsmon and one or more publishers. "
                  << "Either the UseXTypes flag in opendds.ini or DDS_USE_OLD_CDR environment variable."
                  << std::endl;
        return;
    }

    OpenDDS::DCPS::Message_Block_Ptr mbCopy(rawSample.sample_->duplicate());
    OpenDDS::DCPS::Serializer serial(
        rawSample.sample_.get(), rawSample.encoding_kind_, static_cast<OpenDDS::DCPS::Endianness>(rawSample.header_.byte_order_));

    //RJ 2022-01-20 With OpenDDS 3.19.0, the entire message header is read before the sample gets passed to this function.
    //Code that strips off the RTPS header has been removed.
    //Same with the reset_alignment call in the serializer. That has already happened before the sample is passed to this function.

    std::shared_ptr<OpenDynamicData> sample = CreateOpenDynamicData(m_typeCode, globalEncoding, m_extensibility);
    if (globalEncoding != OpenDDS::DCPS::Encoding::KIND_XCDR1)
    {
        std::cout << "Removing delimiter header" << std::endl;
        uint32_t delim_header = 0;
        if (!(serial >> delim_header))
        {
            std::cerr << "TopicMonitor::on_sample_data_received: Could not read stream delimiter" << std::endl;
            return;
        }
    }

    (*(sample.get())) << serial;
    //sample->dump();

    // If a filter was specified, make sure the sample passes
    if (!m_filter.isEmpty())
    {
        bool pass = true;
        try
        {
            if (rawSample.header_.cdr_encapsulation_ &&
                mbCopy->rd_ptr() >= mbCopy->base() + OpenDDS::DCPS::EncapsulationHeader::serialized_size)
            {
                // Before calling this function, RecorderImpl::data_received() read the EncapsulationHeader
                // and advanced the rd_ptr() past that point.  The FilterEvaluator also needs to read this header.
                mbCopy->rd_ptr(mbCopy->rd_ptr() - OpenDDS::DCPS::EncapsulationHeader::serialized_size);
            }

            OpenDDS::DCPS::FilterEvaluator filterTest(m_filter.toUtf8().data(), false);
            DynamicMetaStruct metaInfo(sample);
            const DDS::StringSeq noParams;
            OpenDDS::DCPS::Encoding encoding(rawSample.encoding_kind_, static_cast<OpenDDS::DCPS::Endianness>(rawSample.header_.byte_order_));
            FilterTypeSupport typeSupport(metaInfo, m_extensibility);
            pass = filterTest.eval(mbCopy.get(), encoding, typeSupport, noParams);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Exception: " << e.what() << std::endl;
            pass = false;
        }

        if (!pass)
        {
            return;
        }

    }

    const QDateTime dataTime = QDateTime::fromMSecsSinceEpoch(
        (static_cast<unsigned long long>(rawSample.source_timestamp_.sec) * 1000) +
        (static_cast<unsigned long long>(rawSample.source_timestamp_.nanosec) * 1e-6));

    QString sampleName = dataTime.toString("HH:mm:ss.zzz");
    CommonData::storeSample(m_topicName, sampleName, sample);
}

void TopicMonitor::on_data_available(DDS::DataReader_ptr dr)
{
    if (m_paused) {
        return;
    }

    DDS::DynamicDataReader_var ddr = DDS::DynamicDataReader::_narrow(dr);
    DDS::DynamicDataSeq messages;
    DDS::SampleInfoSeq infos;
    const DDS::ReturnCode_t ret = ddr->take(messages, infos, DDS::LENGTH_UNLIMITED,
      DDS::ANY_SAMPLE_STATE, DDS::ANY_VIEW_STATE, DDS::ANY_INSTANCE_STATE);
    if (ret != DDS::RETCODE_OK && ret != DDS::RETCODE_NO_DATA) {
        std::cerr << "Failed to take samples for topic "
                  << m_topicName.toStdString() << std::endl;
        return;
    }

    for (unsigned int i = 0; i < messages.length(); ++i) {
        if (infos[i].valid_data)
        {
            bool passFilter = true;
            if (!m_filter.isEmpty())
            {
                try
                {
                    passFilter = evaluateDynamicFilter(messages[i].in(), m_filter);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Filter evaluation failed for topic "
                              << m_topicName.toStdString() << ": " << e.what() << std::endl;
                    passFilter = false;
                }

                if (!passFilter)
                {
                    continue; // Skip this sample
                }
            }

            QDateTime dataTime = QDateTime::fromMSecsSinceEpoch(
                (static_cast<unsigned long long>(infos[i].source_timestamp.sec) * 1000) +
                (static_cast<unsigned long long>(infos[i].source_timestamp.nanosec) * 1e-6));
            QString sampleName = dataTime.toString("HH:mm:ss.zzz");
            CommonData::storeDynamicSample(m_topicName, sampleName,
                                           DDS::DynamicData::_duplicate(messages[i].in()));
        }

    }
}

//------------------------------------------------------------------------------
void TopicMonitor::pause()
{
    m_paused = true;
}


//------------------------------------------------------------------------------
void TopicMonitor::unpause()
{
    m_paused = false;
}

bool TopicMonitor::evaluateDynamicFilter(DDS::DynamicData_ptr data, const QString &filter)
{
    // Simple filter implementation for basic field comparisons
    QString trimmedFilter = filter.trimmed();

    QStringList operators = {">=", "<=", "!=", "=", ">", "<"};
    QString op;
    QString fieldName;
    QString value;

    for (const QString &testOp : operators)
    {
        int pos = trimmedFilter.indexOf(testOp);
        if (pos > 0)
        {
            op = testOp;
            fieldName = trimmedFilter.left(pos).trimmed();
            value = trimmedFilter.mid(pos + testOp.length()).trimmed();
            break;
        }
    }

    if (op.isEmpty() || fieldName.isEmpty())
    {
        std::cerr << "Invalid filter format: " << filter.toStdString() << std::endl;
        return false;
    }

    // Remove quotes if present
    if (value.startsWith('"') && value.endsWith('"'))
    {
        value = value.mid(1, value.length() - 2);
    }
    else if (value.startsWith('\'') && value.endsWith('\''))
    {
        value = value.mid(1, value.length() - 2);
    }

    try
    {
        // Get the member ID for the field
        DDS::DynamicType_var type = data->type();
        DDS::MemberId memberId = 0;

        // Try to find member by name
        bool memberFound = false;
        DDS::UInt32 memberCount = type->get_member_count();

        for (DDS::UInt32 i = 0; i < memberCount; ++i)
        {
            DDS::DynamicTypeMember_var member;
            if (type->get_member_by_index(member, i) == DDS::RETCODE_OK)
            {
                DDS::MemberDescriptor_var desc;
                if (member->get_descriptor(desc) == DDS::RETCODE_OK)
                {
                    if (QString(desc->name()) == fieldName)
                    {
                        memberId = desc->id();
                        memberFound = true;
                        break;
                    }
                }
            }
        }

        if (!memberFound)
        {
            std::cerr << "Field '" << fieldName.toStdString() << "' not found in type" << std::endl;
            return false;
        }

        // Get the field value based on type
        DDS::TypeKind kind = type->get_kind();
        DDS::DynamicTypeMember_var member;
        type->get_member(member, memberId);
        DDS::MemberDescriptor_var desc;
        member->get_descriptor(desc);
        DDS::TypeKind memberKind = desc->type()->get_kind();

        // Handle different data types using OpenDDS::XTypes constants
        using namespace OpenDDS::XTypes;
        switch (memberKind)
        {
        case TK_INT32:
        {
            DDS::Int32 fieldValue;
            if (data->get_int32_value(fieldValue, memberId) == DDS::RETCODE_OK)
            {
                int filterValue = value.toInt();
                return compareValues(fieldValue, filterValue, op);
            }
            break;
        }
        case TK_UINT32:
        {
            DDS::UInt32 fieldValue;
            if (data->get_uint32_value(fieldValue, memberId) == DDS::RETCODE_OK)
            {
                unsigned int filterValue = value.toUInt();
                return compareValues(fieldValue, filterValue, op);
            }
            break;
        }
        case TK_INT16:
        {
            DDS::Int16 fieldValue;
            if (data->get_int16_value(fieldValue, memberId) == DDS::RETCODE_OK)
            {
                short filterValue = value.toShort();
                return compareValues(fieldValue, filterValue, op);
            }
            break;
        }
        case TK_UINT16:
        {
            DDS::UInt16 fieldValue;
            if (data->get_uint16_value(fieldValue, memberId) == DDS::RETCODE_OK)
            {
                unsigned short filterValue = value.toUShort();
                return compareValues(fieldValue, filterValue, op);
            }
            break;
        }
        case TK_FLOAT32:
        {
            DDS::Float32 fieldValue;
            if (data->get_float32_value(fieldValue, memberId) == DDS::RETCODE_OK)
            {
                float filterValue = value.toFloat();
                return compareValues(fieldValue, filterValue, op);
            }
            break;
        }
        case TK_FLOAT64:
        {
            DDS::Float64 fieldValue;
            if (data->get_float64_value(fieldValue, memberId) == DDS::RETCODE_OK)
            {
                double filterValue = value.toDouble();
                return compareValues(fieldValue, filterValue, op);
            }
            break;
        }
        case TK_STRING8:
        {
            DDS::String8_var fieldValue;
            if (data->get_string_value(fieldValue, memberId) == DDS::RETCODE_OK)
            {
                QString fieldStr = QString::fromUtf8(fieldValue);
                return compareStrings(fieldStr, value, op);
            }
            break;
        }
        case TK_BOOLEAN:
        {
            DDS::Boolean fieldValue;
            if (data->get_boolean_value(fieldValue, memberId) == DDS::RETCODE_OK)
            {
                bool filterValue = (value.toLower() == "true" || value == "1");
                if (op == "=")
                    return fieldValue == filterValue;
                if (op == "!=")
                    return fieldValue != filterValue;
            }
            break;
        }
        default:
            std::cerr << "Unsupported field type for filtering: " << memberKind << std::endl;
            return false;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception in filter evaluation: " << e.what() << std::endl;
        return false;
    }

    return false;
}

template <typename T>
bool TopicMonitor::compareValues(const T &fieldValue, const T &filterValue, const QString &op)
{
    if (op == "=")
        return fieldValue == filterValue;
    if (op == "!=")
        return fieldValue != filterValue;
    if (op == ">")
        return fieldValue > filterValue;
    if (op == "<")
        return fieldValue < filterValue;
    if (op == ">=")
        return fieldValue >= filterValue;
    if (op == "<=")
        return fieldValue <= filterValue;
    return false;
}

bool TopicMonitor::compareStrings(const QString &fieldValue, const QString &filterValue, const QString &op)
{
    if (op == "=")
        return fieldValue == filterValue;
    if (op == "!=")
        return fieldValue != filterValue;
    if (op == ">")
        return fieldValue > filterValue;
    if (op == "<")
        return fieldValue < filterValue;
    if (op == ">=")
        return fieldValue >= filterValue;
    if (op == "<=")
        return fieldValue <= filterValue;
    return false;
}



#if OPENDDS_MAJOR_VERSION == 3 && OPENDDS_MINOR_VERSION >= 24
TopicMonitor::FilterTypeSupport::FilterTypeSupport(const DynamicMetaStruct& metastruct, OpenDDS::DCPS::Extensibility exten)
  : meta_(metastruct)
  , ext_(exten)
{}

const OpenDDS::DCPS::MetaStruct&
TopicMonitor::FilterTypeSupport::getMetaStructForType() const
{
  return meta_;
}

OpenDDS::DCPS::SerializedSizeBound
TopicMonitor::FilterTypeSupport::serialized_size_bound(const OpenDDS::DCPS::Encoding&) const
{
  return OpenDDS::DCPS::SerializedSizeBound();
}

OpenDDS::DCPS::SerializedSizeBound
TopicMonitor::FilterTypeSupport::key_only_serialized_size_bound(const OpenDDS::DCPS::Encoding&) const
{
  return OpenDDS::DCPS::SerializedSizeBound();
}

OpenDDS::DCPS::Extensibility
TopicMonitor::FilterTypeSupport::max_extensibility() const
{
  return OpenDDS::DCPS::FINAL;
}

OpenDDS::XTypes::TypeIdentifier&
TopicMonitor::FilterTypeSupport::getMinimalTypeIdentifier() const
{
  static OpenDDS::XTypes::TypeIdentifier ti;
  return ti;
}

const OpenDDS::XTypes::TypeMap&
TopicMonitor::FilterTypeSupport::getMinimalTypeMap() const
{
  static OpenDDS::XTypes::TypeMap tm;
  return tm;
}

const OpenDDS::XTypes::TypeIdentifier&
TopicMonitor::FilterTypeSupport::getCompleteTypeIdentifier() const
{
  static OpenDDS::XTypes::TypeIdentifier ti;
  return ti;
}

const OpenDDS::XTypes::TypeMap&
TopicMonitor::FilterTypeSupport::getCompleteTypeMap() const
{
  static OpenDDS::XTypes::TypeMap tm;
  return tm;
}
#endif

/**
 * @}
 */
