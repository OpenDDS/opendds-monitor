#ifndef _DDSMON_EDITOR_DELEGATES_H
#define _DDSMON_EDITOR_DELEGATES_H

#include "first_define.h"
#include <QStringList>
#include <QItemDelegate>


/**
 * @brief ComboBox tableview delegate
 */
class ComboDelegate : public QItemDelegate
{
public:

    /**
     * @brief Constructor for the combobox delegate
     * @param[in] parent The parent of this delegate
     */
    ComboDelegate(QObject* parent);

    /**
     * @brief Destructor for the combobox delegate
     */
    ~ComboDelegate();

    /**
     * @brief Append an item to the combobox list
     * @param[in] itemName The new combobox item text
     * @param[in] value The integer value for the combo item.
     */
    void addItem(const QString& itemName, const int& value);

    /**
     * @brief Remove all the items from the editor
     */
    void removeItems();

    /**
     * @brief Get the integer value for a given combobox string.
     * @param[in] itemName The combobox item name.
     * @return The integer value for the target combobox string.
     */
    int getValue(const QString& itemName) const;

    /**
     * @brief Creates the combobox delegate widget
     * @param[in] parent The parent of the new delegate widget
     * @param[in] option Unused but required by QItemDelegate
     * @param[in] index The model index of this cell
     * @return The editor delegate widget
     */
    QWidget* createEditor(QWidget* parent,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const;

    /**
     * @brief Populates the delegate widget with the current value on the table
     * @param[out] editor Set this delegate widget value to match the view
     * @param[in] index The model index of this cell
     */
    void setEditorData(QWidget* editor, const QModelIndex& index) const;

private:

    /// The list of items to appear in the combobox delegate
    QStringList m_itemNames;

    /// The integer values for each combobox selection
    QList<int> m_values;
};


/**
 * @brief LineEdit tableview delegate
 */
class LineEditDelegate : public QItemDelegate
{
public:

    /**
     * @brief Constructor for the LineEdit delegate
     * @param[in] parent The parent of this delegate
     */
    LineEditDelegate(QObject* parent);

    /**
     * @brief Destructor for the combobox delegate
     */
    ~LineEditDelegate();

    /**
     * @brief Creates the LineEdit delegate widget
     * @param[in] parent The parent of the new delegate widget
     * @param[in] option Unused but required by QItemDelegate
     * @param[in] index The model index of this cell
     * @return The editor delegate widget
     */
    QWidget* createEditor(QWidget* parent,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const;

    /**
     * @brief Populates the delegate widget with the current value on the table
     * @param[out] editor Set this delegate widget value to match the view
     * @param[in] index The model index of this cell
     */
    void setEditorData(QWidget* editor, const QModelIndex& index) const;

};


#endif

/**
 * @}
 */
