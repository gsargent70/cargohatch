#include "mainwindow.h"

#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QMenuBar>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QWindow>
#include <QTreeView>

#include "aboutdialog.h"
#include "shortcutsdialog.h"
#include "aliasdialog.h"
#include "createfolder.h"
#include "favoritesdialog.h"
#include "fileclipboard.h"
#include "fileops.h"
#include "filesortproxy.h"
#include "folderpicker.h"
#include "operationrunner.h"
#include "selectpattern.h"
#include "foldertab.h"
#include "foldertree.h"
#include "listermanager.h"
#include "nameformat.h"
#include "savedsets.h"
#include "tabswitcher.h"
#include "terminal.h"
#include "viewerpane.h"

MainWindow::MainWindow(const QString &startPath, QSettings *settings, QWidget *parent)
    : MainWindow(ListerState::startingIn(settings ? ListerState::load(*settings, "lister") : ListerState(), startPath),
                 settings, nullptr, parent) {}

MainWindow::MainWindow(const ListerState &state, QSettings *settings, ListerManager *manager, QWidget *parent)
    : QMainWindow(parent),
      m_settings(settings),
      m_manager(manager),
      m_viewSplit(new QSplitter(Qt::Horizontal, this)),
      m_viewer(new ViewerPane(m_viewSplit)),
      m_outer(new QSplitter(Qt::Horizontal, m_viewSplit)),
      m_splitter(new QSplitter(Qt::Horizontal, m_outer)),
      m_panels{new QSplitter(Qt::Horizontal, m_splitter), new QSplitter(Qt::Horizontal, m_splitter)},
      m_displays{new FileDisplay(m_panels[0]), new FileDisplay(m_panels[1])},
      m_status(new QLabel(this)),
      m_errorTimer(new QTimer(this)) {
    resize(1200, 760);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(4);
    m_splitter->setObjectName("sides");
    for (QSplitter *splitter : {m_viewSplit, m_outer, m_panels[0], m_panels[1]}) {
        splitter->setChildrenCollapsible(false);
        splitter->setHandleWidth(4);
    }
    // The files come first; the pane goes after them, to the right or below.
    m_viewSplit->insertWidget(0, m_outer);
    m_viewSplit->setStretchFactor(0, 1);
    m_viewSplit->setStretchFactor(1, 0);
    m_viewer->hide();
    connect(m_viewer, &ViewerPane::previousRequested, this, [this] { activeTab()->stepSelection(-1); });
    connect(m_viewer, &ViewerPane::nextRequested, this, [this] { activeTab()->stepSelection(1); });
    connect(m_viewer, &ViewerPane::lockChanged, this, [this](bool locked) {
        if (!locked)
            updatePreview();
        updateChrome();
    });
    setCentralWidget(m_viewSplit);
    createTrees();
    m_favoritesBar = new FavoritesBar(this, this);
    addToolBar(Qt::TopToolBarArea, m_favoritesBar);
    connect(m_favoritesBar, &FavoritesBar::menuRequested, this, &MainWindow::showFavoriteMenu);
    // Not Qt's own menu of tool bars.
    setContextMenuPolicy(Qt::NoContextMenu);
    statusBar()->addWidget(m_status, 1);
    m_errorTimer->setSingleShot(true);
    m_errorTimer->setInterval(8000);
    connect(m_errorTimer, &QTimer::timeout, this, [this] {
        FolderTab *tab = activeTab();
        if (tab && tab->errorId() == m_shownError && !tab->errorLasts())
            tab->clearError();
    });
    m_switcher = new TabSwitcher(this);
    connect(m_switcher, &TabSwitcher::chosen, this, [this](int row) {
        FolderTab *tab = row < m_switcherTabs.size() ? m_switcherTabs.at(row).data() : nullptr;
        const int side = sideOf(tab);
        if (side < 0)
            return;
        m_displays[side]->setCurrentIndex(m_displays[side]->indexOf(tab));
        activateSide(side);
        tab->setFocus();
    });

    for (int side = 0; side < 2; ++side) {
        FileDisplay *display = m_displays[side];
        connect(display, &FileDisplay::currentTabChanged, this, [this, side](FolderTab *tab) {
            if (side == m_active)
                noteActiveTab();
            // A linked tab brings its partner forward on the other side.
            if (m_dual && tab && tab->linkedTab()) {
                FileDisplay *other = m_displays[1 - side];
                const int partner = other->indexOf(tab->linkedTab());
                if (partner >= 0 && partner != other->currentIndex())
                    other->setCurrentIndex(partner);
            }
            updateChrome();
        });
        connect(display, &FileDisplay::linkRequested, this, [this, side](int index) { toggleLink(side, index); });
        connect(display, &FileDisplay::optionsChangeRequested, this, &MainWindow::setTabOptions);
        connect(display, &FileDisplay::contextMenuRequested, this,
                [this, side](FolderTab *, const QPoint &position, bool onItems) {
                    activateSide(side);
                    contextMenu(onItems)->popup(position);
                });
        connect(display, &FileDisplay::tabCountChanged, this, &MainWindow::updateChrome);
        connect(display, &FileDisplay::tabClosed, this, [this, side](FolderTab *tab, int index) {
            m_closedTabs.prepend({side, index, FileDisplay::tabState(tab)});
            while (m_closedTabs.size() > kMaxClosedTabs)
                m_closedTabs.removeLast();
            updateChrome();
        });
        connect(display, &FileDisplay::lastTabCloseRequested, this, [this, side] { closeLastTab(side); });
        connect(display, &FileDisplay::openSelectionRequested, this, &MainWindow::openSelection);
        connect(display, &FileDisplay::tabPathChanged, this, &MainWindow::updateChrome);
        connect(display, &FileDisplay::tabStatusChanged, this, &MainWindow::updateChrome);
        connect(display, &FileDisplay::tabPreviewChanged, this, [this](FolderTab *tab) {
            if (tab == activeTab())
                updatePreview();
        });
        connect(display, &FileDisplay::moveTabRequested, this,
                [this, side](int index) { moveTabToOtherSide(side, index); });
    }

    // Whichever side the user clicks or moves the keyboard focus into becomes
    // the active one. Clicks are watched as well as focus because the tab bar
    // and location bar buttons don't take focus.
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        const int side = sideOf(now);
        if (side >= 0)
            activateSide(side);
    });
    qApp->installEventFilter(this);

    if (m_manager)
        connect(m_manager, &ListerManager::windowsChanged, this, &MainWindow::updateChrome);

    createActions();
    retranslate();
    applyTabOptions(m_manager    ? m_manager->tabOptions()
                    : m_settings ? TabOptions::load(*m_settings)
                                 : TabOptions());
    applyState(state);
    updateRoles();
    noteActiveTab();
}

void MainWindow::applyState(const ListerState &state) {
    if (!state.geometry.isEmpty())
        restoreGeometry(state.geometry);
    setLayout(state.stacked ? Layout::Stacked : Layout::SideBySide);
    addStateTabs(state);
    m_active = qBound(0, state.active, 1);
    if (m_displays[m_active]->tabCount() == 0) {
        if (m_displays[1 - m_active]->tabCount() > 0)
            m_active = 1 - m_active;
        else
            m_displays[m_active]->addTab(QDir::homePath());
    }
    showSide(1 - m_active, false);
    m_splitterState = state.splitter;
    if (state.dual) {
        setDual(true);
        if (!state.splitter.isEmpty())
            m_splitter->restoreState(state.splitter);
    }
    // Branches first, so following the tabs adds to them.
    for (int t = 0; t < 3; ++t) {
        if (state.trees[t].width > 0)
            m_trees[t]->setPreferredWidth(state.trees[t].width);
        if (m_options.tree.remember && !state.trees[t].expanded.isEmpty())
            m_trees[t]->setExpanded(state.trees[t].expanded);
    }
    setTreeShown(state.treeShown);
    m_viewerSize = state.viewer.size;
    m_viewerBelow = state.viewer.below;
    m_viewer->setLocked(state.viewer.locked);
    placeViewer();
    setViewerShown(state.viewer.shown);
}

ListerState MainWindow::state() const {
    ListerState s;
    s.geometry = saveGeometry();
    s.dual = m_dual;
    s.stacked = m_layout == Layout::Stacked;
    s.splitter = m_dual ? m_splitter->saveState() : m_splitterState;
    s.active = m_active;
    s.treeShown = m_treeShown;
    s.viewer.shown = m_viewerShown;
    s.viewer.below = m_viewerBelow;
    s.viewer.size = m_viewerShown ? (m_viewerBelow ? m_viewer->height() : m_viewer->width()) : m_viewerSize;
    s.viewer.locked = m_viewer->isLocked();
    for (int t = 0; t < 3; ++t) {
        s.trees[t].width = m_trees[t]->isVisible() ? m_trees[t]->width() : m_trees[t]->sizeHint().width();
        if (m_options.tree.remember)
            s.trees[t].expanded = m_trees[t]->expanded();
        // The tab showing hasn't had its branches put away yet.
        if (m_options.tree.rememberTabs && m_treeTabs[t] && !m_trees[t]->isHidden())
            m_treeTabs[t]->setTreeBranches(m_trees[t]->expanded());
    }
    for (int side = 0; side < 2; ++side) {
        const FileDisplay *display = m_displays[side];
        for (int i = 0; i < display->tabCount(); ++i) {
            ListerState::Tab tab = FileDisplay::tabState(display->tab(i));
            tab.link = m_displays[1 - side]->indexOf(display->tab(i)->linkedTab());
            s.sides[side].tabs << tab;
        }
        s.sides[side].current = qMax(0, display->currentIndex());
    }
    return s;
}

void MainWindow::addStateTabs(const ListerState &state) {
    int offsets[2];
    for (int side = 0; side < 2; ++side) {
        FileDisplay *display = m_displays[side];
        offsets[side] = display->tabCount();
        for (const ListerState::Tab &tab : state.sides[side].tabs)
            display->restoreTab(tab, display->tabCount());
        if (!state.sides[side].tabs.isEmpty())
            display->setCurrentIndex(offsets[side] +
                                     qBound(0, state.sides[side].current, int(state.sides[side].tabs.size()) - 1));
    }
    const QList<ListerState::Tab> &left = state.sides[0].tabs;
    for (int i = 0; i < left.size(); ++i) {
        FolderTab *right = m_displays[1]->tab(offsets[1] + left.at(i).link);
        if (left.at(i).link >= 0 && right)
            m_displays[0]->tab(offsets[0] + i)->setLinkedTab(right);
    }
}

ListerState MainWindow::tabGroup() const {
    ListerState group = state();
    group.geometry.clear();
    group.splitter.clear();
    return group;
}

void MainWindow::openTabGroup(const ListerState &group, bool replace) {
    if (group.isEmpty())
        return;
    if (replace) {
        // Locked tabs stay: that is what locking them is for.
        for (FileDisplay *display : m_displays)
            for (int i = display->tabCount() - 1; i >= 0; --i)
                if (!display->tab(i)->isLocked()) {
                    FolderTab *tab = display->tab(i);
                    tab->setLinkedTab(nullptr);
                    display->takeTab(i);
                    tab->deleteLater();
                }
    }
    addStateTabs(group);
    if (m_displays[m_active]->tabCount() == 0) {
        if (!m_dual && m_displays[1 - m_active]->tabCount() > 0) {
            // The group only had tabs for the other side, so show that one.
            showSide(m_active, false);
            m_active = 1 - m_active;
            showSide(m_active, true);
        } else {
            m_displays[m_active]->addTab(QDir::homePath());
        }
    }
    if (group.dual && !m_dual)
        setDual(true);
    if (m_dual)
        for (FileDisplay *display : m_displays)
            if (display->tabCount() == 0)
                display->addTab(activeTab()->currentPath());
    updateRoles();
    noteActiveTab();
    if (FolderTab *tab = activeTab())
        tab->setFocus();
}

void MainWindow::toggleLink(int side, int index) {
    FolderTab *tab = m_displays[side]->tab(index);
    if (!tab)
        return;
    if (tab->linkedTab())
        tab->setLinkedTab(nullptr);
    else if (m_dual)
        tab->setLinkedTab(m_displays[1 - side]->currentTab());
}

void MainWindow::openInNewTab() {
    FolderTab *here = activeTab();
    const QString path = here->highlightedFolder();
    if (path.isEmpty()) {
        here->showError(tr("Highlight a folder to open it in a new tab."));
        return;
    }
    FileDisplay *display = m_displays[m_active];
    display->openInNewTab(path, display->currentIndex() + 1)->setFocus();
}

QString MainWindow::askName(const QString &title, const QString &label, const QStringList &taken) {
    bool ok = false;
    const QString name = QInputDialog::getText(this, title, label, QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty())
        return {};
    if (taken.contains(name) &&
        QMessageBox::question(this, title, tr("“%1” already exists. Replace it?").arg(name)) != QMessageBox::Yes)
        return {};
    return name;
}

void MainWindow::saveTabGroupAs() {
    if (!m_settings)
        return;
    const QString name = askName(tr("Save Tab Group"), tr("Name for this set of tabs:"),
                                 SavedSets::tabGroupNames(*m_settings));
    if (!name.isEmpty())
        SavedSets::saveTabGroup(*m_settings, name, tabGroup());
}

void MainWindow::saveLayoutAs() {
    if (!m_settings || !m_manager)
        return;
    const QString name =
        askName(tr("Save Layout"), tr("Name for this set of windows:"), SavedSets::layoutNames(*m_settings));
    if (!name.isEmpty())
        m_manager->saveLayout(name);
}

void MainWindow::fillColumnsMenu() {
    m_columnsMenu->clear();
    FolderTab *tab = activeTab();
    const ViewFormat format = tab->format();
    for (int c = 0; c < Columns::count(); ++c) {
        QAction *a = m_columnsMenu->addAction(Columns::title(Column(c)));
        a->setCheckable(true);
        a->setChecked(format.hasColumn(Column(c)));
        a->setEnabled(Column(c) != Column::Name);
        connect(a, &QAction::triggered, this, [tab, c](bool on) { tab->setColumnVisible(Column(c), on); });
    }
    m_columnsMenu->addSeparator();
    connect(m_columnsMenu->addAction(tr("Size All Columns to Fit")), &QAction::triggered, tab,
            &FolderTab::sizeColumnsToFit);
}

void MainWindow::fillSortMenu() {
    m_sortMenu->clear();
    FolderTab *tab = activeTab();
    const ViewFormat format = tab->format();
    auto *columns = new QActionGroup(m_sortMenu);
    for (int c = 0; c < Columns::count(); ++c) {
        QAction *a = m_sortMenu->addAction(Columns::title(Column(c)));
        a->setCheckable(true);
        a->setChecked(format.sortColumn == Column(c));
        columns->addAction(a);
        connect(a, &QAction::triggered, this, [tab, c] {
            tab->sortBy(Column(c), Columns::sortsDescendingFirst(Column(c)));
        });
    }
    m_sortMenu->addSeparator();
    QAction *reverse = m_sortMenu->addAction(tr("Reverse Order"));
    reverse->setCheckable(true);
    reverse->setChecked(format.reverse);
    connect(reverse, &QAction::triggered, this, [tab](bool on) { tab->sortBy(tab->format().sortColumn, on); });

    m_sortMenu->addSeparator();
    auto *orders = new QActionGroup(m_sortMenu);
    const std::pair<ViewFormat::FolderOrder, QString> folderOrders[] = {
        {ViewFormat::FolderOrder::FoldersFirst, tr("Folders First")},
        {ViewFormat::FolderOrder::FilesFirst, tr("Files First")},
        {ViewFormat::FolderOrder::Mixed, tr("Folders and Files Mixed")},
    };
    for (const auto &[order, text] : folderOrders) {
        QAction *a = m_sortMenu->addAction(text);
        a->setCheckable(true);
        a->setChecked(format.folderOrder == order);
        orders->addAction(a);
        connect(a, &QAction::triggered, this, [tab, order] { tab->setFolderOrder(order); });
    }

    // These apply to every tab.
    m_sortMenu->addSeparator();
    const std::pair<bool TabOptions::*, QString> global[] = {
        {&TabOptions::foldersAlphabetical, tr("Keep Folders in Name Order")},
        {&TabOptions::naturalSort, tr("Sort Numbers by Value (2 before 10)")},
        {&TabOptions::wordSort, tr("Ignore Hyphens and Apostrophes")},
        {&TabOptions::extensionsSeparately, tr("Sort Names Before Extensions")},
    };
    for (const auto &[field, text] : global) {
        QAction *a = m_sortMenu->addAction(text);
        a->setCheckable(true);
        a->setChecked(m_options.*field);
        connect(a, &QAction::triggered, this, [this, field](bool on) {
            TabOptions options = m_options;
            options.*field = on;
            setTabOptions(options);
        });
    }
}

void MainWindow::fillTabGroupMenus() {
    for (QMenu *menu : {m_openGroupMenu, m_addGroupMenu, m_deleteGroupMenu})
        menu->clear();
    const QStringList names = m_settings ? SavedSets::tabGroupNames(*m_settings) : QStringList();
    for (const QString &name : names) {
        const QString text = QString(name).replace('&', "&&");
        connect(m_openGroupMenu->addAction(text), &QAction::triggered, this,
                [this, name] { openTabGroup(SavedSets::loadTabGroup(*m_settings, name), true); });
        connect(m_addGroupMenu->addAction(text), &QAction::triggered, this,
                [this, name] { openTabGroup(SavedSets::loadTabGroup(*m_settings, name), false); });
        connect(m_deleteGroupMenu->addAction(text), &QAction::triggered, this, [this, name] {
            if (QMessageBox::question(this, tr("Delete Tab Group"), tr("Delete the tab group “%1”?").arg(name)) ==
                QMessageBox::Yes)
                SavedSets::removeTabGroup(*m_settings, name);
        });
    }
    for (QMenu *menu : {m_openGroupMenu, m_addGroupMenu, m_deleteGroupMenu})
        menu->setEnabled(!names.isEmpty());
}

void MainWindow::fillLayoutMenus() {
    m_openLayoutMenu->clear();
    m_deleteLayoutMenu->clear();
    const QStringList names = m_settings ? SavedSets::layoutNames(*m_settings) : QStringList();
    for (const QString &name : names) {
        const QString text = QString(name).replace('&', "&&");
        connect(m_openLayoutMenu->addAction(text), &QAction::triggered, this, [this, name] {
            if (m_manager)
                m_manager->openLayout(name);
        });
        connect(m_deleteLayoutMenu->addAction(text), &QAction::triggered, this, [this, name] {
            if (QMessageBox::question(this, tr("Delete Layout"), tr("Delete the layout “%1”?").arg(name)) ==
                QMessageBox::Yes)
                SavedSets::removeLayout(*m_settings, name);
        });
    }
    m_openLayoutMenu->setEnabled(!names.isEmpty());
    m_deleteLayoutMenu->setEnabled(!names.isEmpty());
}

MainWindow::~MainWindow() {
    qApp->removeEventFilter(this);
    // The manager may still announce changes while this window is going.
    if (m_manager)
        disconnect(m_manager, nullptr, this, nullptr);
}

void MainWindow::createActions() {
    auto action = [this](QMenu *menu, std::initializer_list<QKeySequence> keys, auto slot) {
        auto *a = new QAction(this);
        a->setShortcuts(QList<QKeySequence>(keys));
        a->setShortcutContext(Qt::WindowShortcut);
        connect(a, &QAction::triggered, this, slot);
        if (menu)
            menu->addAction(a);
        addAction(a);
        return a;
    };

    // The menu bar holds commands; the choices that stay set are gathered
    // under Settings. Every action is also the window's own (see `action`),
    // so its keys work wherever in the menus it sits.
    m_fileMenu = menuBar()->addMenu(QString());
    m_editMenu = menuBar()->addMenu(QString());
    m_viewMenu = menuBar()->addMenu(QString());
    m_goMenu = menuBar()->addMenu(QString());
    m_tabsMenu = menuBar()->addMenu(QString());
    m_favoritesMenu = menuBar()->addMenu(QString());
    m_windowMenu = menuBar()->addMenu(QString());
    m_settingsMenu = menuBar()->addMenu(QString());
    m_helpMenu = menuBar()->addMenu(QString());

    // A check box in a menu for one of the shared options.
    auto choice = [this](QMenu *menu, bool TabOptions::*field, const char *text) {
        QAction *a = menu->addAction(QString());
        a->setCheckable(true);
        connect(a, &QAction::triggered, this, [this, field](bool on) {
            TabOptions options = m_options;
            options.*field = on;
            setTabOptions(options);
        });
        m_optionActions.append({a, field, text});
        return a;
    };
    auto option = [this, &action](QMenu *menu, bool TabOptions::*field) {
        QAction *a = action(menu, {}, [this, field](bool on) {
            TabOptions options = m_options;
            options.*field = on;
            setTabOptions(options);
        });
        a->setCheckable(true);
        return a;
    };

    // File: what acts on the items themselves, and the window.
    // Enter itself is handled by the file list; the menu shows the key.
    m_open = action(m_fileMenu, {}, [this] {
        FolderTab *tab = activeTab();
        const QStringList paths = tab->selectedPaths();
        if (paths.size() > 1)
            openSelection(tab);
        else if (paths.size() == 1)
            tab->openName(QFileInfo(paths.first()).fileName());
    });
    m_openInNewTab = action(m_fileMenu, {QKeySequence(Qt::CTRL | Qt::Key_Return), QKeySequence(Qt::CTRL | Qt::Key_Enter)},
                            [this] { openInNewTab(); });
    m_openInOtherSide = action(m_fileMenu, {QKeySequence(Qt::SHIFT | Qt::Key_Return), QKeySequence(Qt::SHIFT | Qt::Key_Enter)},
                               [this] { openInOtherSide(); });
    m_openTerminal = action(m_fileMenu, {QKeySequence(Qt::SHIFT | Qt::Key_F4)}, [this] { openTerminal(); });
    m_fileMenu->addSeparator();
    m_createFolder = action(m_fileMenu, {QKeySequence(Qt::Key_F7)}, [this] { createFolder(); });
    m_rename = action(m_fileMenu, {QKeySequence(Qt::Key_F2)}, [this] { activeTab()->startRename(); });
    m_duplicate = action(m_fileMenu, {QKeySequence(Qt::CTRL | Qt::Key_D)}, [this] { duplicateSelection(); });
    m_fileMenu->addSeparator();
    m_trash = action(m_fileMenu, {QKeySequence(Qt::Key_Delete)}, [this] { deleteSelection(false); });
    m_deleteForever = action(m_fileMenu, {QKeySequence(Qt::SHIFT | Qt::Key_Delete)}, [this] { deleteSelection(true); });
    m_fileMenu->addSeparator();
    m_newWindow = action(m_fileMenu, {QKeySequence(Qt::CTRL | Qt::Key_N)}, [this] {
        if (m_manager)
            m_manager->openDefault();
    });
    m_closeWindow = action(m_fileMenu, {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W)}, [this] { close(); });
    m_fileMenu->addSeparator();
    m_quit = action(m_fileMenu, {QKeySequence::Quit}, [this] {
        if (m_manager)
            m_manager->quit(this);
        else
            close();
    });

    // Edit: the clipboard, copying and moving, selecting and finding.
    // Text fields take the clipboard keys first, so typing a path or a name
    // copies and pastes text as usual.
    m_cut = action(m_editMenu, {QKeySequence::Cut}, [this] { copyToClipboard(true); });
    m_copy = action(m_editMenu, {QKeySequence::Copy}, [this] { copyToClipboard(false); });
    m_paste = action(m_editMenu, {QKeySequence::Paste}, [this] { pasteFiles(); });
    m_pasteLinkMenu = m_editMenu->addMenu(QString());
    m_pasteSymlink = action(m_pasteLinkMenu, {}, [this] { pasteLinks(FileOperation::LinkType::Symbolic); });
    m_pasteRelative = action(m_pasteLinkMenu, {}, [this] { pasteLinks(FileOperation::LinkType::Relative); });
    m_pasteHardlink = action(m_pasteLinkMenu, {}, [this] { pasteLinks(FileOperation::LinkType::Hard); });
    m_copyNamesMenu = m_editMenu->addMenu(QString());
    for (int i = 0; i < int(NameFormat::Style::Count); ++i) {
        const auto style = NameFormat::Style(i);
        QList<QKeySequence> keys;
        if (style == NameFormat::Style::FullPath)
            keys << QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C);
        else if (style == NameFormat::Style::Name)
            keys << QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_C);
        QAction *a = action(m_copyNamesMenu, {}, [this, style] { copyNames(NameFormat::format(style)); });
        a->setShortcuts(keys);
        m_copyNameActions << a;
    }
    m_copyNamesMenu->addSeparator();
    m_copyNamesAs = action(m_copyNamesMenu, {}, [this] { copyNamesAs(); });
    m_copyNamesMenu->addSeparator();
    choice(m_copyNamesMenu, &TabOptions::copyNamesOneLine, QT_TR_NOOP("On One Line"));
    m_editMenu->addSeparator();
    m_copyToOther = action(m_editMenu, {QKeySequence(Qt::Key_F5)}, [this] { copySelection(false); });
    m_moveToOther = action(m_editMenu, {QKeySequence(Qt::Key_F6)}, [this] { copySelection(true); });
    m_copyTo = action(m_editMenu, {QKeySequence(Qt::SHIFT | Qt::Key_F5)}, [this] { copySelectionTo(false); });
    m_moveTo = action(m_editMenu, {QKeySequence(Qt::SHIFT | Qt::Key_F6)}, [this] { copySelectionTo(true); });
    m_editMenu->addSeparator();
    m_selectAll = action(m_editMenu, {QKeySequence::SelectAll}, [this] { activeTab()->selectAll(); });
    m_selectMenu = m_editMenu->addMenu(QString());
    m_selectFiles = action(m_selectMenu, {}, [this] { activeTab()->selectFiles(); });
    m_selectFolders = action(m_selectMenu, {}, [this] { activeTab()->selectFolders(); });
    // Escape itself is handled by the file list (see FolderTab), so that it
    // keeps working everywhere else; the menu only shows it.
    m_selectNone = action(m_selectMenu, {}, [this] { activeTab()->selectNone(); });
    m_invertSelection = action(m_selectMenu, {QKeySequence(Qt::CTRL | Qt::Key_I)}, [this] { activeTab()->invertSelection(); });
    m_selectMenu->addSeparator();
    m_reselect = action(m_selectMenu, {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R)}, [this] {
        if (!activeTab()->reselectPrevious())
            activeTab()->showError(tr("There's no earlier selection in this folder."));
    });
    m_selectMatching = action(m_selectMenu, {QKeySequence(Qt::CTRL | Qt::Key_S)}, [this] { selectMatching(); });
    m_editMenu->addSeparator();
    // SEL-11. Typing a name in the list starts it.
    m_findNext = action(m_editMenu, {QKeySequence(Qt::Key_F3)}, [this] { activeTab()->findStep(1); });
    m_findPrevious = action(m_editMenu, {QKeySequence(Qt::SHIFT | Qt::Key_F3)}, [this] { activeTab()->findStep(-1); });

    // View: how the list looks, and how the window is laid out.
    // View modes (VIEW-01), Ctrl+1 to Ctrl+7 in menu order.
    m_modeMenu = m_viewMenu->addMenu(QString());
    m_modeGroup = new QActionGroup(this);
    for (int m = 0; m < int(ViewFormat::Mode::Count); ++m) {
        QAction *a = action(m_modeMenu, {QKeySequence(Qt::CTRL | (Qt::Key_1 + m))},
                            [this, m] { activeTab()->setViewMode(ViewFormat::Mode(m)); });
        a->setCheckable(true);
        m_modeGroup->addAction(a);
        m_modeActions << a;
    }
    m_modeMenu->addSeparator();
    m_cycleMode = action(m_modeMenu, {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V)}, [this] {
        const int next = (int(activeTab()->format().mode) + 1) % int(ViewFormat::Mode::Count);
        activeTab()->setViewMode(ViewFormat::Mode(next));
    });
    m_columnsMenu = m_viewMenu->addMenu(QString());
    connect(m_columnsMenu, &QMenu::aboutToShow, this, &MainWindow::fillColumnsMenu);
    m_sortMenu = m_viewMenu->addMenu(QString());
    connect(m_sortMenu, &QMenu::aboutToShow, this, &MainWindow::fillSortMenu);
    m_defaultFormatMenu = m_viewMenu->addMenu(QString());
    m_saveFormat = action(m_defaultFormatMenu, {}, [this] {
        TabOptions options = m_options;
        options.defaultFormat = activeTab()->format().toString();
        setTabOptions(options);
    });
    m_resetFormat = action(m_defaultFormatMenu, {}, [this] {
        activeTab()->setFormat(ViewFormat::fromString(m_options.defaultFormat));
    });
    m_viewMenu->addSeparator();
    m_showHidden = action(m_viewMenu, {QKeySequence(Qt::CTRL | Qt::Key_H)}, [this] {
        TabOptions options = m_options;
        options.filter.showHidden = !options.filter.showHidden;
        setTabOptions(options);
    });
    m_showHidden->setCheckable(true);
    m_filterBar = action(m_viewMenu, {QKeySequence(Qt::CTRL | Qt::Key_F)}, [this] { activeTab()->openFilterBar(); });
    m_viewMenu->addSeparator();
    m_showTree = action(m_viewMenu, {QKeySequence(Qt::Key_F9)}, [this] { setTreeShown(!m_treeShown); });
    m_showTree->setCheckable(true);
    m_tabBarMenu = m_viewMenu->addMenu(QString());
    m_tabPositionGroup = new QActionGroup(this);
    const std::pair<QAction **, TabOptions::Position> positions[] = {
        {&m_tabsTop, TabOptions::Position::Top},
        {&m_tabsBottom, TabOptions::Position::Bottom},
        {&m_tabsLeft, TabOptions::Position::Left},
        {&m_tabsRight, TabOptions::Position::Right}};
    for (const auto &[slot, position] : positions) {
        *slot = action(m_tabBarMenu, {}, [this, position] {
            TabOptions options = m_options;
            options.position = position;
            setTabOptions(options);
        });
        (*slot)->setCheckable(true);
        m_tabPositionGroup->addAction(*slot);
    }
    m_showViewer = action(m_viewMenu, {QKeySequence(Qt::Key_F11)}, [this] { setViewerShown(!m_viewerShown); });
    m_showViewer->setCheckable(true);
    m_viewerLayoutMenu = m_viewMenu->addMenu(QString());
    auto *viewerGroup = new QActionGroup(this);
    m_viewerBeside = action(m_viewerLayoutMenu, {}, [this] { setViewerBelow(false); });
    m_viewerBelowAction = action(m_viewerLayoutMenu, {}, [this] { setViewerBelow(true); });
    for (QAction *a : {m_viewerBeside, m_viewerBelowAction}) {
        a->setCheckable(true);
        viewerGroup->addAction(a);
    }
    m_viewerSwitch = action(m_viewerLayoutMenu, {}, [this] { setViewerBelow(!m_viewerBelow); });
    m_viewerLayoutMenu->addSeparator();
    m_viewerLock = action(m_viewerLayoutMenu, {}, [this](bool on) { m_viewer->setLocked(on); });
    m_viewerLock->setCheckable(true);
    m_viewMenu->addSeparator();
    m_dualAction = action(m_viewMenu, {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D)},
                          [this] { setDual(!m_dual); });
    m_dualAction->setCheckable(true);
    m_dualLayoutMenu = m_viewMenu->addMenu(QString());
    m_layoutGroup = new QActionGroup(this);
    m_sideBySide = action(m_dualLayoutMenu, {}, [this] { setLayout(Layout::SideBySide); });
    m_stacked = action(m_dualLayoutMenu, {}, [this] { setLayout(Layout::Stacked); });
    for (QAction *a : {m_sideBySide, m_stacked}) {
        a->setCheckable(true);
        m_layoutGroup->addAction(a);
    }
    m_toggleLayout = action(m_dualLayoutMenu, {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L)}, [this] {
        setLayout(m_layout == Layout::SideBySide ? Layout::Stacked : Layout::SideBySide);
    });
    // Sizing the split from the keyboard (LST-07); the handle can also be
    // dragged, or double-clicked to even it out.
    m_dualLayoutMenu->addSeparator();
    m_evenSplit = action(m_dualLayoutMenu, {QKeySequence(Qt::ALT | Qt::Key_Equal)}, [this] { setSplitRatio(0.5); });
    auto resize = [this](double step) {
        // Grows or shrinks the active side, whichever one it is.
        setSplitRatio(splitRatio() + (m_active == 0 ? step : -step));
    };
    m_widenSide = action(m_dualLayoutMenu, {QKeySequence(Qt::ALT | Qt::Key_BracketRight)}, [resize] { resize(0.1); });
    m_narrowSide = action(m_dualLayoutMenu, {QKeySequence(Qt::ALT | Qt::Key_BracketLeft)}, [resize] { resize(-0.1); });
    m_lockMenu = m_viewMenu->addMenu(QString());
    m_lockGroup = new QActionGroup(this);
    m_unlock = action(m_lockMenu, {}, [this] { lockActiveSide(FileDisplay::Role::None); });
    m_lockSource = action(m_lockMenu, {}, [this] { lockActiveSide(FileDisplay::Role::Source); });
    m_lockDestination = action(m_lockMenu, {}, [this] { lockActiveSide(FileDisplay::Role::Destination); });
    for (QAction *a : {m_unlock, m_lockSource, m_lockDestination}) {
        a->setCheckable(true);
        m_lockGroup->addAction(a);
    }

    // Go: moving between folders and sides.
    m_back = action(m_goMenu, {QKeySequence(Qt::ALT | Qt::Key_Left), QKeySequence::Back},
                    [this] { activeTab()->goBack(); });
    m_forward = action(m_goMenu, {QKeySequence(Qt::ALT | Qt::Key_Right), QKeySequence::Forward},
                       [this] { activeTab()->goForward(); });
    m_up = action(m_goMenu, {QKeySequence(Qt::ALT | Qt::Key_Up), QKeySequence(Qt::Key_Backspace)},
                  [this] { activeTab()->goUp(); });
    m_root = action(m_goMenu, {QKeySequence(Qt::CTRL | Qt::Key_Backslash)}, [this] { activeTab()->goRoot(); });
    m_editPath = action(m_goMenu, {QKeySequence(Qt::CTRL | Qt::Key_L), QKeySequence(Qt::ALT | Qt::Key_D)},
                        [this] { activeTab()->editPath(); });
    m_goMenu->addSeparator();
    // Plain Tab is handled in eventFilter so it only switches sides from the
    // file list; this entry shows the key in the menu.
    m_otherSide = action(m_goMenu, {}, [this] {
        if (m_dual)
            m_displays[1 - m_active]->currentTab()->setFocus(Qt::TabFocusReason);
    });
    m_swapFolders = action(m_goMenu, {QKeySequence(Qt::CTRL | Qt::Key_U)}, [this] { swapFolders(); });
    m_folderToOtherSide =
        action(m_goMenu, {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O)}, [this] { showFolderOnOtherSide(); });
    m_goMenu->addSeparator();
    m_focusTree = action(m_goMenu, {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E)}, [this] { focusTree(); });

    // Tabs: every tab command, and what the tab's own menu offers.
    m_newTab = action(m_tabsMenu, {QKeySequence(Qt::CTRL | Qt::Key_T)}, [this] {
        FileDisplay *display = m_displays[m_active];
        const FolderTab *current = display->currentTab();
        display->addTab(current ? current->currentPath() : QDir::homePath())->setFocus();
    });
    m_duplicateTab = action(m_tabsMenu, {}, [this] {
        FileDisplay *display = m_displays[m_active];
        if (FolderTab *copy = display->duplicateTab(display->currentIndex()))
            copy->setFocus();
    });
    m_closeTab = action(m_tabsMenu, {QKeySequence(Qt::CTRL | Qt::Key_W)},
                        [this] { m_displays[m_active]->closeTab(m_displays[m_active]->currentIndex()); });
    // Reopening the last one is the first entry of the list.
    m_closedMenu = m_tabsMenu->addMenu(QString());
    m_reopenTab = action(m_closedMenu, {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T)}, [this] { reopenClosedTab(); });
    connect(m_closedMenu, &QMenu::aboutToShow, this, &MainWindow::fillClosedTabsMenu);
    m_tabsMenu->addSeparator();
    m_nextTab = action(m_tabsMenu, {QKeySequence(Qt::CTRL | Qt::Key_PageDown)},
                       [this] { m_displays[m_active]->cycleTab(1); });
    m_previousTab = action(m_tabsMenu, {QKeySequence(Qt::CTRL | Qt::Key_PageUp)},
                           [this] { m_displays[m_active]->cycleTab(-1); });
    // Ctrl+Tab opens the switcher, or simply steps through the tabs when the
    // switcher is turned off.
    m_switchTab = action(m_tabsMenu, {QKeySequence(Qt::CTRL | Qt::Key_Tab)}, [this] {
        if (m_options.ctrlTabSwitcher)
            openTabSwitcher(1);
        else
            m_displays[m_active]->cycleTab(1);
    });
    m_switchTabBack = action(nullptr, {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab)}, [this] {
        if (m_options.ctrlTabSwitcher)
            openTabSwitcher(-1);
        else
            m_displays[m_active]->cycleTab(-1);
    });
    m_tabList = action(m_tabsMenu, {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A)},
                       [this] { m_displays[m_active]->showTabList(); });
    m_tabsMenu->addSeparator();
    m_renameTab = action(m_tabsMenu, {}, [this] { m_displays[m_active]->renameTab(activeTab()); });
    m_tabLockMenu = m_tabsMenu->addMenu(QString());
    connect(m_tabLockMenu, &QMenu::aboutToShow, this,
            [this] { m_displays[m_active]->fillLockMenu(m_tabLockMenu, activeTab()); });
    m_tabColorMenu = m_tabsMenu->addMenu(QString());
    connect(m_tabColorMenu, &QMenu::aboutToShow, this,
            [this] { m_displays[m_active]->fillColorMenu(m_tabColorMenu, activeTab()); });
    m_tabHome = action(m_tabsMenu, {QKeySequence(Qt::ALT | Qt::Key_Home)}, [this] { activeTab()->goHome(); });
    m_makeHome = action(m_tabsMenu, {}, [this] {
        activeTab()->setHomePath(activeTab()->currentPath());
        updateChrome();
    });
    m_linkTab = action(m_tabsMenu, {}, [this] { toggleLink(m_active, m_displays[m_active]->currentIndex()); });
    m_moveTab = action(m_tabsMenu, {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M)},
                       [this] { moveTabToOtherSide(m_active, m_displays[m_active]->currentIndex()); });
    m_tabsMenu->addSeparator();
    m_groupsMenu = m_tabsMenu->addMenu(QString());
    m_saveGroup = action(m_groupsMenu, {}, [this] { saveTabGroupAs(); });
    m_groupsMenu->addSeparator();
    m_openGroupMenu = m_groupsMenu->addMenu(QString());
    m_addGroupMenu = m_groupsMenu->addMenu(QString());
    m_deleteGroupMenu = m_groupsMenu->addMenu(QString());
    connect(m_groupsMenu, &QMenu::aboutToShow, this, &MainWindow::fillTabGroupMenus);
    m_groupsMenu->menuAction()->setVisible(m_settings);

    // FAV-01. The favourites themselves are put in each time it opens.
    m_favoritesMenu->setToolTipsVisible(true);
    m_addFavorite = action(m_favoritesMenu, {QKeySequence(Qt::CTRL | Qt::Key_B)}, [this] { addFavorite(); });
    m_editFavorites = action(m_favoritesMenu, {}, [this] { editFavorites(); });
    m_showFavoritesBar = action(m_favoritesMenu, {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B)}, [this] {
        TabOptions options = m_options;
        options.favoritesBar = !options.favoritesBar;
        setTabOptions(options);
    });
    m_showFavoritesBar->setCheckable(true);
    // Aliases are saved places too, reached by typing them (NAV-07).
    m_favoritesMenu->addSeparator();
    m_aliasMenu = m_favoritesMenu->addMenu(QString());
    connect(m_aliasMenu, &QMenu::aboutToShow, this, &MainWindow::fillAliasMenu);
    connect(m_favoritesMenu, &QMenu::aboutToShow, this, &MainWindow::fillFavoritesMenu);

    // Windows (LST-01, LST-04, LST-20, LST-21). They need the manager; a
    // window made on its own (as in tests) leaves most of these off.
    m_duplicateWindow = action(m_windowMenu, {}, [this] {
        if (m_manager)
            m_manager->duplicateWindow(this);
    });
    m_closedWindowsMenu = m_windowMenu->addMenu(QString());
    m_reopenWindow = action(m_closedWindowsMenu, {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N)}, [this] {
        if (m_manager)
            m_manager->reopenClosedWindow();
    });
    connect(m_closedWindowsMenu, &QMenu::aboutToShow, this, &MainWindow::fillClosedWindowsMenu);
    m_layoutsMenu = m_windowMenu->addMenu(QString());
    m_saveLayout = action(m_layoutsMenu, {}, [this] { saveLayoutAs(); });
    m_layoutsMenu->addSeparator();
    m_openLayoutMenu = m_layoutsMenu->addMenu(QString());
    m_deleteLayoutMenu = m_layoutsMenu->addMenu(QString());
    connect(m_layoutsMenu, &QMenu::aboutToShow, this, &MainWindow::fillLayoutMenus);
    m_layoutsMenu->menuAction()->setVisible(m_manager && m_settings);
    m_windowMenu->addSeparator();
    m_saveDefault = action(m_windowMenu, {}, [this] {
        if (m_manager)
            m_manager->saveAsDefault(this);
        else
            saveSettings();
    });

    // Settings: the choices that stay set, each applying at once and
    // remembered.
    m_whenExistsMenu = m_settingsMenu->addMenu(QString());
    auto *policies = new QActionGroup(this);
    for (int i = 0; i < int(WhenExists::Count); ++i) {
        QAction *a = m_whenExistsMenu->addAction(QString());
        a->setCheckable(true);
        policies->addAction(a);
        m_whenExistsActions << a;
        connect(a, &QAction::triggered, this, [this, i] {
            TabOptions options = m_options;
            options.whenExists = WhenExistsNames::id(WhenExists(i));
            setTabOptions(options);
        });
    }
    m_whenExistsMenu->addSeparator();
    choice(m_whenExistsMenu, &TabOptions::askBeforeMerging, QT_TR_NOOP("Ask Before Merging Folders"));
    choice(m_whenExistsMenu, &TabOptions::askBeforeReplacingReadOnly, QT_TR_NOOP("Ask Before Replacing Read-Only Files"));
    m_dropMenu = m_settingsMenu->addMenu(QString());
    auto *drops = new QActionGroup(this);
    for (const QString &id : FileDrag::behaviours()) {
        QAction *a = m_dropMenu->addAction(QString());
        a->setCheckable(true);
        a->setData(id);
        drops->addAction(a);
        m_dropActions << a;
        connect(a, &QAction::triggered, this, [this, id] {
            TabOptions options = m_options;
            options.dropAction = id;
            setTabOptions(options);
        });
    }
    m_deleteOptionsMenu = m_settingsMenu->addMenu(QString());
    m_useTrash = choice(m_deleteOptionsMenu, &TabOptions::useTrash, QT_TR_NOOP("Delete Moves to Trash"));
    m_deleteOptionsMenu->addSeparator();
    choice(m_deleteOptionsMenu, &TabOptions::confirmTrash, QT_TR_NOOP("Ask Before Moving to Trash"));
    choice(m_deleteOptionsMenu, &TabOptions::confirmDelete, QT_TR_NOOP("Ask Before Deleting Permanently"));
    choice(m_deleteOptionsMenu, &TabOptions::confirmEachFile, QT_TR_NOOP("Ask About Each File When Deleting Permanently"));
    choice(m_deleteOptionsMenu, &TabOptions::confirmEachFolder, QT_TR_NOOP("Ask About Each Folder When Deleting Permanently"));
    choice(m_deleteOptionsMenu, &TabOptions::deleteReadOnlyWithoutAsking, QT_TR_NOOP("Delete Read-Only Files Without Asking"));
    m_deleteOptionsMenu->addSeparator();
    choice(m_deleteOptionsMenu, &TabOptions::selectNextAfterDelete, QT_TR_NOOP("Select the Next Item After Deleting"));
    m_renameSelectMenu = m_settingsMenu->addMenu(QString());
    auto *renameSelections = new QActionGroup(this);
    for (int i = 0; i < 5; ++i) {
        QAction *a = m_renameSelectMenu->addAction(QString());
        a->setCheckable(true);
        renameSelections->addAction(a);
        m_renameSelectActions << a;
        connect(a, &QAction::triggered, this, [this, i] {
            TabOptions options = m_options;
            options.renameSelect = TabOptions::renameSelectId(RenameDelegate::Selection(i));
            setTabOptions(options);
        });
    }
    m_settingsMenu->addSeparator();
    m_findMenu = m_settingsMenu->addMenu(QString());
    choice(m_findMenu, &TabOptions::findAnywhere, QT_TR_NOOP("Anywhere in the Name"));
    choice(m_findMenu, &TabOptions::findIgnoreAccents, QT_TR_NOOP("Ignore Accents"));
    choice(m_findMenu, &TabOptions::findIgnoreExtensions, QT_TR_NOOP("Ignore Extensions"));
    choice(m_findMenu, &TabOptions::findPreferShorter, QT_TR_NOOP("Prefer Shorter Names"));
    choice(m_findMenu, &TabOptions::findRepeatFirst, QT_TR_NOOP("Repeating a Letter Steps Through Names"));
    m_findMenu->addSeparator();
    choice(m_findMenu, &TabOptions::findHighlight, QT_TR_NOOP("Highlight Matches"));
    choice(m_findMenu, &TabOptions::findScrollMarks, QT_TR_NOOP("Mark Matches on the Scroll Bar"));
    choice(m_findMenu, &TabOptions::findKeepHighlights, QT_TR_NOOP("Keep Highlights Until Escape"));
    m_findMenu->addSeparator();
    choice(m_findMenu, &TabOptions::findEnterOpens, QT_TR_NOOP("Enter Opens the Found Item"));
    m_filterBarMenu = m_settingsMenu->addMenu(QString());
    choice(m_filterBarMenu, &TabOptions::filterRealTime, QT_TR_NOOP("Filter While Typing"));
    choice(m_filterBarMenu, &TabOptions::filterSelectFirst, QT_TR_NOOP("Select the First Match"));
    choice(m_filterBarMenu, &TabOptions::filterClearOnNavigate, QT_TR_NOOP("Clear When Changing Folders"));
    m_completionMenu = m_settingsMenu->addMenu(QString());
    choice(m_completionMenu, &TabOptions::completePaths, QT_TR_NOOP("Complete Paths"));
    m_completionMenu->addSeparator();
    choice(m_completionMenu, &TabOptions::completePopup, QT_TR_NOOP("Show a List"));
    choice(m_completionMenu, &TabOptions::completeOnArrows, QT_TR_NOOP("Only When an Arrow Key Is Pressed"));
    choice(m_completionMenu, &TabOptions::completeLocalOnly, QT_TR_NOOP("Local Drives Only"));
    m_settingsMenu->addSeparator();
    m_tabSettingsMenu = m_settingsMenu->addMenu(QString());
    m_optNewTabButton = option(m_tabSettingsMenu, &TabOptions::newTabButton);
    m_optCloseButtons = option(m_tabSettingsMenu, &TabOptions::closeButtons);
    m_optNextToCurrent = option(m_tabSettingsMenu, &TabOptions::openNextToCurrent);
    m_optFindExisting = option(m_tabSettingsMenu, &TabOptions::findExistingTab);
    m_tabSettingsMenu->addSeparator();
    m_optDoubleClickCloses = option(m_tabSettingsMenu, &TabOptions::doubleClickCloses);
    m_optCloseWithLastTab = option(m_tabSettingsMenu, &TabOptions::closeWindowWithLastTab);
    m_tabSettingsMenu->addSeparator();
    m_optSwitcher = option(m_tabSettingsMenu, &TabOptions::ctrlTabSwitcher);
    m_viewerSettingsMenu = m_settingsMenu->addMenu(QString());
    choice(m_viewerSettingsMenu, &TabOptions::viewerKeepLast, QT_TR_NOOP("Keep the Last Item When Nothing Is Selected"));
    choice(m_viewerSettingsMenu, &TabOptions::viewerWrap, QT_TR_NOOP("Wrap Long Lines of Text"));
    m_windowSettingsMenu = m_settingsMenu->addMenu(QString());
    m_rememberOnClose = action(m_windowSettingsMenu, {}, [this](bool on) {
        if (m_manager)
            m_manager->setRemembersOnClose(on);
    });
    m_rememberOnClose->setCheckable(true);
    m_pairWindows = action(m_windowSettingsMenu, {}, [this](bool on) {
        if (m_manager)
            m_manager->setPairsWindows(on);
    });
    m_pairWindows->setCheckable(true);
    m_windowSettingsMenu->menuAction()->setVisible(m_manager);
    m_settingsMenu->addSeparator();
    m_treeOptions = action(m_settingsMenu, {}, [this] { editTreeOptions(); });
    m_globalFilters = action(m_settingsMenu, {}, [this] { editGlobalFilters(); });

    for (QAction *a : {m_newWindow, m_duplicateWindow})
        a->setVisible(m_manager);
    m_closedWindowsMenu->menuAction()->setVisible(m_manager);

    // CUST-17: every key, listed from these menus.
    // ? opens it too, from the file list (see eventFilter).
    m_shortcuts = action(m_helpMenu, {QKeySequence(Qt::Key_F1)}, [this] { showShortcuts(); });
    m_helpMenu->addSeparator();
    m_about = action(m_helpMenu, {}, [this] { showAbout(); });
}

void MainWindow::setTabOptions(const TabOptions &options) {
    if (m_manager) {
        m_manager->setTabOptions(options);
        return;
    }
    if (m_settings) {
        options.save(*m_settings);
        m_settings->sync();
    }
    applyTabOptions(options);
}

void MainWindow::applyTabOptions(const TabOptions &options) {
    m_options = options;
    FolderAliases::setUser(options.aliases);
    for (FileDisplay *display : m_displays) {
        display->setOptions(options);
        display->setLastTabClosable(options.closeWindowWithLastTab);
    }
    // The bar's favourites show in the tree as a branch of their own.
    QList<Favorite> inTree = options.favorites.items;
    if (!options.favorites.bar.isEmpty())
        inTree.prepend(Favorite::branch(tr("Favourites Bar"), options.favorites.bar));
    for (FolderTree *tree : m_trees)
        tree->setOptions(options.tree, options.filter, inTree);
    m_favoritesBar->setFavorites(options.favorites.bar);
    m_favoritesBar->setVisible(options.favoritesBar);
    m_viewer->setWrapText(options.viewerWrap);
    placeTrees();
    updateOptionActions();
    updateChrome();
}

void MainWindow::setWindowRole(FileDisplay::Role role, bool locked) {
    if (role == m_windowRole && locked == m_windowLocked)
        return;
    m_windowRole = role;
    m_windowLocked = locked;
    updateRoles();
}

FolderTab *MainWindow::partnerTab() const {
    if (m_dual)
        return m_displays[1 - m_active]->currentTab();
    if (!m_manager)
        return nullptr;
    MainWindow *other = nullptr;
    if (m_manager->sourceWindow() == this)
        other = m_manager->destinationWindow();
    else if (m_manager->destinationWindow() == this)
        other = m_manager->sourceWindow();
    return other && other != this && m_windowRole != FileDisplay::Role::None ? other->activeTab() : nullptr;
}

FolderTab *MainWindow::otherSideTab() {
    if (FolderTab *partner = partnerTab())
        return partner;
    setDual(true);
    return partnerTab();
}

void MainWindow::swapFolders() {
    FolderTab *here = activeTab();
    FolderTab *there = partnerTab();
    if (!here || !there)
        return;
    const QString herePath = here->currentPath();
    const QString therePath = there->currentPath();
    here->openPath(therePath);
    there->openPath(herePath);
}

void MainWindow::showFolderOnOtherSide() {
    FolderTab *here = activeTab();
    if (FolderTab *there = otherSideTab())
        there->openPath(here->currentPath());
}

void MainWindow::openInOtherSide() {
    FolderTab *here = activeTab();
    const QString path = here->highlightedFolder();
    if (path.isEmpty()) {
        here->showError(tr("Highlight a folder to open it on the other side."));
        return;
    }
    if (FolderTab *there = otherSideTab())
        there->openPath(path);
}

double MainWindow::splitRatio() const {
    const QList<int> sizes = m_splitter->sizes();
    const int total = sizes.value(0) + sizes.value(1);
    return total > 0 ? double(sizes.value(0)) / total : 0.5;
}

void MainWindow::setSplitRatio(double first) {
    if (!m_dual)
        return;
    first = qBound(0.1, first, 0.9);
    const QList<int> sizes = m_splitter->sizes();
    int total = sizes.value(0) + sizes.value(1);
    if (total <= 0)
        total = m_layout == Layout::SideBySide ? m_splitter->width() : m_splitter->height();
    const int a = qRound(total * first);
    m_splitter->setSizes({a, total - a});
}

void MainWindow::fillClosedWindowsMenu() {
    m_closedWindowsMenu->clear();
    m_closedWindowsMenu->addAction(m_reopenWindow);
    if (!m_manager || m_manager->closedWindows().isEmpty())
        return;
    m_closedWindowsMenu->addSeparator();
    const QList<ListerState> &closed = m_manager->closedWindows();
    for (int i = 0; i < closed.size(); ++i) {
        const QString path = closed.at(i).activePath();
        QString name = QFileInfo(path).fileName();
        if (name.isEmpty())
            name = path;
        const QString text = tr("%1 (%n tabs)", "", closed.at(i).tabCount()).arg(name.replace('&', "&&"));
        QAction *a = m_closedWindowsMenu->addAction(text + '\t' + path);
        connect(a, &QAction::triggered, this, [this, i] { m_manager->reopenClosedWindow(i); });
    }
}

void MainWindow::updateOptionActions() {
    for (QAction *a : std::as_const(m_dropActions))
        a->setChecked(a->data().toString() == m_options.dropAction);
    m_whenExistsActions.at(int(WhenExistsNames::fromId(m_options.whenExists)))->setChecked(true);
    for (const OptionAction &o : std::as_const(m_optionActions))
        o.action->setChecked(m_options.*(o.field));
    m_renameSelectActions.at(int(m_options.renameSelection()))->setChecked(true);
    m_showHidden->setChecked(m_options.filter.showHidden);
    m_showFavoritesBar->setChecked(m_options.favoritesBar);
    // The two delete commands swap places when Delete deletes for good.
    m_trash->setText(m_options.useTrash ? tr("Move to Trash") : tr("Delete Permanently…"));
    m_deleteForever->setText(m_options.useTrash ? tr("Delete Permanently…") : tr("Move to Trash"));
    QAction *const positions[] = {m_tabsTop, m_tabsBottom, m_tabsLeft, m_tabsRight};
    positions[int(m_options.position)]->setChecked(true);
    m_optNewTabButton->setChecked(m_options.newTabButton);
    m_optNextToCurrent->setChecked(m_options.openNextToCurrent);
    m_optCloseButtons->setChecked(m_options.closeButtons);
    m_optDoubleClickCloses->setChecked(m_options.doubleClickCloses);
    m_optCloseWithLastTab->setChecked(m_options.closeWindowWithLastTab);
    m_optFindExisting->setChecked(m_options.findExistingTab);
    m_optSwitcher->setChecked(m_options.ctrlTabSwitcher);
}

void MainWindow::closeLastTab(int side) {
    if (!m_options.closeWindowWithLastTab)
        return;
    if (!m_dual) {
        close();
        return;
    }
    // In a dual display the side goes away and the other carries on.
    activateSide(1 - side);
    setDual(false);
    m_displays[side]->closeAllTabs();
    if (FolderTab *tab = activeTab())
        tab->setFocus();
}

FolderTab *MainWindow::reopenClosedTab(int i) {
    if (i < 0 || i >= m_closedTabs.size())
        return nullptr;
    const ClosedTab closed = m_closedTabs.takeAt(i);
    // Back on its own side if that side is showing, else on the active one.
    const int side = closed.side >= 0 && closed.side < 2 && !m_displays[closed.side]->isHidden() &&
                             m_displays[closed.side]->tabCount() > 0
                         ? closed.side : m_active;
    FolderTab *tab = m_displays[side]->restoreTab(closed.tab, closed.index);
    activateSide(side);
    tab->setFocus();
    updateChrome();
    return tab;
}

void MainWindow::fillClosedTabsMenu() {
    m_closedMenu->clear();
    m_closedMenu->addAction(m_reopenTab);
    if (!m_closedTabs.isEmpty())
        m_closedMenu->addSeparator();
    for (int i = 0; i < m_closedTabs.size(); ++i) {
        const QString &path = m_closedTabs.at(i).tab.path;
        QString name = QFileInfo(path).fileName();
        if (name.isEmpty())
            name = path;
        QAction *a = m_closedMenu->addAction(name.replace('&', "&&") + '\t' + path);
        connect(a, &QAction::triggered, this, [this, i] { reopenClosedTab(i); });
    }
}

void MainWindow::noteActiveTab() {
    FolderTab *tab = activeTab();
    if (!tab)
        return;
    m_recentTabs.removeIf([tab](const QPointer<FolderTab> &p) { return !p || p == tab; });
    m_recentTabs.prepend(tab);
}

QList<FolderTab *> MainWindow::tabsByRecentUse() const {
    QList<FolderTab *> tabs;
    auto showing = [this](FolderTab *tab) {
        const int side = sideOf(tab);
        return side >= 0 && !m_displays[side]->isHidden();
    };
    for (const QPointer<FolderTab> &tab : m_recentTabs)
        if (tab && showing(tab) && !tabs.contains(tab.data()))
            tabs << tab.data();
    // Tabs never used yet follow, in the order they sit on screen.
    for (FileDisplay *display : m_displays)
        if (!display->isHidden())
            for (int i = 0; i < display->tabCount(); ++i)
                if (!tabs.contains(display->tab(i)))
                    tabs << display->tab(i);
    return tabs;
}

void MainWindow::openTabSwitcher(int step) {
    const QList<FolderTab *> tabs = tabsByRecentUse();
    if (tabs.size() < 2)
        return;
    m_switcherTabs.clear();
    QList<TabSwitcher::Entry> entries;
    for (FolderTab *tab : tabs) {
        m_switcherTabs << tab;
        QString side;
        if (m_dual) {
            const bool first = sideOf(tab) == 0;
            side = m_layout == Layout::SideBySide ? (first ? tr("Left") : tr("Right"))
                                                  : (first ? tr("Top") : tr("Bottom"));
        }
        entries << TabSwitcher::Entry{tab->title(), tab->currentPath(), side};
    }
    // The first step lands on the previous tab, so a quick Ctrl+Tab flips
    // between the last two.
    m_switcher->open(entries, step > 0 ? 1 : int(entries.size()) - 1);
}


void MainWindow::saveSettings() {
    if (!m_settings)
        return;
    m_settings->remove("lister");
    state().save(*m_settings, "lister");
    m_settings->sync();
}

void MainWindow::setDual(bool dual) {
    if (dual == m_dual)
        return;
    m_dual = dual;
    const int other = 1 - m_active;
    if (dual) {
        if (m_displays[other]->tabCount() == 0)
            m_displays[other]->addTab(activeTab()->currentPath());
        showSide(other, true);
        const int extent = m_layout == Layout::SideBySide ? m_splitter->width() : m_splitter->height();
        m_splitter->setSizes({extent / 2, extent / 2});
    } else {
        // The side the user is working in stays; the other keeps its tabs,
        // hidden, for when the dual display comes back.
        m_splitterState = m_splitter->saveState();
        showSide(other, false);
        m_lockedSide = -1;
        m_lockedRole = FileDisplay::Role::None;
    }
    updateRoles();
    placeTrees();
    if (m_manager)
        m_manager->windowDualChanged(this);
}

void MainWindow::setLayout(Layout layout) {
    m_layout = layout;
    m_splitter->setOrientation(layout == Layout::SideBySide ? Qt::Horizontal : Qt::Vertical);
    if (m_dual) {
        const int extent = layout == Layout::SideBySide ? m_splitter->width() : m_splitter->height();
        m_splitter->setSizes({extent / 2, extent / 2});
    }
    placeTrees();
    updateChrome();
}

void MainWindow::assignRoles(bool dual, int active, int lockedSide, FileDisplay::Role lockedRole, int *source,
                             int *destination) {
    if (!dual) {
        *source = active;
        *destination = -1;
        return;
    }
    if (lockedSide >= 0 && lockedRole == FileDisplay::Role::Source)
        *source = lockedSide;
    else if (lockedSide >= 0 && lockedRole == FileDisplay::Role::Destination)
        *source = 1 - lockedSide;
    else
        *source = active;
    *destination = 1 - *source;
}

void MainWindow::activateSide(int side) {
    if (side < 0 || side > 1 || side == m_active || m_displays[side]->isHidden())
        return;
    m_active = side;
    updateRoles();
    noteActiveTab();
}

void MainWindow::lockActiveSide(FileDisplay::Role role) {
    if (!m_dual) {
        // A single display locks the whole window's role among the windows.
        if (m_manager)
            m_manager->lockWindow(this, role);
        return;
    }
    m_lockedSide = role == FileDisplay::Role::None ? -1 : m_active;
    m_lockedRole = role;
    updateRoles();
}

void MainWindow::moveTabToOtherSide(int fromSide, int index) {
    if (!m_dual || fromSide < 0 || fromSide > 1 || m_displays[fromSide]->tabCount() <= 1)
        return;
    if (FolderTab *moving = m_displays[fromSide]->tab(index))
        moving->setLinkedTab(nullptr); // linked tabs belong on opposite sides
    FolderTab *tab = m_displays[fromSide]->takeTab(index);
    if (!tab)
        return;
    m_displays[1 - fromSide]->insertTab(tab);
    activateSide(1 - fromSide);
    tab->setFocus();
}

FolderTab *MainWindow::activeTab() const {
    return m_displays[m_active]->currentTab();
}

FolderTab *MainWindow::sourceTab() const {
    // A single window that is the destination takes things from its paired
    // window (IDEA-17).
    if (!m_dual && m_windowRole == FileDisplay::Role::Destination)
        if (FolderTab *partner = partnerTab())
            return partner;
    return m_displays[m_source]->currentTab();
}

FolderTab *MainWindow::destinationTab() const {
    if (m_destination >= 0)
        return m_displays[m_destination]->currentTab();
    // A single window that is the source sends things to its paired window.
    if (m_windowRole == FileDisplay::Role::Source)
        return partnerTab();
    return m_windowRole == FileDisplay::Role::Destination && partnerTab() ? activeTab() : nullptr;
}

int MainWindow::sideOf(const QWidget *widget) const {
    for (int side = 0; side < 2; ++side)
        if (widget && (widget == m_panels[side] || m_panels[side]->isAncestorOf(widget)))
            return side;
    return -1;
}

void MainWindow::updateRoles() {
    assignRoles(m_dual, m_active, m_lockedSide, m_lockedRole, &m_source, &m_destination);
    for (int side = 0; side < 2; ++side) {
        // A single display has no destination, so it shows no role at all.
        // Paired with another window, it shows the window's role instead.
        FileDisplay::Role role = FileDisplay::Role::None;
        bool locked = false;
        if (m_dual) {
            role = side == m_source ? FileDisplay::Role::Source : FileDisplay::Role::Destination;
            locked = side == m_lockedSide;
        } else if (side == m_active) {
            role = m_windowRole;
            locked = m_windowLocked;
        }
        m_displays[side]->setRole(role, locked);
        m_displays[side]->setCanMoveTabs(m_dual);
    }
    updateChrome();
}

void MainWindow::updateChrome() {
    FolderTab *tab = activeTab();
    FolderTab *source = m_displays[m_source]->currentTab();
    if (!tab || !source)
        return;
    setWindowTitle(QStringLiteral("%1 — CargoHatch").arg(source->title()));
    m_status->setText(tab->statusText());
    if (tab->errorId() != m_shownError) {
        m_shownError = tab->errorId();
        m_errorTimer->stop();
        if (m_shownError) {
            // An earlier message would cover the error up.
            statusBar()->clearMessage();
            m_errorTimer->start();
        }
    }
    syncTrees();
    m_showTree->setChecked(m_treeShown);
    m_showViewer->setChecked(m_viewerShown);
    updatePreview();
    m_viewerBeside->setChecked(!m_viewerBelow);
    m_viewerBelowAction->setChecked(m_viewerBelow);
    m_viewerLock->setChecked(m_viewer->isLocked());

    const FileDisplay *display = m_displays[m_active];
    m_closeTab->setEnabled((display->tabCount() > 1 || m_options.closeWindowWithLastTab) && !tab->isLocked());
    m_reopenTab->setEnabled(!m_closedTabs.isEmpty());
    m_closedMenu->setEnabled(!m_closedTabs.isEmpty());
    m_moveTab->setEnabled(m_dual && display->tabCount() > 1);
    m_back->setEnabled(tab->history().canGoBack());
    m_forward->setEnabled(tab->history().canGoForward());
    m_up->setEnabled(tab->currentPath() != "/");
    m_nextTab->setEnabled(display->tabCount() > 1);
    m_previousTab->setEnabled(display->tabCount() > 1);
    const int allTabs = display->tabCount() + (m_dual ? m_displays[1 - m_active]->tabCount() : 0);
    m_switchTab->setEnabled(allTabs > 1);
    m_switchTabBack->setEnabled(allTabs > 1);
    m_tabList->setEnabled(display->tabCount() > 1);
    m_otherSide->setEnabled(m_dual);
    const bool selected = tab->selectedCount() > 0;
    const bool items = tab->itemCount() > 0;
    for (QAction *a : {m_cut, m_copy, m_duplicate, m_trash, m_deleteForever})
        a->setEnabled(selected);
    updateTransferActions();
    m_rename->setEnabled(items);
    for (QAction *a : {m_selectAll, m_selectFiles, m_selectFolders, m_invertSelection, m_selectMatching})
        a->setEnabled(items);
    m_selectNone->setEnabled(selected);
    const ViewFormat::Mode mode = tab->format().mode;
    m_modeActions.at(int(mode))->setChecked(true);
    m_columnsMenu->setEnabled(mode == ViewFormat::Mode::Details || mode == ViewFormat::Mode::Power);
    m_tabHome->setEnabled(!tab->homePath().isEmpty() && tab->homePath() != tab->currentPath());
    m_makeHome->setEnabled(tab->homePath() != tab->currentPath());
    m_linkTab->setText(tab->linkedTab() ? tr("Unlink from Other Side") : tr("Link with Other Side's Current Tab"));
    m_linkTab->setEnabled(m_dual || tab->linkedTab());
    m_open->setEnabled(selected);
    m_swapFolders->setEnabled(partnerTab());
    for (QAction *a : {m_evenSplit, m_widenSide, m_narrowSide})
        a->setEnabled(m_dual);
    if (m_manager) {
        m_reopenWindow->setEnabled(!m_manager->closedWindows().isEmpty());
        m_closedWindowsMenu->setEnabled(!m_manager->closedWindows().isEmpty());
        m_rememberOnClose->setChecked(m_manager->remembersOnClose());
        m_pairWindows->setChecked(m_manager->pairsWindows());
    }

    m_dualAction->setChecked(m_dual);
    (m_layout == Layout::SideBySide ? m_sideBySide : m_stacked)->setChecked(true);
    // A single window paired with another can be locked into its role too.
    m_lockMenu->setEnabled(m_dual || m_windowRole != FileDisplay::Role::None);
    const bool locked = m_dual ? m_lockedSide >= 0 : m_windowLocked;
    const FileDisplay::Role lockedRole = m_dual ? m_lockedRole : m_windowRole;
    if (!locked)
        m_unlock->setChecked(true);
    else
        (lockedRole == FileDisplay::Role::Source ? m_lockSource : m_lockDestination)->setChecked(true);
}

void MainWindow::updateTransferActions() {
    // These act on the source, which a lock can make the other side, or the
    // paired window.
    FolderTab *source = sourceTab();
    for (QAction *a : {m_copyToOther, m_moveToOther, m_copyTo, m_moveTo})
        a->setEnabled(source && source->selectedCount() > 0);
    const bool here = source == activeTab();
    if (here) {
        m_copyToOther->setText(tr("Copy to Other Side"));
        m_moveToOther->setText(tr("Move to Other Side"));
        m_copyTo->setText(tr("Copy To…"));
        m_moveTo->setText(tr("Move To…"));
    } else if (m_dual) {
        m_copyToOther->setText(tr("Copy from Source Side"));
        m_moveToOther->setText(tr("Move from Source Side"));
        m_copyTo->setText(tr("Copy from Source Side To…"));
        m_moveTo->setText(tr("Move from Source Side To…"));
    } else {
        m_copyToOther->setText(tr("Copy from Source Window"));
        m_moveToOther->setText(tr("Move from Source Window"));
        m_copyTo->setText(tr("Copy from Source Window To…"));
        m_moveTo->setText(tr("Move from Source Window To…"));
    }
    // What is selected here is what the paired destination window would
    // copy.
    if (!m_dual && m_windowRole == FileDisplay::Role::Source && m_manager)
        if (MainWindow *other = m_manager->destinationWindow(); other && other != this && !other->m_dual)
            other->updateTransferActions();
}

void MainWindow::setErrorTimeout(int ms) {
    m_errorTimer->setInterval(ms);
}

// Whether `event` is the start of the user doing something in a window, before
// anything has come of it. A key or button press itself won't do: those are
// offered to a widget's parents too, after the widget may have shown an error.
static bool startsUserAction(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress)
        return watched->isWindowType();
    if (event->type() != QEvent::ShortcutOverride)
        return false;
    switch (static_cast<QKeyEvent *>(event)->key()) {
    case Qt::Key_Control:
    case Qt::Key_Shift:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_Meta:
        return false;
    default:
        return true;
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (startsUserAction(watched, event)) {
        const bool here = watched->isWidgetType() ? static_cast<QWidget *>(watched)->window() == this
                                                  : watched == windowHandle();
        if (FolderTab *tab = here ? activeTab() : nullptr)
            tab->clearError();
    }
    if (!watched->isWidgetType())
        return QMainWindow::eventFilter(watched, event);
    auto *widget = static_cast<QWidget *>(watched);
    if (widget->window() != this)
        return QMainWindow::eventFilter(watched, event);

    if (event->type() == QEvent::KeyRelease && m_switcher->isVisible() &&
        static_cast<QKeyEvent *>(event)->key() == Qt::Key_Control) {
        // Ctrl was let go before the switcher had the keyboard.
        m_switcher->commit();
        return true;
    }
    if (event->type() == QEvent::MouseButtonDblClick && widget == m_splitter->handle(1)) {
        setSplitRatio(0.5);
        return true;
    }
    if (event->type() == QEvent::MouseButtonPress) {
        const int side = sideOf(widget);
        if (side >= 0)
            activateSide(side);
    } else if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->text() == QStringLiteral("?") &&
               qobject_cast<QAbstractItemView *>(widget) && !qobject_cast<FolderTreeView *>(widget) && sideOf(widget) >= 0) {
        // Read from the typed text, so it is ? on any keyboard layout.
        showShortcuts();
        return true;
    } else if (event->type() == QEvent::KeyPress && m_dual && !qobject_cast<FolderTreeView *>(widget)) {
        // Tab in a file list jumps to the other side's list, the usual
        // dual-pane habit, instead of walking through every button.
        auto *key = static_cast<QKeyEvent *>(event);
        if ((key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab) && key->modifiers() == Qt::NoModifier &&
            qobject_cast<QAbstractItemView *>(widget)) {
            const int side = sideOf(widget);
            if (side >= 0) {
                m_displays[1 - side]->currentTab()->setFocus(Qt::TabFocusReason);
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    m_closing = true;
    if (m_manager)
        m_manager->windowClosing(this);
    else
        saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    else if (event->type() == QEvent::ActivationChange && isActiveWindow() && m_manager)
        m_manager->windowActivated(this);
    QMainWindow::changeEvent(event);
}

void MainWindow::retranslate() {
    m_fileMenu->setTitle(tr("&File"));
    m_open->setText(tr("Open") + QStringLiteral("\tEnter"));
    m_openInNewTab->setText(tr("Open in New Tab"));
    m_openInOtherSide->setText(tr("Open on Other Side"));
    m_openTerminal->setText(tr("Open Terminal Here"));
    m_createFolder->setText(tr("Create Folder…"));
    m_rename->setText(tr("Rename"));
    m_duplicate->setText(tr("Duplicate"));
    m_newWindow->setText(tr("New Window"));
    m_closeWindow->setText(tr("Close Window"));
    m_quit->setText(tr("Quit"));

    m_editMenu->setTitle(tr("&Edit"));
    m_cut->setText(tr("Cut"));
    m_copy->setText(tr("Copy"));
    m_paste->setText(tr("Paste"));
    m_pasteLinkMenu->setTitle(tr("Paste as Link"));
    m_pasteSymlink->setText(tr("Symbolic Link"));
    m_pasteRelative->setText(tr("Relative Symbolic Link"));
    m_pasteHardlink->setText(tr("Hard Link"));
    m_copyNamesMenu->setTitle(tr("Copy Names"));
    for (int i = 0; i < m_copyNameActions.size(); ++i)
        m_copyNameActions.at(i)->setText(NameFormat::title(NameFormat::Style(i)));
    m_copyNamesAs->setText(tr("Custom Format…"));
    updateTransferActions();
    m_selectAll->setText(tr("Select All"));
    m_selectMenu->setTitle(tr("Select"));
    m_selectFiles->setText(tr("Files Only"));
    m_selectFolders->setText(tr("Folders Only"));
    m_selectNone->setText(tr("None") + QStringLiteral("\tEsc"));
    m_invertSelection->setText(tr("Invert Selection"));
    m_reselect->setText(tr("Reselect Previous"));
    m_selectMatching->setText(tr("Select Matching…"));
    m_findNext->setText(tr("Find Next"));
    m_findPrevious->setText(tr("Find Previous"));

    m_viewMenu->setTitle(tr("&View"));
    m_modeMenu->setTitle(tr("View Mode"));
    for (int m = 0; m < m_modeActions.size(); ++m)
        m_modeActions.at(m)->setText(ViewFormat::modeTitle(ViewFormat::Mode(m)));
    m_cycleMode->setText(tr("Next View Mode"));
    m_columnsMenu->setTitle(tr("Columns"));
    m_sortMenu->setTitle(tr("Sort By"));
    m_defaultFormatMenu->setTitle(tr("Default Format"));
    m_saveFormat->setText(tr("Make This Format the Default"));
    m_resetFormat->setText(tr("Go Back to the Default Format"));
    m_showHidden->setText(tr("Show Hidden Files"));
    m_filterBar->setText(tr("Filter Bar"));
    m_showTree->setText(tr("Folder Tree"));
    m_tabBarMenu->setTitle(tr("Tab Position"));
    m_showViewer->setText(tr("Viewer Pane"));
    m_viewerLayoutMenu->setTitle(tr("Viewer Pane Layout"));
    m_viewerBeside->setText(tr("Beside the Files"));
    m_viewerBelowAction->setText(tr("Below the Files"));
    m_viewerSwitch->setText(tr("Switch Layout"));
    m_viewerLock->setText(tr("Lock Viewer Pane"));
    m_tabsTop->setText(tr("Top"));
    m_tabsBottom->setText(tr("Bottom"));
    m_tabsLeft->setText(tr("Left"));
    m_tabsRight->setText(tr("Right"));
    m_dualAction->setText(tr("Dual Display"));
    m_dualLayoutMenu->setTitle(tr("Dual Display Layout"));
    m_sideBySide->setText(tr("Side by Side"));
    m_stacked->setText(tr("Stacked"));
    m_toggleLayout->setText(tr("Switch Layout"));
    m_evenSplit->setText(tr("Even Split"));
    m_widenSide->setText(tr("Widen This Side"));
    m_narrowSide->setText(tr("Narrow This Side"));
    // Named apart from a tab's lock, which keeps a tab on its folder.
    m_lockMenu->setTitle(tr("Source and Destination"));
    m_unlock->setText(tr("Follow the Active Side"));
    m_lockSource->setText(tr("Keep This Side as Source"));
    m_lockDestination->setText(tr("Keep This Side as Destination"));

    m_goMenu->setTitle(tr("&Go"));
    m_back->setText(tr("Back"));
    m_forward->setText(tr("Forward"));
    m_up->setText(tr("Up"));
    m_root->setText(tr("Root"));
    m_editPath->setText(tr("Edit Path"));
    m_otherSide->setText(tr("Other Side") + QStringLiteral("\tTab"));
    m_swapFolders->setText(tr("Swap Folders with Other Side"));
    m_folderToOtherSide->setText(tr("Show This Folder on Other Side"));
    m_focusTree->setText(tr("Folder Tree"));

    m_tabsMenu->setTitle(tr("&Tabs"));
    m_newTab->setText(tr("New Tab"));
    m_duplicateTab->setText(tr("Duplicate Tab"));
    m_closeTab->setText(tr("Close Tab"));
    m_closedMenu->setTitle(tr("Recently Closed Tabs"));
    m_reopenTab->setText(tr("Reopen Last Closed Tab"));
    m_nextTab->setText(tr("Next Tab"));
    m_previousTab->setText(tr("Previous Tab"));
    m_switchTab->setText(tr("Switch Tab…"));
    m_tabList->setText(tr("All Tabs…"));
    m_renameTab->setText(tr("Rename Tab…"));
    m_tabLockMenu->setTitle(tr("Lock Tab"));
    m_tabColorMenu->setTitle(tr("Colour"));
    m_tabHome->setText(tr("Go to the Tab's Home Folder"));
    m_makeHome->setText(tr("Make This Folder the Tab's Home"));
    m_moveTab->setText(tr("Move Tab to Other Side"));
    m_groupsMenu->setTitle(tr("Tab Groups"));
    m_saveGroup->setText(tr("Save Tabs as Group…"));
    m_openGroupMenu->setTitle(tr("Open Group"));
    m_addGroupMenu->setTitle(tr("Add Group's Tabs"));
    m_deleteGroupMenu->setTitle(tr("Delete Group"));

    m_favoritesMenu->setTitle(tr("F&avourites"));
    m_addFavorite->setText(tr("Add to Favourites…"));
    m_editFavorites->setText(tr("Edit Favourites…"));
    m_showFavoritesBar->setText(tr("Show Favourites Bar"));
    m_aliasMenu->setTitle(tr("Folder Aliases"));

    m_windowMenu->setTitle(tr("&Window"));
    m_duplicateWindow->setText(tr("Duplicate Window"));
    m_closedWindowsMenu->setTitle(tr("Recently Closed Windows"));
    m_reopenWindow->setText(tr("Reopen Last Closed Window"));
    m_layoutsMenu->setTitle(tr("Layouts"));
    m_saveLayout->setText(tr("Save Windows as Layout…"));
    m_openLayoutMenu->setTitle(tr("Open Layout"));
    m_deleteLayoutMenu->setTitle(tr("Delete Layout"));
    m_saveDefault->setText(tr("Save as Default Window"));

    m_settingsMenu->setTitle(tr("&Settings"));
    m_whenExistsMenu->setTitle(tr("When Names Are Taken"));
    for (int i = 0; i < m_whenExistsActions.size(); ++i)
        m_whenExistsActions.at(i)->setText(WhenExistsNames::title(WhenExists(i)));
    m_dropMenu->setTitle(tr("When Dropping Files"));
    for (QAction *a : std::as_const(m_dropActions))
        a->setText(FileDrag::behaviourTitle(a->data().toString()));
    m_deleteOptionsMenu->setTitle(tr("Deleting"));
    m_renameSelectMenu->setTitle(tr("Rename Starts With"));
    const char *const renameParts[] = {QT_TR_NOOP("The Name Without Extension Selected"), QT_TR_NOOP("The Whole Name Selected"),
                                       QT_TR_NOOP("The Extension Selected"), QT_TR_NOOP("The Cursor at the Start"),
                                       QT_TR_NOOP("The Cursor at the End")};
    for (int i = 0; i < m_renameSelectActions.size(); ++i)
        m_renameSelectActions.at(i)->setText(tr(renameParts[i]));
    m_findMenu->setTitle(tr("Find as You Type"));
    m_filterBarMenu->setTitle(tr("Filter Bar"));
    m_completionMenu->setTitle(tr("Path Completion"));
    for (const OptionAction &o : std::as_const(m_optionActions))
        o.action->setText(tr(o.text));
    m_tabSettingsMenu->setTitle(tr("Tabs"));
    m_optNewTabButton->setText(tr("Show New Tab Button"));
    m_optCloseButtons->setText(tr("Close Buttons on Tabs"));
    m_optNextToCurrent->setText(tr("Open New Tabs Next to Current"));
    m_optFindExisting->setText(tr("Switch to an Open Tab Instead of Duplicating"));
    m_optDoubleClickCloses->setText(tr("Double-Click Closes Tab"));
    m_optCloseWithLastTab->setText(tr("Closing Last Tab Closes Window"));
    m_optSwitcher->setText(tr("Ctrl+Tab Opens Switcher"));
    m_viewerSettingsMenu->setTitle(tr("Viewer Pane"));
    m_windowSettingsMenu->setTitle(tr("Windows"));
    m_rememberOnClose->setText(tr("Remember Window on Close"));
    m_pairWindows->setText(tr("Pair Windows as Source and Destination"));
    m_treeOptions->setText(tr("Folder Tree Options…"));
    m_globalFilters->setText(tr("Global Filters…"));

    m_helpMenu->setTitle(tr("&Help"));
    m_shortcuts->setText(tr("Keyboard Shortcuts"));
    m_about->setText(tr("About CargoHatch"));
    updateChrome();
}

OperationRunner *MainWindow::startOperation(FileOperation *operation, FolderTab *source) {
    operation->setWhenExists(WhenExistsNames::fromId(m_options.whenExists));
    operation->setAskBeforeMerging(m_options.askBeforeMerging);
    operation->setAskBeforeReplacingReadOnly(m_options.askBeforeReplacingReadOnly);
    FileOperation::DeleteConfirmation confirm;
    confirm.eachFile = m_options.confirmEachFile;
    confirm.eachFolder = m_options.confirmEachFolder;
    confirm.readOnlyWithoutAsking = m_options.deleteReadOnlyWithoutAsking;
    operation->setDeleteConfirmation(confirm);
    source->rememberSelection();
    OperationRunner *runner = OperationRunner::start(operation, this);
    // A copy leaves its items behind, deselected, so the next command
    // doesn't act on them by accident; Reselect Previous brings them back.
    if (operation->kind() == FileOperation::Kind::Copy) {
        QPointer<FolderTab> tab = source;
        const QString folder = source->currentPath();
        connect(runner, &OperationRunner::finished, source, [tab, folder](FileOperation *op) {
            if (!tab || tab->currentPath() != folder || !op->errors().isEmpty())
                return;
            QStringList keep;
            for (const QString &path : tab->selectedPaths())
                if (!op->sources().contains(path))
                    keep << QFileInfo(path).fileName();
            tab->selectNames(keep);
        });
    }
    return runner;
}

OperationRunner *MainWindow::copySelection(bool move, const QString &folder, FolderTab *from) {
    FolderTab *source = from ? from : sourceTab();
    const QStringList paths = source->selectedPaths();
    // Errors go where the user is, as the source can be the other side.
    if (paths.isEmpty()) {
        activeTab()->showError(tr("Select the items to copy or move first."));
        return nullptr;
    }
    QString target = folder;
    if (target.isEmpty()) {
        FolderTab *destination = from ? partnerTab() : destinationTab();
        if (!destination) {
            activeTab()->showError(tr("There's no other side to copy to. Turn on Dual Display, or use Copy To…"));
            return nullptr;
        }
        target = destination->currentPath();
    }
    const auto kind = move ? FileOperation::Kind::Move : FileOperation::Kind::Copy;
    return startOperation(new FileOperation(kind, paths, target), source);
}

void MainWindow::copySelectionTo(bool move, FolderTab *from) {
    FolderTab *source = from ? from : sourceTab();
    if (source->selectedPaths().isEmpty()) {
        activeTab()->showError(tr("Select the items to copy or move first."));
        return;
    }
    const FolderTab *destination = from ? partnerTab() : destinationTab();
    ChooseFolderDialog dialog(move ? tr("Move To") : tr("Copy To"),
                              destination ? destination->currentPath() : source->currentPath(), source->currentPath(),
                              folderPicker(), this);
    dialog.setCompletionOptions(m_options.completionOptions());
    if (dialog.exec() == QDialog::Accepted)
        copySelection(move, dialog.folder(), from);
}

OperationRunner *MainWindow::duplicateSelection() {
    FolderTab *tab = activeTab();
    const QStringList paths = tab->selectedPaths();
    if (paths.isEmpty()) {
        tab->showError(tr("Select the items to duplicate first."));
        return nullptr;
    }
    auto *operation = new FileOperation(FileOperation::Kind::Copy, paths, tab->currentPath());
    operation->setDuplicate(true);
    return startOperation(operation, tab);
}

bool MainWindow::confirmRemoval(const QStringList &paths, bool permanently) {
    // Asking once per item is done during the delete; this asks up front.
    if (permanently ? m_options.confirmDelete : m_options.confirmTrash) {
        const QString name = QFileInfo(paths.first()).fileName();
        QString what;
        if (permanently)
            what = paths.size() == 1 ? tr("Delete %1 permanently?").arg(name)
                                     : tr("Delete %n items permanently?", "", int(paths.size()));
        else
            what = paths.size() == 1 ? tr("Move %1 to the trash?").arg(name)
                                     : tr("Move %n items to the trash?", "", int(paths.size()));
        QMessageBox box(permanently ? QMessageBox::Warning : QMessageBox::Question,
                        permanently ? tr("Delete Permanently") : tr("Move to Trash"), what, QMessageBox::Cancel, this);
        if (permanently)
            box.setInformativeText(tr("They won't go to the trash, and this can't be undone."));
        QPushButton *remove = box.addButton(permanently ? tr("Delete") : tr("Move to Trash"),
                                            permanently ? QMessageBox::DestructiveRole : QMessageBox::AcceptRole);
        box.setDefaultButton(permanently ? qobject_cast<QPushButton *>(box.button(QMessageBox::Cancel)) : remove);
        box.exec();
        if (box.clickedButton() != remove)
            return false;
    }
    return true;
}

OperationRunner *MainWindow::deleteSelection(bool reverse) {
    FolderTab *tab = activeTab();
    const QStringList paths = tab->selectedPaths();
    if (paths.isEmpty())
        return nullptr;
    const bool permanently = m_options.useTrash == reverse;
    if (!confirmRemoval(paths, permanently))
        return nullptr;
    const auto kind = permanently ? FileOperation::Kind::Delete : FileOperation::Kind::Trash;
    const QString next = m_options.selectNextAfterDelete ? tab->nameAfterSelection() : QString();
    OperationRunner *runner = startOperation(new FileOperation(kind, paths), tab);
    // Land on the item after the deleted ones, so the keyboard carries on.
    if (!next.isEmpty()) {
        QPointer<FolderTab> guard = tab;
        const QString folder = tab->currentPath();
        connect(runner, &OperationRunner::finished, tab, [guard, folder, next] {
            if (guard && guard->currentPath() == folder)
                guard->focusName(next);
        });
    }
    return runner;
}

void MainWindow::showBriefly(const QString &message) {
    statusBar()->showMessage(message, 4000);
}

void MainWindow::copyToClipboard(bool cut) {
    const QStringList paths = activeTab()->selectedPaths();
    if (paths.isEmpty())
        return;
    FileClipboard::instance()->setFiles(paths, cut);
    showBriefly(cut ? tr("%n items cut. Paste moves them.", "", int(paths.size()))
                    : tr("%n items copied. Paste puts copies in a folder.", "", int(paths.size())));
}

OperationRunner *MainWindow::startPaste(FileOperation *operation, FolderTab *tab) {
    OperationRunner *runner = startOperation(operation, tab);
    selectCreated(runner, tab);
    return runner;
}

void MainWindow::selectCreated(OperationRunner *runner, FolderTab *tab) {
    const QString folder = tab->currentPath();
    const QStringList before = tab->selectedPaths();
    QPointer<FolderTab> guard = tab;
    connect(runner, &OperationRunner::finished, tab, [guard, folder, before](FileOperation *op) {
        if (!guard || guard->currentPath() != folder)
            return;
        // Unless the user has picked something else in the meantime.
        for (const QString &path : guard->selectedPaths())
            if (!before.contains(path))
                return;
        QStringList names;
        for (const QString &path : op->created())
            if (QFileInfo(path).path() == folder)
                names << QFileInfo(path).fileName();
        if (!names.isEmpty())
            guard->selectNew(names);
    });
}

OperationRunner *MainWindow::pasteFiles(const QString &folder) {
    FolderTab *tab = activeTab();
    const FileClipboard::Files files = FileClipboard::instance()->files();
    if (files.paths.isEmpty()) {
        showBriefly(files.remote ? tr("Only files on this computer can be pasted for now.")
                                 : tr("There are no files on the clipboard."));
        return nullptr;
    }
    auto *operation = new FileOperation(files.cut ? FileOperation::Kind::Move : FileOperation::Kind::Copy, files.paths,
                                        folder.isEmpty() ? tab->currentPath() : folder);
    operation->setCopyOntoItself(true);
    OperationRunner *runner = startPaste(operation, tab);
    if (files.cut)
        connect(runner, &OperationRunner::finished, FileClipboard::instance(),
                [files] { FileClipboard::instance()->movedAway(files); });
    return runner;
}

void MainWindow::describeDrop(const QString &text) {
    // Only what was said about dropping is cleared.
    if (!text.isEmpty())
        statusBar()->showMessage(text);
    else if (m_describingDrop)
        statusBar()->clearMessage();
    m_describingDrop = !text.isEmpty();
}

void MainWindow::receiveDrop(const FileDrag::Drop &drop) {
    if (drop.action == FileDrag::Action::Ask)
        dropMenu(drop)->popup(drop.position);
    else
        performDrop(drop);
}

QMenu *MainWindow::dropMenu(const FileDrag::Drop &drop) {
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    auto add = [&](const QString &text, FileDrag::Action action, bool can) {
        QAction *a = menu->addAction(text);
        a->setEnabled(can);
        connect(a, &QAction::triggered, this, [this, drop, action] {
            FileDrag::Drop chosen = drop;
            chosen.action = action;
            performDrop(chosen);
        });
    };
    add(tr("Copy Here"), FileDrag::Action::Copy, drop.canCopy);
    add(tr("Move Here"), FileDrag::Action::Move, drop.canMove);
    add(tr("Link Here"), FileDrag::Action::Link, drop.canLink);
    menu->addSeparator();
    menu->addAction(tr("Cancel"));
    return menu;
}

OperationRunner *MainWindow::performDrop(const FileDrag::Drop &drop) {
    auto tabOf = [](QObject *object) {
        while (object && !qobject_cast<FolderTab *>(object))
            object = object->parent();
        return static_cast<FolderTab *>(object);
    };
    // The tab they came from, perhaps in another window, and the one whose
    // list they were dropped on.
    FolderTab *from = tabOf(drop.source);
    FolderTab *to = tabOf(drop.area);
    FolderTab *source = from ? from : to ? to : activeTab();
    if (drop.action == FileDrag::Action::Trash) {
        if (!confirmRemoval(drop.paths, false))
            return nullptr;
        return startOperation(new FileOperation(FileOperation::Kind::Trash, drop.paths), source);
    }
    const auto kind = drop.action == FileDrag::Action::Move   ? FileOperation::Kind::Move
                      : drop.action == FileDrag::Action::Link ? FileOperation::Kind::Link
                                                              : FileOperation::Kind::Copy;
    auto *operation = new FileOperation(kind, drop.paths, drop.folder);
    operation->setCopyOntoItself(true);
    OperationRunner *runner = startOperation(operation, source);
    if (to && to->currentPath() == drop.folder)
        selectCreated(runner, to);
    return runner;
}

OperationRunner *MainWindow::pasteLinks(FileOperation::LinkType type) {
    FolderTab *tab = activeTab();
    const FileClipboard::Files files = FileClipboard::instance()->files();
    if (files.paths.isEmpty()) {
        showBriefly(tr("There are no files on the clipboard."));
        return nullptr;
    }
    auto *operation = new FileOperation(FileOperation::Kind::Link, files.paths, tab->currentPath());
    operation->setLinkType(type);
    return startPaste(operation, tab);
}

void MainWindow::copyNames(const QString &format) {
    QStringList paths = activeTab()->selectedPaths();
    if (paths.isEmpty())
        paths << activeTab()->currentPath();
    const QString text = NameFormat::applyAll(format, paths, m_options.copyNamesOneLine);
    FileClipboard::instance()->setText(text);
    // One line can be shown whole; more are counted.
    showBriefly(paths.size() == 1 ? tr("Copied: %1").arg(text) : tr("Copied %n items as text.", "", int(paths.size())));
}

void MainWindow::copyNamesAs() {
    QStringList paths = activeTab()->selectedPaths();
    if (paths.isEmpty())
        paths << activeTab()->currentPath();
    NameFormatDialog dialog(m_options.nameFormats, paths, m_options.copyNamesOneLine, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString format = dialog.format();
    copyNames(format);
    TabOptions options = m_options;
    options.nameFormats.removeAll(format);
    options.nameFormats.prepend(format);
    options.nameFormats = options.nameFormats.mid(0, 10);
    setTabOptions(options);
}

void MainWindow::openSelection(FolderTab *tab) {
    const QStringList paths = tab->selectedPaths();
    if (paths.size() == 1)
        tab->openName(QFileInfo(paths.first()).fileName());
    if (paths.size() < 2)
        return;
    if (paths.size() > kOpenWithoutAsking) {
        QMessageBox box(QMessageBox::Question, tr("Open"), tr("Open %n items?", "", int(paths.size())),
                        QMessageBox::Cancel, this);
        box.setInformativeText(tr("Each file opens in its application, and each folder in a new tab."));
        QPushButton *open = box.addButton(tr("Open"), QMessageBox::AcceptRole);
        box.setDefaultButton(open);
        box.exec();
        if (box.clickedButton() != open)
            return;
    }
    QStringList failed;
    QStringList folders;
    for (const QString &path : paths) {
        if (QFileInfo(path).isDir())
            folders << path;
        else if (!FolderTab::openFile(path))
            failed << QFileInfo(path).fileName();
    }
    // The folders in order after the tab, showing the first of them.
    const int side = sideOf(tab);
    if (!folders.isEmpty() && side >= 0) {
        FileDisplay *display = m_displays[side];
        int at = display->indexOf(tab) + 1;
        FolderTab *first = nullptr;
        for (const QString &folder : std::as_const(folders)) {
            FolderTab *opened = display->openInNewTab(folder, at);
            at = display->indexOf(opened) + 1;
            if (!first)
                first = opened;
        }
        display->setCurrentIndex(display->indexOf(first));
        first->setFocus();
    }
    if (failed.size() == 1)
        tab->showError(tr("No application is set up to open %1.").arg(failed.first()));
    else if (!failed.isEmpty())
        tab->showError(tr("%n items couldn't be opened, as no application is set up for them.", "", int(failed.size())));
}

void MainWindow::openTerminal(const QString &folder) {
    const QString where = folder.isEmpty() ? activeTab()->currentPath() : folder;
    QPointer<MainWindow> window = this;
    Terminal::open(where, this, [window](const QString &why) {
        if (window)
            window->activeTab()->showError(why);
    });
}

void MainWindow::createFolder() {
    FolderTab *tab = activeTab();
    CreateFolderDialog::Setup setup;
    setup.base = tab->currentPath();
    setup.several = m_options.createSeveral;
    setup.then = CreateFolder::thenFromId(m_options.createThen);
    // A folder named for what is selected, for today, or as before.
    const QStringList selected = tab->selectedPaths();
    if (!selected.isEmpty() && QFileInfo(selected.first()).isFile())
        setup.suggestions << NameFormat::apply(NameFormat::format(NameFormat::Style::Stem), selected.first());
    setup.suggestions << QDate::currentDate().toString(Qt::ISODate) << m_options.createNames;
    CreateFolderDialog dialog(setup, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const CreateFolder::Result &result = dialog.result();
    TabOptions options = m_options;
    options.createSeveral = dialog.isSeveral();
    options.createThen = CreateFolder::thenId(dialog.then());
    if (!dialog.isSeveral())
        for (const QString &path : result.created) {
            options.createNames.removeAll(path);
            options.createNames.prepend(path);
        }
    options.createNames = options.createNames.mid(0, 15);
    setTabOptions(options);

    if (!result.errors.isEmpty()) {
        QMessageBox box(QMessageBox::Warning, tr("Create Folder"),
                        tr("%n folders couldn't be created.", "", int(result.errors.size())), QMessageBox::Ok, this);
        box.setInformativeText(result.errors.first());
        if (result.errors.size() > 1)
            box.setDetailedText(result.errors.join('\n'));
        box.exec();
    }
    if (result.created.isEmpty())
        return;
    const QString first = setup.base == "/" ? '/' + result.created.first() : setup.base + '/' + result.created.first();
    switch (dialog.then()) {
    case CreateFolder::Then::Open:
        tab->openPath(first);
        return;
    case CreateFolder::Then::NewTab:
        m_displays[m_active]->openInNewTab(first)->setFocus();
        return;
    case CreateFolder::Then::OtherSide:
        otherSideTab()->openPath(first);
        break;
    case CreateFolder::Then::Select:
    case CreateFolder::Then::Count:
        break;
    }
    // Of a/b/c, a is what shows here.
    QStringList names;
    for (const QString &path : result.created)
        if (const QString name = path.section('/', 0, 0); !names.contains(name))
            names << name;
    tab->selectNew(names);
}

void MainWindow::addFavorite(const QString &path, bool onBar) {
    const QString item = path.isEmpty() ? activeTab()->currentPath() : path;
    Favorite f = Favorite::folder(item);
    const QFileInfo info(item);
    if (info.exists() && !info.isDir())
        f.type = Favorite::Type::File;
    f.name = f.title();
    TabOptions options = m_options;
    if (onBar) {
        options.favorites.bar << f;
        if (options.favoritesAutoSort)
            FavoriteList::sort(&options.favorites.bar);
    } else {
        FavoriteDialog dialog(options.favorites, f, options.favoritesAutoSort, folderPicker(), this);
        if (dialog.exec() != QDialog::Accepted)
            return;
        options.favorites = dialog.list();
        // Adding to a bar that isn't there would look as if nothing happened.
        options.favoritesBar |= dialog.isOnBar();
    }
    setTabOptions(options);
}

void MainWindow::editFavorites() {
    FavoritesDialog dialog(m_options.favorites, m_options.favoritesAutoSort, folderPicker(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    TabOptions options = m_options;
    options.favorites = dialog.list();
    options.favoritesAutoSort = dialog.autoSort();
    setTabOptions(options);
}

void MainWindow::fillFavoritesMenu() {
    for (QAction *a : std::as_const(m_favoriteItems)) {
        // A branch's entry belongs to its menu.
        if (a->menu())
            delete a->menu();
        else
            delete a;
    }
    m_favoriteItems.clear();
    if (m_options.favorites.isEmpty())
        return;
    m_favoriteItems << m_favoritesMenu->addSeparator();
    QList<Favorite> list = m_options.favorites.items;
    if (!m_options.favorites.bar.isEmpty())
        list.prepend(Favorite::branch(tr("Favourites Bar"), m_options.favorites.bar));
    m_favoriteItems << FavoriteMenus::fill(m_favoritesMenu, list, this);
}

void MainWindow::openFavorite(const Favorite &f, bool newTab, bool otherSide) {
    const QString path = f.realPath();
    if (f.type == Favorite::Type::File) {
        if (!QFileInfo::exists(path) || !QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
            activeTab()->showError(tr("Couldn't open %1.").arg(path));
        return;
    }
    if (f.type != Favorite::Type::Folder)
        return;
    QString error;
    // A new tab on a folder that isn't there would show home instead.
    if (FolderTab::checkFolder(path, &error).isEmpty()) {
        activeTab()->showError(error);
        return;
    }
    if (newTab)
        m_displays[m_active]->openInNewTab(path);
    else if (otherSide)
        otherSideTab()->openPath(path);
    else
        activeTab()->openPath(path);
}

void MainWindow::openAll(const QList<Favorite> &favorites) {
    constexpr int kAsk = 15;
    QList<Favorite> folders;
    for (const Favorite &f : favorites)
        if (f.type == Favorite::Type::Folder)
            folders << f;
    if (folders.size() > kAsk &&
        QMessageBox::question(this, tr("Open All in Tabs"), tr("Open %n folders in tabs?", "", int(folders.size()))) !=
            QMessageBox::Yes)
        return;
    for (const Favorite &f : std::as_const(folders))
        openFavorite(f, true, false);
}

void MainWindow::showFavoriteMenu(int index, const QPoint &position) {
    auto *menu = new QMenu(this);
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    const QList<Favorite> bar = m_options.favorites.bar;
    if (index >= 0 && index < bar.size()) {
        const Favorite f = bar.at(index);
        // Another window may have changed the bar while the menu was open.
        auto change = [this, index, f](auto edit) {
            TabOptions options = m_options;
            if (index >= options.favorites.bar.size() || !(options.favorites.bar.at(index) == f))
                return;
            edit(&options.favorites.bar);
            setTabOptions(options);
        };
        if (f.type == Favorite::Type::Branch) {
            connect(menu->addAction(tr("Open All in Tabs")), &QAction::triggered, this, [this, f] { openAll(f.children); });
        } else {
            QAction *open = menu->addAction(tr("Open"));
            menu->setDefaultAction(open);
            connect(open, &QAction::triggered, this, [this, f] { openFavorite(f, false, false); });
            if (f.type == Favorite::Type::Folder) {
                connect(menu->addAction(tr("Open in New Tab")), &QAction::triggered, this,
                        [this, f] { openFavorite(f, true, false); });
                connect(menu->addAction(tr("Open on Other Side")), &QAction::triggered, this,
                        [this, f] { openFavorite(f, false, true); });
            }
            menu->addSeparator();
            connect(menu->addAction(tr("Edit…")), &QAction::triggered, this, [this, change, f, index] {
                FavoriteDialog dialog(f, folderPicker(), this);
                if (dialog.exec() != QDialog::Accepted)
                    return;
                const Favorite edited = dialog.favorite();
                change([&](QList<Favorite> *list) { (*list)[index] = edited; });
            });
        }
        connect(menu->addAction(tr("Remove")), &QAction::triggered, this, [this, change, f, index] {
            if (!f.children.isEmpty() &&
                QMessageBox::question(this, tr("Delete Branch"),
                                      tr("Delete the branch %1 and everything in it?").arg(f.title())) != QMessageBox::Yes)
                return;
            change([&](QList<Favorite> *list) { list->removeAt(index); });
        });
        menu->addSeparator();
    }
    connect(menu->addAction(tr("Add This Folder to the Bar")), &QAction::triggered, this, [this] { addFavorite({}, true); });
    menu->addAction(m_editFavorites);
    menu->addAction(m_showFavoritesBar);
    menu->popup(position);
}

void MainWindow::createTrees() {
    for (int t = 0; t < 3; ++t) {
        FolderTree *tree = new FolderTree(t < 2 ? static_cast<QWidget *>(m_panels[t]) : m_outer);
        m_trees[t] = tree;
        tree->hide();
        connect(tree, &FolderTree::openRequested, this, [this, t](const QString &path) {
            treeTab(t)->openPath(path);
            // A locked tab opens it elsewhere, and some can't be opened; the
            // tree goes back to where the tab is.
            syncTrees();
        });
        connect(tree, &FolderTree::newTabRequested, this, [this, t](const QString &path) {
            const int side = t < 2 ? t : m_active;
            activateSide(side);
            m_displays[side]->openInNewTab(path);
        });
        connect(tree, &FolderTree::otherSideRequested, this, [this, t](const QString &path) {
            if (t < 2)
                activateSide(t);
            otherSideTab()->openPath(path);
        });
        connect(tree, &FolderTree::leaveRequested, this, [this, t] { treeTab(t)->setFocus(); });
        connect(tree, &FolderTree::cutRequested, this, [this](const QString &path) {
            FileClipboard::instance()->setFiles({path}, true);
            showBriefly(tr("%n items cut. Paste moves them.", "", 1));
        });
        connect(tree, &FolderTree::copyRequested, this, [this](const QString &path) {
            FileClipboard::instance()->setFiles({path}, false);
            showBriefly(tr("%n items copied. Paste puts copies in a folder.", "", 1));
        });
        connect(tree, &FolderTree::pasteRequested, this, [this](const QString &path) { pasteFiles(path); });
        connect(tree, &FolderTree::refused, this,
                [this] { showBriefly(tr("Use the file list to rename, delete or duplicate folders.")); });
        connect(tree, &FolderTree::closeRequested, this, [this] { setTreeShown(false); });
        connect(tree, &FolderTree::menuRequested, this, [this, t](const QString &path, const QPoint &position) {
            treeMenu(t, path)->popup(position);
        });
        tree->setOpenFolders([this] {
            QStringList folders;
            for (const FileDisplay *display : m_displays)
                for (int i = 0; i < display->tabCount(); ++i)
                    folders << display->tab(i)->currentPath();
            return folders;
        });
    }
    // The displays get the room; trees keep their width.
    m_outer->setStretchFactor(m_outer->indexOf(m_splitter), 1);
    for (int side = 0; side < 2; ++side)
        m_panels[side]->setStretchFactor(m_panels[side]->indexOf(m_displays[side]), 1);
}

FolderTab *MainWindow::treeTab(int t) const {
    return t < 2 ? m_displays[t]->currentTab() : activeTab();
}

void MainWindow::showSide(int side, bool shown) {
    m_displays[side]->setVisible(shown);
    m_panels[side]->setVisible(shown);
}

void MainWindow::placeTrees() {
    const TreeOptions &o = m_options.tree;
    const bool each = m_dual && !o.shared;
    auto put = [](QSplitter *splitter, FolderTree *tree, QWidget *rest, bool right) {
        const bool isRight = splitter->indexOf(tree) > splitter->indexOf(rest);
        if (isRight != right) {
            splitter->insertWidget(right ? 1 : 0, tree);
            splitter->setStretchFactor(splitter->indexOf(tree), 0);
            splitter->setStretchFactor(splitter->indexOf(rest), 1);
        }
    };
    put(m_outer, m_trees[2], m_splitter, o.right);
    put(m_panels[0], m_trees[0], m_displays[0], o.right);
    put(m_panels[1], m_trees[1], m_displays[1], o.right || (o.secondOnRight && m_layout == Layout::SideBySide));
    for (int t = 0; t < 3; ++t) {
        const bool show = m_treeShown && (t == 2 ? !each : each);
        if (show != m_trees[t]->isHidden())
            continue;
        if (!show) {
            // Its width, for when it is next shown.
            if (m_trees[t]->isVisible())
                m_trees[t]->setPreferredWidth(m_trees[t]->width());
            if (m_trees[t]->isAncestorOf(QApplication::focusWidget()))
                treeTab(t)->setFocus();
        }
        m_trees[t]->setVisible(show);
        if (show) {
            auto *splitter = t < 2 ? m_panels[t] : m_outer;
            const int width = m_trees[t]->sizeHint().width();
            const int rest = qMax(0, splitter->width() - width);
            splitter->setSizes(splitter->indexOf(m_trees[t]) == 0 ? QList<int>{width, rest} : QList<int>{rest, width});
        }
    }
    syncTrees();
    // The trees have their new sizes once the window has been laid out.
    QTimer::singleShot(0, this, [this] {
        for (FolderTree *tree : m_trees)
            if (tree->isVisible())
                tree->showCurrent();
    });
}

void MainWindow::syncTrees() {
    if (!m_treeShown || m_closing)
        return;
    for (int t = 0; t < 3; ++t) {
        FolderTab *tab = treeTab(t);
        if (m_trees[t]->isHidden() || !tab)
            continue;
        if (m_options.tree.rememberTabs && m_treeTabs[t] != tab) {
            if (m_treeTabs[t]) {
                m_treeTabs[t]->setTreeBranches(m_trees[t]->expanded());
                m_trees[t]->follow(tab->currentPath());
                // A tab not seen before starts with what there is.
                if (tab->hasTreeBranches())
                    m_trees[t]->setExpanded(tab->treeBranches());
            }
        }
        m_treeTabs[t] = tab;
        m_trees[t]->follow(tab->currentPath());
    }
}

void MainWindow::setTreeShown(bool shown) {
    if (shown == m_treeShown)
        return;
    m_treeShown = shown;
    placeTrees();
    updateChrome();
}

void MainWindow::setViewerShown(bool shown) {
    if (shown == m_viewerShown && m_viewer->isHidden() == !shown)
        return;
    if (!shown) {
        if (m_viewer->isVisible())
            m_viewerSize = m_viewerBelow ? m_viewer->height() : m_viewer->width();
        if (m_viewer->isAncestorOf(QApplication::focusWidget()))
            activeTab()->setFocus();
        m_viewer->stop();
    }
    m_viewerShown = shown;
    m_viewer->setVisible(shown);
    if (shown) {
        placeViewer();
        updatePreview();
    }
    updateChrome();
}

void MainWindow::setViewerBelow(bool below) {
    if (below == m_viewerBelow)
        return;
    // A width doesn't make a good height, so the new way starts afresh.
    m_viewerSize = 0;
    m_viewerBelow = below;
    placeViewer();
    updateChrome();
}

void MainWindow::placeViewer() {
    m_viewSplit->setOrientation(m_viewerBelow ? Qt::Vertical : Qt::Horizontal);
    if (!m_viewerShown)
        return;
    int total = m_viewerBelow ? m_viewSplit->height() : m_viewSplit->width();
    // Before the window is first laid out, the splitter has no size yet.
    if (!m_viewSplit->isVisible() || total < 300)
        total = m_viewerBelow ? height() - 100 : width();
    const int wanted = m_viewerSize > 0 ? m_viewerSize : (m_viewerBelow ? total * 2 / 5 : total / 3);
    const int pane = qBound(m_viewerBelow ? 120 : 160, wanted, qMax(160, total - 200));
    m_viewSplit->setSizes({qMax(0, total - pane), pane});
}

void MainWindow::updatePreview() {
    if (!m_viewerShown || m_closing || m_viewer->isLocked())
        return;
    FolderTab *tab = activeTab();
    const QString path = tab ? tab->previewPath() : QString();
    if (path.isEmpty() && m_options.viewerKeepLast)
        return;
    m_viewer->showPath(path);
}

void MainWindow::focusTree() {
    setTreeShown(true);
    for (int t : {m_active, 2})
        if (!m_trees[t]->isHidden())
            m_trees[t]->setFocus();
}

void MainWindow::editTreeOptions() {
    TreeOptionsDialog dialog(m_options.tree, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    TabOptions options = m_options;
    options.tree = dialog.options();
    setTabOptions(options);
}

QMenu *MainWindow::treeMenu(int t, const QString &path) {
    FolderTree *tree = m_trees[t];
    auto *menu = new QMenu(this);
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    QAction *open = menu->addAction(tr("Open"));
    menu->setDefaultAction(open);
    connect(open, &QAction::triggered, tree, [tree, path] { emit tree->openRequested(path); });
    connect(menu->addAction(tr("Open in New Tab")), &QAction::triggered, tree,
            [tree, path] { emit tree->newTabRequested(path); });
    connect(menu->addAction(tr("Open on Other Side")), &QAction::triggered, tree,
            [tree, path] { emit tree->otherSideRequested(path); });
    menu->addSeparator();
    connect(menu->addAction(tr("Cut")), &QAction::triggered, tree, [tree, path] { emit tree->cutRequested(path); });
    connect(menu->addAction(tr("Copy")), &QAction::triggered, tree, [tree, path] { emit tree->copyRequested(path); });
    connect(menu->addAction(tr("Paste Into Folder")), &QAction::triggered, tree,
            [tree, path] { emit tree->pasteRequested(path); });
    connect(menu->addAction(tr("Copy Folder Path")), &QAction::triggered, this, [this, path] {
        FileClipboard::instance()->setText(path);
        showBriefly(tr("Copied: %1").arg(path));
    });
    connect(menu->addAction(tr("Open in Terminal")), &QAction::triggered, this, [this, path] { openTerminal(path); });
    connect(menu->addAction(tr("Add to Favourites…")), &QAction::triggered, this, [this, path] { addFavorite(path); });
    menu->addSeparator();
    connect(menu->addAction(tr("Collapse All")), &QAction::triggered, tree, &FolderTree::collapseAll);
    menu->addAction(m_treeOptions);
    return menu;
}

FolderPicker *MainWindow::folderPicker() const {
    static PortalFolderPicker portal;
    return m_picker ? m_picker : &portal;
}

void MainWindow::fillAliasMenu() {
    m_aliasMenu->clear();
    connect(m_aliasMenu->addAction(tr("Add Alias for This Folder…")), &QAction::triggered, this, &MainWindow::addAlias);
    connect(m_aliasMenu->addAction(tr("Edit Aliases…")), &QAction::triggered, this, &MainWindow::editAliases);
    auto add = [this](const FolderAliases::Alias &a) {
        const QString folder = FolderAliases::expand(a.path);
        QAction *go = m_aliasMenu->addAction(QStringLiteral("~%1\t%2").arg(a.name, fontMetrics().elidedText(
            a.path, Qt::ElideMiddle, 300)).replace('&', "&&"));
        go->setEnabled(QFileInfo(folder).isDir());
        connect(go, &QAction::triggered, this, [this, folder] { activeTab()->openPath(folder); });
    };
    QStringList own;
    if (!m_options.aliases.isEmpty())
        m_aliasMenu->addSeparator();
    for (const FolderAliases::Alias &a : FolderAliases::all())
        if (m_options.aliases.contains(a)) {
            add(a);
            own << a.name;
        }
    m_aliasMenu->addSeparator();
    for (const FolderAliases::Alias &a : FolderAliases::all())
        if (!own.contains(a.name))
            add(a);
}

void MainWindow::addAlias() {
    const QString path = activeTab()->currentPath();
    QStringList taken;
    for (const FolderAliases::Alias &a : m_options.aliases)
        taken << a.name;
    AliasEditDialog dialog(tr("Add Alias"), {FolderAliases::suggestName(path), path}, taken, folderPicker(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    TabOptions options = m_options;
    options.aliases << dialog.alias();
    setTabOptions(options);
    showBriefly(tr("Type ~%1 in a path to come here.").arg(dialog.alias().name));
}

void MainWindow::editAliases() {
    AliasDialog dialog(m_options.aliases, folderPicker(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    TabOptions options = m_options;
    options.aliases = dialog.aliases();
    setTabOptions(options);
}

void MainWindow::showAbout() {
    AboutDialog(this).exec();
}

void MainWindow::showShortcuts() {
    // It stays open beside the window, so the keys can be tried.
    if (!m_shortcutsDialog) {
        m_shortcutsDialog = new ShortcutsDialog(menuBar(), this);
        m_shortcutsDialog->setAttribute(Qt::WA_DeleteOnClose);
    }
    m_shortcutsDialog->show();
    m_shortcutsDialog->raise();
    m_shortcutsDialog->activateWindow();
}

QMenu *MainWindow::contextMenu(bool onItems) {
    FolderTab *tab = activeTab();
    // The menu bar's own actions, which show their keys, wherever they act
    // on the tab that was clicked.
    auto *menu = new QMenu(this);
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    updateChrome();

    if (!onItems) {
        menu->addAction(m_createFolder);
        menu->addSeparator();
        menu->addAction(m_paste);
        menu->addMenu(m_pasteLinkMenu);
        const QString folder = tab->currentPath();
        connect(menu->addAction(tr("Copy Folder Path")), &QAction::triggered, this, [this, folder] {
            FileClipboard::instance()->setText(folder);
            showBriefly(tr("Copied: %1").arg(folder));
        });
        menu->addAction(m_openTerminal);
        menu->addSeparator();
        menu->addAction(m_selectAll);
        menu->addAction(m_selectMatching);
        menu->addSeparator();
        menu->addMenu(m_modeMenu);
        menu->addMenu(m_sortMenu);
        menu->addMenu(m_columnsMenu);
        menu->addSeparator();
        menu->addAction(m_filterBar);
        menu->addAction(m_showHidden);
        menu->addSeparator();
        menu->addAction(m_addFavorite);
        return menu;
    }

    const QStringList paths = tab->selectedPaths();
    const bool one = paths.size() == 1;
    const bool folder = one && QFileInfo(paths.first()).isDir();
    QPointer<FolderTab> guard = tab;
    if (!one) {
        QAction *open = menu->addAction(tr("Open"));
        connect(open, &QAction::triggered, this, [this, guard] {
            if (guard)
                openSelection(guard);
        });
        menu->setDefaultAction(open);
        menu->addSeparator();
    }
    if (one) {
        const QString path = paths.first();
        QAction *open = menu->addAction(tr("Open"));
        connect(open, &QAction::triggered, this, [guard, path] {
            if (guard)
                guard->openName(QFileInfo(path).fileName());
        });
        // Double-clicking does the same, so it stands out.
        menu->setDefaultAction(open);
        if (folder) {
            menu->addAction(m_openInNewTab);
            menu->addAction(m_openInOtherSide);
            connect(menu->addAction(tr("Open in Terminal")), &QAction::triggered, this, [this, path] { openTerminal(path); });
        }
        menu->addSeparator();
    }
    menu->addAction(m_cut);
    menu->addAction(m_copy);
    if (folder) {
        const QString path = paths.first();
        connect(menu->addAction(tr("Paste Into Folder")), &QAction::triggered, this, [this, path] { pasteFiles(path); });
    }
    menu->addMenu(m_copyNamesMenu);
    menu->addSeparator();
    // F5 and F6 copy from the source side, which a lock can make the other
    // one; here it is always what was clicked.
    auto transfer = [this, menu, guard](const QString &text, QAction *shown, auto slot) {
        QAction *a = shown && guard == sourceTab() ? shown : menu->addAction(text);
        if (a == shown)
            menu->addAction(a);
        else
            connect(a, &QAction::triggered, this, [guard, slot] {
                if (guard)
                    slot(guard.data());
            });
    };
    if (partnerTab()) {
        transfer(tr("Copy to Other Side"), m_copyToOther, [this](FolderTab *t) { copySelection(false, {}, t); });
        transfer(tr("Move to Other Side"), m_moveToOther, [this](FolderTab *t) { copySelection(true, {}, t); });
    }
    transfer(tr("Copy To…"), m_copyTo, [this](FolderTab *t) { copySelectionTo(false, t); });
    transfer(tr("Move To…"), m_moveTo, [this](FolderTab *t) { copySelectionTo(true, t); });
    menu->addAction(m_duplicate);
    menu->addSeparator();
    if (one)
        menu->addAction(m_rename);
    menu->addAction(m_trash);
    menu->addAction(m_deleteForever);
    if (one) {
        menu->addSeparator();
        const QString path = paths.first();
        connect(menu->addAction(tr("Add to Favourites…")), &QAction::triggered, this, [this, path] { addFavorite(path); });
    }
    return menu;
}

void MainWindow::editGlobalFilters() {
    GlobalFilterDialog dialog(m_options.filter, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    TabOptions options = m_options;
    options.filter = dialog.filter();
    setTabOptions(options);
}

void MainWindow::selectMatching() {
    FolderTab *tab = activeTab();
    SelectPatternDialog dialog(m_options.selectPatterns, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const SelectPattern pattern = dialog.pattern();
    if (tab->selectMatching(pattern) == 0)
        tab->showError(tr("Nothing here matches %1.").arg(pattern.text));
    TabOptions options = m_options;
    options.selectPatterns.removeAll(pattern.text);
    options.selectPatterns.prepend(pattern.text);
    while (options.selectPatterns.size() > 10)
        options.selectPatterns.removeLast();
    setTabOptions(options);
}
