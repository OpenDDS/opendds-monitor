#ifndef DEF_PARTICIPANT_PAGE_WIDGET
#define DEF_PARTICIPANT_PAGE_WIDGET

#include "first_define.h"
#include "ui_participant_page.h"
#include "participant_table_model.h"

class ParticipantTableModel;


/**
 * @brief The message participant page class.
 */
class ParticipantPage : public QWidget, public Ui::ParticipantPage
{
    Q_OBJECT

public:

    /**
     * @brief Constructor for ParticipantPage.
     * @param[in] parent The parent of this Qt object.
     */
    ParticipantPage(QWidget* parent);

    /**
     * @brief Destructor for ParticipantPage.
     */
    virtual ~ParticipantPage() = default;

    //Add a participant
    void addParticipant(const ParticipantInfo& info);
    void removeParticipant(const ParticipantInfo& info);

private:

    /// Data model for the DDS participants on this page.
    ParticipantTableModel m_tableModel;

}; // End ParticipantPage

#endif


/**
 * @}
 */
