#pragma once

#include <QAbstractItemModel>
#include <QFile>
#include <QFileSystemWatcher>
#include <QHash>
#include <QIcon>
#include <QSet>
#include <QSocketNotifier>
#include <QTimer>

#include <memory>
#include <vector>

#include "favorites.h"
#include "globalfilter.h"

// The folders of the folder tree (LST-08, TREE-03): Home, the file system,
// drives and the trash, with their sub-folders read on worker threads when
// they are first needed. Folders being shown open are watched, so the tree
// keeps up with the disk.
class FolderTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    // The folder's path; empty for Favourites and its branches, which
    // aren't folders.
    static constexpr int PathRole = Qt::UserRole + 1;
    // True for a top-level item of another kind than the one above it: the
    // drives belong together.
    static constexpr int StartsGroupRole = Qt::UserRole + 2;

    struct Options {
        bool hidden = true;
        bool system = false;
        bool drives = true;
        bool trash = true;
        GlobalFilter filter;
        // Shown under an item of their own, if there are any.
        QList<Favorite> favorites;
    };

    // One sub-folder as read from the disk.
    struct Folder {
        QString name;
        // Named in the folder's .hidden file.
        bool listed = false;
        // Whether it has sub-folders, and any that aren't hidden. Taken to
        // have some where finding out would take long.
        bool anyInside = false;
        bool anyVisibleInside = false;
    };

    explicit FolderTreeModel(QObject *parent = nullptr);
    ~FolderTreeModel() override;

    void setOptions(const Options &options);

    QString path(const QModelIndex &index) const;
    // The topmost folder that `index` is under: a top-level item, or a
    // favourite.
    QModelIndex rootOf(const QModelIndex &index) const;
    // The top-level item best for `path`: the one nearest above it.
    QModelIndex rootFor(const QString &path) const;
    // The child of `parent` on the way to `path`, if it is there.
    QModelIndex childTowards(const QModelIndex &parent, const QString &path) const;
    // Adds that child even though the options hide it, or it hasn't been
    // noticed yet. For a folder that is open. Invalid if it isn't one.
    QModelIndex addTowards(const QModelIndex &parent, const QString &path);
    // Takes away the added ones, except those on the way to `keep`.
    void removeAdded(const QString &keep);

    bool isRead(const QModelIndex &index) const;
    // Reads the sub-folders if they haven't been; childrenRead() follows.
    void read(const QModelIndex &index);
    // Watches the folder for changes, while it is expanded.
    void setWatched(const QModelIndex &index, bool watched);

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &) const override { return 1; }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool hasChildren(const QModelIndex &parent = {}) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;

    // The sub-folders of `path`, unsorted. Runs on a worker thread.
    static QList<Folder> readFolders(const QString &path);
    // The places worth showing as drives, from the kernel's list of mounts.
    static QStringList drivesFrom(const QByteArray &mounts, const QString &home);

signals:
    void childrenRead(const QModelIndex &index);

private:
    struct Node {
        quint64 id = 0;
        Node *parent = nullptr;
        QString path;
        QString name;
        QIcon icon;
        // A top-level item that is a drive.
        bool drive = false;
        Folder folder;
        // Favourites, or a branch of it.
        bool isGroup = false;
        // Shown only because it was opened.
        bool added = false;
        bool read = false;
        bool reading = false;
        bool again = false;
        // Everything on disk, shown or not.
        QList<Folder> folders;
        std::vector<std::unique_ptr<Node>> children;
    };

    Node *node(const QModelIndex &index) const;
    QModelIndex indexOf(const Node *node) const;
    bool shows(const Node *parent, const Folder &folder) const;
    void startRead(Node *node);
    // Brings the node's children in line with its folders and the options.
    void arrange(Node *node);
    void rearrangeAll(Node *node);
    void updateRoots();
    void updateFavorites();
    void fillFavorites(Node *node, const QList<Favorite> &list);
    void forget(Node *node);
    std::unique_ptr<Node> make(Node *parent, const QString &path, const QString &name, const QIcon &icon);

    Options m_options;
    GlobalFilter::Matcher m_matcher;
    std::unique_ptr<Node> m_root;
    QHash<quint64, Node *> m_nodes;
    quint64 m_nextId = 1;
    QFileSystemWatcher m_watcher;
    QSet<QString> m_changed;
    QTimer m_changedTimer;
    QFile m_mounts;
    std::unique_ptr<QSocketNotifier> m_mountsNotifier;
    QIcon m_folderIcon;
};
