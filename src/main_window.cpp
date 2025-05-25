#include "main_window.h"
#include "table_page.h"
#include "log_page.h"
#include "dds_data.h"
#include "dds_manager.h"
#include "participant_page.h"
#include "publication_monitor.h"
#include "subscription_monitor.h"

#include <dds/DCPS/transport/framework/TransportRegistry.h>
#include <dds/DCPS/RTPS/RtpsDiscovery.h>

#include <QInputDialog>
#include <QMessageBox>
#include <QSettings>
#include <QTime>

#include <iostream>
#include <iomanip>


//------------------------------------------------------------------------------
DDSMonitorMainWindow::DDSMonitorMainWindow() :
    m_logPage(nullptr),
    m_participantPage(nullptr)
{
    setupUi(this);
    parseCmd();

    // Remove the blank tab page
    on_mainTabWidget_tabCloseRequested(0);

    // Create the log page
    QIcon logIcon(":/images/terminal.png");
    m_logPage = new LogPage(mainTabWidget);
    mainTabWidget->addTab(m_logPage, logIcon, "Log");
    mainTabWidget->setCurrentWidget(m_logPage);


    // Did the user set the domain from the command line?
    int domainID = -2;
    QCoreApplication *thisApp = QApplication::instance();
    if (thisApp->property("domain").isValid())
    {
        domainID = thisApp->property("domain").toInt();
    }

    bool ok = true;

    // Load the previous domain setting
    QSettings settings(SETTINGS_ORG_NAME, SETTINGS_APP_NAME);
    if (domainID == -2)
    {
        // Prompt the user for the domain selection
        domainID = settings.value("domainID").toInt();

        domainID = QInputDialog::getInt(
            this, "Join Domain", "Join DDS Domain:", domainID, -1, 232, 1, &ok);
    }

    if (ok)
    {
        settings.setValue("domainID", domainID);
        setWindowTitle("DDS Monitor - Domain " + QString::number(domainID));
        activateWindow();

        // Join the DDS domain
        try {
            CommonData::m_ddsManager = std::make_unique<DDSManager>();
            m_participantPage = new ParticipantPage(mainTabWidget);
            CommonData::m_ddsManager->joinDomain(domainID, "", [page = m_participantPage](const ParticipantInfo& info) {page->addParticipant(info); },
                [page = m_participantPage](const ParticipantInfo& info) {page->removeParticipant(info); });
            m_publicationMonitor = std::make_unique<PublicationMonitor>();
            m_subscriptionMonitor = std::make_unique<SubscriptionMonitor>();
        }
        catch (std::runtime_error &e) {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.setText(QString("Error starting OpenDDS: ") + e.what());
            msgBox.exec();
            exit(1);
        }

        // Send DDS configuration to the log screen
        reportConfig();

        QIcon participantIcon(":/images/stock_data-table.png");
        mainTabWidget->addTab(m_participantPage, participantIcon, "Participants");

        // Signals and slot connections
        connect(m_publicationMonitor.get(), SIGNAL(newTopic(const QString&)),
            this, SLOT(discoveredTopic(const QString&)));
        connect(m_subscriptionMonitor.get(), SIGNAL(newTopic(const QString&)),
            this, SLOT(discoveredTopic(const QString&)));

        // Now that everything is connected, enable the domain
        CommonData::m_ddsManager->enableDomain();
    }
    else
    {
        Closing = true;
    }

}


//------------------------------------------------------------------------------
DDSMonitorMainWindow::~DDSMonitorMainWindow()
{
    for (int i = 0; i < mainTabWidget->count(); i++)
    {
        QWidget* removedTab = mainTabWidget->widget(i);
        mainTabWidget->removeTab(i);
        removedTab->close();
        delete removedTab;
    }
    mainTabWidget->clear();

    CommonData::cleanup();
    ShutdownDDS();
}


//------------------------------------------------------------------------------
void DDSMonitorMainWindow::discoveredTopic(const QString& topicName)
{
    std::shared_ptr<TopicInfo> topicInfo = CommonData::getTopicInfo(topicName);
    if (topicInfo == nullptr)
    {
        return;
    }

    QTreeWidgetItem* dataItem = nullptr;
    QTreeWidgetItem* topicItem = new QTreeWidgetItem(topicTree);
    topicItem->setText(0, topicName);

    dataItem = new QTreeWidgetItem(topicItem);
    dataItem->setText(0, "Type: " + QString(topicInfo->typeName().c_str()));

    DDS::DurabilityQosPolicy durabilityQos = topicInfo->topicQos().durability;
    DDS::ReliabilityQosPolicy reliabilityQos = topicInfo->topicQos().reliability;
    DDS::OwnershipQosPolicy ownerQos = topicInfo->topicQos().ownership;
    DDS::DestinationOrderQosPolicy orderQos = topicInfo->topicQos().destination_order;

    dataItem = new QTreeWidgetItem(topicItem);
    dataItem->setText(0, "Durability: " +
        DURABILITY_QOS_POLICY_STRINGS[durabilityQos.kind]);

    dataItem = new QTreeWidgetItem(topicItem);
    dataItem->setText(0, "Reliability: " +
        RELIABILITY_QOS_POLICY_STRINGS[reliabilityQos.kind]);

    dataItem = new QTreeWidgetItem(topicItem);
    dataItem->setText(0, "Ownership: " +
        OWNERSHIP_QOS_POLICY_STRINGS[ownerQos.kind]);

    dataItem = new QTreeWidgetItem(topicItem);
    dataItem->setText(0, "Dest Order: " +
        DESTINATION_ORDER_QOS_POLICY_STRING[orderQos.kind]);

    topicTree->sortByColumn(0, Qt::AscendingOrder);

}


//------------------------------------------------------------------------------
void DDSMonitorMainWindow::on_topicTree_itemClicked(QTreeWidgetItem* item, int)
{
    // Topic names are at the root level
    if (item->parent())
    {
        return;
    }

    // If a topic page for this topic already exists, we're done
    const QString topicName = item->text(0);
    for (int i = 0; i < mainTabWidget->count(); i++)
    {
        if (mainTabWidget->tabText(i) == topicName)
        {
            mainTabWidget->setCurrentIndex(i);
            return;
        }
    }

    std::shared_ptr<TopicInfo> topicInfo = CommonData::getTopicInfo(topicName);
    if (topicInfo == nullptr)
    {
        std::cerr << "Could not find topic information for topic \"" << topicName.toStdString() << "\"" << std::endl;
        return;
    }

    QString selectedPartition = "";
    // If we only have 1 partition, use it and don't prompt the user
    if (topicInfo->partitions().count() == 1)
    {
        selectedPartition = topicInfo->partitions().at(0);
    }

    // Prompt the user for a partition if they have a choice
    if (topicInfo->partitions().count() > 1)
    {
        bool ok = false;
        selectedPartition = QInputDialog::getItem(
            this,
            SETTINGS_APP_NAME,
            "Choose a Partition:",
            topicInfo->partitions(),
            0,
            false,
            &ok);

        if (!ok)
        {
            return;
        }
    }

    DDS::PartitionQosPolicy partitionInfo;

    // Use the target partition if it was specified
    if (!selectedPartition.isEmpty())
    {
        partitionInfo.name.length(1);
        partitionInfo.name[0] = selectedPartition.toStdString().c_str();
    }

    topicInfo->subQos().partition = partitionInfo;
    topicInfo->pubQos().partition = partitionInfo;

    // Create a new topic page
    QIcon tableIcon(":/images/stock_data-table.png");

    try {
      TablePage* page = new TablePage(topicName, mainTabWidget);
      mainTabWidget->addTab(page, tableIcon, topicName);

      // Jump to the new tab
      mainTabWidget->setCurrentWidget(page);
    }
    catch (std::runtime_error &e) {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(QString("Error creating table page for topic \"") + topicName + "\":\n" + e.what());
        msgBox.exec();
    }
}


//------------------------------------------------------------------------------
void DDSMonitorMainWindow::on_mainTabWidget_tabCloseRequested(int pageIndex)
{
    // Don't let the user close the log and participant pages
    const QString pageName = mainTabWidget->tabText(pageIndex);
    if (pageName == "Log" || pageName == "Participants")
    {
        return;
    }

    QWidget* removedTab = mainTabWidget->widget(pageIndex);
    mainTabWidget->removeTab(pageIndex);
    removedTab->close();
    delete removedTab;
}


//------------------------------------------------------------------------------
void DDSMonitorMainWindow::parseCmd()
{
    QCoreApplication* thisApp = QApplication::instance();
    QStringList argv = thisApp->arguments();
    int argc = argv.count();
    QStringList argList;
    QString argString;

    // Split <arg1>=<arg2> into seperate list rows.
    for (int i = 0; i < argc; i++)
    {
        argString = argv[i];
        if (argString.contains("="))
        {
            argList << argString.split("=");
        }
        else
        {
            argList << argString;
        }
    }

    // Loop through all the command line arguments
    for (int i = 1; i < argList.count(); i++)
    {
        argString = argList.at(i);
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        argString.remove(QRegExp("^-*")); // Remove any - prefix
#else
        argString.remove(QRegularExpression("^-*")); // Remove any - prefix
#endif

        // Help!
        if (argString == "h" || argString == "help")
        {
            std::cout
                << "\nUsage: "
                << argList.at(0).toStdString()
                << " --domain=[-1-232]"
                << std::endl;

            exit(0);
        }

        // Make sure the index is in range
        if (i + 1 >= argList.count())
        {
            continue;
        }

        // Did the user specify a domain?
        if (argString == "domain")
        {
            int domainID = argList.at(i + 1).toInt();
            if (domainID < -1 || domainID > 232)
            {
                std::cerr << "Invalid domain command line argument. "
                          << "The ID must be -1 to 232."
                          << std::endl;

                //QMessageBox::critical(this,
                //    "Invalid Argument",
                //    "Invalid domain command line argument.\n"
                //    "The ID must be 0 to 232.");

                exit(1);
            }
            thisApp->setProperty("domain", domainID);
        }

    }

}


//------------------------------------------------------------------------------
void DDSMonitorMainWindow::reportConfig() const
{
    if (!CommonData::m_ddsManager)
    {
        return;
    }

    int domainID = 0;
    DDS::DomainParticipant* domain = CommonData::m_ddsManager ? CommonData::m_ddsManager->getDomainParticipant() : nullptr;

    if (domain)
    {
        domainID = domain->get_domain_id();
    }

    // Get the participant ID for this domain
    //const uint32_t pID = domain->get_instance_handle();

    // Report the high level info to the log window
    OpenDDS::DCPS::Service_Participant* service = TheServiceParticipant;
    std::cout << "\nDomain ID:                       " << domainID;
    //std::cout << "\nParticipate ID:                  " << pID;
    std::cout << "\nDefault address:                 " << service->default_address().to_addr().get_host_addr();

    // Report the discovery information to the log window
    using OpenDDS::DCPS::Discovery;
    using OpenDDS::DCPS::Service_Participant;

    const Discovery::RepoKey discoveryKey = service->get_default_discovery();
    const Service_Participant::RepoKeyDiscoveryMap& discoveryMap = service->discoveryMap();
    if (discoveryMap.find(discoveryKey) != discoveryMap.end())
    {
        const OpenDDS::DCPS::Discovery_rch discoveryInfo = discoveryMap.at(discoveryKey);
        const OpenDDS::RTPS::RtpsDiscovery* rtpsInfo =
            dynamic_cast<const OpenDDS::RTPS::RtpsDiscovery*>
            (discoveryInfo.in());

        // Only care about RTPS for now
        if (rtpsInfo)
        {
            // Ports are defined from RTPS discovery spec
            const uint16_t pb = rtpsInfo->pb(); // Port base [7400]
            const uint16_t dg = rtpsInfo->dg(); // Domain ID gain [250]
            //const uint16_t pg = rtspInfo->pg(); // Participant ID gain [2]
            const uint16_t d0 = rtpsInfo->d0(); // Discovery multicast port offset [0]
            //const uint16_t d1 = rtspInfo->d1(); // Discovery unicast port offset [10]
            //const uint16_t d2 = 1; // User data multicast port offset
            //const uint16_t d3 = 11; // User data unicast port offset
            //const uint16_t dx = rtspInfo->dx(); // ???

            const uint16_t discoveryMulicastPort = pb + (dg * domainID) + d0;
            //const uint16_t discoveryUnicastPort = pb + (dg * domainID) + (pg * pID) + d1;
            //const uint16_t userUnicastPort = pb + (dg * domainID) + (pg * pID) + d3;
            //const uint16_t userMulicastPort = pb + (dg * domainID) + d2;

            std::cout << "\nDiscovery multicast:             "
                      << rtpsInfo->default_multicast_group(
#if OPENDDS_VERSION_AT_LEAST(3, 27, 0)
                        domainID).to_addr(
#endif
                        ).get_host_addr()
                      << ":"
                      << discoveryMulicastPort
                      //<< "\nDiscovery unicast:               "
                      //<< rtspInfo->spdp_local_address()
                      //<< ":"
                      //<< discoveryUnicastPort
                      //<< "\nUser multicast:                  "
                      //<< rtspInfo->default_multicast_group()
                      //<< ":"
                      //<< userMulicastPort
                      //<< "\nUser unicast:                    "
                      //<< rtspInfo->spdp_local_address()
                      //<< ":"
                      //<< userUnicastPort
                      //<< "\nSEDP local address:              "
                      //<< rtspInfo->sedp_local_address()
                      //<< "\nSEDP multicast:                  "
                      //<< rtspInfo->sedp_multicast()
                      //<< "\nMulticast interface:             "
                      //<< rtspInfo->multicast_interface()
                      //<< "\nGUID interface:                  "
                      //<< rtspInfo->guid_interface()
                      << "\nSupports liveliness:             "
                      << rtpsInfo->supports_liveliness()
                      << "\nResend period:                   "
                      << rtpsInfo->resend_period().to_dds_duration().sec
                      << "\nTTL:                             "
                      << static_cast<int>(rtpsInfo->ttl())
                      //<< "\nPB:                              " << pb
                      //<< "\nDG:                              " << dg
                      //<< "\nPG:                              " << pg
                      //<< "\nD0:                              " << d0
                      //<< "\nD1:                              " << d1
                      //<< "\nDX:                              " << dx
                      << "\n\n";
        } // End rtspInfo check
    } // End discoveryMap check


    // Report the transport information to the log window
    const OpenDDS::DCPS::TransportRegistry* transportReg =
        OpenDDS::DCPS::TransportRegistry::instance();

    // DDSManager creates a config-# name based on the domain.
    const OpenDDS::DCPS::TransportConfig_rch transportConfig =
        transportReg->get_config("config-" + std::to_string(domainID));

    if (transportConfig.is_nil())
    {
        return;
    }

    std::cout << "Configuration name:              "
              << transportConfig->name()
              << "\n\n";

    const size_t transportConfigCount = transportConfig->instances_.size();
    for (size_t i = 0; i < transportConfigCount; i++)
    {
        const OpenDDS::DCPS::TransportInst_rch transportInstance =
            transportConfig->instances_[i];

        std::cout << transportInstance->dump_to_str(
#if OPENDDS_VERSION_AT_LEAST(3, 27, 0)
          domainID
#endif
        ) << "\n";
    }

}


/**
 * @}
 */
