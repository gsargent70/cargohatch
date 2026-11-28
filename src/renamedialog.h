#pragma once

#include "batchrename.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QTreeWidget;

// Rename Several (REN-03 in part, REN-05, REN-10): find and replace in the
// selected names and change their capitalisation, with a preview of every
// new name that updates as the choices change. Rename stays off while
// nothing would change or a new name has a problem.
class RenameDialog : public QDialog {
    Q_OBJECT

public:
    // `items` are the names to rename and whether each is a folder; `others`
    // the rest of the folder's names.
    RenameDialog(const BatchRename &rules, const QList<QPair<QString, bool>> &items, const QSet<QString> &others,
                 QWidget *parent = nullptr);

    BatchRename rules() const;
    QList<BatchRename::Item> plan() const { return m_plan; }

    // For tests.
    QLineEdit *findField() const { return m_find; }
    QLineEdit *replaceField() const { return m_replace; }
    QTreeWidget *previewList() const { return m_preview; }
    QString summary() const;
    bool canRename() const;

    static QString caseTitle(BatchRename::Case how);
    static QString extensionCaseTitle(BatchRename::ExtensionCase how);

private:
    void updatePreview();

    QList<QPair<QString, bool>> m_items;
    QSet<QString> m_others;
    QList<BatchRename::Item> m_plan;
    QLineEdit *m_find;
    QLineEdit *m_replace;
    QCheckBox *m_matchCase;
    QCheckBox *m_ignoreExtension;
    QComboBox *m_case;
    QComboBox *m_extensionCase;
    QCheckBox *m_applyWithoutMatch;
    QTreeWidget *m_preview;
    QLabel *m_summary;
    QDialogButtonBox *m_buttons;
};
