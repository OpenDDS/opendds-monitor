#include "recorder_dialog.h"
#include "dds_data.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QTime>

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include <QTextCodec>
#endif

//------------------------------------------------------------------------------
RecorderDialog::RecorderDialog(const QString& topicName,
                               const QStringList& members,
                               QWidget* parent) :
                               QDialog(parent),
                               m_topicMembers(members),
                               m_topicName(topicName),
                               m_latestTimestamp("00:00:00.000"),
                               m_delimiter(","),
                               m_updateTimer(this),
                               m_rowCount(0)
{
    setupUi(this);
    setWindowTitle("DDS Data Recorder - " + m_topicName);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(
        Qt::CustomizeWindowHint |
        Qt::WindowTitleHint |
        Qt::Window |
        Qt::WindowCloseButtonHint);

    QSettings settings(SETTINGS_ORG_NAME, SETTINGS_APP_NAME);
    QString recorderFile = settings.value("recorderFile").toString();
    dataFileEdit->setText(recorderFile);
    memberListWidget->addItems(m_topicMembers);

    recordingStatusLabel->setVisible(false);
    stopButton->setVisible(false);

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(dumpData()));
    m_updateTimer.setInterval(UPDATE_RATE);
}


//------------------------------------------------------------------------------
RecorderDialog::~RecorderDialog()
{
    if (m_updateTimer.isActive())
    {
        m_updateTimer.stop();
    }
    if (m_outputFile.isOpen())
    {
        m_outputFile.close();
    }
}


//------------------------------------------------------------------------------
bool RecorderDialog::isRecording() const
{
    return m_updateTimer.isActive();
}


//------------------------------------------------------------------------------
void RecorderDialog::on_delimiterCombo_currentIndexChanged(int newIndex)
{
    switch (newIndex)
    {
    case DELIMITER_COMMA:
        m_delimiter = ",";
        break;

    case DELIMITER_SPACE:
        m_delimiter = " ";
        break;

    case DELIMITER_TAB:
        m_delimiter = "\t";
        break;

    default:
        m_delimiter = ",";
    }
}


//------------------------------------------------------------------------------
void RecorderDialog::on_dataFileButton_clicked()
{
    QString outputFile = dataFileEdit->text();

    outputFile = QFileDialog::getSaveFileName(
        this,
        "Select an Output File",
        outputFile,
        "Comma-separated Files (*.csv);;"
        "Tab-separated Files (*.tab *.tsv);;"
        "All Files (*.*)",
        0,
        QFileDialog::DontConfirmOverwrite);

    if (outputFile.isEmpty())
    {
        return;
    }

    dataFileEdit->setText(outputFile);
}


//------------------------------------------------------------------------------
void RecorderDialog::on_recordButton_clicked()
{
    QString outputFilePath = dataFileEdit->text();
    QSettings settings(SETTINGS_ORG_NAME, SETTINGS_APP_NAME);

    // If the file already exists, confirm overwrite
    QFileInfo fileInfo(outputFilePath);
    if (fileInfo.exists())
    {
        QMessageBox::StandardButton confirmButton = QMessageBox::warning(
            this,
            "Overwrite Existing File?",
            "Overwrite '" +
            outputFilePath +
            "'?\n",
            QMessageBox::Ok | QMessageBox::Abort);

        if (confirmButton != QMessageBox::Ok)
        {
            return;
        }
    }

    settings.setValue("recorderFile", outputFilePath);
    m_outputFile.setFileName(outputFilePath);
    if (!m_outputFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(
            this,
            "Error Creating File",
            "Unable to open '" +
            outputFilePath +
            "'\n",
            QMessageBox::Ok);

        return;
    }


    // Prepare the output stream
    m_outputStream.setDevice(&m_outputFile);
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    m_outputStream.setCodec(QTextCodec::codecForName("UTF-8"));
#else
    m_outputStream.setEncoding(QStringConverter::Utf8);
#endif
    m_outputStream.setRealNumberNotation(QTextStream::FixedNotation);
    m_outputStream.setRealNumberPrecision(6);

    // Add the header to the data file
    m_outputStream << "Time";
    for (int i = 0; i < m_topicMembers.count(); i++)
    {
        m_outputStream << m_delimiter << m_topicMembers.at(i);
    }
    m_outputStream << "\n";


    // Don't allow the user to do anything except stop recording
    recordingStatusLabel->setVisible(true);
    dataFileEdit->setEnabled(false);
    dataFileButton->setEnabled(false);
    delimiterCombo->setEnabled(false);
    recordButton->setVisible(false);
    stopButton->setVisible(true);
    closeButton->setVisible(false);


    // Only record data with a timestamp after the current time
    m_latestTimestamp = QTime::currentTime().toString("HH:mm:ss.zzz");
    m_rowCount = 0;

    dumpData();
    m_updateTimer.start();

} // End RecorderDialog::on_recordButton_clicked


//------------------------------------------------------------------------------
void RecorderDialog::on_stopButton_clicked()
{
    m_updateTimer.stop();
    m_outputFile.close();

    recordingStatusLabel->setVisible(false);
    dataFileEdit->setEnabled(true);
    dataFileButton->setEnabled(true);
    delimiterCombo->setEnabled(true);
    recordButton->setVisible(true);
    stopButton->setVisible(false);
    closeButton->setVisible(true);
}


//------------------------------------------------------------------------------
void RecorderDialog::dumpData()
{
    QStringList sampleNames = CommonData::getSampleList(m_topicName);

    // Loop through all samples for this topic
    for (int i = sampleNames.count() - 1; i >= 0; i--)
    {
        // Skip previously read samples
        if (sampleNames.at(i) <= m_latestTimestamp)
        {
            continue;
        }

        // Insert the timestamp
        m_outputStream << sampleNames.at(i);

        // Insert the values for each member variable
        for (int ii = 0; ii < m_topicMembers.count(); ii++)
        {
            QVariant memberValue = CommonData::readValue(
                m_topicName,
                m_topicMembers.at(ii),
                i);

            m_outputStream << m_delimiter << memberValue.toString();
        }

        m_outputStream << "\n";
        ++m_rowCount;
    }

    // Force writing to the file
    m_outputStream.flush();


    // Update the timestamp to the latest sample
    if (sampleNames.count() > 0)
    {
        m_latestTimestamp = sampleNames.at(0);
    }


    // If midnight rolled by, reset the latest received sample time
    QTime now = QTime::currentTime();
    if (now.hour() == 0 &&
        now.minute() == 0 &&
        now.second() == 0 &&
        m_latestTimestamp != "00:00:00.000")
    {
        m_latestTimestamp = "00:00:00.000";

        // Reset the samples, so we don't record samples from the previous day.
        // Doing this isn't ideal, but it shouldn't happen unless someone is
        // recording all night.
        CommonData::flushSamples(m_topicName);
    }

    rowCountLabel->setText(QString::number(m_rowCount));

} // End RecorderDialog::dumpData


/**
 * @}
 */
