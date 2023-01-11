#include "common.h"

#include "testTypeSupportImpl.h"

#include <dds/DCPS/NetworkResource.h>
#include <dds/DCPS/ValueDispatcher.h>

#include <ace/OS_main.h>

#include <chrono>
#include <iostream>
#include <thread>
#include <sstream>

const char basic_topic_name[] = "Unmanaged-Basic";
const char complex_topic_name[] = "Unmanaged-Complex";

int main(int argc, char* argv[])
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
    if (!participant) {
      std::cerr << "create_participant failed." << std::endl;
      return 1;
    }

    test::BasicMessageTypeSupportImpl::_var_type basic_tsi = new test::BasicMessageTypeSupportImpl();

    if (DDS::RETCODE_OK != basic_tsi->register_type(participant, "")) {
      std::cerr << "register_type failed." << std::endl;
      exit(1);
    }

    CORBA::String_var basic_type_name = basic_tsi->get_type_name();

    DDS::TopicQos basic_topic_qos;
    participant->get_default_topic_qos(basic_topic_qos);
    DDS::Topic_var basic_topic =
      participant->create_topic(basic_topic_name,
                                basic_type_name,
                                basic_topic_qos,
                                DDS::TopicListener::_nil(),
                                ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!basic_topic) {
      std::cerr << "create_topic failed for '" << basic_topic_name << "'" << std::endl;
      exit(1);
    }

    test::ComplexMessageTypeSupportImpl::_var_type complex_tsi = new test::ComplexMessageTypeSupportImpl();

    if (DDS::RETCODE_OK != complex_tsi->register_type(participant, "")) {
      std::cerr << "register_type failed." << std::endl;
      exit(1);
    }

    CORBA::String_var complex_type_name = complex_tsi->get_type_name();

    DDS::TopicQos complex_topic_qos;
    participant->get_default_topic_qos(complex_topic_qos);
    DDS::Topic_var complex_topic =
      participant->create_topic(complex_topic_name,
                                complex_type_name,
                                complex_topic_qos,
                                DDS::TopicListener::_nil(),
                                ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!complex_topic) {
      std::cerr << "create_topic failed for '" << complex_topic_name << "'" << std::endl;
      exit(1);
    }

    DDS::PublisherQos pub_qos;
    participant->get_default_publisher_qos(pub_qos);

    DDS::Publisher_var pub =
      participant->create_publisher(pub_qos, DDS::PublisherListener::_nil(),
                                    ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    if (!pub) {
      std::cerr << "create_publisher failed." << std::endl;
      exit(1);
    }

    DDS::DataWriterQos datawriter_qos;
    pub->get_default_datawriter_qos(datawriter_qos);

    DDS::DataWriter_var basic_dw =
      pub->create_datawriter(basic_topic,
                             datawriter_qos,
                             DDS::DataWriterListener::_nil(),
                             ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!basic_dw) {
      std::cerr << "create_datawriter failed for '" << basic_topic_name << "'" << std::endl;
      exit(1);
    }

    test::BasicMessageDataWriter_var basic_message_dw =
      test::BasicMessageDataWriter::_narrow(basic_dw);

    if (!basic_message_dw) {
      std::cerr << "test::BasicMessageDataWriter::_narrow() failed." << std::endl;
      exit(1);
    }

    DDS::DataWriter_var complex_dw =
      pub->create_datawriter(complex_topic,
                             datawriter_qos,
                             DDS::DataWriterListener::_nil(),
                             ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!complex_dw) {
      std::cerr << "create_datawriter failed for '" << complex_topic_name << "'" << std::endl;
      exit(1);
    }

    test::ComplexMessageDataWriter_var complex_message_dw =
      test::ComplexMessageDataWriter::_narrow(complex_dw);

    if (!complex_message_dw) {
      std::cerr << "test::ComplexMessageDataWriter::_narrow() failed." << std::endl;
      exit(1);
    }

    bool run = true;

    const std::string ex = std::string(argv[0]);
    const std::string id = ex.substr(ex.find_last_of("/\\") + 1) + '-' + OpenDDS::DCPS::get_fully_qualified_hostname() + "-" + to_str(ACE_OS::getpid());
    CORBA::ULongLong count = 0;

    std::thread thread([&](){
      std::mt19937 mt;
      mt.seed(static_cast<std::mt19937::result_type>(time(0)));

      while (run) {
        test::BasicMessage basic_message{};
        basic_message.origin = id.c_str();

        test::ComplexMessage complex_message{};
        complex_message.origin = id.c_str();

        generate_samples(mt, count, basic_message, complex_message);

        basic_message_dw->write(basic_message, DDS::HANDLE_NIL);

        complex_message_dw->write(complex_message, DDS::HANDLE_NIL);

        ++count;

        std::this_thread::sleep_for(std::chrono::seconds(5));
      }
    });

    std::string line;
    std::getline(std::cin, line);

    run = false;

    thread.join();

    participant->delete_contained_entities();
    dpf->delete_participant(participant);
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
