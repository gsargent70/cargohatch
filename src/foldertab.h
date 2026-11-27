#pragma once

#include <QPointer>
#include <QTimer>

#include <functional>
#include <memory>
#include <QWidget>

#include "filemodel.h"
#include "findbar.h"
#include "globalfilter.h"
#include "quickfilter.h"
#include "history.h"
#include "renamedelegate.h"
#include "viewformat.h"

struct SelectPattern;
class Breadcrumb;
class FileSortProxy;
class QAbstractItemView;
class QContextMenuEvent;
class QListView;
class QModelIndex;
class QStackedWidget;
class QToolButton;
class QTreeView;
class TileDelegate;

// One folder tab: its own navigation buttons, breadcrumb location bar, file
// list and back/forward history (TAB-01, NAV-01, NAV-02).
//
// The list has its own model, sort and view format, so sorting or changing
// the view in one tab leaves the others alone (VIEW-01, VIEW-02, SORT-*).
// Details and Power modes use a tree view with columns; the icon, list,
// thumbnail and tile modes use a list view over the same items and
// selection.
class FolderTab : public QWidget {
    Q_OBJECT

public:
    // How a tab is locked (TAB-08).
    enum class Lock {
        None,
        // Stays on its folder; going elsewhere opens a new tab beside it.
        Locked,
        // Can browse, but can't be closed and can be sent back to the
        // folder it was locked on.
        AllowChanges,
        // Like Locked, but going elsewhere reuses one tab beside it rather
        // than opening a new tab each time.
        Reuse,
    };

    // Opens `path`, or the home folder if `path` can't be opened.
    explicit FolderTab(const QString &path, QWidget *parent = nullptr);

    QString currentPath() const { return m_path; }
    QString ghostPath() const { return m_ghost; }
    QString title() const;
    QString statusText() const;
    // The view showing the files in the current mode.
    QAbstractItemView *fileView() const;
    QTreeView *detailsView() const { return m_details; }
    QListView *iconView() const { return m_icons; }
    FileModel *model() const { return m_model; }
    FileSortProxy *proxy() const { return m_proxy; }

    // The view mode, columns and sort, as they are now (column widths and
    // order included).
    ViewFormat format() const;
    void setFormat(const ViewFormat &format);
    void setViewMode(ViewFormat::Mode mode);
    void setColumnVisible(Column column, bool visible);
    void sortBy(Column column, bool reverse);
    // A click on a heading: the same column again reverses; a new one
    // starts in its natural direction (SORT-01).
    void sortByHeading(Column column);
    void setFolderOrder(ViewFormat::FolderOrder order);
    void sizeColumnsToFit();
    // How names compare and whether folders stay A to Z; shared by all tabs.
    void setSortOptions(const FileModel::SortOptions &options, bool foldersAlphabetical);
    Breadcrumb *breadcrumb() const { return m_breadcrumb; }
    const History &history() const { return m_history; }
    // Used when a tab is duplicated, so the copy can go back as well.
    void setHistory(const History &history);
    // The number of items showing in the current folder once it has been read, or -1
    // while it is still loading.
    int itemCount() const { return m_itemCount; }
    // How many more there are that the global filters hide, and that the
    // filter bar leaves out.
    int hiddenCount() const { return m_hidden; }
    int filteredOutCount() const { return m_filteredOut; }
    void setGlobalFilter(const GlobalFilter &filter);

    // SEL-11: typing in the file list opens the find bar.
    FindBar *findBar() const { return m_findBar; }
    void setFindOptions(const FindOptions &options);
    // Goes to the next match (1) or the one before (-1) of what was last
    // looked for. False, saying why, if there is none.
    bool findStep(int step);

    // FILT-01.
    FilterBar *filterBar() const { return m_filterBar; }
    struct FilterBehaviour {
        bool realTime = true;
        bool clearOnNavigate = true;
        bool selectFirst = true;
    };
    void setFilterOptions(const QuickFilter::Mode &mode, const FilterBehaviour &behaviour);
    void openFilterBar();

    // How files are opened in their applications; for tests, which mustn't
    // start any. True if it could be asked.
    using Opener = std::function<bool(const QString &path)>;
    static void setOpener(Opener opener);
    static bool openFile(const QString &path);

    // Opens the folder or file called `name`.
    void openName(const QString &name);

    // A name the user gave the tab, shown instead of the folder's (TAB-11).
    QString label() const { return m_label; }
    void setLabel(const QString &label);
    // The label if there is one, else the folder name.
    QString displayName() const;

    Lock lock() const { return m_lock; }
    // Locking remembers the current folder as the one to go back to.
    void setLock(Lock lock, const QString &lockedPath = {});
    bool isLocked() const { return m_lock != Lock::None; }
    QString lockedPath() const { return m_lockedPath; }
    // Goes back to the folder the tab was locked on.
    void returnToLockedFolder();
    // The tab a Reuse lock sends its folders to.
    FolderTab *reuseTarget() const { return m_reuseTarget; }
    void setReuseTarget(FolderTab *tab) { m_reuseTarget = tab; }

    // Its expanded branches of the folder tree, kept here while another tab
    // shows (TREE-04).
    QStringList treeBranches() const { return m_treeBranches; }
    // False until it has been given some, even none.
    bool hasTreeBranches() const { return m_hasTreeBranches; }
    void setTreeBranches(const QStringList &branches) {
        m_treeBranches = branches;
        m_hasTreeBranches = true;
    }

    // The tab's home folder (TAB-18): the first folder it opened unless set.
    QString homePath() const { return m_home; }
    void setHomePath(const QString &path);
    void goHome();

    // A hue name from the theme ("red", "blue"…), or empty (TAB-10).
    QString colorName() const { return m_color; }
    void setColorName(const QString &name);

    // The tab on the other side this one is shown with (TAB-09).
    FolderTab *linkedTab() const { return m_linked; }
    // Links both ways; null unlinks both.
    void setLinkedTab(FolderTab *tab);
    // The folder highlighted in the file list, or empty if the highlighted
    // item isn't a folder.
    QString highlightedFolder() const;
    // The selected items' full paths, in display order.
    QStringList selectedPaths() const;
    // What the viewer pane shows (LST-09): the item under the cursor when it
    // is selected, or else the first selected item; empty when nothing is.
    QString previewPath() const;
    // Moves the cursor `delta` items on and selects just that item, for the
    // viewer pane's previous and next buttons. False at either end.
    bool stepSelection(int delta);
    int selectedCount() const;
    // Selects exactly the items with these names, if they are here.
    void selectNames(const QStringList &names);
    // The same for items that were just made: reads the folder again first
    // and scrolls to them. Says so if they are hidden.
    void selectNew(const QStringList &names);
    // Keeps the current selection for reselectPrevious(); the select
    // commands and file operations call this before changing it.
    void rememberSelection();

    // The first unselected item after the selection (or before it, at the
    // end of the list): where to land after deleting the selection.
    QString nameAfterSelection() const;
    // Puts the cursor on `name` and selects it alone.
    void focusName(const QString &name);

    // REN-01: renames `oldName` in this folder to `newName`, never over an
    // existing item. False, with the reason in the status bar, if it can't.
    bool renameItem(const QString &oldName, const QString &newName);
    // The same on disk alone; returns what went wrong, or an empty string.
    static QString renameOnDisk(const QString &folder, const QString &oldName, const QString &newName);
    // Which part of the name inline rename starts with selected.
    void setRenameSelection(RenameDelegate::Selection selection);

    // Shows `message` in the status bar, until the window clears it or the
    // next folder opens.
    void showError(const QString &message) { setError(message); }
    void clearError();
    // A new number for each error, or 0 if there isn't one.
    quint64 errorId() const { return m_error.isEmpty() ? 0 : m_errorId; }
    // Whether the error is worth keeping until the user does something.
    bool errorLasts() const { return m_errorLasts; }

    // The ghost path to show after moving from `oldPath` to `newPath`: going
    // up keeps the deeper folder visible; going anywhere else drops it.
    static QString nextGhost(const QString &oldPath, const QString &oldGhost, const QString &newPath);
    // The canonical form of `path` if it is a readable folder; otherwise an
    // empty string, with the reason in `error`.
    static QString checkFolder(const QString &path, QString *error);

public slots:
    // Returns false, and says why in the status text, if the path isn't a
    // readable folder.
    bool openPath(const QString &path);
    bool goBack(int steps = 1);
    bool goForward(int steps = 1);
    void goUp();
    // The top of the filesystem this folder is on, or / if already there.
    void goRoot();
    void editPath();
    // Starts renaming the highlighted item in place (F2).
    void startRename();

    // SEL-01. Each of these remembers the selection it replaces.
    void selectAll();
    void selectFiles();
    void selectFolders();
    void selectNone();
    void invertSelection();
    // Brings back the selection from before the last select command or
    // file operation in this folder. False if there is none.
    bool reselectPrevious();
    // SEL-02. Returns how many items matched.
    int selectMatching(const SelectPattern &pattern);

signals:
    void pathChanged(const QString &path);
    void statusChanged();
    void folderLoaded(int itemCount);
    // The label, lock, colour or link changed, so the tab bar should redraw.
    void decorationChanged();
    // The view mode, columns or sort changed.
    void formatChanged();
    // A locked tab was asked to go to `path`; the display opens it elsewhere.
    void redirectRequested(const QString &path);
    // The user asked to open `path` in a new tab (middle-click a folder).
    void newTabRequested(const QString &path);
    // Enter, or Open, on several selected items (IDEA-11).
    void openSelectionRequested();
    // The cursor or the selection moved, so previewPath() may have changed.
    void previewChanged();
    // A right click, or the menu key, at `position` on the screen; on the
    // selected items, or on the folder itself (CUST-15).
    void contextMenuRequested(const QPoint &position, bool onItems);
    // The filter bar's ways of matching are to be the ones to start with.
    void filterDefaultsRequested(const QuickFilter::Mode &mode);

protected:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool navigate(const QString &path, bool record);
    // True, after asking for the folder to open elsewhere, if a lock keeps
    // this tab where it is.
    // `sent`, if given, is what the other tab is to open instead of `path`.
    bool redirectIfLocked(const QString &path, const QString &sent = {});
    bool goSteps(int steps);
    void activate(const QModelIndex &index);
    void setError(const QString &message);
    void countItems();
    QString whyNotShowing(int modelRow) const;
    QString nameAt(int proxyRow) const;
    bool isShowing(int modelRow) const;
    bool showContextMenu(QAbstractItemView *view, QContextMenuEvent *event);
    // Find-as-you-type: what matches now, the cursor's place in it.
    void find(const QString &text);
    void updateMatches();
    void goToMatch(int proxyRow);
    void closeFind(bool refocus);
    void clearHighlights();
    void setHighlights(const std::shared_ptr<const FindMatcher> &matcher);
    // IDEA-18: whether pressing the mouse at `pos` in `view` can begin to drag the
    // selection, rather than select by dragging.
    bool dragsFrom(QAbstractItemView *view, const QPoint &pos) const;
    void startDrag();
    // Where a name starts and ends in Details: pressing beside it selects.
    QRect nameRect(const QModelIndex &index) const;
    void acceptDrops(QAbstractItemView *view);
    QList<int> matchingRows(const FindMatcher &matcher, bool *anyStarts) const;
    void applyQuickFilter(const QuickFilter &filter);
    void selectPending();
    void updateButtons();
    void updateIcons();
    void fillHistoryMenu(QToolButton *button, bool back);
    void retranslate();
    void applyMode();
    void showHeaderMenu(const QPoint &pos);
    QString pathAt(const QModelIndex &proxyIndex) const;
    // Selects the rows `wanted` says yes to, as whole rows.
    void setSelection(const std::function<bool(int row, bool selected)> &wanted);
    bool isFolderAt(const QModelIndex &proxyIndex) const;

    FileModel *m_model;
    FileSortProxy *m_proxy;
    QStackedWidget *m_views;
    QTreeView *m_details;
    QListView *m_icons;
    TileDelegate *m_tileDelegate;
    RenameDelegate *m_plainDelegate;
    RenameDelegate *m_detailsDelegate;
    ViewFormat m_format;
    Breadcrumb *m_breadcrumb;
    QToolButton *m_back;
    QToolButton *m_forward;
    QToolButton *m_up;
    History m_history;
    QString m_path;
    QString m_ghost;
    QString m_error;
    quint64 m_errorId = 0;
    bool m_errorLasts = false;
    QString m_selectWhenLoaded;
    int m_itemCount = -1;
    QString m_label;
    Lock m_lock = Lock::None;
    QString m_lockedPath;
    QPointer<FolderTab> m_reuseTarget;
    QString m_home;
    QStringList m_treeBranches;
    bool m_hasTreeBranches = false;
    QString m_color;
    QPointer<FolderTab> m_linked;
    QString m_previousFolder;
    QStringList m_previousSelection;
    QStringList m_pendingSelection;
    int m_hidden = 0;
    int m_filteredOut = 0;
    FindBar *m_findBar;
    FilterBar *m_filterBar;
    FindOptions m_findOptions;
    FilterBehaviour m_filterBehaviour;
    std::shared_ptr<const FindMatcher> m_find;
    // The rows that match, in order.
    QList<int> m_matches;
    // Where the cursor was when the find bar opened.
    int m_findFrom = -1;
    bool m_highlighting = false;
    QTimer m_matchTimer;
};
