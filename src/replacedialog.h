#pragma once

#include <QDialog>

#include "fileops.h"

class QCheckBox;

// Asks what to do about an item whose name is taken (COPY-03): the existing
// and incoming items side by side, with previews, sizes and dates, and the
// newer or larger one marked.
class ReplaceDialog : public QDialog {
    Q_OBJECT

public:
    explicit ReplaceDialog(const FileOperation::Conflict &conflict, QWidget *parent = nullptr);
    FileOperation::Resolution resolution() const { return m_resolution; }

    static FileOperation::Resolution ask(const FileOperation::Conflict &conflict, QWidget *parent);
    // A preview for the dialog: a picture's thumbnail, or its type's icon.
    static QPixmap preview(const QString &path, bool isDir, int size);

private:
    void choose(WhenExists action, const QString &newName = {});
    void chooseName();

    FileOperation::Conflict m_conflict;
    FileOperation::Resolution m_resolution;
    QCheckBox *m_forAll;
};
