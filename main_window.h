#ifndef DDS_MONITOR_MAINWINDOW_H
#define DDS_MONITOR_MAINWINDOW_H

#include "first_define.h"
#include "ui_main_window.h"

#include <QMainWindow>
#include <QString>

class DDSManager;
class PublicationMonitor;
class SubscriptionMonitor;
class ParticipantPage;
class LogPage;


/**
 * @brief The main dialog for the DDS Monitor application.
 */
class DDSMonitorMainWindow : public QMainWindow, public Ui::DDSMonitorMainWindow
{
    Q_OBJECT

public:

    /**
     * @brief Constructor for the DDS Monitor main dialog.
     */
    DDSMonitorMainWindow();

    /**
     * @brief Destructor for the DDS Monitor main dialog.
     */
    ~DDSMonitorMainWindow();


    bool Closing = false;

private slots:

    /**
     * @brief Add a recently discovered topic to the topic tree.
     * @param[in] topicName The name of the recently discovered topic.
     */
    void discoveredTopic(const QString& topicName);

    /**
     * @brief Create a publisher and subscriber for a selected topic.
     * @param[in] item The selected topic item.
     * @param[in] column Unused but required for auto signal/slot connection.
     */
    void on_topicTree_itemClicked(QTreeWidgetItem* item, int column);

    /**
     * @brief Close the selected page.
     * @param[in] pageIndex Close the page with this index.
     */
    void on_mainTabWidget_tabCloseRequested(int pageIndex);

private:

    /**
     * @brief Load application settings from the command line.
     * @remarks Any discovered settings are stored in the
     *          QCoreApplication property list for future reference.
     */
    void parseCmd();

    /**
     * @brief Send DDS configuration information to the log page.
     */
    void reportConfig() const;

    /// The message log page widget.
    LogPage* m_logPage;

    /// The DDS participant page widget.
    ParticipantPage* m_participantPage;

    /// Monitors domain publication changes.
    std::unique_ptr<PublicationMonitor> m_publicationMonitor;

    /// Monitors domain subscription changes.
    std::unique_ptr <SubscriptionMonitor> m_subscriptionMonitor;

};

#endif

/**
 * @}
 */
