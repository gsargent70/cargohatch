#pragma once

#include <QWidget>

#include "listerstate.h"
#include "taboptions.h"

class FolderTab;
class FolderTabBar;
class History;
class QLabel;
class QMenu;
class QStackedWidget;
class QToolButton;

// One side of a Lister: a row of folder tabs (TAB-01) and the strip that
// shows whether this side is the source or the destination (LST-03).
//
// The tab bar and the folders are kept in step by hand, rather than with a
// QTabWidget, so the bar can sit on any side with level text, a new-tab
// button right after the last tab and a list of every tab when they don't
// fit (TAB-02, TAB-04, TAB-05).
class FileDisplay : public QWidget {
    Q_OBJECT

public:
    enum class Role { None, Source, Destination };

    explicit FileDisplay(QWidget *parent = nullptr);

    int tabCount() const;
    int currentIndex() const;
    FolderTab *tab(int index) const;
    FolderTab *currentTab() const;
    int indexOf(const FolderTab *tab) const;
    FolderTabBar *tabBar() const { return m_tabBar; }
    QString tabText(int index) const;
    QString tabToolTip(int index) const;

    const TabOptions &options() const { return m_options; }
    void setOptions(const TabOptions &options);
    // Whether the last tab may be closed, in which case closing it asks the
    // window to deal with it (lastTabCloseRequested).
    void setLastTabClosable(bool closable);

    // A new tab goes at the end, or beside the current tab when the options
    // say so.
    FolderTab *addTab(const QString &path);
    // A tab set up from `state` at `index`, as when one is reopened. Its
    // link is left to the window, which knows the other side.
    FolderTab *restoreTab(const ListerState::Tab &state, int index);
    // What restoreTab() needs to bring `tab` back.
    static ListerState::Tab tabState(const FolderTab *tab);
    // Opens `path` in a new tab at `index` (or where new tabs go), or, with
    // the option on, switches to a tab already showing it (TAB-15).
    FolderTab *openInNewTab(const QString &path, int index = -1);
    FolderTab *duplicateTab(int index);
    // The last tab isn't closed here: a display always shows a folder. The
    // window is told instead. Locked tabs stay until they are unlocked.
    bool closeTab(int index);
    // Closes every tab, leaving the display empty; for a side that is going
    // away.
    void closeAllTabs();
    // Removes a tab without deleting it, so it can move to the other side.
    FolderTab *takeTab(int index);
    void insertTab(FolderTab *tab, int index = -1);
    void setCurrentIndex(int index);
    void cycleTab(int step);

    // True when the tabs don't all fit and the list button is showing.
    bool isOverflowing() const;
    // Pops up the list of every tab (TAB-05, TAB-19).
    void showTabList();
    // The submenus of a tab's lock and colour, filled afresh for `target`
    // each time they open.
    void fillLockMenu(QMenu *menu, FolderTab *target);
    void fillColorMenu(QMenu *menu, FolderTab *target);
    // The menu showTabList() pops up, filled with the current tabs.
    QMenu *tabListMenu();

    Role role() const { return m_role; }
    bool isRoleLocked() const { return m_locked; }
    void setRole(Role role, bool locked);

    // Whether "Move to Other Side" is offered; only in a dual display.
    void setCanMoveTabs(bool canMove);
    // Asks for a new name for `tab` (TAB-11).
    void renameTab(FolderTab *tab);

signals:
    void currentTabChanged(FolderTab *tab);
    void tabPathChanged(FolderTab *tab);
    void tabStatusChanged(FolderTab *tab);
    void tabCountChanged();
    void moveTabRequested(int index);
    // Sent just before a closed tab is deleted, so it can be reopened later.
    void tabClosed(FolderTab *tab, int index);
    void lastTabCloseRequested();
    void openSelectionRequested(FolderTab *tab);
    // The user asked to link the tab at `index` with the other side's
    // current tab, or to unlink it.
    void linkRequested(int index);
    // A right click in `tab`'s file list (CUST-15).
    void contextMenuRequested(FolderTab *tab, const QPoint &position, bool onItems);
    // The user changed an option from inside the display: a folder colour
    // rule from a tab's menu, or the filter bar's defaults.
    void optionsChangeRequested(const TabOptions &options);

protected:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void newTabFromCurrent();
    // A new tab in the default format with the current sort options.
    FolderTab *makeTab(const QString &path) const;
    void applySortOptions(FolderTab *tab) const;
    void connectTab(FolderTab *tab);
    void updateTabLabel(FolderTab *tab);
    void showTabMenu(const QPoint &pos);
    void openRedirected(FolderTab *from, const QString &path);
    QColor hue(const QString &name) const;
    void fillTabList();
    void addCloseButton(FolderTab *tab);
    void updateCloseButtons();
    void updateIcons();
    void retranslate();
    void updateRoleIndicator();
    void rebuildLayout();
    void scheduleOverflowCheck();
    void checkOverflow();

    FolderTabBar *m_tabBar;
    QStackedWidget *m_stack;
    QWidget *m_header;
    QWidget *m_strip;
    QLabel *m_roleLabel;
    QToolButton *m_newTabButton;
    QToolButton *m_listButton;
    QMenu *m_listMenu;
    TabOptions m_options;
    Role m_role = Role::None;
    bool m_locked = false;
    bool m_canMoveTabs = false;
    bool m_lastTabClosable = false;
    bool m_overflowCheckPending = false;
    QHash<QString, QColor> m_hues;
};
