#ifndef __DDS_LOGGING__
#define __DDS_LOGGING__

#include <functional>
#include <string>
#include "dds_manager_defs.h"

enum class LogMessageType {
    DDS_ERROR,
    DDS_WARNING,
    DDS_INFO,
};

DLL_PUBLIC void SetACELogger(std::function<void(LogMessageType mt, const std::string& message)> messageHandler);


#endif // __CALLBACK_H__
