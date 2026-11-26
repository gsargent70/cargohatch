#pragma once

#include <QDialog>

#include "folderaliases.h"
#include "pathcompleter.h"

class FolderPicker;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;

// One alias: its name and folder.
class AliasEditDialog : public QDialog {
    Q_OBJECT

public:
    // `taken` are the user's other aliases.
    AliasEditDialog(const QString &title, const FolderAliases::Alias &alias, const QStringList &taken,
                    FolderPicker *picker, QWidget *parent = nullptr);
    FolderAliases::Alias alias() const;

private:
    void validate();

    QLineEdit *m_name;
    QLineEdit *m_path;
    QLabel *m_note;
    QPushButton *m_ok;
    QStringList m_taken;
    FolderPicker *m_picker;
};

// Go > Folder Aliases > Edit Aliases… (NAV-07).
class AliasDialog : public QDialog {
    Q_OBJECT

public:
    AliasDialog(const QList<FolderAliases::Alias> &aliases, FolderPicker *picker, QWidget *parent = nullptr);
    QList<FolderAliases::Alias> aliases() const { return m_aliases; }

    // Those whose folders aren't there.
    static QList<FolderAliases::Alias> broken(const QList<FolderAliases::Alias> &aliases);

private:
    void fill();
    void updateButtons();
    int selected() const;
    QStringList namesExcept(int index) const;
    void edit(int index, const QString &title, FolderAliases::Alias alias);

    QList<FolderAliases::Alias> m_aliases;
    FolderPicker *m_picker;
    QLineEdit *m_search;
    QTreeWidget *m_list;
    QCheckBox *m_showBuiltIn;
    QPushButton *m_edit, *m_duplicate, *m_delete, *m_deleteBroken;
};
