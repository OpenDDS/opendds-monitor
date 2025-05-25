#ifndef __DDS_PUBLICATION_MONITOR_H__
#define __DDS_PUBLICATION_MONITOR_H__

#include "first_define.h"

#include "dds_manager.h"

#include <QObject>
#include <QString>


/**
 * @ Listener class which receives information about publishers on the bus.
 */
class PublicationMonitor : public QObject, public virtual GenericReaderListener
{
    Q_OBJECT

public:

    /**
     * @brief Constructor for the DDS publication monitor class.
     */
    PublicationMonitor();

    /**
     * @brief Destructor for the DDS publication monitor class.
     */
    ~PublicationMonitor();

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

    /// Get DynamicType for the corresponding topic
    bool get_dynamic_type(DDS::DynamicType_var& type, const DDS::BuiltinTopicKey_t& key,
                          const char* topic_name, const char* type_name);

    /// Stores the built-in data reader for the Publication topic
    DDS::DataReader_ptr m_dataReader;

};

#endif

/**
 * @}
 */
