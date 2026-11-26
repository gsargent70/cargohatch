#include "pathcompleter.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QDir>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QKeyEvent>
#include <QLineEdit>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QStorageInfo>
#include <QtConcurrent/QtConcurrentRun>

#include "breadcrumb.h"
#include "filemodel.h"
#include "folderaliases.h"
#include "namematch.h"

namespace {

// More than this would be scrolling, not completing.
constexpr int kMaxOffered = 200;
constexpr int kVisible = 12;

bool isNetworkDrive(const QString &folder) {
    const QByteArray type = QStorageInfo(folder).fileSystemType().toLower();
    for (const char *name : {"nfs", "cifs", "smb", "sshfs", "rclone", "davfs", "ceph", "9p", "afs", "gvfs", "glusterfs"})
        if (type.contains(name))
            return true;
    return false;
}

QString shortPath(const QString &path) {
    const QString home = QDir::homePath();
    return path.startsWith(home + '/') ? '~' + path.mid(home.size()) : path;
}

} // namespace

PathCompleter::PathCompleter(QLineEdit *edit)
    : QObject(edit), m_edit(edit), m_completer(new QCompleter(this)), m_model(new QStandardItemModel(this)) {
    m_completer->setModel(m_model);
    m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    m_completer->setMaxVisibleItems(kVisible);
    m_completer->setWidget(edit);
    m_edit->installEventFilter(this);
    // Installed after the completer's own filter, so it sees keys first.
    m_completer->popup()->installEventFilter(this);
    connect(m_completer, qOverload<const QModelIndex &>(&QCompleter::activated), this, [this](const QModelIndex &index) {
        take(index.row());
        m_typed = m_edit->text();
        hidePopup();
        emit chosen(m_typed);
    });
    connect(m_edit, &QLineEdit::textEdited, this, [this](const QString &text) {
        m_typed = text;
        m_row = -1;
        if (m_options.enabled && !m_options.onArrows && m_options.popup)
            update(Then::Show);
        else
            hidePopup();
    });
}

void PathCompleter::setOptions(const Options &options) {
    m_options = options;
    if (!options.enabled)
        reset();
}

void PathCompleter::reset() {
    hidePopup();
    ++m_generation;
    m_reading = false;
    m_typed.clear();
    m_row = -1;
    m_model->clear();
    // Read again next time: folders come and go between visits.
    m_listedFolder.clear();
    m_folders.clear();
}

bool PathCompleter::isPopupVisible() const {
    return m_completer->popup()->isVisible();
}

void PathCompleter::hidePopup() {
    if (isPopupVisible())
        m_completer->popup()->hide();
}

QStringList PathCompleter::completions() const {
    QStringList list;
    for (int row = 0; row < m_model->rowCount(); ++row)
        list << rowText(row);
    return list;
}

QString PathCompleter::rowText(int row) const {
    return m_model->index(row, 0).data(Qt::UserRole).toString();
}

QStringList PathCompleter::foldersIn(const QString &folder, bool localOnly) {
    if (localOnly && isNetworkDrive(folder))
        return {};
    return Breadcrumb::subfolders(folder);
}

void PathCompleter::update(Then then) {
    ++m_generation;
    m_typed = m_edit->text();
    m_row = -1;
    m_reading = false;
    // Completing in the middle of a path would put the rest in the wrong
    // place; quotes and addresses are pasted whole.
    if (!m_options.enabled || m_edit->cursorPosition() != m_typed.size() || m_typed.startsWith('"') ||
        m_typed.startsWith('\'') || m_typed.contains("://")) {
        m_model->clear();
        hidePopup();
        return;
    }
    const int slash = int(m_typed.lastIndexOf('/'));
    m_prefix = m_typed.left(slash + 1);
    m_fragment = m_typed.mid(slash + 1);
    if (slash < 0 && m_typed.startsWith('~')) {
        listed(then);
        return;
    }
    QString folder = m_prefix.isEmpty() ? m_base : FolderAliases::expand(m_prefix, m_base);
    if (folder.isEmpty()) {
        m_model->clear();
        hidePopup();
        return;
    }
    folder = QDir::cleanPath(folder);
    if (folder == m_listedFolder) {
        listed(then);
        return;
    }
    m_reading = true;
    const int generation = m_generation;
    const bool localOnly = m_options.localOnly;
    auto *watcher = new QFutureWatcher<QStringList>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation, folder, then] {
        watcher->deleteLater();
        // More has been typed, or the field left, in the meantime.
        if (generation != m_generation)
            return;
        m_reading = false;
        m_listedFolder = folder;
        m_folders = watcher->result();
        listed(then);
    });
    watcher->setFuture(QtConcurrent::run(&PathCompleter::foldersIn, folder, localOnly));
}

void PathCompleter::listed(Then then) {
    m_model->clear();
    const QIcon icon = QFileIconProvider().icon(QFileIconProvider::Folder);
    auto add = [&](const QString &shown, const QString &text) {
        if (m_model->rowCount() >= kMaxOffered)
            return;
        auto *item = new QStandardItem(icon, shown);
        item->setData(text, Qt::UserRole);
        m_model->appendRow(item);
    };
    if (m_prefix.isEmpty() && m_fragment.startsWith('~')) {
        const QString name = m_fragment.mid(1);
        for (const FolderAliases::Alias &a : FolderAliases::all())
            if (a.name.startsWith(name, Qt::CaseInsensitive))
                add(QStringLiteral("~%1   %2").arg(a.name, shortPath(a.path)), '~' + a.name);
    } else {
        // Hidden folders are offered once a dot asks for them.
        const GlobalFilter::Matcher matcher(m_filter);
        const QSet<QString> hidden = FileModel::hiddenNames(m_listedFolder);
        const QString wanted = NameMatch::strip(m_fragment, true).text;
        QStringList sameCase, otherCase;
        for (const QString &name : std::as_const(m_folders)) {
            if (!NameMatch::strip(name, true).text.startsWith(wanted, Qt::CaseInsensitive))
                continue;
            if (!m_fragment.startsWith('.') && matcher.hides(name, true, hidden.contains(name)))
                continue;
            (name.startsWith(m_fragment) ? sameCase : otherCase) << name;
        }
        for (const QString &name : sameCase + otherCase)
            add(name, m_prefix + name);
    }

    // A name typed in full needs no list of one.
    const bool whole = m_model->rowCount() == 1 && rowText(0) == m_typed;
    switch (then) {
    case Then::Nothing:
        break;
    case Then::Show:
        if (m_model->rowCount() == 0 || whole)
            hidePopup();
        else
            showPopup();
        break;
    case Then::Complete:
        complete();
        break;
    case Then::Next:
    case Then::Previous:
        if (m_model->rowCount() == 0)
            break;
        if (m_options.popup)
            showPopup();
        step(then == Then::Next ? 1 : -1);
        break;
    }
}

void PathCompleter::showPopup() {
    // The answer may come after the keyboard has gone elsewhere.
    if (!m_edit->isVisible() || (!m_edit->hasFocus() && !isPopupVisible()))
        return;
    QAbstractItemView *popup = m_completer->popup();
    popup->setCurrentIndex(QModelIndex());
    // A line around it in a colour that shows on dark themes, where Fusion's
    // own frame and the list behind look the same.
    popup->setFrameStyle(QFrame::Box | QFrame::Plain);
    popup->setIconSize(QSize(16, 16));
    QPalette colors = m_edit->palette();
    colors.setColor(QPalette::WindowText, colors.color(QPalette::Accent));
    popup->setPalette(colors);
    // Wide enough for its longest line, but no wider than the field needs.
    QRect rect = m_edit->rect();
    const int wanted = popup->sizeHintForColumn(0) + popup->verticalScrollBar()->sizeHint().width() + 16;
    rect.setWidth(qBound(qMin(m_edit->width(), 240), wanted, m_edit->width()));
    m_completer->complete(rect);
}

void PathCompleter::take(int row) {
    if (row < 0 || row >= m_model->rowCount())
        return;
    m_row = row;
    m_edit->setText(rowText(row));
}

void PathCompleter::step(int by) {
    // Past either end is what was typed, and then round again.
    const int rows = m_model->rowCount();
    int row = m_row + by;
    if (row >= rows)
        row = -1;
    else if (row < -1)
        row = rows - 1;
    m_row = row;
    m_edit->setText(row < 0 ? m_typed : rowText(row));
    if (isPopupVisible()) {
        // Says which without the completer putting text in by itself.
        const QSignalBlocker block(m_completer);
        m_completer->popup()->setCurrentIndex(row < 0 ? QModelIndex() : m_completer->completionModel()->index(row, 0));
    }
}

// Tab, as in a shell: as much as all the choices share, then one at a time.
void PathCompleter::complete() {
    const int rows = m_model->rowCount();
    if (rows == 0)
        return;
    int row = rows == 1 ? 0 : m_row;
    if (row < 0) {
        QString shared = rowText(0);
        for (int i = 1; i < rows; ++i) {
            const QString other = rowText(i);
            int n = 0;
            while (n < shared.size() && n < other.size() && shared.at(n).toLower() == other.at(n).toLower())
                ++n;
            shared.truncate(n);
        }
        // Letters all the choices agree on, but perhaps not their case; the
        // first choice's is as good as any.
        if (shared.size() > m_typed.size()) {
            m_edit->setText(shared);
            update(m_options.popup ? Then::Show : Then::Nothing);
            return;
        }
        if (m_options.popup && !isPopupVisible())
            showPopup();
        step(1);
        return;
    }
    // One is chosen: go on to what is inside it.
    m_edit->setText(rowText(row) + '/');
    update(m_options.popup && !m_options.onArrows ? Then::Show : Then::Nothing);
}

bool PathCompleter::handleKey(QKeyEvent *key, bool inPopup) {
    if (!m_options.enabled || key->modifiers() & ~Qt::KeypadModifier)
        return false;
    const bool popup = isPopupVisible();
    // A key pressed while a folder is being read is for what it turns out
    // to hold.
    switch (key->key()) {
    case Qt::Key_Down:
    case Qt::Key_Up: {
        const int by = key->key() == Qt::Key_Down ? 1 : -1;
        // Still stepping through the same choices.
        const bool stepping = m_row >= 0 && m_row < m_model->rowCount() && m_edit->text() == rowText(m_row);
        if ((popup || stepping) && !m_reading)
            step(by);
        else
            update(by > 0 ? Then::Next : Then::Previous);
        return true;
    }
    case Qt::Key_Tab: {
        const QString folder = FolderAliases::expand(m_edit->text(), m_base);
        const bool stepping = m_row >= 0 && m_row < m_model->rowCount() && m_edit->text() == rowText(m_row);
        // A whole path has nothing to complete, so Tab moves on as usual.
        if (!popup && !stepping && (m_edit->text().isEmpty() || QFileInfo(folder).isDir()))
            return false;
        if ((popup || stepping) && !m_reading)
            complete();
        else
            update(Then::Complete);
        return true;
    }
    case Qt::Key_Escape:
        if (!popup)
            return false;
        hidePopup();
        // Back to what was typed.
        if (m_row >= 0)
            m_edit->setText(m_typed);
        m_row = -1;
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        hidePopup();
        if (!inPopup)
            return false;
        // The field, and the dialog it may be in, take it from here.
        {
            QKeyEvent again(QEvent::KeyPress, key->key(), key->modifiers(), key->text());
            QApplication::sendEvent(m_edit, &again);
        }
        return true;
    default:
        return false;
    }
}

bool PathCompleter::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (watched == m_edit)
            return handleKey(key, false);
        if (watched == m_completer->popup())
            return handleKey(key, true);
    } else if (watched == m_edit && event->type() == QEvent::FocusOut) {
        // While its list shows, the completer keeps this from the field's
        // owner, who may need to know; so the list goes first.
        const Qt::FocusReason reason = static_cast<QFocusEvent *>(event)->reason();
        if (reason != Qt::PopupFocusReason && reason != Qt::ActiveWindowFocusReason)
            hidePopup();
    } else if (watched == m_edit && event->type() == QEvent::Hide) {
        reset();
    }
    return QObject::eventFilter(watched, event);
}
