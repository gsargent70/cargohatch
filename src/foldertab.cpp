#include "foldertab.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QStorageInfo>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

#include "breadcrumb.h"
#include "fileclipboard.h"
#include "filedrag.h"
#include "folderaliases.h"
#include "filesortproxy.h"
#include "icons.h"
#include "selectpattern.h"
#include "tiledelegate.h"

namespace {

// How many folders the back and forward drop-downs list.
constexpr int kHistoryMenuEntries = 25;

bool isBelow(const QString &child, const QString &parent) {
    const QString prefix = parent.endsWith('/') ? parent : parent + '/';
    return child.startsWith(prefix) && child.size() > prefix.size();
}

QToolButton *navButton(QWidget *parent) {
    auto *button = new QToolButton(parent);
    button->setIconSize(QSize(16, 16));
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

} // namespace

FolderTab::FolderTab(const QString &path, QWidget *parent)
    : QWidget(parent),
      m_model(new FileModel(this)),
      m_proxy(new FileSortProxy(m_model, this)),
      m_views(new QStackedWidget(this)),
      m_details(new FileDragView<QTreeView>([this] { startDrag(); }, this)),
      m_icons(new FileDragView<QListView>([this] { startDrag(); }, this)),
      m_tileDelegate(new TileDelegate(m_proxy, this)),
      m_plainDelegate(new RenameDelegate(this)),
      m_detailsDelegate(new RenameDelegate(this)),
      m_breadcrumb(new Breadcrumb(this)),
      m_back(navButton(this)),
      m_forward(navButton(this)),
      m_up(navButton(this)),
      m_findBar(new FindBar(this)),
      m_filterBar(new FilterBar(this)) {
    // Details and Power: a flat list with movable columns, and no tree
    // expansion (that is the separate expandable-folders feature). Sorting
    // is done here rather than by the view, so a heading click can pick the
    // column's natural direction.
    m_details->setModel(m_proxy);
    m_details->setItemDelegate(m_detailsDelegate);
    m_details->setRootIsDecorated(false);
    m_details->setItemsExpandable(false);
    m_details->setUniformRowHeights(true);
    m_details->setAllColumnsShowFocus(true);
    m_details->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_details->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_details->setFrameShape(QFrame::NoFrame);
    QHeaderView *header = m_details->header();
    header->setSectionsMovable(true);
    header->setStretchLastSection(false);
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setSectionsClickable(true);
    header->setSortIndicatorShown(true);
    header->setFirstSectionMovable(false);
    header->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header, &QHeaderView::sectionClicked, this, [this](int section) { sortByHeading(Column(section)); });
    connect(header, &QWidget::customContextMenuRequested, this, &FolderTab::showHeaderMenu);
    connect(header, &QHeaderView::sectionMoved, this, &FolderTab::formatChanged);
    connect(header, &QHeaderView::sectionResized, this, &FolderTab::formatChanged);

    // The icon, list, thumbnail and tile modes share the items and the
    // selection, so switching keeps what is selected.
    m_icons->setModel(m_proxy);
    QItemSelectionModel *unused = m_icons->selectionModel();
    m_icons->setSelectionModel(m_details->selectionModel());
    delete unused;
    m_icons->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // Whole rows, though only names show: the selection is shared with
    // Details, where an item is selected when its whole row is.
    m_icons->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_icons->setFrameShape(QFrame::NoFrame);
    m_icons->setResizeMode(QListView::Adjust);
    m_icons->setMovement(QListView::Static);
    m_icons->setUniformItemSizes(true);
    m_icons->setSelectionRectVisible(true);
    m_icons->setTextElideMode(Qt::ElideMiddle);

    m_views->addWidget(m_details);
    m_views->addWidget(m_icons);
    for (QAbstractItemView *view : {static_cast<QAbstractItemView *>(m_details), static_cast<QAbstractItemView *>(m_icons)}) {
        view->viewport()->installEventFilter(this);
        view->installEventFilter(this);
        acceptDrops(view);
        // Renaming starts only from F2 or the menu, never from a click
        // that was meant to open or select.
        view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        view->setVerticalScrollBar(new MatchScrollBar(Qt::Vertical, view));
        view->setHorizontalScrollBar(new MatchScrollBar(Qt::Horizontal, view));
        connect(view, &QAbstractItemView::activated, this, &FolderTab::activate);
    }
    for (RenameDelegate *delegate : {m_plainDelegate, m_detailsDelegate, static_cast<RenameDelegate *>(m_tileDelegate)})
        delegate->setRenamer([this](const QModelIndex &index, const QString &name) {
            renameItem(index.data(Qt::EditRole).toString(), name);
        });
    setFormat(ViewFormat());
    connect(m_details->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FolderTab::statusChanged);
    connect(m_details->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FolderTab::previewChanged);
    connect(m_details->selectionModel(), &QItemSelectionModel::currentChanged, this, &FolderTab::previewChanged);

    // The back and forward buttons go one step on a click and list the
    // history when held or when their arrow is clicked.
    for (QToolButton *button : {m_back, m_forward}) {
        button->setPopupMode(QToolButton::MenuButtonPopup);
        auto *menu = new QMenu(button);
        const bool back = button == m_back;
        connect(menu, &QMenu::aboutToShow, this, [this, button, back] { fillHistoryMenu(button, back); });
        button->setMenu(menu);
    }
    connect(m_back, &QToolButton::clicked, this, [this] { goBack(); });
    connect(m_forward, &QToolButton::clicked, this, [this] { goForward(); });
    connect(m_up, &QToolButton::clicked, this, &FolderTab::goUp);

    auto *nav = new QHBoxLayout;
    nav->setContentsMargins(2, 2, 2, 2);
    nav->setSpacing(0);
    nav->addWidget(m_back);
    nav->addWidget(m_forward);
    nav->addWidget(m_up);
    nav->addSpacing(4);
    nav->addWidget(m_breadcrumb, 1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(nav);
    layout->addWidget(m_views, 1);
    layout->addWidget(m_filterBar);
    layout->addWidget(m_findBar);

    connect(m_findBar, &FindBar::textChanged, this, &FolderTab::find);
    connect(m_findBar, &FindBar::stepRequested, this, &FolderTab::findStep);
    connect(m_findBar, &FindBar::closeRequested, this, &FolderTab::closeFind);
    connect(m_findBar, &FindBar::accepted, this, [this] {
        const QModelIndex current = fileView()->currentIndex();
        const bool found = !m_matches.isEmpty();
        closeFind(true);
        // Enter on text that matches nothing opens nothing.
        if (found && m_findOptions.enterOpens)
            activate(current);
    });
    connect(m_filterBar, &FilterBar::filterChanged, this, &FolderTab::applyQuickFilter);
    connect(m_filterBar, &FilterBar::leaveRequested, this, [this] { fileView()->setFocus(); });
    connect(m_filterBar, &FilterBar::closed, this, [this] { fileView()->setFocus(); });
    connect(m_filterBar, &FilterBar::defaultsRequested, this, &FolderTab::filterDefaultsRequested);
    // While finding, the matches follow the list. Filtering a big folder
    // changes it in many small steps, which are gathered into one update.
    m_matchTimer.setSingleShot(true);
    m_matchTimer.setInterval(0);
    connect(&m_matchTimer, &QTimer::timeout, this, &FolderTab::updateMatches);
    auto changed = [this] { m_matchTimer.start(); };
    connect(m_proxy, &QAbstractItemModel::rowsInserted, this, changed);
    connect(m_proxy, &QAbstractItemModel::rowsRemoved, this, changed);
    connect(m_proxy, &QAbstractItemModel::layoutChanged, this, changed);
    connect(m_proxy, &QAbstractItemModel::modelReset, this, changed);

    connect(m_breadcrumb, &Breadcrumb::pathRequested, this, &FolderTab::openPath);
    connect(m_model, &FileModel::loaded, this, [this] {
        m_itemCount = 0;
        countItems();
        if (!m_selectWhenLoaded.isEmpty()) {
            const int row = m_model->rowOf(QFileInfo(m_selectWhenLoaded).fileName());
            const QModelIndex index = row < 0 ? QModelIndex() : m_proxy->mapFromSource(m_model->index(row, 0));
            // The cursor's frame marks it, without selecting it: a key
            // pressed next shouldn't copy or delete it by surprise.
            if (index.isValid()) {
                fileView()->selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
                fileView()->scrollTo(index);
            }
            m_selectWhenLoaded.clear();
        }
        emit statusChanged();
        emit folderLoaded(m_itemCount);
    });
    // Files coming and going later keep the count up to date.
    for (auto signal : {&QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved})
        connect(m_model, signal, this, &FolderTab::countItems);
    // A rename can hide or show an item. Thumbnails arriving come through
    // here too, and change only the picture.
    connect(m_model, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &, const QModelIndex &, const QList<int> &roles) {
                if (roles.isEmpty() || roles.contains(Qt::DisplayRole))
                    countItems();
            });
    connect(m_model, &FileModel::settled, this, &FolderTab::selectPending);
    // Cut items are dimmed, here and in any other tab showing them.
    connect(FileClipboard::instance(), &FileClipboard::cutChanged, this, [this] {
        m_details->viewport()->update();
        m_icons->viewport()->update();
    });

    updateIcons();
    retranslate();
    if (!openPath(path)) {
        const QString error = m_error;
        openPath(QDir::homePath());
        // Keep the reason the requested folder didn't open in view.
        setError(error);
        m_errorLasts = true;
    }
}

QString FolderTab::checkFolder(const QString &rawPath, QString *error) {
    // Only ~ is filled in: anything else is a real path as it stands, spaces
    // and all. Where the user types one, more is understood.
    const QString path = rawPath.startsWith('~') ? FolderAliases::expand(rawPath) : rawPath;
    const QFileInfo info(path);
    QString message;
    if (path.isEmpty() || !info.exists())
        message = tr("%1 does not exist.").arg(path);
    else if (!info.isDir())
        message = tr("%1 is not a folder.").arg(path);
    else if (!info.isReadable() || !info.isExecutable())
        message = tr("You don't have permission to open %1.").arg(path);
    if (error)
        *error = message;
    return message.isEmpty() ? info.canonicalFilePath() : QString();
}

QString FolderTab::nextGhost(const QString &oldPath, const QString &oldGhost, const QString &newPath) {
    // The deepest folder the user has been down this branch.
    const QString deepest = !oldGhost.isEmpty() && (oldGhost == oldPath || isBelow(oldGhost, oldPath))
                                ? oldGhost : oldPath;
    if (isBelow(deepest, newPath))
        return deepest;
    return {};
}

bool FolderTab::openPath(const QString &path) {
    // IDEA-12: a file's path opens its folder with the file selected.
    const QFileInfo file(path.startsWith('~') ? FolderAliases::expand(path) : path);
    if (file.exists() && !file.isDir()) {
        const QString folder = file.absolutePath();
        if (redirectIfLocked(folder, path))
            return false;
        if (!navigate(folder, true))
            return false;
        m_selectWhenLoaded.clear();
        m_pendingSelection = {file.fileName()};
        return true;
    }
    if (redirectIfLocked(path))
        return false;
    return navigate(path, true);
}

bool FolderTab::redirectIfLocked(const QString &rawPath, const QString &sent) {
    if (m_lock != Lock::Locked && m_lock != Lock::Reuse)
        return false;
    QString error;
    const QString path = checkFolder(rawPath, &error);
    if (path.isEmpty() || path == m_path)
        return false;
    emit redirectRequested(sent.isEmpty() ? path : sent);
    return true;
}

bool FolderTab::navigate(const QString &rawPath, bool record) {
    QString error;
    const QString path = checkFolder(rawPath, &error);
    if (path.isEmpty()) {
        setError(error);
        return false;
    }

    if (m_home.isEmpty())
        m_home = path;
    m_ghost = m_path.isEmpty() ? QString() : nextGhost(m_path, m_ghost, path);
    // NAV-08: going up, by any way, lands on the folder that was left.
    m_selectWhenLoaded.clear();
    if (!m_path.isEmpty() && isBelow(m_path, path))
        m_selectWhenLoaded = m_path.left(m_path.indexOf('/', path == "/" ? 1 : path.size() + 1));
    m_path = path;
    m_error.clear();
    m_itemCount = -1;
    m_hidden = 0;
    m_filteredOut = 0;
    m_pendingSelection.clear();
    // Looking for something, or filtering, is about one folder.
    closeFind(m_findBar->editor()->hasFocus());
    clearHighlights();
    if (m_filterBehaviour.clearOnNavigate && !m_filterBar->isHidden()) {
        const bool hadFocus = isAncestorOf(QApplication::focusWidget());
        QWidget *focus = QApplication::focusWidget();
        m_filterBar->close();
        // Closing takes the keyboard to the list; if it was elsewhere in
        // another tab or side, it goes back there.
        if (!hadFocus && focus)
            focus->setFocus();
    }
    if (record)
        m_history.visit(path);
    m_model->setFolder(path);
    m_breadcrumb->setPath(path, m_ghost);
    updateButtons();
    emit pathChanged(path);
    emit statusChanged();
    return true;
}

bool FolderTab::goSteps(int steps) {
    const QString target = m_history.peek(steps);
    if (target.isEmpty() || redirectIfLocked(target))
        return false;
    const QString from = m_path;
    if (!navigate(target, false))
        return false;
    m_history.go(steps);
    updateButtons();
    return true;
}

bool FolderTab::goBack(int steps) {
    return goSteps(-steps);
}

bool FolderTab::goForward(int steps) {
    return goSteps(steps);
}

void FolderTab::goUp() {
    QDir dir(m_path);
    if (!dir.cdUp())
        return;
    openPath(dir.absolutePath());
}

void FolderTab::goRoot() {
    QString root = QStorageInfo(m_path).rootPath();
    if (root.isEmpty() || root == m_path)
        root = QStringLiteral("/");
    if (root != m_path)
        openPath(root);
}

void FolderTab::editPath() {
    m_breadcrumb->startEditing();
}

void FolderTab::setHistory(const History &history) {
    m_history = history;
    updateButtons();
}

void FolderTab::setLabel(const QString &label) {
    const QString trimmed = label.trimmed();
    if (trimmed == m_label)
        return;
    m_label = trimmed;
    emit decorationChanged();
}

QString FolderTab::displayName() const {
    return m_label.isEmpty() ? title() : m_label;
}

void FolderTab::setLock(Lock lock, const QString &lockedPath) {
    m_lock = lock;
    m_lockedPath = lock == Lock::None ? QString() : lockedPath.isEmpty() ? m_path : lockedPath;
    if (lock != Lock::Reuse)
        m_reuseTarget = nullptr;
    emit decorationChanged();
}

void FolderTab::returnToLockedFolder() {
    if (!m_lockedPath.isEmpty() && m_lockedPath != m_path)
        navigate(m_lockedPath, true);
}

void FolderTab::setHomePath(const QString &path) {
    m_home = path;
}

void FolderTab::goHome() {
    if (!m_home.isEmpty())
        openPath(m_home);
}

void FolderTab::setColorName(const QString &name) {
    if (name == m_color)
        return;
    m_color = name;
    emit decorationChanged();
}

void FolderTab::setLinkedTab(FolderTab *tab) {
    if (tab == m_linked)
        return;
    FolderTab *old = m_linked;
    m_linked = tab;
    if (old && old->m_linked == this) {
        old->m_linked = nullptr;
        emit old->decorationChanged();
    }
    if (tab) {
        if (tab->m_linked && tab->m_linked != this)
            tab->m_linked->setLinkedTab(nullptr);
        tab->m_linked = this;
        emit tab->decorationChanged();
    }
    emit decorationChanged();
}

QString FolderTab::title() const {
    const QString name = QFileInfo(m_path).fileName();
    return name.isEmpty() ? m_path : name;
}

QString FolderTab::statusText() const {
    if (!m_error.isEmpty())
        return m_error;
    if (m_itemCount < 0)
        return tr("Reading folder…");
    const int selected = selectedCount();
    QString text = selected ? tr("%n items, %1 selected", "", m_itemCount).arg(selected) : tr("%n items", "", m_itemCount);
    if (m_filteredOut)
        text += QStringLiteral("  ·  ") + tr("%n filtered out", "", m_filteredOut);
    if (m_hidden)
        text += QStringLiteral("  ·  ") + tr("%n hidden", "", m_hidden);
    return text;
}

void FolderTab::countItems() {
    if (m_itemCount < 0)
        return;
    const int items = m_proxy->rowCount();
    int hidden = 0, filteredOut = 0;
    if (items < m_model->rowCount()) {
        for (int row = 0; row < m_model->rowCount(); ++row) {
            if (m_proxy->isHidden(row))
                ++hidden;
            else if (m_proxy->isFilteredOut(row))
                ++filteredOut;
        }
    }
    if (items == m_itemCount && hidden == m_hidden && filteredOut == m_filteredOut)
        return;
    m_itemCount = items;
    m_hidden = hidden;
    m_filteredOut = filteredOut;
    emit statusChanged();
}

bool FolderTab::isShowing(int row) const {
    return row >= 0 && m_proxy->mapFromSource(m_model->index(row, 0)).isValid();
}

QString FolderTab::whyNotShowing(int row) const {
    const QString name = m_model->entry(row).name;
    if (m_proxy->isHidden(row))
        return name.startsWith('.') || m_model->entry(row).isListedHidden
                   ? tr("%1 is hidden here. Ctrl+H shows hidden files.").arg(name)
                   : tr("%1 is hidden by the global filters.").arg(name);
    return tr("%1 doesn't match the filter, so it isn't showing.").arg(name);
}

QString FolderTab::nameAt(int proxyRow) const {
    return m_model->entry(m_proxy->mapToSource(m_proxy->index(proxyRow, 0)).row()).name;
}

void FolderTab::setGlobalFilter(const GlobalFilter &filter) {
    if (filter == m_proxy->globalFilter())
        return;
    m_proxy->setGlobalFilter(filter);
    m_breadcrumb->setGlobalFilter(filter);
    countItems();
}

void FolderTab::selectNew(const QStringList &names) {
    m_pendingSelection = names;
    m_model->refresh();
}

void FolderTab::selectPending() {
    const QStringList names = m_pendingSelection;
    m_pendingSelection.clear();
    if (names.isEmpty())
        return;
    QStringList showing;
    QString why;
    int hidden = 0;
    for (const QString &name : names) {
        const int row = m_model->rowOf(name);
        if (isShowing(row)) {
            showing << name;
        } else if (row >= 0) {
            ++hidden;
            why = whyNotShowing(row);
        }
    }
    if (!showing.isEmpty()) {
        selectNames(showing);
        fileView()->scrollTo(fileView()->currentIndex());
    } else if (hidden > 0) {
        setError(hidden == 1 ? why : tr("The new items are hidden or filtered out here."));
    }
}

QString FolderTab::highlightedFolder() const {
    const QModelIndex index = fileView()->currentIndex();
    return isFolderAt(index) ? pathAt(index) : QString();
}

namespace {
FolderTab::Opener opener;
} // namespace

void FolderTab::setOpener(Opener o) {
    opener = std::move(o);
}

bool FolderTab::openFile(const QString &path) {
    return opener ? opener(path) : QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void FolderTab::activate(const QModelIndex &index) {
    if (!index.isValid())
        return;
    // On one of several selected items, all of them. A double-click has
    // selected the one clicked by now.
    if (selectedCount() > 1 && m_details->selectionModel()->isRowSelected(index.row(), QModelIndex())) {
        emit openSelectionRequested();
        return;
    }
    const QString path = pathAt(index);
    if (isFolderAt(index)) {
        openPath(path);
        return;
    }
    if (!openFile(path))
        setError(tr("No application is set up to open %1.").arg(QFileInfo(path).fileName()));
}

void FolderTab::setError(const QString &message) {
    static quint64 lastId = 0;
    m_error = message;
    m_errorId = ++lastId;
    m_errorLasts = false;
    emit statusChanged();
}

void FolderTab::clearError() {
    if (m_error.isEmpty())
        return;
    m_error.clear();
    emit statusChanged();
}

void FolderTab::updateButtons() {
    m_back->setEnabled(m_history.canGoBack());
    m_forward->setEnabled(m_history.canGoForward());
    m_up->setEnabled(m_path != "/");
}

void FolderTab::fillHistoryMenu(QToolButton *button, bool back) {
    QMenu *menu = button->menu();
    menu->clear();
    const QStringList entries = back ? m_history.backList() : m_history.forwardList();
    const QIcon icon = QFileIconProvider().icon(QFileIconProvider::Folder);
    for (int i = 0; i < entries.size() && i < kHistoryMenuEntries; ++i) {
        const int steps = back ? -(i + 1) : i + 1;
        QAction *action = menu->addAction(icon, QString(entries.at(i)).replace('&', "&&"));
        connect(action, &QAction::triggered, this, [this, steps] { goSteps(steps); });
    }
}

bool FolderTab::eventFilter(QObject *watched, QEvent *event) {
    // Escape in the file list clears the selection (SEL-01). It is handled
    // here rather than as a window shortcut, so it still closes menus and
    // cancels typing in the path field.
    if ((watched == m_details || watched == m_icons) && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape && key->modifiers() == Qt::NoModifier) {
            // One thing at a time: kept highlights, the selection, the filter.
            if (m_highlighting)
                clearHighlights();
            else if (selectedCount() > 0)
                selectNone();
            else if (!m_filterBar->isHidden())
                m_filterBar->close();
            else
                return QWidget::eventFilter(watched, event);
            return true;
        }
        // Typing a name finds it (SEL-11). Not a space first: that selects
        // items.
        const QString text = key->text();
        const Qt::KeyboardModifiers held = key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
        if (!text.isEmpty() && text.at(0).isPrint() && !text.at(0).isSpace() && held == Qt::NoModifier) {
            rememberSelection();
            m_findFrom = fileView()->currentIndex().row();
            m_findBar->open(text);
            return true;
        }
    }
    // The right mouse button's event goes to the viewport, the menu key's to
    // the view itself.
    if (event->type() == QEvent::ContextMenu) {
        auto *view = fileView();
        if (watched == view || watched == view->viewport())
            return showContextMenu(view, static_cast<QContextMenuEvent *>(event));
    }
    // The mouse's back and forward side buttons move through the history.
    if ((watched == m_details->viewport() || watched == m_icons->viewport()) &&
        event->type() == QEvent::MouseButtonPress) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        const auto button = mouse->button();
        fileView()->setDragEnabled(button == Qt::LeftButton && dragsFrom(fileView(), mouse->position().toPoint()));
        // Middle-clicking a folder opens it in a new tab.
        if (button == Qt::MiddleButton) {
            const QModelIndex index = fileView()->indexAt(mouse->position().toPoint());
            if (isFolderAt(index)) {
                emit newTabRequested(pathAt(index));
                return true;
            }
        }
        if (button == Qt::BackButton) {
            goBack();
            return true;
        }
        if (button == Qt::ForwardButton) {
            goForward();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FolderTab::updateIcons() {
    m_back->setIcon(Icons::arrow(Qt::LeftArrow, palette()));
    m_forward->setIcon(Icons::arrow(Qt::RightArrow, palette()));
    m_up->setIcon(Icons::arrow(Qt::UpArrow, palette()));
}

void FolderTab::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    else if (event->type() == QEvent::PaletteChange)
        updateIcons();
    QWidget::changeEvent(event);
}

void FolderTab::retranslate() {
    m_back->setToolTip(tr("Back (Alt+Left)"));
    m_forward->setToolTip(tr("Forward (Alt+Right)"));
    m_up->setToolTip(tr("Up (Alt+Up)"));
    emit statusChanged();
}

QString FolderTab::pathAt(const QModelIndex &proxyIndex) const {
    return proxyIndex.isValid() ? m_model->filePath(m_proxy->mapToSource(proxyIndex).row()) : QString();
}

bool FolderTab::isFolderAt(const QModelIndex &proxyIndex) const {
    return proxyIndex.isValid() && m_model->entry(m_proxy->mapToSource(proxyIndex).row()).isDir;
}

QAbstractItemView *FolderTab::fileView() const {
    return m_format.isDetailed() ? static_cast<QAbstractItemView *>(m_details) : m_icons;
}

ViewFormat FolderTab::format() const {
    ViewFormat format = m_format;
    // The header holds the latest column order and widths.
    const QHeaderView *header = m_details->header();
    format.columns.clear();
    for (int visual = 0; visual < header->count(); ++visual) {
        const int logical = header->logicalIndex(visual);
        if (!header->isSectionHidden(logical))
            format.columns << ViewFormat::ColumnState{Column(logical), header->sectionSize(logical)};
    }
    if (format.columns.isEmpty())
        format.columns = m_format.columns;
    return format;
}

void FolderTab::setFormat(const ViewFormat &format) {
    m_format = format;
    QHeaderView *header = m_details->header();
    for (int c = 0; c < Columns::count(); ++c)
        header->setSectionHidden(c, !format.hasColumn(Column(c)));
    for (int i = 0; i < format.columns.size(); ++i) {
        const int logical = int(format.columns.at(i).column);
        header->moveSection(header->visualIndex(logical), i);
        header->resizeSection(logical, format.columns.at(i).width);
    }
    m_proxy->setFolderOrder(format.folderOrder);
    sortBy(format.sortColumn, format.reverse);
    applyMode();
    emit formatChanged();
}

void FolderTab::setViewMode(ViewFormat::Mode mode) {
    if (mode == m_format.mode)
        return;
    m_format = format(); // keep any column changes made by hand
    m_format.mode = mode;
    applyMode();
    emit formatChanged();
}

void FolderTab::applyMode() {
    const bool hadFocus = m_views->isAncestorOf(QApplication::focusWidget());
    const QModelIndex current = fileView()->currentIndex();
    const QFontMetrics metrics = fontMetrics();
    const int line = metrics.height();
    m_model->setThumbnailSize(m_format.mode == ViewFormat::Mode::Thumbnails ? 128 : 0);
    m_icons->setItemDelegate(m_format.mode == ViewFormat::Mode::Tiles ? static_cast<QStyledItemDelegate *>(m_tileDelegate)
                                                                      : m_plainDelegate);
    m_plainDelegate->setMaximumWidth(0);
    switch (m_format.mode) {
    case ViewFormat::Mode::Details:
    case ViewFormat::Mode::Power:
        // Power mode selects by clicking, without holding Ctrl.
        m_details->setSelectionMode(m_format.mode == ViewFormat::Mode::Power ? QAbstractItemView::MultiSelection
                                                                             : QAbstractItemView::ExtendedSelection);
        m_views->setCurrentWidget(m_details);
        break;
    case ViewFormat::Mode::LargeIcons:
        m_icons->setViewMode(QListView::IconMode);
        // List mode's downward flow would otherwise stay.
        m_icons->setFlow(QListView::LeftToRight);
        m_icons->setWrapping(true);
        m_icons->setIconSize(QSize(48, 48));
        m_icons->setGridSize(QSize(qMax(104, metrics.averageCharWidth() * 17), 48 + line * 2 + 24));
        m_icons->setWordWrap(true);
        break;
    case ViewFormat::Mode::Thumbnails:
        m_icons->setViewMode(QListView::IconMode);
        // List mode's downward flow would otherwise stay.
        m_icons->setFlow(QListView::LeftToRight);
        m_icons->setWrapping(true);
        m_icons->setIconSize(QSize(128, 128));
        m_icons->setGridSize(QSize(qMax(152, metrics.averageCharWidth() * 20), 128 + line * 2 + 24));
        m_icons->setWordWrap(true);
        break;
    case ViewFormat::Mode::SmallIcons:
    case ViewFormat::Mode::List:
    case ViewFormat::Mode::Tiles: {
        const bool tiles = m_format.mode == ViewFormat::Mode::Tiles;
        m_icons->setViewMode(QListView::ListMode);
        // Small icons run across in rows; List runs down in columns.
        m_icons->setFlow(m_format.mode == ViewFormat::Mode::List ? QListView::TopToBottom : QListView::LeftToRight);
        m_icons->setWrapping(true);
        m_icons->setWordWrap(false);
        m_plainDelegate->setMaximumWidth(metrics.averageCharWidth() * 25);
        m_icons->setIconSize(tiles ? QSize(TileDelegate::kIconSize, TileDelegate::kIconSize) : QSize(16, 16));
        m_icons->setGridSize(tiles ? TileDelegate::tileSize(metrics)
                                   : QSize(metrics.averageCharWidth() * 26, qMax(20, line + 6)));
        break;
    }
    case ViewFormat::Mode::Count:
        break;
    }
    if (!m_format.isDetailed()) {
        // Each label needs its own size; with uniform sizes every name is cut
        // to the first one's width. Tiles really are all one size.
        m_icons->setUniformItemSizes(m_format.mode == ViewFormat::Mode::Tiles);
        m_icons->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_views->setCurrentWidget(m_icons);
        // Qt keeps its own grid until the layout is redone.
        m_icons->doItemsLayout();
    }
    setFocusProxy(fileView());
    updateMatches();
    if (current.isValid()) {
        // The selection stays as it is.
        fileView()->selectionModel()->setCurrentIndex(current, QItemSelectionModel::NoUpdate);
        fileView()->scrollTo(current);
    }
    if (hadFocus)
        fileView()->setFocus();
}

void FolderTab::setColumnVisible(Column column, bool visible) {
    if (column == Column::Name)
        return;
    QHeaderView *header = m_details->header();
    if (visible && header->isSectionHidden(int(column)) && header->sectionSize(int(column)) < 20)
        header->resizeSection(int(column), Columns::defaultWidth(column));
    header->setSectionHidden(int(column), !visible);
    m_format = format();
    emit formatChanged();
}

void FolderTab::sortBy(Column column, bool reverse) {
    m_format.sortColumn = column;
    m_format.reverse = reverse;
    const Qt::SortOrder order = reverse ? Qt::DescendingOrder : Qt::AscendingOrder;
    m_proxy->sort(int(column), order);
    m_details->header()->setSortIndicator(int(column), order);
    emit formatChanged();
}

void FolderTab::sortByHeading(Column column) {
    if (column == m_format.sortColumn)
        sortBy(column, !m_format.reverse);
    else
        sortBy(column, Columns::sortsDescendingFirst(column));
}

void FolderTab::setFolderOrder(ViewFormat::FolderOrder order) {
    m_format.folderOrder = order;
    m_proxy->setFolderOrder(order);
    emit formatChanged();
}

void FolderTab::sizeColumnsToFit() {
    for (int c = 0; c < Columns::count(); ++c)
        if (!m_details->header()->isSectionHidden(c))
            m_details->resizeColumnToContents(c);
}

void FolderTab::setSortOptions(const FileModel::SortOptions &options, bool foldersAlphabetical) {
    m_model->setSortOptions(options);
    m_proxy->setKeepFoldersAlphabetical(foldersAlphabetical);
    m_proxy->resort();
}

void FolderTab::showHeaderMenu(const QPoint &pos) {
    QHeaderView *header = m_details->header();
    const int clicked = header->logicalIndexAt(pos);
    QMenu menu(this);
    for (int c = 0; c < Columns::count(); ++c) {
        QAction *a = menu.addAction(Columns::title(Column(c)));
        a->setCheckable(true);
        a->setChecked(!header->isSectionHidden(c));
        a->setEnabled(Column(c) != Column::Name);
        connect(a, &QAction::triggered, this, [this, c](bool on) { setColumnVisible(Column(c), on); });
    }
    menu.addSeparator();
    QAction *fit = menu.addAction(tr("Size Column to Fit"));
    fit->setEnabled(clicked >= 0);
    connect(fit, &QAction::triggered, this, [this, clicked] { m_details->resizeColumnToContents(clicked); });
    connect(menu.addAction(tr("Size All Columns to Fit")), &QAction::triggered, this, &FolderTab::sizeColumnsToFit);
    menu.exec(header->mapToGlobal(pos));
}

QStringList FolderTab::selectedPaths() const {
    QModelIndexList rows = m_details->selectionModel()->selectedRows(0);
    std::sort(rows.begin(), rows.end(), [](const QModelIndex &a, const QModelIndex &b) { return a.row() < b.row(); });
    QStringList paths;
    for (const QModelIndex &index : rows)
        paths << pathAt(index);
    return paths;
}

QString FolderTab::previewPath() const {
    const QItemSelectionModel *model = m_details->selectionModel();
    const QModelIndex current = model->currentIndex();
    if (current.isValid() && model->isRowSelected(current.row(), QModelIndex()))
        return pathAt(current);
    const QStringList selected = selectedPaths();
    return selected.isEmpty() ? QString() : selected.first();
}

bool FolderTab::stepSelection(int delta) {
    QAbstractItemView *view = fileView();
    const int rows = m_proxy->rowCount();
    if (rows == 0)
        return false;
    const QModelIndex current = view->currentIndex();
    const int row = current.isValid() ? current.row() + delta : (delta > 0 ? 0 : rows - 1);
    if (row < 0 || row >= rows)
        return false;
    const QModelIndex index = m_proxy->index(row, 0);
    view->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    view->scrollTo(index);
    return true;
}

int FolderTab::selectedCount() const {
    return int(m_details->selectionModel()->selectedRows(0).size());
}

void FolderTab::setSelection(const std::function<bool(int, bool)> &wanted) {
    QItemSelectionModel *model = m_details->selectionModel();
    const int rows = m_proxy->rowCount();
    const int lastColumn = m_proxy->columnCount() - 1;
    // Runs of neighbouring rows become one range, which keeps selecting
    // thousands of items quick.
    QItemSelection selection;
    int start = -1;
    for (int row = 0; row <= rows; ++row) {
        const bool want = row < rows && wanted(row, model->isRowSelected(row, QModelIndex()));
        if (want && start < 0)
            start = row;
        if (!want && start >= 0) {
            selection.select(m_proxy->index(start, 0), m_proxy->index(row - 1, lastColumn));
            start = -1;
        }
    }
    model->select(selection, QItemSelectionModel::ClearAndSelect);
    // Put the cursor on the first selected item, without moving the
    // selection, so the keyboard carries on from there.
    if (!selection.isEmpty())
        model->setCurrentIndex(selection.first().topLeft(), QItemSelectionModel::NoUpdate);
}

void FolderTab::rememberSelection() {
    const QStringList paths = selectedPaths();
    if (paths.isEmpty())
        return;
    m_previousFolder = m_path;
    m_previousSelection.clear();
    for (const QString &path : paths)
        m_previousSelection << QFileInfo(path).fileName();
}

void FolderTab::selectNames(const QStringList &names) {
    const QSet<QString> wanted(names.begin(), names.end());
    setSelection([this, &wanted](int row, bool) {
        return wanted.contains(m_model->entry(m_proxy->mapToSource(m_proxy->index(row, 0)).row()).name);
    });
}

void FolderTab::selectAll() {
    rememberSelection();
    setSelection([](int, bool) { return true; });
}

void FolderTab::selectFiles() {
    rememberSelection();
    setSelection([this](int row, bool) { return !isFolderAt(m_proxy->index(row, 0)); });
}

void FolderTab::selectFolders() {
    rememberSelection();
    setSelection([this](int row, bool) { return isFolderAt(m_proxy->index(row, 0)); });
}

void FolderTab::selectNone() {
    rememberSelection();
    m_details->selectionModel()->clearSelection();
}

void FolderTab::invertSelection() {
    rememberSelection();
    setSelection([](int, bool selected) { return !selected; });
}

bool FolderTab::reselectPrevious() {
    if (m_previousFolder != m_path || m_previousSelection.isEmpty())
        return false;
    const QStringList names = m_previousSelection;
    rememberSelection();
    selectNames(names);
    return true;
}

int FolderTab::selectMatching(const SelectPattern &pattern) {
    if (!pattern.error().isEmpty())
        return 0;
    rememberSelection();
    int matched = 0;
    setSelection([this, &pattern, &matched](int row, bool selected) {
        const FileEntry &entry = m_model->entry(m_proxy->mapToSource(m_proxy->index(row, 0)).row());
        const bool match = pattern.applies(entry.isDir) && pattern.matches(entry.name);
        matched += match;
        switch (pattern.action) {
        case SelectPattern::Action::Select: return selected || match;
        case SelectPattern::Action::Deselect: return selected && !match;
        case SelectPattern::Action::SelectOnly: return match;
        }
        return selected;
    });
    return matched;
}

QString FolderTab::nameAfterSelection() const {
    QItemSelectionModel *model = m_details->selectionModel();
    const int rows = m_proxy->rowCount();
    int last = -1, first = -1;
    for (int row = 0; row < rows; ++row)
        if (model->isRowSelected(row, QModelIndex())) {
            last = row;
            if (first < 0)
                first = row;
        }
    if (last < 0)
        return {};
    for (int row = last + 1; row < rows; ++row)
        if (!model->isRowSelected(row, QModelIndex()))
            return nameAt(row);
    for (int row = first - 1; row >= 0; --row)
        if (!model->isRowSelected(row, QModelIndex()))
            return nameAt(row);
    return {};
}

void FolderTab::focusName(const QString &name) {
    const int row = m_model->rowOf(name);
    if (row < 0)
        return;
    const QModelIndex index = m_proxy->mapFromSource(m_model->index(row, 0));
    if (!index.isValid())
        return; // hidden
    fileView()->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    fileView()->scrollTo(index);
}

void FolderTab::startRename() {
    QModelIndex index = fileView()->currentIndex();
    if (!index.isValid()) {
        const QModelIndexList rows = m_details->selectionModel()->selectedRows(0);
        if (rows.isEmpty()) {
            setError(tr("Highlight an item to rename it."));
            return;
        }
        index = rows.first();
    }
    index = index.siblingAtColumn(int(Column::Name));
    fileView()->scrollTo(index);
    fileView()->setFocus();
    fileView()->edit(index);
}

void FolderTab::setRenameSelection(RenameDelegate::Selection selection) {
    for (RenameDelegate *delegate : {m_plainDelegate, m_detailsDelegate, static_cast<RenameDelegate *>(m_tileDelegate)})
        delegate->setSelection(selection);
}

QString FolderTab::renameOnDisk(const QString &folder, const QString &oldName, const QString &newName) {
    if (newName.isEmpty())
        return tr("A name can't be empty.");
    if (newName.contains('/'))
        return tr("A name can't contain /.");
    if (newName == "." || newName == "..")
        return tr("%1 can't be used as a name.").arg(newName);
    if (newName == oldName)
        return {};
    const QByteArray from = QFile::encodeName(folder + '/' + oldName);
    const QByteArray to = QFile::encodeName(folder + '/' + newName);
    // Never over an existing item. renameat2 checks and renames in one
    // step; filesystems without it get a check first.
    if (::renameat2(AT_FDCWD, from.constData(), AT_FDCWD, to.constData(), RENAME_NOREPLACE) == 0)
        return {};
    int error = errno;
    if (error == EINVAL || error == ENOSYS) {
        struct stat a {}, b {};
        const bool taken = ::lstat(to.constData(), &b) == 0;
        // On a filesystem that ignores case, "Photo" and "photo" are the
        // same file, and changing the case is allowed.
        const bool same = taken && ::lstat(from.constData(), &a) == 0 && a.st_ino == b.st_ino && a.st_dev == b.st_dev;
        if (taken && !same)
            return tr("%1 already exists.").arg(newName);
        if (::rename(from.constData(), to.constData()) == 0)
            return {};
        error = errno;
    }
    if (error == EEXIST)
        return tr("%1 already exists.").arg(newName);
    return tr("Couldn't rename %1: %2").arg(oldName, QString::fromLocal8Bit(std::strerror(error)));
}

bool FolderTab::renameItem(const QString &oldName, const QString &rawNewName) {
    const QString newName = rawNewName.trimmed();
    const QString error = renameOnDisk(m_path, oldName, newName);
    if (!error.isEmpty()) {
        setError(error);
        return false;
    }
    if (newName == oldName)
        return true;
    m_model->renameEntry(m_model->rowOf(oldName), newName);
    focusName(newName);
    const int row = m_model->rowOf(newName);
    if (row >= 0 && !isShowing(row)) {
        setError(whyNotShowing(row));
    } else {
        clearError();
    }
    return true;
}

void FolderTab::openName(const QString &name) {
    const int row = m_model->rowOf(name);
    if (isShowing(row))
        activate(m_proxy->mapFromSource(m_model->index(row, 0)));
}

bool FolderTab::dragsFrom(QAbstractItemView *view, const QPoint &pos) const {
    const QModelIndex index = view->indexAt(pos);
    if (!index.isValid())
        return false;
    // Dragging across unselected rows of Details, beside the names, goes on
    // selecting them.
    return view != m_details || m_details->selectionModel()->isRowSelected(index.row(), QModelIndex()) ||
           nameRect(index).contains(pos);
}

QRect FolderTab::nameRect(const QModelIndex &index) const {
    const QModelIndex name = index.siblingAtColumn(int(Column::Name));
    QRect rect = m_details->visualRect(name);
    const int width = qMin(rect.width(), m_details->sizeHintForIndex(name).width());
    if (isRightToLeft())
        rect.setLeft(rect.right() - width + 1);
    else
        rect.setWidth(width);
    return rect;
}

void FolderTab::startDrag() {
    const QStringList paths = selectedPaths();
    if (paths.isEmpty())
        return;
    const QModelIndex first = m_proxy->mapFromSource(m_model->index(m_model->rowOf(QFileInfo(paths.first()).fileName()), 0));
    FileDrag::start(fileView(), paths, first.data(Qt::DecorationRole).value<QIcon>());
}

void FolderTab::acceptDrops(QAbstractItemView *view) {
    new FileDropTarget(view->viewport(), [this, view](const QPoint &pos) -> FileDropTarget::Place {
        // On a folder, into it; in Details, only on its name, so there is
        // somewhere left that means the folder being shown.
        const QModelIndex index = view->indexAt(pos);
        if (isFolderAt(index) && (view != m_details || index.column() == int(Column::Name)))
            return {pathAt(index), view->visualRect(index)};
        return {m_path, view->viewport()->rect()};
    });
}

bool FolderTab::showContextMenu(QAbstractItemView *view, QContextMenuEvent *event) {
    QItemSelectionModel *model = view->selectionModel();
    if (event->reason() != QContextMenuEvent::Mouse) {
        // Beside the cursor's item if it is selected, else for the folder.
        QModelIndex current = view->currentIndex().siblingAtColumn(0);
        const QModelIndexList selected = model->selectedRows(0);
        const bool onItems = !selected.isEmpty();
        // Rename and Open in New Tab act on the cursor's item, which has to
        // be one of those the menu is about.
        if (onItems && !model->isRowSelected(current.row(), QModelIndex())) {
            current = selected.first();
            model->setCurrentIndex(current, QItemSelectionModel::NoUpdate);
        }
        QPoint at(8, 8);
        if (onItems && current.isValid()) {
            view->scrollTo(current);
            const QRect r = view->visualRect(current);
            at = QPoint(qMin(r.center().x(), r.left() + 80), r.bottom());
        }
        emit contextMenuRequested(view->viewport()->mapToGlobal(at), onItems);
        return true;
    }
    // For a click on an item the menu is about the selection it is part of,
    // or else about that item alone.
    const QModelIndex index = view->indexAt(view->viewport()->mapFromGlobal(event->globalPos())).siblingAtColumn(0);
    if (index.isValid()) {
        if (!model->isRowSelected(index.row(), QModelIndex())) {
            rememberSelection();
            model->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        } else {
            model->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        }
    }
    emit contextMenuRequested(event->globalPos(), index.isValid());
    return true;
}

void FolderTab::setFindOptions(const FindOptions &options) {
    if (options == m_findOptions)
        return;
    m_findOptions = options;
    if (!m_findBar->isHidden())
        find(m_findBar->text());
    else if (!options.keepHighlights)
        clearHighlights();
}

QList<int> FolderTab::matchingRows(const FindMatcher &matcher, bool *anyStarts) const {
    QList<int> rows;
    *anyStarts = false;
    for (int row = 0; row < m_proxy->rowCount(); ++row) {
        const FileEntry &e = m_model->entry(m_proxy->mapToSource(m_proxy->index(row, 0)).row());
        const int at = matcher.match(e.name, e.isDir).first;
        if (at < 0)
            continue;
        rows << row;
        *anyStarts |= at == 0;
    }
    return rows;
}

void FolderTab::find(const QString &text) {
    if (text.isEmpty()) {
        m_matches.clear();
        m_find.reset();
        clearHighlights();
        m_findBar->setResult(0, 0);
        return;
    }
    const int rows = m_proxy->rowCount();
    auto matcher = std::make_shared<const FindMatcher>(text, m_findOptions);
    bool anyStarts = false;
    QList<int> matches = matchingRows(*matcher, &anyStarts);
    int from = qBound(0, m_findFrom, qMax(0, rows - 1));
    bool startsOnly = anyStarts;
    bool repeating = false;

    // "ddd" with no name starting that way: the third name starting with d.
    if (m_findOptions.repeatFirst && text.size() > 1 && !anyStarts &&
        text.count(text.at(0), Qt::CaseInsensitive) == text.size()) {
        FindOptions first = m_findOptions;
        first.anywhere = false;
        auto letter = std::make_shared<const FindMatcher>(text.left(1), first);
        bool starts = false;
        const QList<int> repeated = matchingRows(*letter, &starts);
        if (!repeated.isEmpty()) {
            matcher = letter;
            matches = repeated;
            startsOnly = true;
            repeating = true;
            from = (fileView()->currentIndex().row() + 1) % qMax(1, rows);
        }
    }

    m_find = matcher;
    m_matches = matches;
    setHighlights(matcher);
    // Names starting with the text come first; then, if asked, the shortest;
    // then the next one down from where the cursor was, and round again.
    int best = -1;
    auto better = [&](int a, int b) {
        if (m_findOptions.preferShorter && !repeating && nameAt(a).size() != nameAt(b).size())
            return nameAt(a).size() < nameAt(b).size();
        return (a - from + rows) % rows < (b - from + rows) % rows;
    };
    for (const int row : std::as_const(matches)) {
        if (startsOnly) {
            const FileEntry &e = m_model->entry(m_proxy->mapToSource(m_proxy->index(row, 0)).row());
            if (matcher->match(e.name, e.isDir).first != 0)
                continue;
        }
        if (best < 0 || better(row, best))
            best = row;
    }
    if (best >= 0)
        goToMatch(best);
    else
        m_findBar->setResult(0, 0);
}

void FolderTab::goToMatch(int row) {
    QAbstractItemView *view = fileView();
    const QModelIndex index = m_proxy->index(row, 0);
    // In Power mode the selection is built up by clicking, so only the
    // cursor moves.
    view->selectionModel()->setCurrentIndex(index, m_format.mode == ViewFormat::Mode::Power
                                                       ? QItemSelectionModel::NoUpdate
                                                       : QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    // The bar may have only just taken its room from the list.
    layout()->activate();
    view->scrollTo(index);
    m_findBar->setResult(int(m_matches.indexOf(row)) + 1, int(m_matches.size()));
}

bool FolderTab::findStep(int step) {
    if (!m_find) {
        setError(tr("Type part of a name to find it."));
        return false;
    }
    bool anyStarts = false;
    m_matches = matchingRows(*m_find, &anyStarts);
    if (m_matches.isEmpty()) {
        setError(tr("Nothing else here matches."));
        return false;
    }
    const int current = fileView()->currentIndex().row();
    int next = step > 0 ? m_matches.first() : m_matches.last();
    if (step > 0) {
        for (const int row : std::as_const(m_matches))
            if (row > current) {
                next = row;
                break;
            }
    } else {
        for (const int row : std::as_const(m_matches))
            if (row < current)
                next = row;
            else
                break;
    }
    goToMatch(next);
    return true;
}

void FolderTab::updateMatches() {
    if (!m_highlighting || !m_find)
        return;
    bool anyStarts = false;
    m_matches = matchingRows(*m_find, &anyStarts);
    setHighlights(m_find);
    if (!m_findBar->isHidden())
        m_findBar->setResult(int(m_matches.indexOf(fileView()->currentIndex().row())) + 1, int(m_matches.size()));
}

void FolderTab::setHighlights(const std::shared_ptr<const FindMatcher> &matcher) {
    m_highlighting = bool(matcher);
    for (RenameDelegate *delegate : {m_plainDelegate, m_detailsDelegate, static_cast<RenameDelegate *>(m_tileDelegate)})
        delegate->setFindMatcher(m_findOptions.highlight ? matcher : nullptr);
    QList<double> marks;
    const int rows = m_proxy->rowCount();
    if (matcher && m_findOptions.scrollMarks)
        for (const int row : std::as_const(m_matches))
            marks << double(row) / rows;
    for (QAbstractItemView *view : {static_cast<QAbstractItemView *>(m_details), static_cast<QAbstractItemView *>(m_icons)}) {
        // In the list that runs down in columns, the way along is sideways;
        // everywhere else it is down.
        const bool sideways = view == m_icons && m_format.mode == ViewFormat::Mode::List;
        static_cast<MatchScrollBar *>(view->verticalScrollBar())->setMarks(sideways ? QList<double>() : marks);
        static_cast<MatchScrollBar *>(view->horizontalScrollBar())->setMarks(sideways ? marks : QList<double>());
        view->viewport()->update();
    }
}

void FolderTab::clearHighlights() {
    if (m_highlighting)
        setHighlights(nullptr);
}

void FolderTab::closeFind(bool refocus) {
    if (m_findBar->isHidden())
        return;
    // Moving the keyboard closes it again; it is already on its way.
    m_findBar->hide();
    if (refocus)
        fileView()->setFocus();
    if (!m_findOptions.keepHighlights)
        clearHighlights();
}

void FolderTab::setFilterOptions(const QuickFilter::Mode &mode, const FilterBehaviour &behaviour) {
    m_filterBehaviour = behaviour;
    m_filterBar->setDefaultMode(mode);
    m_filterBar->setRealTime(behaviour.realTime);
}

void FolderTab::openFilterBar() {
    m_filterBar->open();
}

void FolderTab::applyQuickFilter(const QuickFilter &filter) {
    // The selection from before filtering can be brought back afterwards.
    if (!m_proxy->quickFilter().isActive() && filter.isActive())
        rememberSelection();
    m_proxy->setQuickFilter(filter);
    countItems();
    if (filter.isActive() && m_filterBehaviour.selectFirst && m_proxy->rowCount() > 0)
        focusName(nameAt(0));
    else if (fileView()->currentIndex().isValid())
        fileView()->scrollTo(fileView()->currentIndex());
}
