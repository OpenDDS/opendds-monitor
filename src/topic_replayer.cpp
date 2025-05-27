#include "topic_replayer.h"
#include "open_dynamic_data.h"
#include "dds_manager.h"
#include "dds_data.h"
#include "qos_dictionary.h"

#include <dds/DCPS/EncapsulationHeader.h>

#include <QDateTime>
#include <iostream>


//------------------------------------------------------------------------------
TopicReplayer::TopicReplayer(const QString& topicName) :
    m_topicName(topicName),
    m_typeCode(nullptr),
    m_topic(nullptr),
    m_replayer(nullptr)
{
    // Make sure we have an information object for this topic
    std::shared_ptr<TopicInfo> topicInfo = CommonData::getTopicInfo(topicName);
    if (topicInfo == nullptr)
    {
        std::cerr << "Unable to find topic information for "
                  << topicName.toStdString()
                  << std::endl;
        return;
    }

    // Make sure the type code is valid
    m_typeCode = topicInfo->typeCode();
    if (m_typeCode == nullptr)
    {
        std::cerr << "Unable to find type code information for "
                  << topicName.toStdString()
                  << std::endl;
        return;
    }

    // Store extensibility
    m_extensibility = topicInfo->extensibility();


    OpenDDS::DCPS::Service_Participant* service = TheServiceParticipant;
    DDS::DomainParticipant* domain = CommonData::m_ddsManager->getDomainParticipant();

    m_topic = service->create_typeless_topic(domain,
        topicInfo->topicName().c_str(),
        topicInfo->typeName().c_str(),
        topicInfo->hasKey(),
        topicInfo->topicQos(),
        new GenericTopicListener,
        DDS::INCONSISTENT_TOPIC_STATUS);

    if (!m_topic)
    {
        std::cerr << "Failed to create topic \"" << topicInfo->topicName() << "\"" << std::endl;
        return;
    }

    m_replayer = service->create_replayer(
        domain,
        m_topic,
        topicInfo->pubQos(),
        topicInfo->writerQos(),
        OpenDDS::DCPS::RcHandle<OpenDDS::DCPS::ReplayerListener>()
    );

    if (!m_replayer)
    {
        std::cerr << "Failed to created replayer" << std::endl;
        return;
    }
}


//------------------------------------------------------------------------------
void TopicReplayer::publishSample(const std::shared_ptr<OpenDynamicData> sample)
{
    OpenDDS::DCPS::Encoding::Kind globalEncoding = QosDictionary::getEncodingKind();
    //This used to be hard-coded to 4096.
    ssize_t num_data_bytes = sample->getEncapsulationLength();
    //if(globalEncoding !=  OpenDDS::DCPS::Encoding::KIND_XCDR1)
    {
        //add 2 bytes for encaspsulation header, 2 bytes for 'options'
        //But for whatever reason that I do not understand that still isn't enough.
        //I have spent more time on this than I have so i'm going to just add 1kb and hope that's enough.
        num_data_bytes += sizeof(CORBA::ULong) + 1024;
    }
    //std::cout << "DEBUG Size of new ace message block: " << num_data_bytes << std::endl;
    ACE_Message_Block block(num_data_bytes);

    //num_data_bytes += sizeof(encap);
    OpenDDS::DCPS::Serializer serial(&block, globalEncoding);
    bool pass = true;

    //Create Encoding and EncapsulationHeader
    OpenDDS::DCPS::Encoding enc(globalEncoding, OpenDDS::DCPS::ENDIAN_LITTLE);
    const OpenDDS::DCPS::EncapsulationHeader encap(enc, m_extensibility);
    if (!encap.is_good()) {
        std::cerr << "TopicReplayer::publishSample "
                  <<"failed to initialize Encapsulation Header"
                  << std::endl;
        return;
    }
    //std::cout << "DEBUG CDR Encapsulation is " << encap.to_string() << std::endl;

    //Serialize the Encapsulation Header
    pass &= (serial << encap);

    if(globalEncoding !=  OpenDDS::DCPS::Encoding::KIND_XCDR1)
    {
        CORBA::ULong delim_header= num_data_bytes; //sample->getEncapsulationLength();
        //std::cout << "DEBUG TopicReplayer::publishSample encapsulation length " << delim_header << std::endl;
        if (! (serial << delim_header)) {
            std::cerr << "TopicReplayer::publishSample "
                        << "Could not serialize delimiter header"
                        << std::endl;
            return;
        }
    }

    serial.reset_alignment();

    //Serialize the sample
    pass &= ((*sample) >> serial);

    if (!pass)
    {
        std::cerr << "Failed to serialize '"
            << sample->getName()
            << "'"
            << std::endl;
        return;
    }

    // Update the timestamp
    QDateTime currentTime = QDateTime::currentDateTime();
    // FIXME? int32_t epochTimeSec = static_cast<int32_t>(currentTime.toSecsSinceEpoch());
    int32_t epochTimeSec = static_cast<int32_t>(currentTime.toMSecsSinceEpoch() / 1000);
    uint32_t epochTimeNSec = static_cast<uint32_t>(currentTime.time().msec() * 1000000);


    //OpenDDS::DCPS::Discovery_rch disc =
    //    TheServiceParticipant->get_discovery(m_topic->get_participant()->get_domain_id());
    //OpenDDS::DCPS::PublicationId pub_id =
    //    disc->bit_key_to_repo_id(partipant_, BUILT_IN_PUBLICATION_TOPIC, data.key);

    OpenDDS::DCPS::GUID_t pubID = OpenDDS::DCPS::GUID_UNKNOWN; //RJ TODO Reference writer here?

    //3.18.1 added DataSampleHeader. Not sure what to do with it yet. My belief is that it will be populated in DataWriterImpl::create_sample_data_message, called by ReplayerImpl::write
    OpenDDS::DCPS::DataSampleHeader sampleHdr;
    sampleHdr.message_id_ = OpenDDS::DCPS::SAMPLE_DATA;
    sampleHdr.publication_id_ = pubID;
    sampleHdr.message_length_ = static_cast<uint32_t>(num_data_bytes);


    OpenDDS::DCPS::RawDataSample rawSample(
        sampleHdr,
        static_cast<OpenDDS::DCPS::MessageId> (sampleHdr.message_id_),
        epochTimeSec,
        epochTimeNSec,
        pubID,
        true,  //use little endian
        &block,
        QosDictionary::getEncodingKind());

    //printf("\n=== TopicReplayer::publishSample ===\n");
    //printf("Size = %zu bytes\n", rawSample.sample_->length());

    m_replayer->write(rawSample);

} // End TopicReplayer::publishSample


//------------------------------------------------------------------------------
TopicReplayer::~TopicReplayer()
{}


/**
 * @}
 */
