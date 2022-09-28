#ifndef DEF_RECORDER_DIALOG
#define DEF_RECORDER_DIALOG

#include <QTextStream>
#include <QStringList>
#include <QString>
#include <QDialog>
#include <QTimer>
#include <QFile>

#include "ui_recorder_dialog.h"


/**
 * @brief The DDS data recorder dialog class.
 */
class RecorderDialog : public QDialog, public Ui::RecorderForn
{
    Q_OBJECT

public:

    /**
     * @brief Constructor for the DDS data recorder dialog.
     * @param[in] topicName Record data from this DDS topic.
     * @param[in] members Record these DDS data members.
     * @param[in] parent The parent of this Qt object.
     */
    RecorderDialog(const QString& topicName,
                   const QStringList& members,
                   QWidget* parent = 0);

    /**
     * @brief Destructor for the DDS data recorder dialog.
     */
    ~RecorderDialog();

    /**
     * @brief Return the recording status of the dialog.
     * @return True if recording; false otherwise.
     */
    bool isRecording() const;

private slots:

    /**
     * @brief Update the dialog to use the selected delimiter.
     * @param[in] newIndex The new delimiter selected index.
     */
    void on_delimiterCombo_currentIndexChanged(int newIndex);

    /**
     * @brief Prompt the user for an output file.
     */
    void on_dataFileButton_clicked();

    /**
     * @brief Begin recording DDS data to a text file.
     */
    void on_recordButton_clicked();

    /**
     * @brief Stop recording DDS data and close the file.
     */
    void on_stopButton_clicked();

    /**
     * @brief Dump any new DDS samples into the data file.
     */
    void dumpData();

private:

    /// IDs for delimiter selections.
    enum eDelimiterTypeIDs
    {
        DELIMITER_COMMA,
        DELIMITER_SPACE,
        DELIMITER_TAB,
    };

    /// Stores the topic member names to record.
    QStringList m_topicMembers;

    /// Stores the target topic member to record.
    QString m_topicName;

    /// Record data after this timestamp.
    QString m_latestTimestamp;

    /// Separate data rows with this delimiter.
    QString m_delimiter;

    /// Write to this output file object.
    QFile m_outputFile;

    /// Write to this output file stream object.
    QTextStream m_outputStream;

    /// The timer to check for new data.
    QTimer m_updateTimer;

    /// Counts the number of rows recorded.
    unsigned int m_rowCount;

    /// The data dump rate in ms.
    static const int UPDATE_RATE = 250;

}; // End RecorderDialog

#endif


/**
 * @}
 */
