#pragma once

#include <QObject>
#include <QPointer>

#include "fileops.h"
#include "progresswindow.h"

class QWidget;

// Runs a FileOperation on a worker thread, shows its progress window once
// it has taken more than a moment (COPY-08), answers its questions with
// dialogs (COPY-03, DEL-02), and reports errors and skipped items at the
// end. Deletes itself and the operation when done.
class OperationRunner : public QObject {
    Q_OBJECT

public:
    // Starts `operation`, which the runner takes over. Dialogs are parented
    // to `window` while it exists.
    static OperationRunner *start(FileOperation *operation, QWidget *window);

    FileOperation *operation() const { return m_operation; }
    // Leaves errors to whoever handles finished() instead of showing them;
    // for tests, which can't answer a dialog.
    void setQuiet(bool quiet) { m_quiet = quiet; }
    // The progress window, once it is showing.
    ProgressWindow *progressWindow() const { return m_window; }
    // Shows the progress window now rather than after the usual delay.
    void showProgress();

    // How a permanent delete asks about one item (DEL-02).
    static FileOperation::DeleteAnswer askDelete(const FileOperation::DeleteQuestion &question, QWidget *parent);

signals:
    // After the errors (if any) have been shown.
    void finished(FileOperation *operation);

private:
    OperationRunner(FileOperation *operation, QWidget *window);
    void done();

    FileOperation *m_operation;
    QPointer<QWidget> m_parent;
    QPointer<ProgressWindow> m_window;
    bool m_finished = false;
    bool m_quiet = false;
    // The latest progress, for a window that opens late.
    qint64 m_done = 0, m_total = 0, m_fileDone = 0, m_fileSize = 0;
    int m_number = 0, m_count = 0;
    QString m_path;
};
