#include "editor_delegates.h"

#include <QComboBox>
#include <QListView>
#include <QLineEdit>


//------------------------------------------------------------------------------
ComboDelegate::ComboDelegate(QObject* parent) : QItemDelegate(parent)
{
    removeItems();
}


//------------------------------------------------------------------------------
ComboDelegate::~ComboDelegate()
{}


//------------------------------------------------------------------------------
void ComboDelegate::addItem(const QString& itemName, const int& value)
{
    m_itemNames.append(itemName);
    m_values.append(value);
}


//------------------------------------------------------------------------------
void ComboDelegate::removeItems()
{
    m_itemNames.clear();
    m_values.clear();

    m_itemNames.append("-");
    m_values.append(0);
}


//------------------------------------------------------------------------------
int ComboDelegate::getValue(const QString& itemName) const
{
    for (int i = 0; i < m_itemNames.size(); i++)
    {
        if (m_itemNames.at(i) == itemName && i < m_values.size())
        {
            return m_values.at(i);
        }
    }

    return 0;
}


//------------------------------------------------------------------------------
QWidget*  ComboDelegate::createEditor(QWidget* parent,
    const QStyleOptionViewItem& ,
    const QModelIndex& ) const
{
    QComboBox* editor = new QComboBox(parent);

    editor->setView(new QListView(editor));
    editor->addItems(m_itemNames);

    connect(editor, SIGNAL(currentIndexChanged(int)), editor, SLOT(close()));
    return editor;
}


//------------------------------------------------------------------------------
void ComboDelegate::setEditorData(QWidget* editor,
    const QModelIndex& index) const
{
    QComboBox* delegate = dynamic_cast<QComboBox*>(editor);

    // Jump to the current item if it's found
    QString currentText = index.data().toString();
    const int targetIndex = delegate->findText(currentText);
    if (targetIndex != -1)
    {
        delegate->setCurrentIndex(targetIndex);
    }
}


//------------------------------------------------------------------------------
LineEditDelegate::LineEditDelegate(QObject* parent) : QItemDelegate(parent)
{}


//------------------------------------------------------------------------------
LineEditDelegate::~LineEditDelegate()
{}


//------------------------------------------------------------------------------
QWidget*  LineEditDelegate::createEditor(QWidget* parent,
    const QStyleOptionViewItem&,
    const QModelIndex&) const
{
    QLineEdit* editor = new QLineEdit(parent);
    connect(editor, SIGNAL(editingFinished()), editor, SLOT(close()));
    return editor;
}


//------------------------------------------------------------------------------
void LineEditDelegate::setEditorData(QWidget* editor,
    const QModelIndex& index) const
{
    QLineEdit* delegate = dynamic_cast<QLineEdit*>(editor);
    delegate->setText(index.data().toString());
}


/**
 * @}
 */
