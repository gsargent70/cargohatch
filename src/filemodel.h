#pragma once

#include <QAbstractTableModel>
#include <QCollator>
#include <QDateTime>
#include <QFile>
#include <QFileSystemWatcher>
#include <QHash>
#include <QIcon>
#include <QSet>
#include <QTimer>

#include <optional>

#include "viewformat.h"

// One item in a folder, read once and kept, so sorting and painting never
// touch the disk.
struct FileEntry {
    QString name;
    bool isDir = false;
    bool isLink = false;
    bool isHidden = false;
    // Named in the folder's .hidden file (FILT-03).
    bool isListedHidden = false;
    qint64 size = 0;
    QDateTime modified;
    QDateTime created;
    QDateTime accessed;
    QFile::Permissions permissions;
    uint ownerId = 0;
    uint groupId = 0;
    QString mimeName;
    QString linkTarget;

    // Sort keys, made the first time a sort needs them (SORT-03).
    mutable std::optional<QCollatorSortKey> nameKey;
    mutable std::optional<QCollatorSortKey> baseKey;
    mutable std::optional<QCollatorSortKey> extensionKey;

    // The part after the last dot, unless the name starts with its only dot.
    QString extension() const;
    QString baseName() const;
    // Whether anything shown about the item differs.
    bool sameAs(const FileEntry &other) const;
};

// The items of one folder (VIEW-02): CargoHatch's own model instead of Qt's
// QFileSystemModel, so it can have any columns, sort names naturally and
// later list things that aren't local folders. The folder is read on a
// worker thread and re-read when it changes on disk; changes are merged in
// so the selection and scroll position survive.
class QMimeDatabase;
class QFileInfo;

class FileModel : public QAbstractTableModel {
    Q_OBJECT

public:
    // True for an item that has been cut and not pasted yet (CLIP-01).
    static constexpr int CutRole = Qt::UserRole + 1;
    // Whether the item is a folder.
    static constexpr int FolderRole = Qt::UserRole + 2;

    explicit FileModel(QObject *parent = nullptr);

    QString folder() const { return m_folder; }
    // Clears the list and reads `path` in the background; loaded() follows.
    void setFolder(const QString &path);
    // Reads the folder again and merges the differences.
    void refresh();
    bool isLoading() const { return m_loading; }

    const FileEntry &entry(int row) const { return m_entries.at(row); }
    QString filePath(int row) const;
    // The row of the item called `name`, or -1.
    int rowOf(const QString &name) const;

    // How names compare (SORT-03).
    struct SortOptions {
        bool natural = true;
        // Hyphens and apostrophes are skipped, so "co-op" sorts with "coop".
        bool wordSort = false;
        // Names are compared without their extensions first.
        bool extensionsSeparately = false;
    };
    void setSortOptions(const SortOptions &options);
    const SortOptions &sortOptions() const { return m_sortOptions; }
    // Negative, zero or positive, as for strcmp.
    int compareNames(int a, int b) const;

    // Image files show a preview this many pixels across; 0 for none.
    void setThumbnailSize(int size);

    // Reads `path` without touching the model; it runs on a worker thread.
    static QList<FileEntry> readFolder(const QString &path);
    // The names in `folder`'s .hidden file, one per line.
    static QSet<QString> hiddenNames(const QString &folder);
    // Reads one item.
    static FileEntry readEntry(const QFileInfo &info, const QMimeDatabase &mimes);
    // After the item in `row` has been renamed on disk to `newName`: updates
    // it in place, so it keeps its place in the selection (REN-01).
    void renameEntry(int row, const QString &newName);
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // A size as shown everywhere: "1 byte", "512 bytes", "1.4 MB".
    static QString sizeText(qint64 bytes);

    // Text for each column, also used by tests.
    QString text(int row, Column column) const;

signals:
    // The folder has been read for the first time.
    void loaded();
    // The list is up to date: nothing more is waiting to be read.
    void settled();

private:
    void startRead(bool merge);
    void apply(const QList<FileEntry> &entries, bool merge);
    QIcon iconFor(const FileEntry &entry) const;
    QString typeName(const FileEntry &entry) const;
    QString userName(uint id) const;
    QString groupName(uint id) const;
    const QCollatorSortKey &key(const FileEntry &entry, int which) const;
    void requestThumbnail(int row) const;

    QString m_folder;
    QList<FileEntry> m_entries;
    QHash<QString, int> m_rows;
    int m_generation = 0;
    bool m_loading = false;
    bool m_rereadWanted = false;
    QFileSystemWatcher m_watcher;
    QTimer m_refreshTimer;
    SortOptions m_sortOptions;
    QCollator m_collator;
    int m_thumbnailSize = 0;
    int m_thumbnailGeneration = 0;
    mutable QHash<QString, QIcon> m_thumbnails;
    mutable QSet<QString> m_thumbnailsPending;
    mutable QHash<QString, QIcon> m_icons;
    mutable QHash<QString, QString> m_typeNames;
    mutable QHash<uint, QString> m_users;
    mutable QHash<uint, QString> m_groups;
};
