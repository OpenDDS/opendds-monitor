#include <QApplication>

#include "main_window.h"

#include <memory>

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
