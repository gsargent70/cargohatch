#include "filedisplay.h"

#include "breadcrumb.h"

#include <QApplication>
#include <QBoxLayout>
#include <QEvent>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QToolButton>

#include "filedrag.h"
#include "foldertab.h"
#include "icons.h"
#include "tabbar.h"
#include "theme.h"

namespace {

QString escaped(const QString &text) {
    return QString(text).replace('&', "&&");
}

QToolButton *barButton(QWidget *parent) {
    auto *button = new QToolButton(parent);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setIconSize(QSize(14, 14));
    return button;
}

QTabBar::Shape shapeFor(TabOptions::Position position) {
    switch (position) {
    case TabOptions::Position::Bottom:
        return QTabBar::RoundedSouth;
    case TabOptions::Position::Left:
        return QTabBar::RoundedWest;
    case TabOptions::Position::Right:
        return QTabBar::RoundedEast;
    case TabOptions::Position::Top:
        break;
    }
    return QTabBar::RoundedNorth;
}

} // namespace

FileDisplay::FileDisplay(QWidget *parent)
    : QWidget(parent),
      m_tabBar(nullptr),
      m_stack(new QStackedWidget(this)),
      m_header(new QWidget(this)),
      m_strip(new QWidget(this)),
      m_roleLabel(new QLabel(m_header)),
      m_newTabButton(barButton(m_header)),
      m_listButton(barButton(m_header)),
      m_listMenu(new QMenu(this)) {
    m_tabBar = new FolderTabBar(m_header);
    m_tabBar->setDocumentMode(true);
    m_tabBar->setMovable(true);
    m_tabBar->setUsesScrollButtons(true);
    m_tabBar->setElideMode(Qt::ElideMiddle);
    m_tabBar->setExpanding(false);
    m_tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tabBar->installEventFilter(this);
    // Files can be dropped on a tab, or held over it to show its folder.
    auto *drops = new FileDropTarget(m_tabBar, [this](const QPoint &pos) -> FileDropTarget::Place {
        const int index = m_tabBar->tabAt(pos);
        return index < 0 ? FileDropTarget::Place{} : FileDropTarget::Place{tab(index)->currentPath(), m_tabBar->tabRect(index)};
    });
    drops->setHover([this](const QPoint &pos) {
        if (const int index = m_tabBar->tabAt(pos); index >= 0)
            m_tabBar->setCurrentIndex(index);
    });
    m_header->installEventFilter(this);
    m_roleLabel->setContentsMargins(8, 0, 8, 0);

    // The list of tabs is filled each time it opens, so it is never stale.
    m_listButton->setPopupMode(QToolButton::InstantPopup);
    m_listButton->setMenu(m_listMenu);
    m_listButton->setStyleSheet("QToolButton::menu-indicator { image: none; width: 0; }");
    m_listButton->hide();
    connect(m_listMenu, &QMenu::aboutToShow, this, &FileDisplay::fillTabList);
    connect(m_newTabButton, &QToolButton::clicked, this, &FileDisplay::newTabFromCurrent);

    // A thin bar across the top in the accent color marks the source; the
    // destination's is a raised background color, plainly different from the
    // accent even in themes whose muted text is close to it.
    m_strip->setFixedHeight(3);
    m_strip->setAutoFillBackground(true);

    connect(m_tabBar, &QTabBar::currentChanged, this, [this](int index) {
        if (index < 0 || index >= m_stack->count())
            return;
        m_stack->setCurrentIndex(index);
        emit currentTabChanged(currentTab());
    });
    connect(m_tabBar, &QTabBar::tabMoved, this, [this](int from, int to) {
        QWidget *moved = m_stack->widget(from);
        m_stack->removeWidget(moved);
        m_stack->insertWidget(to, moved);
        m_stack->setCurrentIndex(m_tabBar->currentIndex());
    });
    connect(m_tabBar, &QWidget::customContextMenuRequested, this, &FileDisplay::showTabMenu);

    updateIcons();
    retranslate();
    rebuildLayout();
    setOptions(m_options);
    updateRoleIndicator();
}

int FileDisplay::tabCount() const { return m_stack->count(); }

int FileDisplay::currentIndex() const { return m_tabBar->currentIndex(); }

FolderTab *FileDisplay::tab(int index) const {
    return qobject_cast<FolderTab *>(m_stack->widget(index));
}

FolderTab *FileDisplay::currentTab() const {
    return tab(currentIndex());
}

int FileDisplay::indexOf(const FolderTab *tab) const {
    return m_stack->indexOf(const_cast<FolderTab *>(tab));
}

QString FileDisplay::tabText(int index) const { return m_tabBar->tabText(index); }

QString FileDisplay::tabToolTip(int index) const { return m_tabBar->tabToolTip(index); }

void FileDisplay::setOptions(const TabOptions &options) {
    const bool moved = options.position != m_options.position;
    m_options = options;
    m_newTabButton->setVisible(options.newTabButton);
    if (moved)
        rebuildLayout();
    for (int i = 0; i < tabCount(); ++i) {
        const auto side = QTabBar::ButtonPosition(
            style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, m_tabBar));
        QWidget *button = m_tabBar->tabButton(i, side);
        if (options.closeButtons && !button) {
            addCloseButton(tab(i));
        } else if (!options.closeButtons && button) {
            m_tabBar->setTabButton(i, side, nullptr);
            button->deleteLater();
        }
    }
    // Colour rules and sort options may have changed.
    for (int i = 0; i < tabCount(); ++i) {
        updateTabLabel(tab(i));
        applySortOptions(tab(i));
    }
    updateCloseButtons();
    scheduleOverflowCheck();
}

void FileDisplay::setLastTabClosable(bool closable) {
    m_lastTabClosable = closable;
    updateCloseButtons();
}

void FileDisplay::rebuildLayout() {
    const bool vertical = m_options.position == TabOptions::Position::Left ||
                          m_options.position == TabOptions::Position::Right;
    m_tabBar->setShape(shapeFor(m_options.position));
    // The bar may be squeezed along its length, where it scrolls, but not
    // across it.
    if (vertical)
        m_tabBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    else
        m_tabBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    delete m_header->layout();
    if (vertical) {
        // A column: the role, the tabs, then the buttons under the last tab.
        auto *column = new QVBoxLayout(m_header);
        column->setContentsMargins(0, 2, 0, 2);
        column->setSpacing(2);
        column->addWidget(m_roleLabel);
        column->addWidget(m_tabBar);
        auto *buttons = new QHBoxLayout;
        buttons->setSpacing(0);
        buttons->addWidget(m_newTabButton);
        buttons->addStretch(1);
        buttons->addWidget(m_listButton);
        column->addLayout(buttons);
        column->addStretch(1);
    } else {
        auto *row = new QHBoxLayout(m_header);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(0);
        row->addWidget(m_tabBar);
        row->addWidget(m_newTabButton);
        row->addStretch(1);
        row->addWidget(m_listButton);
        row->addWidget(m_roleLabel);
    }

    delete layout();
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(m_strip);
    switch (m_options.position) {
    case TabOptions::Position::Top:
        outer->addWidget(m_header);
        outer->addWidget(m_stack, 1);
        break;
    case TabOptions::Position::Bottom:
        outer->addWidget(m_stack, 1);
        outer->addWidget(m_header);
        break;
    case TabOptions::Position::Left:
    case TabOptions::Position::Right: {
        auto *row = new QHBoxLayout;
        row->setSpacing(0);
        if (m_options.position == TabOptions::Position::Left)
            row->addWidget(m_header);
        row->addWidget(m_stack, 1);
        if (m_options.position == TabOptions::Position::Right)
            row->addWidget(m_header);
        outer->addLayout(row, 1);
        break;
    }
    }
    m_roleLabel->setAlignment(vertical ? Qt::AlignLeft | Qt::AlignVCenter : Qt::AlignRight | Qt::AlignVCenter);
}

FolderTab *FileDisplay::makeTab(const QString &path) const {
    auto *tab = new FolderTab(path);
    tab->setFormat(ViewFormat::fromString(m_options.defaultFormat));
    applySortOptions(tab);
    return tab;
}

void FileDisplay::applySortOptions(FolderTab *tab) const {
    FileModel::SortOptions sort;
    sort.natural = m_options.naturalSort;
    sort.wordSort = m_options.wordSort;
    sort.extensionsSeparately = m_options.extensionsSeparately;
    tab->setSortOptions(sort, m_options.foldersAlphabetical);
    // Not sorting, but shared by every tab in the same way.
    tab->setRenameSelection(m_options.renameSelection());
    tab->setGlobalFilter(m_options.filter);
    tab->breadcrumb()->setCompletionOptions(m_options.completionOptions());
    tab->setFindOptions(m_options.findOptions());
    tab->setFilterOptions(m_options.filterMode, {m_options.filterRealTime, m_options.filterClearOnNavigate,
                                                 m_options.filterSelectFirst});
}

FolderTab *FileDisplay::addTab(const QString &path) {
    FolderTab *tab = makeTab(path);
    insertTab(tab, m_options.openNextToCurrent && tabCount() > 0 ? currentIndex() + 1 : -1);
    return tab;
}

FolderTab *FileDisplay::restoreTab(const ListerState::Tab &state, int index) {
    FolderTab *tab = makeTab(state.path);
    // A folder that has gone away opens home instead, and the old history
    // would no longer match it.
    if (QFileInfo(tab->currentPath()) == QFileInfo(state.path))
        tab->setHistory(state.history);
    if (!state.format.isEmpty())
        tab->setFormat(ViewFormat::fromString(state.format));
    tab->setLabel(state.label);
    tab->setColorName(state.color);
    if (!state.tree.isEmpty())
        tab->setTreeBranches(state.tree);
    if (!state.home.isEmpty())
        tab->setHomePath(state.home);
    if (state.lock != 0)
        tab->setLock(FolderTab::Lock(state.lock), state.lockedPath);
    insertTab(tab, qBound(0, index, tabCount()));
    return tab;
}

ListerState::Tab FileDisplay::tabState(const FolderTab *tab) {
    ListerState::Tab state;
    state.path = tab->currentPath();
    state.history = tab->history();
    state.label = tab->label();
    state.lock = int(tab->lock());
    state.lockedPath = tab->lockedPath();
    state.home = tab->homePath();
    state.color = tab->colorName();
    state.format = tab->format().toString();
    state.tree = tab->treeBranches();
    return state;
}

FolderTab *FileDisplay::openInNewTab(const QString &path, int index) {
    if (m_options.findExistingTab) {
        const QFileInfo wanted(path);
        for (int i = 0; i < tabCount(); ++i)
            if (QFileInfo(tab(i)->currentPath()) == wanted) {
                setCurrentIndex(i);
                return tab(i);
            }
    }
    if (index < 0)
        return addTab(path);
    FolderTab *opened = makeTab(path);
    insertTab(opened, index);
    return opened;
}

void FileDisplay::openRedirected(FolderTab *from, const QString &path) {
    const bool hadFocus = from->isAncestorOf(QApplication::focusWidget());
    FolderTab *target = from->reuseTarget();
    if (from->lock() == FolderTab::Lock::Reuse && target && indexOf(target) >= 0 && !target->isLocked()) {
        target->openPath(path);
        setCurrentIndex(indexOf(target));
    } else {
        target = openInNewTab(path, indexOf(from) + 1);
        if (from->lock() == FolderTab::Lock::Reuse && target != from)
            from->setReuseTarget(target);
    }
    if (hadFocus)
        target->setFocus();
}

FolderTab *FileDisplay::duplicateTab(int index) {
    FolderTab *source = tab(index);
    if (!source)
        return nullptr;
    FolderTab *copy = makeTab(source->currentPath());
    copy->setHistory(source->history());
    copy->setFormat(source->format());
    copy->setLabel(source->label());
    copy->setColorName(source->colorName());
    copy->setHomePath(source->homePath());
    insertTab(copy, index + 1);
    return copy;
}

bool FileDisplay::closeTab(int index) {
    if (index < 0 || index >= tabCount())
        return false;
    FolderTab *closing = tab(index);
    if (closing->isLocked()) {
        closing->showError(tr("This tab is locked. Unlock it to close it."));
        return false;
    }
    if (tabCount() == 1) {
        emit lastTabCloseRequested();
        return false;
    }
    closing->setLinkedTab(nullptr);
    emit tabClosed(closing, index);
    takeTab(index);
    closing->deleteLater();
    return true;
}

void FileDisplay::closeAllTabs() {
    while (tabCount() > 0) {
        const int index = tabCount() - 1;
        FolderTab *closing = tab(index);
        closing->setLinkedTab(nullptr);
        emit tabClosed(closing, index);
        takeTab(index);
        closing->deleteLater();
    }
}

FolderTab *FileDisplay::takeTab(int index) {
    FolderTab *taken = tab(index);
    if (!taken)
        return nullptr;
    disconnect(taken, nullptr, this, nullptr);
    // The page goes first, so the tab bar's currentChanged (sent while it
    // removes the tab) finds the remaining pages in the same order.
    m_stack->removeWidget(taken);
    m_tabBar->removeTab(index);
    taken->setParent(nullptr);
    if (tabCount() > 0)
        m_stack->setCurrentIndex(m_tabBar->currentIndex());
    updateCloseButtons();
    scheduleOverflowCheck();
    emit tabCountChanged();
    return taken;
}

void FileDisplay::insertTab(FolderTab *tab, int index) {
    connectTab(tab);
    const int at = index < 0 || index > tabCount() ? tabCount() : index;
    m_stack->insertWidget(at, tab);
    m_tabBar->insertTab(at, QString());
    if (m_options.closeButtons)
        addCloseButton(tab);
    updateTabLabel(tab);
    m_tabBar->setCurrentIndex(at);
    m_stack->setCurrentIndex(at);
    updateCloseButtons();
    scheduleOverflowCheck();
    emit tabCountChanged();
}

void FileDisplay::setCurrentIndex(int index) {
    m_tabBar->setCurrentIndex(index);
}

void FileDisplay::cycleTab(int step) {
    if (tabCount() > 1)
        setCurrentIndex((currentIndex() + step + tabCount()) % tabCount());
}

void FileDisplay::newTabFromCurrent() {
    const FolderTab *current = currentTab();
    addTab(current ? current->currentPath() : QString())->setFocus();
}

void FileDisplay::connectTab(FolderTab *tab) {
    connect(tab, &FolderTab::pathChanged, this, [this, tab] {
        updateTabLabel(tab);
        emit tabPathChanged(tab);
    });
    connect(tab, &FolderTab::statusChanged, this, [this, tab] { emit tabStatusChanged(tab); });
    connect(tab, &FolderTab::previewChanged, this, [this, tab] { emit tabPreviewChanged(tab); });
    // The window's View menu shows the tab's mode, so it hears of changes too.
    connect(tab, &FolderTab::formatChanged, this, [this, tab] { emit tabStatusChanged(tab); });
    connect(tab, &FolderTab::decorationChanged, this, [this, tab] {
        updateTabLabel(tab);
        updateCloseButtons();
    });
    connect(tab, &FolderTab::redirectRequested, this,
            [this, tab](const QString &path) { openRedirected(tab, path); });
    connect(tab, &FolderTab::filterDefaultsRequested, this, [this](const QuickFilter::Mode &mode) {
        TabOptions options = m_options;
        options.filterMode = mode;
        emit optionsChangeRequested(options);
    });
    connect(tab, &FolderTab::contextMenuRequested, this, [this, tab](const QPoint &position, bool onItems) {
        emit contextMenuRequested(tab, position, onItems);
    });
    connect(tab, &FolderTab::openSelectionRequested, this, [this, tab] { emit openSelectionRequested(tab); });
    connect(tab, &FolderTab::newTabRequested, this,
            [this, tab](const QString &path) { openInNewTab(path, indexOf(tab) + 1); });
}

void FileDisplay::updateTabLabel(FolderTab *tab) {
    const int index = indexOf(tab);
    if (index < 0)
        return;
    m_tabBar->setTabText(index, escaped(tab->displayName()));
    QStringList tip{tab->currentPath()};
    switch (tab->lock()) {
    case FolderTab::Lock::Locked:
        tip << tr("Locked: other folders open in a new tab");
        break;
    case FolderTab::Lock::AllowChanges:
        tip << tr("Locked on %1, changes allowed").arg(tab->lockedPath());
        break;
    case FolderTab::Lock::Reuse:
        tip << tr("Locked: other folders open in the tab beside it");
        break;
    case FolderTab::Lock::None:
        break;
    }
    if (tab->linkedTab())
        tip << tr("Linked with %1 on the other side").arg(tab->linkedTab()->displayName());
    m_tabBar->setTabToolTip(index, tip.join('\n'));
    m_tabBar->setTabIcon(index, tab->isLocked()     ? Icons::padlock(palette())
                                : tab->linkedTab() ? Icons::link(palette())
                                                   : QIcon());
    const QString colorName = !tab->colorName().isEmpty() ? tab->colorName() : m_options.colorFor(tab->currentPath());
    m_tabBar->setTabColor(index, colorName.isEmpty() ? QColor() : hue(colorName));
    scheduleOverflowCheck();
}

void FileDisplay::addCloseButton(FolderTab *tab) {
    auto *button = barButton(m_tabBar);
    button->setIcon(Icons::close(palette()));
    button->setIconSize(QSize(10, 10));
    button->setFixedSize(18, 18);
    button->setToolTip(tr("Close Tab"));
    button->setProperty("closeButton", true);
    // The tab, not its index, is captured: indexes change as tabs move.
    connect(button, &QToolButton::clicked, this, [this, tab] { closeTab(indexOf(tab)); });
    const auto side =
        QTabBar::ButtonPosition(style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, m_tabBar));
    m_tabBar->setTabButton(indexOf(tab), side, button);
}

void FileDisplay::updateCloseButtons() {
    // A lone tab that can't be closed, or a locked one, shows no button that
    // would do nothing.
    const bool show = tabCount() > 1 || m_lastTabClosable;
    const auto side =
        QTabBar::ButtonPosition(style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, m_tabBar));
    for (int i = 0; i < tabCount(); ++i)
        if (QWidget *button = m_tabBar->tabButton(i, side))
            button->setVisible(show && !tab(i)->isLocked());
}

void FileDisplay::scheduleOverflowCheck() {
    // QTabBar only tells the layout its size changed while it is on screen,
    // so tabs added to a window that isn't showing yet would be squeezed.
    m_tabBar->updateGeometry();
    if (m_overflowCheckPending)
        return;
    m_overflowCheckPending = true;
    // After the layout has given the tab bar its new size.
    QTimer::singleShot(0, this, &FileDisplay::checkOverflow);
}

void FileDisplay::checkOverflow() {
    m_overflowCheckPending = false;
    m_listButton->setVisible(isOverflowing());
}

bool FileDisplay::isOverflowing() const {
    if (!m_tabBar->isVisible() || tabCount() < 2)
        return false;
    const QSize wanted = m_tabBar->sizeHint();
    return m_tabBar->isVertical() ? m_tabBar->height() < wanted.height() : m_tabBar->width() < wanted.width();
}

void FileDisplay::fillTabList() {
    m_listMenu->clear();
    const QIcon folder = QFileIconProvider().icon(QFileIconProvider::Folder);
    for (int i = 0; i < tabCount(); ++i) {
        // The folder path sits in the shortcut column, to the right.
        QAction *action = m_listMenu->addAction(folder, escaped(tab(i)->title()) + '\t' + tab(i)->currentPath());
        action->setCheckable(true);
        action->setChecked(i == currentIndex());
        connect(action, &QAction::triggered, this, [this, i] {
            setCurrentIndex(i);
            if (FolderTab *chosen = currentTab())
                chosen->setFocus();
        });
    }
}

QMenu *FileDisplay::tabListMenu() {
    fillTabList();
    return m_listMenu;
}

void FileDisplay::showTabList() {
    // Under the list button when it is showing, otherwise under the current
    // tab.
    QPoint at;
    if (m_listButton->isVisible())
        at = m_listButton->mapToGlobal(QPoint(0, m_listButton->height()));
    else
        at = m_tabBar->mapToGlobal(m_tabBar->tabRect(qMax(0, currentIndex())).bottomLeft());
    m_listMenu->popup(at);
}

void FileDisplay::setRole(Role role, bool locked) {
    if (role == m_role && locked == m_locked)
        return;
    m_role = role;
    m_locked = locked;
    updateRoleIndicator();
}

void FileDisplay::setCanMoveTabs(bool canMove) {
    m_canMoveTabs = canMove;
}

void FileDisplay::updateRoleIndicator() {
    QColor color;
    QString text;
    switch (m_role) {
    case Role::Source:
        color = palette().color(QPalette::Accent);
        text = m_locked ? tr("Source (locked)") : tr("Source");
        break;
    case Role::Destination:
        color = palette().color(QPalette::AlternateBase);
        text = m_locked ? tr("Destination (locked)") : tr("Destination");
        break;
    case Role::None:
        break;
    }

    m_strip->setVisible(m_role != Role::None);
    m_roleLabel->setVisible(m_role != Role::None);
    QPalette stripPalette = m_strip->palette();
    stripPalette.setColor(QPalette::Window, color);
    m_strip->setPalette(stripPalette);
    QPalette labelPalette = m_roleLabel->palette();
    labelPalette.setColor(QPalette::WindowText,
                          m_role == Role::Source ? color : palette().color(QPalette::PlaceholderText));
    m_roleLabel->setPalette(labelPalette);
    m_roleLabel->setText(text);
}

void FileDisplay::updateIcons() {
    m_newTabButton->setIcon(Icons::plus(palette()));
    m_listButton->setIcon(Icons::chevron(palette()));
    for (QToolButton *button : m_tabBar->findChildren<QToolButton *>())
        if (button->property("closeButton").toBool())
            button->setIcon(Icons::close(palette()));
}

// The tab's lock (TAB-08), as a submenu of the tab's own menu and of the
// menu bar's Tabs menu.
void FileDisplay::fillLockMenu(QMenu *menu, FolderTab *target) {
    menu->clear();
    const std::pair<FolderTab::Lock, QString> locks[] = {
        {FolderTab::Lock::None, tr("Unlocked")},
        {FolderTab::Lock::Locked, tr("Locked")},
        {FolderTab::Lock::AllowChanges, tr("Locked, Changes Allowed")},
        {FolderTab::Lock::Reuse, tr("Locked, Reusing One Tab")},
    };
    for (const auto &[lock, text] : locks) {
        QAction *a = menu->addAction(text);
        a->setCheckable(true);
        a->setChecked(target->lock() == lock);
        connect(a, &QAction::triggered, this, [target, lock] { target->setLock(lock); });
    }
    if (target->lock() == FolderTab::Lock::AllowChanges) {
        menu->addSeparator();
        QAction *back = menu->addAction(tr("Return to Locked Folder"));
        back->setEnabled(target->currentPath() != target->lockedPath());
        connect(back, &QAction::triggered, target, &FolderTab::returnToLockedFolder);
    }
}

// Colours (TAB-10), named after the theme's hues so they follow it.
void FileDisplay::fillColorMenu(QMenu *menu, FolderTab *target) {
    menu->clear();
    QAction *none = menu->addAction(tr("No Colour"));
    none->setCheckable(true);
    none->setChecked(target->colorName().isEmpty());
    connect(none, &QAction::triggered, this, [target] { target->setColorName({}); });
    const QHash<QString, QString> hueLabels = {
        {"red", tr("Red")},   {"orange", tr("Orange")}, {"yellow", tr("Yellow")},   {"green", tr("Green")},
        {"cyan", tr("Cyan")}, {"blue", tr("Blue")},     {"magenta", tr("Magenta")},
    };
    for (const QString &name : Theme::hueNames()) {
        QPixmap swatch(16, 16);
        swatch.fill(hue(name));
        QAction *a = menu->addAction(QIcon(swatch), hueLabels.value(name, name));
        a->setCheckable(true);
        a->setChecked(target->colorName() == name);
        connect(a, &QAction::triggered, this, [target, name] { target->setColorName(name); });
    }
    menu->addSeparator();
    // A rule colours every tab showing this folder or anything inside it.
    const QString folder = target->currentPath();
    QAction *remember = menu->addAction(tr("Use This Colour for This Folder and Below"));
    remember->setEnabled(!target->colorName().isEmpty());
    connect(remember, &QAction::triggered, this, [this, target, folder] {
        TabOptions options = m_options;
        options.setColorRule(folder, target->colorName());
        target->setColorName({});
        emit optionsChangeRequested(options);
    });
    const bool hasOwnRule = std::any_of(m_options.colorRules.begin(), m_options.colorRules.end(),
                                        [&](const TabOptions::ColorRule &r) { return r.pattern == folder; });
    QAction *forget = menu->addAction(tr("Forget the Colour for This Folder"));
    forget->setEnabled(hasOwnRule);
    connect(forget, &QAction::triggered, this, [this, folder] {
        TabOptions options = m_options;
        options.setColorRule(folder, {});
        emit optionsChangeRequested(options);
    });
}

void FileDisplay::showTabMenu(const QPoint &pos) {
    const int index = m_tabBar->tabAt(pos);
    QMenu menu(this);
    connect(menu.addAction(tr("New Tab")), &QAction::triggered, this, [this, index] {
        if (index >= 0)
            setCurrentIndex(index);
        newTabFromCurrent();
    });
    if (index >= 0) {
        connect(menu.addAction(tr("Duplicate Tab")), &QAction::triggered, this,
                [this, index] { duplicateTab(index)->setFocus(); });
        QAction *move = menu.addAction(tr("Move Tab to Other Side"));
        move->setEnabled(m_canMoveTabs && tabCount() > 1);
        connect(move, &QAction::triggered, this, [this, index] { emit moveTabRequested(index); });
        menu.addSeparator();
        FolderTab *target = tab(index);
        connect(menu.addAction(tr("Rename Tab…")), &QAction::triggered, this, [this, target] { renameTab(target); });

        fillLockMenu(menu.addMenu(tr("Lock Tab")), target);
        fillColorMenu(menu.addMenu(tr("Colour")), target);

        // Home folder (TAB-18) and linking (TAB-09).
        QAction *home = menu.addAction(tr("Make This Folder the Tab's Home"));
        home->setEnabled(target->homePath() != target->currentPath());
        connect(home, &QAction::triggered, this, [target] { target->setHomePath(target->currentPath()); });
        QAction *link = menu.addAction(target->linkedTab() ? tr("Unlink from Other Side")
                                                           : tr("Link with Other Side's Current Tab"));
        link->setEnabled(m_canMoveTabs || target->linkedTab());
        connect(link, &QAction::triggered, this, [this, index] { emit linkRequested(index); });

        menu.addSeparator();
        QAction *close = menu.addAction(tr("Close Tab"));
        close->setEnabled((tabCount() > 1 || m_lastTabClosable) && !target->isLocked());
        connect(close, &QAction::triggered, this, [this, index] { closeTab(index); });
    }
    menu.exec(m_tabBar->mapToGlobal(pos));
}

bool FileDisplay::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_tabBar || watched == m_header) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show) {
            scheduleOverflowCheck();
        } else if (event->type() == QEvent::MouseButtonRelease) {
            // Middle-clicking a tab closes it, as in a web browser.
            auto *mouse = static_cast<QMouseEvent *>(event);
            const int index = watched == m_tabBar ? m_tabBar->tabAt(mouse->position().toPoint()) : -1;
            if (mouse->button() == Qt::MiddleButton && index >= 0) {
                closeTab(index);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            const int index = watched == m_tabBar ? m_tabBar->tabAt(mouse->position().toPoint()) : -1;
            if (mouse->button() == Qt::LeftButton) {
                // Double-clicking empty bar space opens a tab; double-clicking
                // a tab closes it if the user wants that.
                if (index < 0) {
                    newTabFromCurrent();
                    return true;
                }
                if (m_options.doubleClickCloses) {
                    closeTab(index);
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FileDisplay::renameTab(FolderTab *tab) {
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Rename Tab"),
                                               tr("Tab name (leave empty to show the folder's name):"),
                                               QLineEdit::Normal, tab->label(), &ok);
    if (ok)
        tab->setLabel(name);
}

QColor FileDisplay::hue(const QString &name) const {
    if (m_hues.isEmpty()) {
        // Read once and again whenever the theme (and so the palette)
        // changes.
        auto *self = const_cast<FileDisplay *>(this);
        self->m_hues = Theme::fallbackColors();
        const auto current = Theme::readColors(Theme::defaultDirectory() + "/theme/colors.toml");
        for (auto it = current.begin(); it != current.end(); ++it)
            self->m_hues.insert(it.key(), it.value());
    }
    return m_hues.value(name, QColor("#808080"));
}

void FileDisplay::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange || event->type() == QEvent::PaletteChange)
        updateRoleIndicator();
    if (event->type() == QEvent::PaletteChange) {
        m_hues.clear();
        updateIcons();
        for (int i = 0; i < tabCount(); ++i)
            updateTabLabel(tab(i));
    }
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

void FileDisplay::retranslate() {
    m_newTabButton->setToolTip(tr("New Tab (Ctrl+T)"));
    m_listButton->setToolTip(tr("All Tabs (Ctrl+Shift+A)"));
    for (QToolButton *button : m_tabBar->findChildren<QToolButton *>())
        if (button->property("closeButton").toBool())
            button->setToolTip(tr("Close Tab"));
}
