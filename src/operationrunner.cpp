#include "operationrunner.h"

#include <QApplication>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QtConcurrent>

#include "progresswindow.h"
#include "replacedialog.h"

namespace {

// How long an operation runs before its progress window appears, so quick
// ones don't flash a window.
constexpr int kProgressDelayMs = 400;

} // namespace

OperationRunner *OperationRunner::start(FileOperation *operation, QWidget *window) {
    return new OperationRunner(operation, window);
}

FileOperation::DeleteAnswer OperationRunner::askDelete(const FileOperation::DeleteQuestion &q, QWidget *parent) {
    const QString name = QFileInfo(q.path).fileName();
    QString text;
    switch (q.reason) {
    case FileOperation::DeleteQuestion::Reason::File:
        text = tr("Delete the file %1 permanently?").arg(name);
        break;
    case FileOperation::DeleteQuestion::Reason::Folder:
        text = tr("Delete the folder %1 and everything in it permanently?").arg(name);
        break;
    case FileOperation::DeleteQuestion::Reason::ReadOnly:
        text = tr("%1 is read-only. Delete it anyway?").arg(name);
        break;
    }
    QMessageBox box(QMessageBox::Question, tr("Delete Permanently"), text, QMessageBox::NoButton, parent);
    box.setInformativeText(QFileInfo(q.path).path());
    QPushButton *yes = box.addButton(tr("Delete"), QMessageBox::DestructiveRole);
    QPushButton *all = box.addButton(tr("Delete All"), QMessageBox::DestructiveRole);
    all->setToolTip(tr("Don't ask again about the rest"));
    QPushButton *skip = box.addButton(tr("Skip"), QMessageBox::RejectRole);
    QPushButton *cancel = box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(skip);
    box.setEscapeButton(cancel);
    box.exec();
    if (box.clickedButton() == yes)
        return FileOperation::DeleteAnswer::Delete;
    if (box.clickedButton() == all)
        return FileOperation::DeleteAnswer::DeleteAll;
    if (box.clickedButton() == skip)
        return FileOperation::DeleteAnswer::Skip;
    return FileOperation::DeleteAnswer::Cancel;
}

OperationRunner::OperationRunner(FileOperation *operation, QWidget *window)
    : QObject(nullptr), m_operation(operation), m_parent(window) {
    operation->setParent(nullptr);
    // The worker waits in these while the GUI thread asks.
    operation->setResolver([this](const FileOperation::Conflict &c) {
        FileOperation::Resolution r;
        r.cancel = true;
        if (!m_operation->isCancelled())
            QMetaObject::invokeMethod(
                this, [this, &r, &c] { r = ReplaceDialog::ask(c, m_window ? m_window.data() : m_parent.data()); },
                Qt::BlockingQueuedConnection);
        return r;
    });
    operation->setDeleteResolver([this](const FileOperation::DeleteQuestion &q) {
        FileOperation::DeleteAnswer a = FileOperation::DeleteAnswer::Cancel;
        if (!m_operation->isCancelled())
            QMetaObject::invokeMethod(
                this, [this, &a, &q] { a = askDelete(q, m_window ? m_window.data() : m_parent.data()); },
                Qt::BlockingQueuedConnection);
        return a;
    });
    connect(operation, &FileOperation::progress, this,
            [this](qint64 done, qint64 total, qint64 fileDone, qint64 fileSize, const QString &name) {
                m_done = done;
                m_total = total;
                m_fileDone = fileDone;
                m_fileSize = fileSize;
                if (m_window)
                    m_window->setProgress(done, total, fileDone, fileSize, name);
            });
    connect(operation, &FileOperation::fileStarted, this, [this](int number, int count, const QString &path) {
        m_number = number;
        m_count = count;
        m_path = path;
        if (m_window)
            m_window->setFile(number, count, path);
    });
    // Quitting stops the work at the next file rather than waiting for it.
    connect(qApp, &QCoreApplication::aboutToQuit, operation, [operation] {
        operation->setPaused(false);
        operation->cancel();
    });

    auto *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, &OperationRunner::done);
    watcher->setFuture(QtConcurrent::run([operation] { operation->run(); }));
    QTimer::singleShot(kProgressDelayMs, this, &OperationRunner::showProgress);
}

void OperationRunner::showProgress() {
    if (m_window || m_finished)
        return;
    auto *window = new ProgressWindow(m_operation, m_parent);
    window->setAttribute(Qt::WA_DeleteOnClose);
    if (m_count > 0)
        window->setFile(m_number, m_count, m_path);
    window->setProgress(m_done, m_total, m_fileDone, m_fileSize, {});
    m_window = window;
    window->show();
}

void OperationRunner::done() {
    m_finished = true;
    if (m_window) {
        // Closing it would count as Abort; the work is over, so just go.
        m_window->disconnect();
        m_window->hide();
        m_window->deleteLater();
    }
    const QStringList errors = m_operation->errors();
    const QStringList skipped = m_operation->skipped();
    if ((!errors.isEmpty() || !skipped.isEmpty()) && !m_quiet) {
        const QStringList all = errors + skipped;
        QString summary;
        if (!errors.isEmpty())
            summary = tr("%n items had a problem.", "", int(errors.size()));
        if (!skipped.isEmpty())
            summary += (summary.isEmpty() ? QString() : QStringLiteral(" ")) +
                       tr("%n items were left alone.", "", int(skipped.size()));
        QMessageBox box(errors.isEmpty() ? QMessageBox::Information : QMessageBox::Warning, tr("CargoHatch"), summary,
                        QMessageBox::Ok, m_parent);
        box.setInformativeText(all.first());
        if (all.size() > 1)
            box.setDetailedText(all.join('\n'));
        box.exec();
    }
    emit finished(m_operation);
    m_operation->deleteLater();
    deleteLater();
}
