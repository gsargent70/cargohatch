#pragma once

#include <QMainWindow>
#include <QPointer>

#include "favoritesbar.h"
#include "filedisplay.h"
#include "filedrag.h"
#include "fileops.h"
#include "history.h"
#include "listerstate.h"
#include "taboptions.h"

class FolderPicker;
class FolderTab;
class FolderTree;
class ListerManager;
class ShortcutsDialog;
class OperationRunner;
class QAction;
class QActionGroup;
class QLabel;
class QSettings;
class QSplitter;
class TabSwitcher;

// A Lister: one window holding one or two file displays (LST-02), with one
// side the source and the other the destination (LST-03).
class MainWindow : public QMainWindow, private FavoriteMenus::Receiver, public FileDrag::Receiver {
    Q_OBJECT

public:
    enum class Layout { SideBySide, Stacked };

    // A window set up from `state`. `settings` holds the options; `manager`,
    // when there is one, knows about the other windows.
    explicit MainWindow(const ListerState &state, QSettings *settings = nullptr, ListerManager *manager = nullptr,
                        QWidget *parent = nullptr);
    // The window remembered in `settings`, showing `startPath`. On close it
    // remembers itself there again.
    explicit MainWindow(const QString &startPath, QSettings *settings = nullptr, QWidget *parent = nullptr);
    ~MainWindow() override;

    // Side 0 is the left (or top) display, side 1 the right (or bottom).
    FileDisplay *display(int side) const { return m_displays[side]; }
    // The display the user last clicked or focused.
    int activeSide() const { return m_active; }
    FolderTab *activeTab() const;

    int sourceSide() const { return m_source; }
    // -1 in a single display, which has no destination.
    int destinationSide() const { return m_destination; }
    FolderTab *sourceTab() const;
    // In a single display, the active tab of the window paired with this one
    // as its destination, if any (LST-04).
    FolderTab *destinationTab() const;
    // The tab on "the other side": the other display, or in a single display
    // the paired window. Null if there is none.
    FolderTab *partnerTab() const;

    // This window's folders, tabs and layout, to remember or reopen.
    ListerState state() const;
    // How much of the split the first display has, from 0 to 1.
    double splitRatio() const;
    bool isClosing() const { return m_closing; }

    bool isDual() const { return m_dual; }
    Layout layout() const { return m_layout; }

    // Which side is the source and which the destination: the active side is
    // the source, unless one side is locked into a role.
    static void assignRoles(bool dual, int active, int lockedSide, FileDisplay::Role lockedRole,
                            int *source, int *destination);

    const TabOptions &tabOptions() const { return m_options; }

    // A closed tab, kept so it can be reopened (TAB-12).
    struct ClosedTab {
        int side;
        int index;
        ListerState::Tab tab;
    };
    // Most recently closed first.
    const QList<ClosedTab> &closedTabs() const { return m_closedTabs; }
    static constexpr int kMaxClosedTabs = 25;
    // Opening more items than this at once asks first.
    static constexpr int kOpenWithoutAsking = 10;

    // Every tab on a showing side, most recently used first: the order of
    // the tab switcher.
    QList<FolderTab *> tabsByRecentUse() const;
    TabSwitcher *tabSwitcher() const { return m_switcher; }

public slots:
    void setDual(bool dual);
    void setLayout(Layout layout);
    void activateSide(int side);
    // Locks the active side as the source or destination; Role::None unlocks.
    void lockActiveSide(FileDisplay::Role role);
    void moveTabToOtherSide(int fromSide, int index);
    // Changes the tab options of every window.
    void setTabOptions(const TabOptions &options);
    // Applies them to this window only; the manager calls this.
    void applyTabOptions(const TabOptions &options);
    // The role this single-display window plays among the windows; set by
    // the manager.
    void setWindowRole(FileDisplay::Role role, bool locked);
    void setSplitRatio(double first);
    // Swaps the folders of this side and its partner (LST-06).
    void swapFolders();
    // Shows this folder on the other side, turning on the dual display if
    // there is no other side yet.
    void showFolderOnOtherSide();
    // Opens the highlighted folder on the other side.
    void openInOtherSide();
    // Opens the highlighted folder in a new tab on this side.
    void openInNewTab();
    // Links the tab at `index` on `side` with the other side's current tab,
    // or unlinks it if it is linked (TAB-09).
    void toggleLink(int side, int index);
    // This window's tabs, as a tab group (TAB-13).
    ListerState tabGroup() const;
    // Opens a tab group's tabs, replacing this window's unlocked tabs or
    // adding to them.
    void openTabGroup(const ListerState &group, bool replace);
    // Reopens closedTabs()[i] where it was, and returns it.
    FolderTab *reopenClosedTab(int i = 0);
    // Opens the Ctrl+Tab switcher one step forward (1) or back (-1).
    void openTabSwitcher(int step);
    void saveSettings();

    // COPY-01: copies or moves the source side's selection to `folder`, or
    // to the destination side if `folder` is empty. Null, with the reason
    // in the status bar, if there's nothing to do.
    // With `from`, that tab's selection instead, to the side it isn't on.
    OperationRunner *copySelection(bool move, const QString &folder = {}, FolderTab *from = nullptr);
    // Asks for a folder, then copies or moves there.
    void copySelectionTo(bool move, FolderTab *from = nullptr);
    // Copies the selection into its own folder under new names.
    OperationRunner *duplicateSelection();
    // DEL-01: to the trash, or for good after asking. `reverse` is Shift
    // held, which swaps the two.
    OperationRunner *deleteSelection(bool reverse);
    // CLIP-01: puts the selection on the clipboard, to copy or to move.
    void copyToClipboard(bool cut);
    // Copies or moves the clipboard's files into the active folder, or into
    // `folder`. Null, with the reason in the status bar, if there are none.
    OperationRunner *pasteFiles(const QString &folder = {});
    // IDEA-18.
    QString dropBehaviour() const override { return m_options.dropAction; }
    void describeDrop(const QString &text) override;
    void receiveDrop(const FileDrag::Drop &drop) override;
    OperationRunner *performDrop(const FileDrag::Drop &drop);
    // What to do with `drop`, as a menu that deletes itself once closed.
    QMenu *dropMenu(const FileDrag::Drop &drop);
    OperationRunner *pasteLinks(FileOperation::LinkType type);
    // CLIP-03: the selection's names as text, or the folder's if nothing is
    // selected.
    void copyNames(const QString &format);
    void copyNamesAs();
    // IDEA-11: opens `tab`'s selected items: files in their applications,
    // folders in new tabs after it. Asks first if there are many.
    void openSelection(FolderTab *tab);
    // FCMD-01.
    void createFolder();
    // IDEA-10: a terminal in `folder`, or in the active tab's folder.
    void openTerminal(const QString &folder = {});
    // LST-08: the folder tree, one for the window or one for each side.
    bool isTreeShown() const { return m_treeShown; }
    void setTreeShown(bool shown);
    // The window's tree, or a side's (0 or 1).
    FolderTree *windowTree() const { return m_trees[2]; }
    FolderTree *sideTree(int side) const { return m_trees[side]; }
    // Puts the keyboard in the tree, showing it if need be.
    void focusTree();
    void editTreeOptions();
    // FAV-01: asks for a name and branch, then adds `path` (or the current
    // folder); `onBar` adds it to the bar without asking.
    void addFavorite(const QString &path = {}, bool onBar = false);
    void editFavorites();
    FavoritesBar *favoritesBar() const { return m_favoritesBar; }
    QMenu *favoritesMenu() const { return m_favoritesMenu; }
    // How long an error stays in the status bar if the user does nothing.
    void setErrorTimeout(int ms);
    // NAV-07.
    void addAlias();
    void editAliases();
    // CFG-16.
    void showAbout();
    // CUST-17: every key, gathered from the menus.
    void showShortcuts();
    // The right-click menu for the selection in the active tab, or for its
    // folder (CUST-15). It deletes itself once closed.
    QMenu *contextMenu(bool onItems);
    // FILT-03.
    void editGlobalFilters();
    // Asks for a pattern and selects by it (SEL-02).
    void selectMatching();
    // Used by "Browse…" when asking for a folder; the desktop portal if unset.
    void setFolderPicker(FolderPicker *picker) { m_picker = picker; }

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void createActions();
    void applyState(const ListerState &state);
    FolderTab *otherSideTab();
    void fillClosedWindowsMenu();
    void fillTabGroupMenus();
    void fillColumnsMenu();
    void fillSortMenu();
    void fillLayoutMenus();
    void saveTabGroupAs();
    void saveLayoutAs();
    // Restores `state`'s tabs after those already on each side, linking
    // the pairs it records.
    void addStateTabs(const ListerState &state);
    // Asks for a name to save under, confirming before replacing one.
    QString askName(const QString &title, const QString &label, const QStringList &taken);
    int sideOf(const QWidget *widget) const;
    OperationRunner *startOperation(FileOperation *operation, FolderTab *source);
    // Starts `operation`, which puts items into `tab`'s folder, and selects
    // them there afterwards.
    OperationRunner *startPaste(FileOperation *operation, FolderTab *tab);
    // Selects what `runner` makes in `tab`'s folder, unless the user has
    // selected something else meanwhile.
    void selectCreated(OperationRunner *runner, FolderTab *tab);
    // Asks, if the options say to, before the trash or a permanent delete.
    bool confirmRemoval(const QStringList &paths, bool permanently);
    // Shows `message` in the status bar for a few seconds.
    void showBriefly(const QString &message);
    void closeLastTab(int side);
    void noteActiveTab();
    void fillClosedTabsMenu();
    void updateOptionActions();
    void updateRoles();
    void updateChrome();
    // Whether F5, F6, Copy To and Move To can act, and where from, when it
    // isn't the side or window in use.
    void updateTransferActions();
    void retranslate();

    QSettings *m_settings;
    ListerManager *m_manager;
    // The window's tree beside everything else.
    QSplitter *m_outer;
    // The two sides, and each side's tree beside its display.
    QSplitter *m_splitter;
    QSplitter *m_panels[2];
    FileDisplay *m_displays[2];
    QLabel *m_status;
    // The error in the status bar, and the time until it goes by itself.
    QTimer *m_errorTimer;
    quint64 m_shownError = 0;
    bool m_dual = false;
    Layout m_layout = Layout::SideBySide;
    int m_active = 0;
    int m_lockedSide = -1;
    FileDisplay::Role m_lockedRole = FileDisplay::Role::None;
    int m_source = 0;
    int m_destination = -1;
    TabOptions m_options;
    FileDisplay::Role m_windowRole = FileDisplay::Role::None;
    bool m_windowLocked = false;
    bool m_closing = false;
    QByteArray m_splitterState;
    QList<ClosedTab> m_closedTabs;
    QList<QPointer<FolderTab>> m_recentTabs;
    QList<QPointer<FolderTab>> m_switcherTabs;
    TabSwitcher *m_switcher;

    FolderPicker *m_picker = nullptr;
    QMenu *m_editMenu, *m_whenExistsMenu, *m_pasteLinkMenu, *m_copyNamesMenu;
    QAction *m_createFolder, *m_cut, *m_copy, *m_paste, *m_pasteSymlink, *m_pasteRelative, *m_pasteHardlink;
    QList<QAction *> m_copyNameActions;
    QAction *m_copyNamesAs, *m_showHidden, *m_globalFilters;
    QMenu *m_filterBarMenu, *m_findMenu;
    void openFavorite(const Favorite &favorite, bool newTab, bool otherSide) override;
    void openAll(const QList<Favorite> &favorites) override;
    void fillFavoritesMenu();
    void showFavoriteMenu(int index, const QPoint &position);
    FavoritesBar *m_favoritesBar;
    QMenu *m_favoritesMenu;
    QAction *m_addFavorite, *m_editFavorites, *m_showFavoritesBar;
    QList<QAction *> m_favoriteItems;
    // The sides' trees, then the window's.
    FolderTree *m_trees[3];
    bool m_treeShown = false;
    // Whose branches the trees show, when each tab has its own.
    QPointer<FolderTab> m_treeTabs[3];
    void createTrees();
    void placeTrees();
    void syncTrees();
    // The tab tree `t` follows.
    FolderTab *treeTab(int t) const;
    void showSide(int side, bool shown);
    QMenu *treeMenu(int t, const QString &path);
    QAction *m_showTree, *m_treeOptions, *m_focusTree;
    QMenu *m_tabsMenu, *m_settingsMenu, *m_selectMenu, *m_defaultFormatMenu, *m_dualLayoutMenu;
    QMenu *m_tabLockMenu, *m_tabColorMenu, *m_tabSettingsMenu, *m_windowSettingsMenu;
    QAction *m_open, *m_makeHome, *m_linkTab, *m_shortcuts;
    QPointer<ShortcutsDialog> m_shortcutsDialog;
    QAction *m_filterBar, *m_findNext, *m_findPrevious;
    QMenu *m_aliasMenu, *m_completionMenu;
    FolderPicker *folderPicker() const;
    void fillAliasMenu();
    QAction *m_copyToOther, *m_moveToOther, *m_copyTo, *m_moveTo, *m_duplicate, *m_trash, *m_deleteForever;
    QAction *m_useTrash, *m_selectAll, *m_selectFiles, *m_selectFolders, *m_selectNone, *m_invertSelection;
    QAction *m_reselect, *m_selectMatching;
    QList<QAction *> m_whenExistsActions;
    QMenu *m_dropMenu;
    QList<QAction *> m_dropActions;
    bool m_describingDrop = false;
    QAction *m_rename, *m_openTerminal;
    QMenu *m_renameSelectMenu, *m_deleteOptionsMenu;
    QList<QAction *> m_renameSelectActions;
    // Check boxes in the menus bound to a TabOptions field.
    struct OptionAction {
        QAction *action;
        bool TabOptions::*field;
        const char *text;
    };
    QList<OptionAction> m_optionActions;
    QMenu *m_fileMenu, *m_viewMenu, *m_goMenu, *m_lockMenu, *m_tabBarMenu, *m_closedMenu;
    QMenu *m_windowMenu, *m_closedWindowsMenu, *m_helpMenu;
    QAction *m_about;
    QAction *m_newWindow, *m_duplicateWindow, *m_reopenWindow, *m_saveDefault, *m_rememberOnClose;
    QAction *m_pairWindows, *m_closeWindow;
    QAction *m_evenSplit, *m_widenSide, *m_narrowSide;
    QAction *m_swapFolders, *m_folderToOtherSide, *m_openInOtherSide;
    QAction *m_openInNewTab, *m_tabHome, *m_renameTab, *m_optFindExisting;
    QMenu *m_groupsMenu, *m_openGroupMenu, *m_addGroupMenu, *m_deleteGroupMenu;
    QAction *m_saveGroup;
    QMenu *m_layoutsMenu, *m_openLayoutMenu, *m_deleteLayoutMenu;
    QAction *m_saveLayout;
    QMenu *m_modeMenu, *m_columnsMenu, *m_sortMenu;
    QList<QAction *> m_modeActions;
    QActionGroup *m_modeGroup;
    QAction *m_cycleMode, *m_saveFormat, *m_resetFormat;
    QAction *m_newTab, *m_duplicateTab, *m_moveTab, *m_closeTab, *m_reopenTab, *m_quit;
    QAction *m_tabsTop, *m_tabsBottom, *m_tabsLeft, *m_tabsRight;
    QAction *m_optNewTabButton, *m_optNextToCurrent, *m_optCloseButtons, *m_optDoubleClickCloses;
    QAction *m_optCloseWithLastTab, *m_optSwitcher;
    QActionGroup *m_tabPositionGroup;
    QAction *m_dualAction, *m_sideBySide, *m_stacked, *m_toggleLayout;
    QAction *m_lockSource, *m_lockDestination, *m_unlock;
    QAction *m_back, *m_forward, *m_up, *m_root, *m_editPath;
    QAction *m_nextTab, *m_previousTab, *m_switchTab, *m_switchTabBack, *m_tabList, *m_otherSide;
    QActionGroup *m_layoutGroup, *m_lockGroup;
};
