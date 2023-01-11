#ifndef DEF_TABLE_PAGE_WIDGET
#define DEF_TABLE_PAGE_WIDGET

#include "first_define.h"
#include "ui_table_page.h"

#include <QTableWidgetItem>
#include <QStringList>
#include <QString>
#include <QTimer>

#include <memory>

class TopicTableModel;
class TopicReplayer;
class TopicMonitor;

/**
 * @brief The DDS topic page class.
 */
class TablePage : public QWidget, public Ui::TablePageForm
{
    Q_OBJECT

public:

    /**
     * @brief Constructor for TablePage.
     * @param[in] topicName The name of the topic used on this page.
     * @param[in] parent The parent of this Qt object.
     */
    TablePage(const QString& topicName, QWidget* parent = 0);

    /**
     * @brief Destructor for TablePage.
     */
    ~TablePage();

private slots:

    /**
     * @brief Clear the data sample history.
     */
    void on_clearSamplesButton_clicked();

    /**
     * @brief Prompt the user for a data filter for this topic.
     */
    void on_filterButton_clicked();

    /**
     * @brief Toggles the option of viewing the latest sample on the page.
     */
    void on_useLatestButton_clicked();

    /**
     * @brief Disable new data samples from appearing on the page.
     */
    void on_freezeButton_clicked();

    /**
     * @brief Enable new data samples from appearing on the page.
     */
    void on_unfreezeButton_clicked();

    /**
     * @brief Export an INI file from the current sample.
     */
    void on_iniButton_clicked();

    /**
     * @brief Revert any edit operations to the original sample.
     */
    void on_revertButton_clicked();

    /**
     * @brief Publish the modified data sample to the bus.
     */
    void on_publishButton_clicked();

    /**
     * @brief Switch which data sample is view on this page.
     */
    void on_historyTable_itemSelectionChanged();

    /**
     * @brief Create a new plot from the selected variables.
     */
    void on_newPlotButton_clicked();

    /**
     * @brief Attach the selected variables to an existing plot.
     */
    void on_attachPlotButton_clicked();

    /**
     * @brief Start recording the selected variables to a file.
     */
    void on_recordButton_clicked();

    /**
     * @brief Disable the scroll to latest option if the user started editing.
     * @param[in] index The clicked table index.
     */
    void on_topicTableView_pressed(const QModelIndex& index);

    /**
     * @brief Enable appropriate buttons when data has changed in the model.
     */
    void dataHasChanged();

    /**
     * @brief Update the history table with the latest data from DDS.
     * @remarks This is called from the m_refreshTimer timer.
     */
    void refreshPage();

private:

    /**
     * @brief Set the data sample used by this page.
     * @param[in] The name of the data sample (timestamp).
     */
    void setSample(const QString& sampleName);

    /// The number of MS to wait until updating the history widget.
    static const int REFRESH_TIMEOUT = 250;

    /// Data model for the topic used on this page.
    std::unique_ptr<TopicTableModel> m_tableModel;

    /// Topic monitor for the topic used on this page (Leak on purpose since DDS shutdown isn't quite right).
    std::unique_ptr <TopicMonitor> m_topicMonitor;

    /// Topic replayer for the topic used on this page (Leak on purpose since DDS shutdown isn't quite right).
    std::unique_ptr<TopicReplayer> m_topicReplayer;

    /// The name of the topic used on this page.
    QString m_topicName;

    /// The name of the selected sample.
    QString m_selectedSample;

    /// Refresh timer for all tables.
    QTimer m_refreshTimer;

    /// Stores the history sample names.
    QStringList m_historyList;

}; // End TablePage

#endif


/**
 * @}
 */
