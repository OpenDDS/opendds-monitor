#include "common.h"

#include "testTypeSupportImpl.h"
#include "std_qosC.h"

#include <dds_manager.h>

#include <dds/DCPS/NetworkResource.h>
#include <dds/DCPS/ValueDispatcher.h>

#include <ace/OS_main.h>

#include <chrono>
#include <iostream>
#include <thread>

const char basic_topic_name[] = "Managed-Basic";
const char complex_topic_name[] = "Managed-Complex";

int main(int argc, char* argv[])
{
  ACE_UNUSED_ARG(argc);

  const int domain_id = 4;
  std::cout << "Testing..." << std::endl;
  std::unique_ptr<DDSManager> dds_manager = std::make_unique<DDSManager>();

  std::set<std::string> mpg;
  std::mutex mpgm;
  using mpgl = std::unique_lock<std::mutex>;

  bool join_result =
    dds_manager->joinDomain(domain_id,
                            "",
                            [&](const ParticipantInfo& info) { mpgl lock(mpgm); if (mpg.insert(info.guid).second) { std::cout << "Adding Participant   : " << info.guid << std::endl; } },
                            [&](const ParticipantInfo& info) { mpgl lock(mpgm); if (mpg.erase(info.guid)) { std::cout << "Removing Participant : " << info.guid << std::endl; } });
  OPENDDS_ASSERT(join_result);

  bool enable_result = dds_manager->enableDomain();
  OPENDDS_ASSERT(enable_result);

  bool register_basic_message_result = dds_manager->registerTopic<test::BasicMessage>(basic_topic_name, STD_QOS::LATEST_RELIABLE);
  OPENDDS_ASSERT(register_basic_message_result);

  bool register_complex_message_result = dds_manager->registerTopic<test::ComplexMessage>(complex_topic_name, STD_QOS::LATEST_RELIABLE);
  OPENDDS_ASSERT(register_complex_message_result);

  bool create_basic_message_publisher_result = dds_manager->createPublisher(basic_topic_name);
  OPENDDS_ASSERT(create_basic_message_publisher_result);

  bool create_complex_message_publisher_result = dds_manager->createPublisher(complex_topic_name);
  OPENDDS_ASSERT(create_complex_message_publisher_result);

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

      dds_manager->writeSample(basic_message, basic_topic_name);

      dds_manager->writeSample(complex_message, complex_topic_name);

      ++count;

      std::this_thread::sleep_for(std::chrono::seconds(5));
    }
  });

  std::string line;
  std::getline(std::cin, line);

  run = false;

  thread.join();

  return 0;
}
