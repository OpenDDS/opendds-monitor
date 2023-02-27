#include "log_page.h"

#include <QPrintDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QPrinter>
#include <QWidget>
#include <QString>
#include <QMutex>
#include <QFile>

#include <memory>

//------------------------------------------------------------------------------
LogPage::LogPage(QWidget* parent) :
    QWidget(parent)
{
    setupUi(this);
    logEdit->document()->setMaximumBlockCount(MAX_BLOCK_COUNT);

    QFont monoFont("Monospace", 10);
    monoFont.setStyleHint(QFont::Monospace);
    logEdit->setFont(monoFont);

    // Redirect cout and cerr to the log window
    m_coutStream = std::make_unique<LogPage::LogStream>(std::cout, this);
    m_cerrStream = std::make_unique<LogPage::LogStream>(std::cerr, this);

    timerId = startTimer(1000);
}


//------------------------------------------------------------------------------
LogPage::~LogPage()
{
    killTimer(timerId);
}


void LogPage::timerEvent(QTimerEvent* /*event*/)
{
    std::lock_guard<std::mutex> lk(newMessageMutex);
    for (const auto& message : newMessages) {
        logEdit->append(message.c_str());
    }
    newMessages.clear();
}


void LogPage::AddMessage(const std::string& str)
{
    std::lock_guard<std::mutex> lk(newMessageMutex);
    newMessages.push_back(str);
}

//------------------------------------------------------------------------------
void LogPage::on_clearButton_clicked()
{
    logEdit->clear();
}


//------------------------------------------------------------------------------
void LogPage::on_printButton_clicked()
{   
    QPrinter printer;

    QPrintDialog printDialog(&printer, this);
    printDialog.setWindowTitle("Print Log");

    if (printDialog.exec())
    {
        logEdit->print(&printer);
    }
}


//------------------------------------------------------------------------------
void LogPage::on_saveButton_clicked()
{
    QFile file;
    QString fileName = QFileDialog::getSaveFileName(
        this, "Save Log As", "", "Text Documents (*.txt)");

    // Bail if the user hit the cancel button
    if (fileName.isEmpty())
    {
        return;
    }

    // Make sure the file can be opened
    file.setFileName(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::critical(this, "Save Error",
            "Unable to save file: Access is denied");
        file.close();
        return;
    }

    QTextStream outputStream(&file);
    outputStream << logEdit->toPlainText();

    file.close();

} // End LogPage::on_saveButton_clicked


//------------------------------------------------------------------------------
LogPage::LogStream::LogStream(std::ostream& stream, LogPage* logPage) :
    m_originalStream(stream),
    m_originalBuffer(NULL),
    m_logPage(logPage)
{
    m_originalBuffer = stream.rdbuf();
    stream.rdbuf(this);
}


//------------------------------------------------------------------------------
LogPage::LogStream::~LogStream()
{
    m_originalStream.rdbuf(m_originalBuffer);
}


//------------------------------------------------------------------------------
std::streambuf::int_type LogPage::LogStream::overflow(int_type v)
{
    std::lock_guard<std::mutex> lk(writeMutex);

    if (v == '\n')
    {
        m_logPage->AddMessage(m_string);
        m_string.clear();
    }
    else
    {
        m_string += v;
    }

    return v;
}


//------------------------------------------------------------------------------
std::streamsize LogPage::LogStream::xsputn(const char* p, std::streamsize n)
{
    std::lock_guard<std::mutex> lk(writeMutex);
    m_string.append(p, p + n);
    size_t pos = 0;

    while (pos != std::string::npos)
    {
        pos = m_string.find('\n');
        if (pos != std::string::npos)
        {
            const std::string tmp(m_string.begin(), m_string.begin() + pos);
            m_logPage->AddMessage(tmp);
            m_string.erase(m_string.begin(), m_string.begin() + pos + 1);
        }
    }

    return n;
}


/**
 * @}
 */
