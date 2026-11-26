#pragma once

#include <QDialog>
#include <QString>

#include "pathcompleter.h"

class QLabel;
class QLineEdit;
class QPushButton;

// Picks a folder through the desktop's own file chooser, so the dialog
// matches the rest of the desktop. An interface so tests can do without one.
class FolderPicker {
public:
    virtual ~FolderPicker() = default;
    // The chosen folder, or empty if the user cancelled. Sets `error` if
    // no chooser could be shown.
    virtual QString pickFolder(QWidget *parent, const QString &title, const QString &start, QString *error) = 0;
};

// The xdg-desktop-portal FileChooser, over D-Bus.
class PortalFolderPicker : public FolderPicker {
public:
    QString pickFolder(QWidget *parent, const QString &title, const QString &start, QString *error) override;
};

// Asks where to copy or move to (COPY-01): a path that can be typed, with
// completion, or chosen with the desktop's chooser. A folder that doesn't
// exist yet is created by the copy.
class ChooseFolderDialog : public QDialog {
    Q_OBJECT

public:
    // A typed relative path is taken from `base`.
    ChooseFolderDialog(const QString &title, const QString &start, const QString &base, FolderPicker *picker,
                       QWidget *parent = nullptr);
    // The typed path made absolute, with ~ expanded.
    QString folder() const;
    void setCompletionOptions(const PathCompleter::Options &options) { m_completer->setOptions(options); }

private:
    void validate();

    QLineEdit *m_path;
    PathCompleter *m_completer;
    QLabel *m_note;
    QPushButton *m_ok;
    FolderPicker *m_picker;
    QString m_base;
};
