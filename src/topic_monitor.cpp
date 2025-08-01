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
#include <dds/DCPS/DomainParticipantImpl.h>

#include <QDateTime>
#include <QRegExp>

#include <iostream>
#include <stdexcept>

//------------------------------------------------------------------------------
TopicMonitor::TopicMonitor(const QString &topicName)
    : m_topicName(topicName), m_filter(""), m_recorder_listener(OpenDDS::DCPS::make_rch<RecorderListener>(OpenDDS::DCPS::ref(*this))), m_recorder(nullptr), m_dr_listener(new DataReaderListenerImpl(*this)), m_topic(nullptr), m_paused(false)
{
    std::cout << "=== TopicMonitor Constructor Start ===" << std::endl;
    std::cout << "Initializing TopicMonitor for topic: \"" << topicName.toStdString() << "\"" << std::endl;

    // Make sure we have an information object for this topic
    std::shared_ptr<TopicInfo> topicInfo = CommonData::getTopicInfo(topicName);
    if (topicInfo == nullptr)
    {
        std::cerr << "CRITICAL ERROR: Unable to find topic information for topic \"" << topicName.toStdString() << "\"" << std::endl;
        throw std::runtime_error(std::string("Unable to find topic information for topic \"") + topicName.toStdString() + "\"");
    }

    std::cout << "Topic info retrieved successfully:" << std::endl;
    std::cout << "  - Topic name: \"" << topicInfo->topicName() << "\"" << std::endl;
    std::cout << "  - Type name: \"" << topicInfo->typeName() << "\"" << std::endl;
    std::cout << "  - Has TypeCode: " << (topicInfo->typeCode() ? "YES" : "NO") << std::endl;
    std::cout << "  - Has key: " << (topicInfo->hasKey() ? "YES" : "NO") << std::endl;

    // Store extensibility
    m_extensibility = topicInfo->extensibility();
    OpenDDS::DCPS::Service_Participant *service = TheServiceParticipant;
    DDS::DomainParticipant_var participant;
    if (CommonData::m_ddsManager)
    {
        participant = CommonData::m_ddsManager->getDomainParticipant();
    }

    if (!participant)
    {
        std::cerr << "CRITICAL ERROR: No domain participant available!" << std::endl;
        std::cerr << "  - CommonData::m_ddsManager exists: " << (CommonData::m_ddsManager ? "YES" : "NO") << std::endl;
        throw std::runtime_error("No domain participant");
    }

    // Add participant debugging information
    std::cout << "Domain participant obtained successfully" << std::endl;
    DDS::DomainId_t domainId = participant->get_domain_id();
    std::cout << "Domain participant domain ID: " << domainId << std::endl;

    // Check if we can cast to OpenDDS implementation
    OpenDDS::DCPS::DomainParticipantImpl *participantImpl =
        dynamic_cast<OpenDDS::DCPS::DomainParticipantImpl *>(participant.in());
    if (!participantImpl)
    {
        std::cerr << "WARNING: Could not cast participant to OpenDDS::DCPS::DomainParticipantImpl" << std::endl;
        std::cerr << "This may limit debugging capabilities but should not prevent topic creation" << std::endl;
    }
    else
    {
        std::cout << "Successfully cast participant to OpenDDS implementation" << std::endl;
    }

    // The operation mode is determined by whether the TypeCode for the topic is available
    // at the time the TopicMonitor is instantiated.
    if (topicInfo->typeCode())
    {
        // Use the existing mechanism based on TypeCode.
        std::cout << "Creating topic with TypeCode approach..." << std::endl;
        std::cout << "Topic name: \"" << topicInfo->topicName() << "\"" << std::endl;
        std::cout << "Type name: \"" << topicInfo->typeName() << "\"" << std::endl;
        std::cout << "Has key: " << (topicInfo->hasKey() ? "YES" : "NO") << std::endl;

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
            std::cerr << "DETAILED ERROR: Failed to create typeless topic \"" << topicInfo->topicName() << "\"" << std::endl;
            std::cerr << "  - Topic name: \"" << topicInfo->topicName() << "\"" << std::endl;
            std::cerr << "  - Type name: \"" << topicInfo->typeName() << "\"" << std::endl;
            std::cerr << "  - Has key: " << (topicInfo->hasKey() ? "YES" : "NO") << std::endl;
            std::cerr << "  - Domain ID: " << participant->get_domain_id() << std::endl;
            throw std::runtime_error(std::string("Failed to create typeless topic \"") + topicInfo->topicName() + "\"");
        }

        std::cout << "Typeless topic \"" << topicInfo->topicName() << "\" created successfully" << std::endl;

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

        std::cout << "Creating topic with DynamicDataReader approach..." << std::endl;
        std::cout << "Topic name: \"" << topicInfo->topicName() << "\"" << std::endl;
        std::cout << "Type name: \"" << topicInfo->typeName() << "\"" << std::endl;

        // Check if we can cast to OpenDDS implementation for debugging
        OpenDDS::DCPS::DomainParticipantImpl *participantImpl =
            dynamic_cast<OpenDDS::DCPS::DomainParticipantImpl *>(participant.in());

        if (!participantImpl)
        {
            std::cerr << "WARNING: Could not cast to OpenDDS::DCPS::DomainParticipantImpl" << std::endl;
            std::cerr << "Type registration check will be skipped" << std::endl;
        }
        else
        {
            std::cout << "Successfully cast participant to OpenDDS implementation" << std::endl;
        }

        // Check if a topic with this name already exists
        DDS::Topic_var existingTopic = participant->find_topic(topicInfo->topicName().c_str(), DDS::Duration_t{0, 0});
        if (existingTopic)
        {
            std::cout << "Topic \"" << topicInfo->topicName() << "\" already exists" << std::endl;
            // Check if the existing topic has the same type
            CORBA::String_var existingTypeName = existingTopic->get_type_name();
            if (strcmp(existingTypeName.in(), topicInfo->typeName().c_str()) != 0)
            {
                std::cerr << "ERROR: Topic \"" << topicInfo->topicName()
                          << "\" already exists with different type name: \""
                          << existingTypeName.in() << "\" (expected: \""
                          << topicInfo->typeName() << "\")" << std::endl;
            }
        }

        // Validate QoS settings
        DDS::TopicQos topicQos = topicInfo->topicQos();
        std::cout << "Topic QoS durability kind: " << topicQos.durability.kind << std::endl;
        std::cout << "Topic QoS reliability kind: " << topicQos.reliability.kind << std::endl;

        // Check domain participant status
        DDS::DomainParticipantQos participantQos;
        if (participant->get_qos(participantQos) == DDS::RETCODE_OK)
        {
            std::cout << "Domain participant QoS retrieved successfully" << std::endl;
        }
        else
        {
            std::cerr << "WARNING: Could not retrieve domain participant QoS" << std::endl;
        }

        // Check domain ID
        DDS::DomainId_t domainId = participant->get_domain_id();
        std::cout << "Domain participant domain ID: " << domainId << std::endl;

        m_topic = participant->create_topic(topicInfo->topicName().c_str(),
                                            topicInfo->typeName().c_str(),
                                            topicInfo->topicQos(),
                                            0,
                                            0);
        if (!m_topic)
        {
            // Enhanced error reporting
            std::cerr << "DETAILED ERROR: Failed to create topic \"" << topicInfo->topicName() << "\"" << std::endl;
            std::cerr << "  - Topic name: \"" << topicInfo->topicName() << "\"" << std::endl;
            std::cerr << "  - Type name: \"" << topicInfo->typeName() << "\"" << std::endl;
            std::cerr << "  - Domain ID: " << domainId << std::endl;
            std::cerr << "  - OpenDDS cast successful: " << (participantImpl ? "YES" : "NO") << std::endl;
            std::cerr << "  - Existing topic found: " << (existingTopic ? "YES" : "NO") << std::endl;

            // Try to get more specific error information
            DDS::StatusCondition_var condition = participant->get_statuscondition();
            if (condition)
            {
                DDS::StatusMask mask = condition->get_enabled_statuses();
                std::cerr << "  - Participant status mask: 0x" << std::hex << mask << std::dec << std::endl;
            }

            throw std::runtime_error(std::string("Failed to create topic \"") + topicInfo->topicName() +
                                     "\". Check type registration and QoS compatibility.");
        }

        std::cout << "Topic \"" << topicInfo->topicName() << "\" created successfully" << std::endl;

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

bool TopicMonitor::isTypeRegistered(DDS::DomainParticipant_ptr participant, const std::string &typeName) const
{
    if (!participant)
    {
        return false;
    }

    // Cast to OpenDDS implementation to access lookup_typesupport
    OpenDDS::DCPS::DomainParticipantImpl *participantImpl =
        dynamic_cast<OpenDDS::DCPS::DomainParticipantImpl *>(participant);

    if (!participantImpl)
    {
        std::cerr << "WARNING: Could not cast to OpenDDS::DCPS::DomainParticipantImpl in isTypeRegistered" << std::endl;
        return false;
    }

    // Note: The lookup_typesupport method may not be available in all OpenDDS versions
    // This method is a simplified version that returns true to indicate we cannot verify
    std::cout << "Type registration check skipped - method not available in this OpenDDS version" << std::endl;
    return true;
}

void TopicMonitor::logParticipantTypes(DDS::DomainParticipant_ptr participant) const
{
    if (!participant)
    {
        std::cerr << "Cannot log types - participant is null" << std::endl;
        return;
    }
    std::cout << "Type enumeration not available through standard DDS API" << std::endl;
}

//------------------------------------------------------------------------------
TopicMonitor::~TopicMonitor()
{
    close();
}

//------------------------------------------------------------------------------
void TopicMonitor::setFilter(const QString &filter)
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
        // OpenDDS::DCPS::RecorderImpl* readerImpl =
        //    dynamic_cast<OpenDDS::DCPS::RecorderImpl*>(m_recorder);
        // if (readerImpl)
        //{
        //    readerImpl->remove_all_associations();
        //    readerImpl->set_deleted(true);
        //}

        OpenDDS::DCPS::Service_Participant *service = TheServiceParticipant;
        service->delete_recorder(m_recorder);
        m_recorder = nullptr;
    }

    DDS::DomainParticipant *domain = CommonData::m_ddsManager ? CommonData::m_ddsManager->getDomainParticipant() : nullptr;
    if (domain)
    {
        domain->delete_topic(m_topic);
    }
    m_topic = nullptr;
}

//------------------------------------------------------------------------------
void TopicMonitor::on_sample_data_received(OpenDDS::DCPS::Recorder *,
                                           const OpenDDS::DCPS::RawDataSample &rawSample)
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

    // RJ 2022-01-20 With OpenDDS 3.19.0, the entire message header is read before the sample gets passed to this function.
    // Code that strips off the RTPS header has been removed.
    // Same with the reset_alignment call in the serializer. That has already happened before the sample is passed to this function.

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
    // sample->dump();

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
        catch (const std::exception &e)
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
    if (m_paused)
    {
        return;
    }

    DDS::DynamicDataReader_var ddr = DDS::DynamicDataReader::_narrow(dr);
    DDS::DynamicDataSeq messages;
    DDS::SampleInfoSeq infos;
    const DDS::ReturnCode_t ret = ddr->take(messages, infos, DDS::LENGTH_UNLIMITED,
                                            DDS::ANY_SAMPLE_STATE, DDS::ANY_VIEW_STATE, DDS::ANY_INSTANCE_STATE);
    if (ret != DDS::RETCODE_OK && ret != DDS::RETCODE_NO_DATA)
    {
        std::cerr << "Failed to take samples for topic "
                  << m_topicName.toStdString() << std::endl;
        return;
    }

    for (unsigned int i = 0; i < messages.length(); ++i)
    {
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
    try
    {
        return evaluateFilterExpression(data, cleanFilter(filter));
    }
    catch (const std::exception &e)
    {
        std::cerr << "Filter evaluation error: " << e.what() << std::endl;
        return false;
    }
}

QString TopicMonitor::cleanFilter(const QString &filter)
{
    QString cleaned = filter.simplified();
    cleaned.replace(QRegExp("\\s+"), " ");                            // Replace multiple spaces with a single space
    cleaned.replace(QRegExp("--.*"), "");                             // Remove comments starting with --
    cleaned.replace(QRegExp("/\\*.*?\\*/", Qt::CaseInsensitive), ""); // Remove C-style comments
    return cleaned.trimmed();
}

bool TopicMonitor::evaluateFilterExpression(DDS::DynamicData_ptr data, const QString &expression)
{
    QString expr = expression.trimmed();
    if (expr.isEmpty())
    {
        return true;
    }

    // parentheses first
    while (expr.contains('('))
    {
        int depth = 0;
        int start = -1;
        int end = -1;

        for (int i = 0; i < expr.length(); ++i)
        {
            if (expr[i] == '(')
            {
                if (depth == 0)
                    start = i;
                depth++;
            }
            else if (expr[i] == ')')
            {
                depth--;
                if (depth == 0)
                {
                    end = i;
                    break;
                }
            }
        }

        if (start == -1 || end == -1)
        {
            throw std::runtime_error("Mismatched parentheses in filter expression");
        }

        QString innerExpr = expr.mid(start + 1, end - start - 1);
        bool innerResult = evaluateFilterExpression(data, innerExpr);

        // Replace the parenthetical expression with its result
        expr = expr.left(start) + (innerResult ? "TRUE" : "FALSE") + expr.mid(end + 1);
    }

    // OR is the lowest precedence
    QStringList orParts = splitOnOperator(expr, " OR ");
    if (orParts.size() > 1)
    {
        for (const QString &part : orParts)
        {
            if (evaluateFilterExpression(data, part))
            {
                return true;
            }
        }
        return false;
    }

    // Higher than OR
    QStringList andParts = splitOnOperator(expr, " AND ");
    if (andParts.size() > 1)
    {
        for (const QString &part : andParts)
        {
            if (!evaluateFilterExpression(data, part))
            {
                return false;
            }
        }
        return true;
    }

    // Highest precedence
    if (expr.startsWith("NOT ", Qt::CaseInsensitive))
    {
        QString notExpr = expr.mid(4).trimmed();
        return !evaluateFilterExpression(data, notExpr);
    }

    // Handle boolean literals
    if (expr.compare("TRUE", Qt::CaseInsensitive) == 0)
    {
        return true;
    }
    if (expr.compare("FALSE", Qt::CaseInsensitive) == 0)
    {
        return false;
    }

    // basic comparison
    return evaluateSimpleComparison(data, expr);
}

QStringList TopicMonitor::splitOnOperator(const QString &expression, const QString &op)
{
    QStringList parts;
    int pos = 0;
    int depth = 0;
    int lastSplit = 0;

    while (pos < expression.length())
    {
        if (expression[pos] == '(')
        {
            depth++;
        }
        else if (expression[pos] == ')')
        {
            depth--;
        }
        else if (depth == 0 && expression.mid(pos).startsWith(op, Qt::CaseInsensitive))
        {
            parts.append(expression.mid(lastSplit, pos - lastSplit).trimmed());
            pos += op.length();
            lastSplit = pos;
            continue;
        }
        pos++;
    }

    if (lastSplit < expression.length())
    {
        parts.append(expression.mid(lastSplit).trimmed());
    }

    return parts;
}

bool TopicMonitor::evaluateSimpleComparison(DDS::DynamicData_ptr data, const QString &expression)
{
    QString expr = expression.trimmed();

    QStringList operators = {">=", "<=", "!=", "<>", "=", ">", "<", " LIKE ", " IN ", " NOT IN "};
    QString op;
    QString fieldName;
    QString value;

    for (const QString &testOp : operators)
    {
        int pos = expr.indexOf(testOp, 0, Qt::CaseInsensitive);
        if (pos > 0)
        {
            op = testOp.trimmed().toUpper();
            fieldName = expr.left(pos).trimmed();
            value = expr.mid(pos + testOp.length()).trimmed();
            break;
        }
    }

    if (op.isEmpty() || fieldName.isEmpty())
    {
        std::cerr << "Invalid comparison format: " << expression.toStdString() << std::endl;
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

    if (op == "<>")
    {
        op = "!=";
    }

    if (op == "IN" || op == "NOT IN")
    {
        return evaluateInOperator(data, fieldName, value, op == "NOT IN");
    }

    if (op == "LIKE")
    {
        return evaluateLikeOperator(data, fieldName, value);
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
        case TK_INT8:
        {
            DDS::Int8 fieldValue;
            if (data->get_int8_value(fieldValue, memberId) == DDS::RETCODE_OK)
            {
                int filterValue = value.toInt();
                return compareValues(static_cast<int>(fieldValue), filterValue, op);
            }
            break;
        }
        case TK_UINT8:
        {
            DDS::UInt8 fieldValue;
            if (data->get_uint8_value(fieldValue, memberId) == DDS::RETCODE_OK)
            {
                unsigned int filterValue = value.toUInt();
                return compareValues(static_cast<unsigned int>(fieldValue), filterValue, op);
            }
            break;
        }
        case TK_INT64:
        {
            DDS::Int64 fieldValue;
            if (data->get_int64_value(fieldValue, memberId) == DDS::RETCODE_OK)
            {
                DDS::Int64 filterValue = static_cast<DDS::Int64>(value.toLongLong());
                return compareValues(fieldValue, filterValue, op);
            }
            break;
        }
        case TK_UINT64:
        {
            DDS::UInt64 fieldValue;
            if (data->get_uint64_value(fieldValue, memberId) == DDS::RETCODE_OK)
            {
                DDS::UInt64 filterValue = static_cast<DDS::UInt64>(value.toULongLong());
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
        case TK_CHAR8:
        {
            DDS::Char8 fieldValue;
            if (data->get_char8_value(fieldValue, memberId) == DDS::RETCODE_OK)
            {
                char filterValue = value.isEmpty() ? '\0' : value[0].toLatin1();
                return compareValues(fieldValue, filterValue, op);
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
        std::cerr << "Exception in simple comparison evaluation: " << e.what() << std::endl;
        return false;
    }

    return false;
}

bool TopicMonitor::evaluateInOperator(DDS::DynamicData_ptr data, const QString &fieldName, const QString &valueList, bool isNotIn)
{
    // Parse the value list (e.g., "(1, 2, 3)" or "('a', 'b', 'c')")
    QString cleanList = valueList.trimmed();
    if (cleanList.startsWith('(') && cleanList.endsWith(')'))
    {
        cleanList = cleanList.mid(1, cleanList.length() - 2);
    }

    QStringList values = cleanList.split(',');
    for (QString &val : values)
    {
        val = val.trimmed();
        if (val.startsWith('"') && val.endsWith('"'))
        {
            val = val.mid(1, val.length() - 2);
        }
        else if (val.startsWith('\'') && val.endsWith('\''))
        {
            val = val.mid(1, val.length() - 2);
        }
    }

    // Check if field value matches any of the values in the list
    for (const QString &val : values)
    {
        QString comparison = fieldName + " = " + val;
        if (evaluateSimpleComparison(data, comparison))
        {
            return !isNotIn;
        }
    }

    return isNotIn;
}

bool TopicMonitor::evaluateLikeOperator(DDS::DynamicData_ptr data, const QString &fieldName, const QString &pattern)
{
    try
    {
        DDS::DynamicType_var type = data->type();
        DDS::MemberId memberId = 0;
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
            std::cerr << "Field '" << fieldName.toStdString() << "' not found for LIKE operation" << std::endl;
            return false;
        }

        DDS::DynamicTypeMember_var member;
        type->get_member(member, memberId);
        DDS::MemberDescriptor_var desc;
        member->get_descriptor(desc);
        DDS::TypeKind memberKind = desc->type()->get_kind();

        QString fieldValue;

        if (memberKind == OpenDDS::XTypes::TK_STRING8)
        {
            DDS::String8_var stringValue;
            if (data->get_string_value(stringValue, memberId) == DDS::RETCODE_OK)
            {
                fieldValue = QString::fromUtf8(stringValue);
            }
        }
        else
        {
            std::cerr << "LIKE operator only supported for string fields" << std::endl;
            return false;
        }

        // Convert to regex
        QString regexPattern = sqlLikeToRegex(pattern);
        QRegExp regex(regexPattern, Qt::CaseInsensitive);
        return regex.exactMatch(fieldValue);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception in LIKE evaluation: " << e.what() << std::endl;
        return false;
    }
}

QString TopicMonitor::sqlLikeToRegex(const QString &likePattern)
{
    QString regex = likePattern;

    regex.replace("\\", "\\\\");
    regex.replace(".", "\\.");
    regex.replace("^", "\\^");
    regex.replace("$", "\\$");
    regex.replace("+", "\\+");
    regex.replace("?", "\\?");
    regex.replace("*", "\\*");
    regex.replace("[", "\\[");
    regex.replace("]", "\\]");
    regex.replace("{", "\\{");
    regex.replace("}", "\\}");
    regex.replace("(", "\\(");
    regex.replace(")", "\\)");
    regex.replace("|", "\\|");

    regex.replace("%", ".*");
    regex.replace("_", ".");

    return regex;
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
TopicMonitor::FilterTypeSupport::FilterTypeSupport(const DynamicMetaStruct &metastruct, OpenDDS::DCPS::Extensibility exten)
    : meta_(metastruct), ext_(exten)
{
}

const OpenDDS::DCPS::MetaStruct &
TopicMonitor::FilterTypeSupport::getMetaStructForType() const
{
    return meta_;
}

OpenDDS::DCPS::SerializedSizeBound
TopicMonitor::FilterTypeSupport::serialized_size_bound(const OpenDDS::DCPS::Encoding &) const
{
    return OpenDDS::DCPS::SerializedSizeBound();
}

OpenDDS::DCPS::SerializedSizeBound
TopicMonitor::FilterTypeSupport::key_only_serialized_size_bound(const OpenDDS::DCPS::Encoding &) const
{
    return OpenDDS::DCPS::SerializedSizeBound();
}

OpenDDS::DCPS::Extensibility
TopicMonitor::FilterTypeSupport::max_extensibility() const
{
    return OpenDDS::DCPS::FINAL;
}

OpenDDS::XTypes::TypeIdentifier &
TopicMonitor::FilterTypeSupport::getMinimalTypeIdentifier() const
{
    static OpenDDS::XTypes::TypeIdentifier ti;
    return ti;
}

const OpenDDS::XTypes::TypeMap &
TopicMonitor::FilterTypeSupport::getMinimalTypeMap() const
{
    static OpenDDS::XTypes::TypeMap tm;
    return tm;
}

const OpenDDS::XTypes::TypeIdentifier &
TopicMonitor::FilterTypeSupport::getCompleteTypeIdentifier() const
{
    static OpenDDS::XTypes::TypeIdentifier ti;
    return ti;
}

const OpenDDS::XTypes::TypeMap &
TopicMonitor::FilterTypeSupport::getCompleteTypeMap() const
{
    static OpenDDS::XTypes::TypeMap tm;
    return tm;
}
#endif

/**
 * @}
 */
