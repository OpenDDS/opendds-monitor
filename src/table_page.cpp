#include "table_page.h"
#include "dds_manager.h"
#include "dynamic_meta_struct.h"
#include "open_dynamic_data.h"
#include "topic_table_model.h"
#include "recorder_dialog.h"
#include "topic_replayer.h"
#include "topic_monitor.h"
#include "dds_data.h"
#include "graph_page.h"
#include "qos_dictionary.h"
#include <QMessageBox>
#include <iostream>
#include <exception>


//------------------------------------------------------------------------------
TablePage::TablePage(const QString& topicName,
                     QWidget *parent) :
                     QWidget(parent),
                     m_topicName(topicName),
                     m_refreshTimer(this)
{
    setupUi(this);

//     QFont monoFont("Monospace", 10);
//     monoFont.setStyleHint(QFont::Monospace);
//     topicTableView->setFont(monoFont);
//     historyTable->setFont(monoFont);

    matchedPubTitleLabel->hide(); // Pub match count not supported yet
    matchedSubTitleLabel->hide(); // Sub match count not supported yet
    unfreezeButton->hide();
    revertButton->setEnabled(false);
    newPlotButton->setEnabled(false);
    attachPlotButton->setEnabled(false);
    recordButton->setEnabled(false);

    // Create a data model for this topic
    m_tableModel = std::make_unique<TopicTableModel>(topicTableView, m_topicName);
    topicTableView->setModel(m_tableModel.get());
    //topicTableView->setColumnWidth(TopicTableModel::STATUS_COLUMN, 21);
    connect(m_tableModel.get(), SIGNAL(dataHasChanged()), this, SLOT(dataHasChanged()));

    // Create a topic monitor to receive the data samples
    m_topicMonitor = std::make_unique<TopicMonitor>(topicName);
    m_topicReplayer = std::make_unique<TopicReplayer>(topicName);

    connect(&m_refreshTimer, SIGNAL(timeout()), this, SLOT(refreshPage()));
    m_refreshTimer.start(REFRESH_TIMEOUT);

} // End TablePage::TablePage


//------------------------------------------------------------------------------
TablePage::~TablePage()
{
    CommonData::flushSamples(m_topicName);
}


//------------------------------------------------------------------------------
void TablePage::on_clearSamplesButton_clicked()
{
    std::shared_ptr<TopicInfo> topicInfo = CommonData::getTopicInfo(m_topicName);
    if (!topicInfo || !topicInfo->typeCode)
    {
        return;
    }

    std::shared_ptr<OpenDynamicData> blankSample = CreateOpenDynamicData(topicInfo->typeCode, QosDictionary::getEncodingKind(), topicInfo->extensibility);
    CommonData::clearSamples(m_topicName);
    m_tableModel->setSample(blankSample);

    refreshPage();
}


//------------------------------------------------------------------------------
void TablePage::on_filterButton_clicked()
{
    const std::string topicName = m_topicName.toStdString();
    QItemSelectionModel* selectionModel = topicTableView->selectionModel();
    QModelIndexList indexList = selectionModel->selectedIndexes();
    QString initialText = "";


    // Create a default filter based on the table selection
    for (int i = 0; i < indexList.size(); i++)
    {
        // Skip non-name selections
        if (indexList.at(i).column() != TopicTableModel::NAME_COLUMN)
        {
            continue;
        }

        // Get the name of this selected topic member
        QModelIndex nameIndex = indexList.at(i);
        if (!nameIndex.isValid())
        {
            continue;
        }
        QString nameString = nameIndex.data(Qt::DisplayRole).toString();

        // Get the type string of this selected topic member
        QModelIndex typeIndex = nameIndex.sibling(nameIndex.row(), TopicTableModel::TYPE_COLUMN);
        if (!typeIndex.isValid())
        {
            continue;
        }
        QString typeString = typeIndex.data(Qt::DisplayRole).toString();

        // Get the value of this selected topic member
        QModelIndex valueIndex = nameIndex.sibling(nameIndex.row(), TopicTableModel::VALUE_COLUMN);
        if (!valueIndex.isValid())
        {
            continue;
        }
        QString valueString = valueIndex.data(Qt::DisplayRole).toString();

        // Append this topic member to the initial string
        if (!initialText.isEmpty())
        {
            initialText += " AND ";
        }

        initialText += nameString;

        // Add quotes around the value depending on type
        if (typeString == "enum" || typeString == "string")
        {
            initialText += " = '" + valueString + "'";
        }
        else
        {
            initialText += " = " + valueString;
        }

    } // End initial filter loop


    bool ok = false;
    QString usageString;
    usageString += "Install a topic filter using SQL syntax.\n";
    usageString += "A blank string will remove any existing filter.\n";
    usageString += "Example usage: color = 'BLUE' AND id = 1\n";

    // Prompt the user for the topic filter string
    QString filter = QInputDialog::getText(
        this,
        "Topic Filter",
        usageString,
        QLineEdit::Normal,
        initialText,
        &ok);

    // Only abort if the user didn't click 'ok'.
    // An empty string will remove the filter.
    if (!ok)
    {
        return;
    }

    // Make sure we have an information object for this topic
    std::shared_ptr<TopicInfo> topicInfo = CommonData::getTopicInfo(m_topicName);
    if (topicInfo == nullptr)
    {
        return;
    }


    if (!filter.isEmpty())
    {
        // Let OpenDDS validate the filter. An exception is thrown with details.
        // This will only check the SQL syntax.
        try
        {
            OpenDDS::DCPS::FilterEvaluator filterTest(filter.toUtf8().data(), false);
        }
        catch (const std::exception& e)
        {
            QString errorMessage = e.what();
            QMessageBox::warning(
                this,
                "Invalid Filter",
                "An invalid filter was created:\n" + errorMessage);

            return;
        }
    }

    m_refreshTimer.stop();
    CommonData::flushSamples(m_topicName);
    m_topicMonitor->setFilter(filter);
    m_refreshTimer.start(REFRESH_TIMEOUT);


    // We're getting data now, so show the correct freeze button state
    freezeButton->show();
    unfreezeButton->hide();

} // End TablePage::on_filterButton_clicked


//------------------------------------------------------------------------------
void TablePage::on_useLatestButton_clicked()
{
    // Use the latest sample if the button is checked
    if (useLatestButton->isChecked() && historyTable->rowCount() > 1)
    {
        QTableWidgetItem* item = historyTable->item(0, 0);
        if (item)
        {
            historyTable->clearSelection();
            historyTable->setCurrentItem(item);
            item->setSelected(true);
            setSample(item->text());
        }
    }

    refreshPage();
}


//------------------------------------------------------------------------------
void TablePage::on_freezeButton_clicked()
{
    freezeButton->hide();
    unfreezeButton->show();
    m_topicMonitor->pause();
}


//------------------------------------------------------------------------------
void TablePage::on_unfreezeButton_clicked()
{
    freezeButton->show();
    unfreezeButton->hide();
    m_topicMonitor->unpause();
}


//------------------------------------------------------------------------------
void TablePage::on_iniButton_clicked()
{
    // Prompt the user for an INI file
    QString iniPath = QFileDialog::getSaveFileName(
        this,
        "Save as INI",
        "",
        "*.ini");

    if (iniPath.isEmpty())
    {
        return;
    }


    QFile iniFile(iniPath);
    iniFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&iniFile);

    out << "[" << m_topicName << "]\n";
    for (int i = 0; i < m_tableModel->rowCount(); i++)
    {
        QModelIndex nameIndex =
            m_tableModel->index(i, TopicTableModel::NAME_COLUMN);
        QModelIndex valueIndex =
            m_tableModel->index(i, TopicTableModel::VALUE_COLUMN);

        out << m_tableModel->data(nameIndex).toString()
            << "="
            << m_tableModel->data(valueIndex).toString()
            << "\n";
    }

    iniFile.close();

} // End TablePage::on_iniButton_clicked


//------------------------------------------------------------------------------
void TablePage::on_revertButton_clicked()
{
    m_tableModel->revertChanges();
    revertButton->setEnabled(false);
}


//------------------------------------------------------------------------------
void TablePage::on_publishButton_clicked()
{
    const std::shared_ptr<OpenDynamicData> sample = m_tableModel->commitSample();
    if (!sample)
    {
        return;
    }

    m_topicReplayer->publishSample(sample);
    revertButton->setEnabled(false);
}


//------------------------------------------------------------------------------
void TablePage::on_historyTable_itemSelectionChanged()
{
    QList<QTableWidgetItem*> items = historyTable->selectedItems();
    for (int i = 0; i < items.count(); i++)
    {
        QTableWidgetItem* selectedItem = items.at(i);
        if (selectedItem->row() != 0)
        {
            useLatestButton->setChecked(false);
        }

        setSample(selectedItem->text());
    }
}


//------------------------------------------------------------------------------
void TablePage::on_newPlotButton_clicked()
{
    QItemSelectionModel* selectionModel = topicTableView->selectionModel();
    QModelIndexList indexList = selectionModel->selectedIndexes();
    QStringList selectedVariables;

    // Populate the list of variables we want to plot
    for (int i = 0; i < indexList.size(); i++)
    {
        if (indexList.at(i).column() == TopicTableModel::NAME_COLUMN)
        {
            selectedVariables << indexList.at(i).data(Qt::DisplayRole).toString();
        }
    }

    // If nothing was selected, don't create the plot page
    if (selectedVariables.isEmpty())
    {
        return;
    }

    QDialog *plotDialog = new QDialog(this);
    QVBoxLayout *layout = new QVBoxLayout(plotDialog);
    GraphPage *graphPage = new GraphPage(plotDialog);
    static int plotCounter = 0;
    plotCounter++;

    plotDialog->setAttribute(Qt::WA_DeleteOnClose);
    plotDialog->setObjectName("plotDialog");
    plotDialog->setWindowFlags(
        Qt::CustomizeWindowHint |
        Qt::WindowTitleHint |
        Qt::Window |
        Qt::WindowCloseButtonHint);

    QString windowTitle =
        "DDS Variable Plot " +
        QString::number(plotCounter) +
        " [" + m_topicName + "]";

    // Store the graph object name for reference with attaching new members
    QString objectName =
        "Plot " +
        QString::number(plotCounter) +
        " [" + m_topicName + "]";
    graphPage->setObjectName(objectName);

    layout->addWidget(graphPage);
    plotDialog->setWindowTitle(windowTitle);
    plotDialog->setWindowIcon(QIcon(":/images/monitor.png"));
    plotDialog->setSizeGripEnabled(true);
    plotDialog->setLayout(layout);
    plotDialog->resize(700, 500);
    plotDialog->show();

    // Add the selected variables to the plot page
    for (int i = 0; i < selectedVariables.size(); i++)
    {
        graphPage->addVariable(m_topicName, selectedVariables.at(i));
    }

} // End TablePage::on_newPlotButton_clicked


//------------------------------------------------------------------------------
void TablePage::on_attachPlotButton_clicked()
{
    QItemSelectionModel* selectionModel = topicTableView->selectionModel();
    QModelIndexList indexList = selectionModel->selectedIndexes();
    QMap<QString, GraphPage*> graphPages;
    QStringList selectedVariables;
    QStringList graphNames;

    // Populate the list of variables we want to plot
    for (int i = 0; i < indexList.size(); i++)
    {
        if (indexList.at(i).column() == TopicTableModel::NAME_COLUMN)
        {
            selectedVariables << indexList.at(i).data(Qt::DisplayRole).toString();
        }
    }

    // If nothing was selected, don't create the plot page
    if (selectedVariables.isEmpty())
    {
        return;
    }


    // Find the target graph page object
    QWidgetList mainWidgets = QApplication::topLevelWidgets();
    for (int i = 0; i < mainWidgets.count(); i++)
    {
        QList<GraphPage*> graphWidgets =
            mainWidgets.at(i)->findChildren<GraphPage*>();

        for (int ii = 0; ii < graphWidgets.count(); ii++)
        {
            if (!graphWidgets.at(ii) ||
                graphWidgets.at(ii)->objectName().isEmpty())
            {
                continue;
            }

            // Store this graph page for future reference
            QString graphName = graphWidgets.at(ii)->objectName();
            graphNames << graphName;
            graphPages[graphName] = graphWidgets.at(ii);
        }
    }

    if (graphNames.isEmpty())
    {
        return;
    }

    graphNames.removeDuplicates();
    graphNames.sort();


    QString targetPlotName = graphNames.at(0);

    // Prompt the user for a target plot if more than 1 exists
    if (graphNames.count() > 1)
    {
        targetPlotName = QInputDialog::getItem(
            this,
            "Choose Existing Plot",
            "Attach to:",
            graphNames,
            0,
            false);
    }

    if (targetPlotName.isEmpty())
    {
        return;
    }

    GraphPage *graphPage = graphPages.value(targetPlotName);
    if (!graphPage)
    {
        return;
    }

    // Add the selected variables to the plot page
    for (int i = 0; i < selectedVariables.size(); i++)
    {
        graphPage->addVariable(m_topicName, selectedVariables.at(i));
    }

} // End TablePage::on_attachPlotButton_clicked


//------------------------------------------------------------------------------
void TablePage::on_recordButton_clicked()
{
    QItemSelectionModel* selectionModel = topicTableView->selectionModel();
    QModelIndexList indexList = selectionModel->selectedIndexes();
    QStringList selectedVariables;

    // Populate the list of variables we want to plot
    for (int i = 0; i < indexList.size(); i++)
    {
        if (indexList.at(i).column() == TopicTableModel::NAME_COLUMN)
        {
            selectedVariables << indexList.at(i).data(Qt::DisplayRole).toString();
        }
    }

    // If nothing was selected, don't create the plot page
    if (selectedVariables.isEmpty())
    {
        return;
    }


    RecorderDialog* recorder = new RecorderDialog(m_topicName, selectedVariables, this);
    recorder->show();

} // End TablePage::on_recordButton_clicked


//------------------------------------------------------------------------------
void TablePage::on_topicTableView_pressed(const QModelIndex& index)
{
    if (index.column() == TopicTableModel::VALUE_COLUMN)
    {
        useLatestButton->setChecked(false);
    }
}


//------------------------------------------------------------------------------
void TablePage::dataHasChanged()
{
    revertButton->setEnabled(true);
}


//------------------------------------------------------------------------------
void TablePage::refreshPage()
{
    // Display the number of readers and writers for this topic
    const std::string topicName = m_topicName.toStdString();

    //dds::sub::DataReader<DynamicData> dr =
    //    CommonData::m_ddsManager->getReader<DynamicData>
    //    (topicName, DATA_READER_NAME);

    //dds::pub::DataWriter<DynamicData> dw =
    //    CommonData::m_ddsManager->getWriter<DynamicData>(topicName);

    //if (dr != nullptr && dw != nullptr)
    //{
    //    dds::core::status::SubscriptionMatchedStatus subStatus =
    //        dr->subscription_matched_status();
    //    dds::core::status::PublicationMatchedStatus pubStatus =
    //        dw->publication_matched_status();

    //    matchedSubLabel->setText(QString::number(pubStatus.current_count()));
    //    matchedPubLabel->setText(QString::number(subStatus.current_count()));
    //}


    // Disable the plot buttons if we don't have a valid selection
    QItemSelectionModel* selectionModel = topicTableView->selectionModel();
    QModelIndexList indexList = selectionModel->selectedIndexes();
    if (indexList.isEmpty())
    {
        newPlotButton->setEnabled(false);
        attachPlotButton->setEnabled(false);
        recordButton->setEnabled(false);
    }
    else
    {
        newPlotButton->setEnabled(true);
        attachPlotButton->setEnabled(true);
        recordButton->setEnabled(true);
    }


    // Don't repopulate the history widget is nothing changed
    QStringList sampleNames = CommonData::getSampleList(m_topicName);
    if (m_historyList == sampleNames)
    {
        return;
    }

    m_historyList = sampleNames;
    historyTable->clearContents();
    historyTable->setRowCount(sampleNames.size());
    for (int i = 0; i < sampleNames.size(); i++)
    {
        QTableWidgetItem* item = new QTableWidgetItem;
        item->setText(sampleNames.at(i));
        historyTable->setItem(i, 0, item);

        if (item->text() == m_selectedSample && !useLatestButton->isChecked())
        {
            item->setSelected(true);
        }
    }

    // Use the latest sample if the button is checked
    if (useLatestButton->isChecked() && historyTable->rowCount() > 0)
    {
        QTableWidgetItem* item = historyTable->item(0, 0);
        if (item)
        {
            historyTable->clearSelection();
            historyTable->setCurrentItem(item);
            item->setSelected(true);
            setSample(item->text());
        }
    }

} // End TablePage::refreshPage


//------------------------------------------------------------------------------
void TablePage::setSample(const QString& sampleName)
{
    if (sampleName.isEmpty())
    {
        return;
    }

    const int index = historyTable->currentRow();
    if (index < 0)
    {
        return;
    }

    m_selectedSample = sampleName;

    const auto topicInfo = CommonData::getTopicInfo(m_topicName);
    if (topicInfo && topicInfo->typeCode)
    {
        auto sample = CommonData::copySample(m_topicName, index);
        if (sample != nullptr)
        {
            m_tableModel->setSample(sample);
            //printf("\n=== %s ===\n", m_topicName.toUtf8().data());
            //sample->dump();
        }
    }
    else
    {
        DDS::DynamicData_var sample = CommonData::copyDynamicSample(m_topicName, index);
        if (sample)
        {
            m_tableModel->setSample(sample);
        }
    }

    revertButton->setEnabled(false);
}


/**
 * @}
 */
