#ifndef DEF_LOG_WIDGET
#define DEF_LOG_WIDGET

#include "first_define.h"
#include "ui_log_page.h"

#include <iostream>
#include <memory>
#include <mutex>

/**
 * @brief The message log page class.
 */
class LogPage : public QWidget, public Ui::LogPage
{
    Q_OBJECT

public:

    /**
     * @brief Constructor for LogPage.
     * @param[in] parent The parent of this Qt object.
     */
    LogPage(QWidget* parent = 0);

    /**
     * @brief Destructor for LogPage.
     */
    ~LogPage();

    /**
     * @brief Add a message to the log page.  Note:  This can sefely be called from another thread.
     * @param[in] msg the string to add
     */
    void AddMessage(const std::string& msg);

protected:
    /**
     * @brief Add a message to the log page.  Note:  This can sefely be called from another thread.
     * @param[in] event the timer event object
     */
    void timerEvent(QTimerEvent* event);

private slots:

    ///  Clears the content of the log window.
    void on_clearButton_clicked();

    ///  Prints the content of the log window.
    void on_printButton_clicked();

    ///  Saves the content of the log window to a file.
    void on_saveButton_clicked();

private:

    /**
     * @brief STL stream buffer that redirects std::cout or std::cerr to a
     *        QTextEdit widget.
     */
    class LogStream : public std::basic_streambuf<char>
    {
    public:

        /**
         * @brief Constructor for STL stream redirector.
         * @param[in,out] stream Redirect this stream to the QTextEdit widget.
         * @param[in] log page for adding messagest.
         */
        LogStream(std::ostream& stream, LogPage* logpage);

        /**
         * @brief Destructor for the STL stream redirectory.
         */
        ~LogStream();

    protected:
        std::streambuf::int_type overflow(int_type v) override;
        std::streamsize xsputn(const char* p, std::streamsize n) override;
    private:

        /// The original stream that this class took the redirection from.
        std::ostream& m_originalStream;

        /// The original stream buffer that this class took redirection from.
        std::streambuf* m_originalBuffer;

        /// Mutex to make the log window thread safe.
        std::mutex writeMutex;

        /// Direct all stream messages to this widget.
        LogPage* m_logPage;

        /// Stores the incoming data until a newline char is received.
        std::string m_string;

    }; // End LogStream

    /// The maximum number of blocks to display on the log widget.
    static const int MAX_BLOCK_COUNT = 10000;

    //Mutext to keep the new 
    std::mutex newMessageMutex;

    //Vector containing new messages to add to the window
    std::vector<std::string> newMessages;

    //Id for the window timer
    int timerId = 0;

    /// Redirects the data from std::cout to the log window.
    std::unique_ptr<LogStream> m_coutStream;

    /// Redirects the data from std::cerr to the log window.
    std::unique_ptr<LogStream> m_cerrStream;

}; // End LogPage

#endif


/**
 * @}
 */
