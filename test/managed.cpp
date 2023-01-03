#include "testC.h"
#include "testTypeSupportImpl.h"
#include "std_qosC.h"

#include <dds_manager.h>

#include <dds/DCPS/ValueDispatcher.h>

#include <ace/OS_main.h>

#include <iostream>
#include <thread>
#include <sstream>

template <typename T>
std::string to_str(const T& t) {
  std::ostringstream oss;
  oss << t;
  return oss.str();
}

const char basic_topic_name[] = "Managed-Basic";
const char complex_topic_name[] = "Managed-Complex";

int main(int argc, char* argv[])
{
  const int domain_id = 4;
  std::cout << "Testing..." << std::endl;
  std::unique_ptr<DDSManager> dds_manager = std::make_unique<DDSManager>();

  bool join_result =
    dds_manager->joinDomain(domain_id,
                            "",
                            [](const ParticipantInfo& info) { std::cout << "Adding Participant" << std::endl; },
                            [](const ParticipantInfo& info) { std::cout << "Removing Participant" << std::endl; });
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

  const std::string id = std::string(argv[0]) + '-' + to_str(std::this_thread::get_id());
  CORBA::Long count = 0;

  std::thread thread([&](){
    while (run) {
      test::BasicMessage basic_message{};
      basic_message.origin = id.c_str();
      basic_message.count = count;
      dds_manager->writeSample(basic_message, basic_topic_name);

      test::ComplexMessage complex_message{};
      complex_message.origin = id.c_str();
      complex_message.count = count;
      complex_message.ct.cuts.length(1);
      test::TreeNode tn;
      tn.et = test::EnumType::three;
      tn.ut.str("a string");
      complex_message.ct.cuts[0].tn(tn);
      dds_manager->writeSample(complex_message, complex_topic_name);

      ++count;

      ACE_OS::sleep(ACE_Time_Value(5, 0));
    }
  });

  std::string line;
  std::getline(std::cin, line);

  run = false;

  thread.join();

  return 0;
}
