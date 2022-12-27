#include "testC.h"
#include "testTypeSupportImpl.h"

#include <dds/DCPS/ValueDispatcher.h>

#include <ace/Init_ACE.h>

#include <iostream>
#include <thread>

#include <sstream>

template <typename T>
std::string to_str(const T& t) {
  std::ostringstream oss;
  oss << t << std::flush;
  return oss.str();
}

const char basic_topic_name[] = "Unmanaged-Basic";
const char complex_topic_name[] = "Unmanaged-Complex";

int ACE_TMAIN(int argc, ACE_TCHAR* argv[])
{
  const int domain_id = 4;
  std::cout << "Testing..." << std::endl;

  try
  {
    DDS::DomainParticipantFactory_var dpf =
      TheParticipantFactoryWithArgs(argc, argv);

    DDS::DomainParticipantQos participant_qos;
    dpf->get_default_participant_qos(participant_qos);
    DDS::DomainParticipant_var participant =
      dpf->create_participant(domain_id,
                              participant_qos,
                              DDS::DomainParticipantListener::_nil(),
                              ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    if (CORBA::is_nil(participant.in())) {
      std::cerr << "create_participant failed." << std::endl;
      return 1;
    }

    test::BasicMessageTypeSupportImpl::_var_type basic_tsi = new test::BasicMessageTypeSupportImpl();

    if (DDS::RETCODE_OK != basic_tsi->register_type(participant.in(), "")) {
      std::cerr << "register_type failed." << std::endl;
      exit(1);
    }

    CORBA::String_var basic_type_name = basic_tsi->get_type_name();

    DDS::TopicQos basic_topic_qos;
    participant->get_default_topic_qos(basic_topic_qos);
    DDS::Topic_var basic_topic =
      participant->create_topic(basic_topic_name,
                                basic_type_name.in(),
                                basic_topic_qos,
                                DDS::TopicListener::_nil(),
                                ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (CORBA::is_nil(basic_topic.in())) {
      std::cerr << "create_topic failed for '" << basic_topic_name << "'" << std::endl;
      exit(1);
    }

    test::BasicMessageTypeSupportImpl::_var_type complex_tsi = new test::BasicMessageTypeSupportImpl();

    if (DDS::RETCODE_OK != complex_tsi->register_type(participant.in(), "")) {
      std::cerr << "register_type failed." << std::endl;
      exit(1);
    }

    CORBA::String_var complex_type_name = complex_tsi->get_type_name();

    DDS::TopicQos complex_topic_qos;
    participant->get_default_topic_qos(complex_topic_qos);
    DDS::Topic_var complex_topic =
      participant->create_topic(complex_topic_name,
                                complex_type_name.in(),
                                complex_topic_qos,
                                DDS::TopicListener::_nil(),
                                ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (CORBA::is_nil(complex_topic.in())) {
      std::cerr << "create_topic failed for '" << complex_topic_name << "'" << std::endl;
      exit(1);
    }

    DDS::PublisherQos pub_qos;
    participant->get_default_publisher_qos(pub_qos);

    DDS::Publisher_var pub =
      participant->create_publisher(pub_qos, DDS::PublisherListener::_nil(),
                                    ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    if (CORBA::is_nil(pub.in())) {
      std::cerr << "create_publisher failed." << std::endl;
      exit(1);
    }

    DDS::DataWriterQos datawriter_qos;
    pub->get_default_datawriter_qos(datawriter_qos);

    DDS::DataWriter_var basic_dw =
      pub->create_datawriter(basic_topic.in(),
                              datawriter_qos,
                              DDS::DataWriterListener::_nil(),
                              ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (CORBA::is_nil(basic_dw.in())) {
      std::cerr << "create_datawriter failed for '" << basic_topic_name << "'" << std::endl;
      exit(1);
    }

    test::BasicMessageDataWriter_var basic_message_dw =
      test::BasicMessageDataWriter::_narrow(basic_dw.in());

    if (CORBA::is_nil(basic_message_dw.in())) {
      std::cerr << "test::BasicMessageDataWriter::_narrow() failed." << std::endl;
      exit(1);
    }

    DDS::DataWriter_var complex_dw =
      pub->create_datawriter(complex_topic.in(),
                              datawriter_qos,
                              DDS::DataWriterListener::_nil(),
                              ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (CORBA::is_nil(complex_dw.in())) {
      std::cerr << "create_datawriter failed for '" << complex_topic_name << "'" << std::endl;
      exit(1);
    }

    test::ComplexMessageDataWriter_var complex_message_dw =
      test::ComplexMessageDataWriter::_narrow(complex_dw.in());

    if (CORBA::is_nil(complex_message_dw.in())) {
      std::cerr << "test::ComplexMessageDataWriter::_narrow() failed." << std::endl;
      exit(1);
    }

    //bool create_basic_message_subscriber_result = dds_manager->createSubscriber(basic_topic_name, "basic_message_subscriber");
    //OPENDDS_ASSERT(create_basic_message_subscriber_result);

    //bool create_complex_message_subscriber_result = dds_manager->createSubscriber(complex_topic_name, "complex_message_subscriber");
    //OPENDDS_ASSERT(create_complex_message_subscriber_result);

    bool run = true;

    const std::string id = std::string(argv[0]) + '-' + to_str(std::this_thread::get_id());
    CORBA::Long count = 0;

    srand(time(0));

    std::thread thread([&](){
      while (run) {
        test::BasicMessage basic_message{};
        basic_message.origin = id.c_str();
        basic_message.count = count;
        basic_message_dw->write(basic_message, DDS::HANDLE_NIL);

        test::ComplexMessage complex_message{};
        complex_message.origin = id.c_str();
        complex_message.count = count;
        complex_message.ct.cuts.length(1);
        test::TreeNode tn;
        tn.ut.str("a string");
        tn.et = test::EnumType::three;
        complex_message.ct.cuts[0]._d(test::EnumType::one);
        complex_message.ct.cuts[0].tn(tn);
        complex_message_dw->write(complex_message, DDS::HANDLE_NIL);

        ++count;

        ACE_OS::sleep(ACE_Time_Value(5, 0));
      }
    });

    std::string line;
    std::getline(std::cin, line);

    run = false;

    thread.join();

    participant->delete_contained_entities();
    dpf->delete_participant(participant.in());
  }
  catch (const CORBA::Exception& e)
  {
    std::cerr << "PUB: Exception caught in main.cpp:" << std::endl
         << e << std::endl;
    exit(1);
  }

  TheServiceParticipant->shutdown();

  return 0;
}