#ifndef __DDS_SUBSCRIPTION_MONITOR_H__
#define __DDS_SUBSCRIPTION_MONITOR_H__

#include "first_define.h"

#include <QObject>
#include <QString>

#include "dds_manager.h"


/**
 * @ Listener class which receives information about subscribers on the bus.
 */
class SubscriptionMonitor : public QObject, public virtual GenericReaderListener
{
    Q_OBJECT

public:

    /**
     * @brief Constructor for the DDS subscription monitor class.
     */
    SubscriptionMonitor();

    /**
     * @brief Destructor for the DDS subscription monitor class.
     */
    ~SubscriptionMonitor();

    /**
     * @brief Callback method to handle the DDS::DATA_AVAILABLE_STATUS message.
     * @param[in] reader The data reader containing the new message.
     */
    void on_data_available(DDS::DataReader_ptr reader);

signals:

    /**
     * @brief Notify the main thread that we have a new topic
     * @param[out] topicName The name of the new topic.
     */
    void newTopic(const QString& topicName);

private:

    /// Stores the built-in data reader for the Subscription topic
    DDS::DataReader_ptr m_dataReader;
};

#endif

/**
 * @}
 */
