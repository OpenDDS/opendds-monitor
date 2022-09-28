#include "participant_page.h"
#include "participant_monitor.h"
#include "participant_table_model.h"

#include <QWidget>


//------------------------------------------------------------------------------
ParticipantPage::ParticipantPage(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);

    participantTableView->setModel(&m_tableModel);
    participantTableView->hideColumn(ParticipantTableModel::COLUMN_GUID);
}





void ParticipantPage::addParticipant(const ParticipantInfo& info)
{ 
    m_tableModel.addParticipant(info); 
}


void ParticipantPage::removeParticipant(const ParticipantInfo& info)
{ 
    m_tableModel.removeParticipant(info); 
}


/**
 * @}
 */
