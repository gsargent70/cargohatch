#pragma once

#include <QPersistentModelIndex>
#include <QPointer>
#include <QTimer>
#include <QTreeView>
#include <QWidget>

#include <functional>

#include "foldertreemodel.h"
#include "treeoptions.h"

class QLabel;
class QToolButton;

// The tree itself: draws branches in the chosen style, and tells clicks and
// keys apart from changes made by following the file list.
class FolderTreeView : public QTreeView {
    Q_OBJECT

public:
    explicit FolderTreeView(QWidget *parent = nullptr);
    void setTreeOptions(const TreeOptions &options);
    void relayout() { scheduleDelayedItemsLayout(); }

signals:
    // Enter, or the cursor moved by a key and left there a moment.
    void keyOpened(const QModelIndex &index);
    void newTabRequested(const QModelIndex &index);
    void otherSideRequested(const QModelIndex &index);
    void leaveRequested();
    void cutRequested(const QModelIndex &index);
    void copyRequested(const QModelIndex &index);
    void pasteRequested(const QModelIndex &index);
    // Delete, F2 and such, which the tree doesn't do.
    void refused();
    void menuRequested(const QModelIndex &index, const QPoint &position);

protected:
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const override;
    void drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    TreeOptions m_options;
    QTimer m_keyTimer;
};

// The folder tree panel (LST-08): a heading and the tree, which follows one
// file display's folder and opens folders there.
class FolderTree : public QWidget {
    Q_OBJECT

public:
    explicit FolderTree(QWidget *parent = nullptr);

    void setOptions(const TreeOptions &options, const GlobalFilter &filter, const QList<Favorite> &favorites = {});
    FolderTreeView *view() const { return m_view; }
    FolderTreeModel *model() const { return m_model; }

    // Shows where `path` is, expanding and collapsing branches as the
    // options say. Nothing happens if it is there already.
    void follow(const QString &path);
    // The folder it has got to.
    QString currentPath() const;

    // The expanded branches, to bring back later with setExpanded().
    QStringList expanded() const;
    void setExpanded(const QStringList &branches);
    void collapseAll();
    // Scrolls to the current folder.
    void showCurrent();

    // Which folders are open in tabs, for "except those open in other tabs".
    void setOpenFolders(std::function<QStringList()> folders) { m_openFolders = std::move(folders); }

    // How wide it would like to be, when there is room.
    void setPreferredWidth(int width);
    QSize sizeHint() const override;

signals:
    void openRequested(const QString &path);
    void newTabRequested(const QString &path);
    void otherSideRequested(const QString &path);
    // The keyboard should go to the file list.
    void leaveRequested();
    void cutRequested(const QString &path);
    void copyRequested(const QString &path);
    void pasteRequested(const QString &path);
    void refused();
    void menuRequested(const QString &path, const QPoint &position);
    void closeRequested();

protected:
    void changeEvent(QEvent *event) override;

private:
    void open(const QModelIndex &index);
    // Carries on towards the folder being followed, as far as has been read.
    void advance();
    void arrived(const QModelIndex &index);
    void restoreBranches(const QModelIndex &parent);
    QString key(const QModelIndex &index) const;
    void retranslate();
    void updateIcons();

    FolderTreeModel *m_model;
    FolderTreeView *m_view;
    QWidget *m_header;
    QLabel *m_title;
    QToolButton *m_close;
    TreeOptions m_options;
    std::function<QStringList()> m_openFolders;
    // The folder to get to, and whether it has been reached.
    QString m_target;
    bool m_arrived = false;
    // Where in the tree to start looking for it.
    QPersistentModelIndex m_from;
    QPersistentModelIndex m_current;
    // Branches still to be expanded as they are read.
    QSet<QString> m_toExpand;
    int m_preferredWidth = 240;
};
