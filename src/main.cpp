#include <QApplication>

#include "main_window.h"

#include <iostream>
#include <memory>
#include <dds_logging.h>


/**
 * @brief Main function for the DDS Monitor application.
 *
 * @param[in] argc The number of arguments passed in from the command line.
 * @param[in] argv The text from the passed in arguments.
 *
 * @return The result from the QApplication when it's closed.
 */
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    
	auto aceLogging = [](LogMessageType mt, const std::string& message) {
#ifdef _DEBUG
        if (mt == LogMessageType::DDS_ERROR)
        {
            std::cerr << message;
        }
        else {
            std::cout << message;
        }
#else
        if (mt == LogMessageType::DDS_ERROR)
        {
            std::cerr << message;
        }
#endif
    };

	SetACELogger(aceLogging);

    auto mainWindow = std::make_unique <DDSMonitorMainWindow>();

    int returnCode = 2;

    if (!mainWindow->Closing)
    {
        mainWindow->show();

        returnCode = app.exec();
    }

    return returnCode;
}


/**
 * @}
 */
