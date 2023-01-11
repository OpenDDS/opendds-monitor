#include "dds_logging.h"
#include "ace/streams.h"
#include "ace/Log_Msg_Callback.h"
#include "ace/Log_Msg.h"
#include "ace/Log_Record.h"	
#include "ace/SString.h"
#include <memory>
#include <sstream>

class ManagerAceCallback : public ACE_Log_Msg_Callback {
public:
	ManagerAceCallback(std::function<void(LogMessageType mt, const std::string& message)> messageHandler);
	void log(ACE_Log_Record& log_record) override;

private:
	std::function<void(LogMessageType mt, const std::string& message)> m_cb;
};

ManagerAceCallback::ManagerAceCallback(std::function<void(LogMessageType mt, const std::string& message)> messageHandler) : m_cb(messageHandler)
{
}

void ManagerAceCallback::log(ACE_Log_Record& record)
{
	const unsigned long msg_severity = record.type();
	const ACE_Log_Priority prio = static_cast<ACE_Log_Priority>(msg_severity);
	
	LogMessageType lt;

	switch (prio) {
		case LM_ERROR:
		case LM_ALERT:
		case LM_CRITICAL:
		case LM_EMERGENCY:
			lt = LogMessageType::DDS_ERROR;
			break;

		case LM_WARNING:
			lt = LogMessageType::DDS_WARNING;
			break;

		case LM_NOTICE:
			lt = LogMessageType::DDS_INFO;
			break;

		default:
			return;  //Nothing to do with other message types
	}

	std::stringstream sstr;
	record.print(ACE_TEXT(""), 0, sstr);
	std::string line(sstr.str());
	auto endLine = line.find('\n');  //Avoid doubling up end of line characters
	if (endLine != std::string::npos) {
		line.erase(endLine);
	}
	m_cb(lt, line);
}

std::unique_ptr<ManagerAceCallback> m_cb;

void SetACELogger(std::function<void(LogMessageType mt, const std::string& message)> messageHandler)
{
	m_cb = std::make_unique<ManagerAceCallback>(messageHandler);
	ACE_LOG_MSG->set_flags(ACE_Log_Msg::MSG_CALLBACK);
	ACE_LOG_MSG->clr_flags(ACE_Log_Msg::STDERR);
	ACE_LOG_MSG->msg_callback(m_cb.get());
}





/**
 * @}
 */
