#include "foldertreemodel.h"

#include <QCollator>
#include <QDir>
#include <QElapsedTimer>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

#include <dirent.h>
#include <sys/stat.h>

#include "filemodel.h"
#include "icons.h"

namespace {

// How many items are looked through to see if a folder has sub-folders.
constexpr int kLookInside = 2000;
constexpr int kLookInsideMs = 1000;
// Watching is for what is on screen; this is plenty.
constexpr int kMaxWatched = 400;

bool isUnder(const QString &path, const QString &folder) {
    return path == folder || folder == "/" || path.startsWith(folder + '/');
}

bool isSystemFolder(const QString &path) {
    static const QSet<QString> folders{"/proc", "/sys", "/dev", "/run", "/lost+found"};
    return folders.contains(path);
}

QString joined(const QString &folder, const QString &name) {
    return folder == "/" ? folder + name : folder + '/' + name;
}

const QCollator &collator() {
    static const QCollator c = [] {
        QCollator made;
        made.setNumericMode(true);
        made.setCaseSensitivity(Qt::CaseInsensitive);
        return made;
    }();
    return c;
}

bool before(const QString &a, const QString &b) {
    const int c = collator().compare(a, b);
    return c != 0 ? c < 0 : a < b;
}

bool isDirectory(int folder, const dirent *entry) {
    if (entry->d_type == DT_DIR)
        return true;
    if (entry->d_type != DT_LNK && entry->d_type != DT_UNKNOWN)
        return false;
    // A link to a folder counts as one, as it does in the file list.
    struct stat st {};
    return ::fstatat(folder, entry->d_name, &st, 0) == 0 && S_ISDIR(st.st_mode);
}

void lookInside(const QString &path, FolderTreeModel::Folder *folder) {
    DIR *dir = ::opendir(QFile::encodeName(path).constData());
    if (!dir)
        return;
    int looked = 0;
    while (const dirent *entry = ::readdir(dir)) {
        const QByteArray name(entry->d_name);
        if (name == "." || name == "..")
            continue;
        if (++looked > kLookInside) {
            folder->anyInside = folder->anyVisibleInside = true;
            break;
        }
        if (!isDirectory(::dirfd(dir), entry))
            continue;
        folder->anyInside = true;
        if (!name.startsWith('.')) {
            folder->anyVisibleInside = true;
            break;
        }
    }
    ::closedir(dir);
}

} // namespace

QList<FolderTreeModel::Folder> FolderTreeModel::readFolders(const QString &path) {
    QList<Folder> folders;
    DIR *dir = ::opendir(QFile::encodeName(path).constData());
    if (!dir)
        return folders;
    const QSet<QString> listed = FileModel::hiddenNames(path);
    QElapsedTimer clock;
    clock.start();
    while (const dirent *entry = ::readdir(dir)) {
        const QByteArray raw(entry->d_name);
        if (raw == "." || raw == ".." || !isDirectory(::dirfd(dir), entry))
            continue;
        Folder f;
        f.name = QFile::decodeName(raw);
        f.listed = listed.contains(f.name);
        // On a slow drive, the rest are taken to have something inside.
        if (clock.elapsed() < kLookInsideMs)
            lookInside(joined(path, f.name), &f);
        else
            f.anyInside = f.anyVisibleInside = true;
        folders << f;
    }
    ::closedir(dir);
    return folders;
}

QStringList FolderTreeModel::drivesFrom(const QByteArray &mounts, const QString &home) {
    QStringList drives;
    for (const QByteArray &line : mounts.split('\n')) {
        const QList<QByteArray> fields = line.split(' ');
        if (fields.size() < 3)
            continue;
        // Spaces and such are written as octal.
        QByteArray raw;
        const QByteArray &field = fields.at(1);
        for (int i = 0; i < field.size(); ++i) {
            bool octal = false;
            const int code = field.at(i) == '\\' ? field.mid(i + 1, 3).toInt(&octal, 8) : 0;
            if (octal)
                i += 3;
            raw += octal ? char(code) : field.at(i);
        }
        const QString at = QFile::decodeName(raw);
        const QByteArray &type = fields.at(2);
        bool network = false;
        for (const char *name : {"nfs", "cifs", "smb", "sshfs", "rclone", "davfs", "ceph", "9p", "glusterfs"})
            network |= type.contains(name);
        const bool removable = at.startsWith("/run/media/") || at.startsWith("/media/") || at.startsWith("/mnt");
        bool wanted = removable || network;
        if (!wanted && fields.at(0).startsWith("/dev/") && !fields.at(0).startsWith("/dev/loop")) {
            // A disk mounted somewhere of its own, but not the pieces the
            // system itself is spread over.
            wanted = at != "/" && at != home;
            for (const char *part : {"/boot", "/efi", "/home", "/var", "/usr", "/opt", "/tmp", "/root", "/srv", "/nix",
                                     "/swap", "/.snapshots", "/snap", "/etc", "/proc", "/sys", "/dev", "/run"})
                wanted &= !isUnder(at, QString::fromLatin1(part));
        }
        // Parts of the desktop's plumbing mount things too.
        if (at.startsWith("/run/user/") || at.startsWith("/proc/") || at.startsWith("/sys/"))
            wanted = network;
        if (wanted && !drives.contains(at))
            drives << at;
    }
    std::sort(drives.begin(), drives.end(), before);
    return drives;
}

FolderTreeModel::FolderTreeModel(QObject *parent)
    : QAbstractItemModel(parent),
      m_matcher(m_options.filter),
      m_root(new Node),
      m_folderIcon(QFileIconProvider().icon(QFileIconProvider::Folder)) {
    m_root->read = true;
    m_changedTimer.setSingleShot(true);
    m_changedTimer.setInterval(300);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &path) {
        m_changed.insert(path);
        m_changedTimer.start();
    });
    connect(&m_changedTimer, &QTimer::timeout, this, [this] {
        const QSet<QString> changed = std::exchange(m_changed, {});
        for (Node *n : std::as_const(m_nodes))
            if (n->read && changed.contains(n->path))
                startRead(n);
    });
    // The kernel says when something is mounted or unmounted.
    m_mounts.setFileName("/proc/self/mounts");
    if (m_mounts.open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
        m_mountsNotifier = std::make_unique<QSocketNotifier>(m_mounts.handle(), QSocketNotifier::Exception);
        connect(m_mountsNotifier.get(), &QSocketNotifier::activated, this, [this] {
            // Reading it is what stops the kernel from saying so again.
            m_mounts.seek(0);
            m_mounts.readAll();
            updateRoots();
        });
    }
    updateRoots();
}

FolderTreeModel::~FolderTreeModel() = default;

void FolderTreeModel::setOptions(const Options &options) {
    const bool roots = options.drives != m_options.drives || options.trash != m_options.trash;
    const bool contents =
        options.hidden != m_options.hidden || options.system != m_options.system || options.filter != m_options.filter;
    const bool favorites = !(options.favorites == m_options.favorites);
    m_options = options;
    m_matcher = GlobalFilter::Matcher(options.filter);
    if (roots)
        updateRoots();
    if (favorites)
        updateFavorites();
    if (contents)
        for (const auto &child : m_root->children)
            rearrangeAll(child.get());
}

std::unique_ptr<FolderTreeModel::Node> FolderTreeModel::make(Node *parent, const QString &path, const QString &name,
                                                             const QIcon &icon) {
    auto n = std::make_unique<Node>();
    n->id = m_nextId++;
    n->parent = parent;
    n->path = path;
    n->name = name;
    n->icon = icon;
    n->folder.name = name;
    n->folder.anyInside = n->folder.anyVisibleInside = true;
    m_nodes.insert(n->id, n.get());
    return n;
}

void FolderTreeModel::forget(Node *n) {
    for (const auto &child : n->children)
        forget(child.get());
    m_nodes.remove(n->id);
}

void FolderTreeModel::updateRoots() {
    struct Root {
        QString path, name;
        const char *icon;
        bool drive = false;
    };
    // Home by its real path, since that is what tabs show.
    QString home = QFileInfo(QDir::homePath()).canonicalFilePath();
    if (home.isEmpty())
        home = QDir::homePath();
    QList<Root> roots{{home, tr("Home"), "user-home"}, {"/", tr("File System"), "drive-harddisk"}};
    if (m_options.drives && m_mounts.isOpen()) {
        m_mounts.seek(0);
        for (const QString &at : drivesFrom(m_mounts.readAll(), home))
            if (at != home && at != "/")
                roots.append({at, QFileInfo(at).fileName(), "drive-removable-media", true});
    }
    if (m_options.trash) {
        const QString trash =
            QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Trash/files";
        const QString real = QFileInfo(trash).canonicalFilePath();
        if (!real.isEmpty())
            roots.append({real, tr("Trash"), "user-trash"});
    }

    auto wanted = [&roots](const QString &path) {
        for (const Root &r : std::as_const(roots))
            if (r.path == path)
                return true;
        return false;
    };
    for (int i = int(m_root->children.size()) - 1; i >= 0; --i) {
        if (m_root->children[i]->isGroup || wanted(m_root->children[i]->path))
            continue;
        beginRemoveRows({}, i, i);
        forget(m_root->children[i].get());
        m_root->children.erase(m_root->children.begin() + i);
        endRemoveRows();
    }
    // After Favourites, if they are there.
    const int first = !m_root->children.empty() && m_root->children[0]->isGroup ? 1 : 0;
    for (int n = 0; n < roots.size(); ++n) {
        const Root &root = roots.at(n);
        const int i = first + n;
        if (i < int(m_root->children.size()) && m_root->children[i]->path == root.path)
            continue;
        beginInsertRows({}, i, i);
        m_root->children.insert(m_root->children.begin() + i,
                                make(m_root.get(), root.path, root.name, QIcon::fromTheme(root.icon, m_folderIcon)));
        m_root->children[i]->drive = root.drive;
        endInsertRows();
    }
}

void FolderTreeModel::updateFavorites() {
    if (!m_root->children.empty() && m_root->children[0]->isGroup) {
        beginRemoveRows({}, 0, 0);
        forget(m_root->children[0].get());
        m_root->children.erase(m_root->children.begin());
        endRemoveRows();
    }
    if (m_options.favorites.isEmpty())
        return;
    beginInsertRows({}, 0, 0);
    auto group = make(m_root.get(), {}, tr("Favourites"), QIcon());
    group->isGroup = true;
    group->read = true;
    fillFavorites(group.get(), m_options.favorites);
    m_root->children.insert(m_root->children.begin(), std::move(group));
    endInsertRows();
}

void FolderTreeModel::fillFavorites(Node *n, const QList<Favorite> &list) {
    for (const Favorite &f : list) {
        if (f.type == Favorite::Type::Folder) {
            n->children.push_back(make(n, f.realPath(), f.title(), m_folderIcon));
        } else if (f.type == Favorite::Type::Branch) {
            auto branch = make(n, {}, f.title(), QIcon());
            branch->isGroup = true;
            branch->read = true;
            fillFavorites(branch.get(), f.children);
            n->children.push_back(std::move(branch));
        }
    }
}

FolderTreeModel::Node *FolderTreeModel::node(const QModelIndex &index) const {
    return index.isValid() ? static_cast<Node *>(index.internalPointer()) : m_root.get();
}

QModelIndex FolderTreeModel::indexOf(const Node *n) const {
    if (!n || n == m_root.get())
        return {};
    const auto &siblings = n->parent->children;
    for (int row = 0; row < int(siblings.size()); ++row)
        if (siblings[row].get() == n)
            return createIndex(row, 0, const_cast<Node *>(n));
    return {};
}

QString FolderTreeModel::path(const QModelIndex &index) const {
    return index.isValid() ? node(index)->path : QString();
}

QModelIndex FolderTreeModel::rootOf(const QModelIndex &index) const {
    QModelIndex at = index;
    while (at.parent().isValid() && !node(at.parent())->isGroup)
        at = at.parent();
    return at;
}

QModelIndex FolderTreeModel::rootFor(const QString &path) const {
    const Node *best = nullptr;
    for (const auto &root : m_root->children)
        if (!root->isGroup && isUnder(path, root->path) && (!best || root->path.size() > best->path.size()))
            best = root.get();
    return indexOf(best);
}

QModelIndex FolderTreeModel::childTowards(const QModelIndex &parent, const QString &path) const {
    for (const auto &child : node(parent)->children)
        if (!child->isGroup && isUnder(path, child->path))
            return indexOf(child.get());
    return {};
}

QModelIndex FolderTreeModel::addTowards(const QModelIndex &parent, const QString &path) {
    Node *n = node(parent);
    if (n == m_root.get() || n->isGroup || !isUnder(path, n->path) || path == n->path)
        return {};
    const int start = n->path == "/" ? 1 : int(n->path.size()) + 1;
    const QString name = path.mid(start).section('/', 0, 0);
    if (name.isEmpty() || !QFileInfo(joined(n->path, name)).isDir())
        return {};
    int row = 0;
    while (row < int(n->children.size()) && before(n->children[row]->name, name))
        ++row;
    beginInsertRows(parent, row, row);
    auto child = make(n, joined(n->path, name), name, m_folderIcon);
    child->added = true;
    n->children.insert(n->children.begin() + row, std::move(child));
    endInsertRows();
    return index(row, 0, parent);
}

void FolderTreeModel::removeAdded(const QString &keep) {
    // Parents first, so nothing inside one that goes is looked at again.
    QList<Node *> queue{m_root.get()};
    while (!queue.isEmpty()) {
        Node *n = queue.takeFirst();
        for (int i = int(n->children.size()) - 1; i >= 0; --i) {
            Node *child = n->children[i].get();
            if (child->added && !isUnder(keep, child->path)) {
                beginRemoveRows(indexOf(n), i, i);
                forget(child);
                n->children.erase(n->children.begin() + i);
                endRemoveRows();
            } else {
                queue << child;
            }
        }
    }
}

bool FolderTreeModel::isRead(const QModelIndex &index) const {
    return node(index)->read;
}

void FolderTreeModel::read(const QModelIndex &index) {
    Node *n = node(index);
    if (n->read)
        emit childrenRead(index);
    else
        startRead(n);
}

void FolderTreeModel::startRead(Node *n) {
    if (n->reading) {
        n->again = true;
        return;
    }
    n->reading = true;
    n->again = false;
    const quint64 id = n->id;
    auto *watcher = new QFutureWatcher<QList<Folder>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, id] {
        watcher->deleteLater();
        // It may have gone in the meantime.
        Node *read = m_nodes.value(id);
        if (!read)
            return;
        read->reading = false;
        read->read = true;
        read->folders = watcher->result();
        std::sort(read->folders.begin(), read->folders.end(),
                  [](const Folder &a, const Folder &b) { return before(a.name, b.name); });
        arrange(read);
        const bool again = read->again;
        emit childrenRead(indexOf(read));
        if (again && (read = m_nodes.value(id)))
            startRead(read);
    });
    watcher->setFuture(QtConcurrent::run(&FolderTreeModel::readFolders, n->path));
}

bool FolderTreeModel::shows(const Node *parent, const Folder &f) const {
    if ((f.name.startsWith('.') || f.listed) && !m_options.hidden)
        return false;
    if (!m_options.system && parent->path == "/" && isSystemFolder('/' + f.name))
        return false;
    return !m_matcher.hides(f.name, true, f.listed);
}

void FolderTreeModel::arrange(Node *n) {
    QHash<QString, const Folder *> wanted;
    for (const Folder &f : std::as_const(n->folders))
        if (shows(n, f))
            wanted.insert(f.name, &f);
    QSet<QString> onDisk;
    for (const Folder &f : std::as_const(n->folders))
        onDisk.insert(f.name);

    for (int i = int(n->children.size()) - 1; i >= 0; --i) {
        Node *child = n->children[i].get();
        const Folder *f = wanted.value(child->name);
        if (f) {
            // One that was added shows on its own account now.
            child->added = false;
            const bool had = hasChildren(indexOf(child));
            child->folder = *f;
            if (had != hasChildren(indexOf(child)))
                emit dataChanged(indexOf(child), indexOf(child));
            continue;
        }
        // Added ones stay for as long as they exist.
        if (child->added && onDisk.contains(child->name))
            continue;
        beginRemoveRows(indexOf(n), i, i);
        forget(child);
        n->children.erase(n->children.begin() + i);
        endRemoveRows();
    }

    // Both lists are in the same order, so new ones go in as they come.
    int row = 0;
    for (const Folder &f : std::as_const(n->folders)) {
        if (!wanted.contains(f.name))
            continue;
        while (row < int(n->children.size()) && before(n->children[row]->name, f.name))
            ++row;
        if (row < int(n->children.size()) && n->children[row]->name == f.name)
            continue;
        beginInsertRows(indexOf(n), row, row);
        auto child = make(n, joined(n->path, f.name), f.name, m_folderIcon);
        child->folder = f;
        n->children.insert(n->children.begin() + row, std::move(child));
        endInsertRows();
    }
}

void FolderTreeModel::rearrangeAll(Node *n) {
    if (n->read && !n->isGroup)
        arrange(n);
    for (const auto &child : n->children)
        rearrangeAll(child.get());
}

void FolderTreeModel::setWatched(const QModelIndex &index, bool watched) {
    const QString folder = path(index);
    if (folder.isEmpty())
        return;
    if (!watched) {
        m_watcher.removePath(folder);
        return;
    }
    if (m_watcher.directories().contains(folder))
        return;
    if (m_watcher.directories().size() < kMaxWatched)
        m_watcher.addPath(folder);
    // It wasn't being watched, so it may have changed since it was read.
    Node *n = node(index);
    if (n->read)
        startRead(n);
}

QModelIndex FolderTreeModel::index(int row, int column, const QModelIndex &parent) const {
    const Node *n = node(parent);
    if (row < 0 || column != 0 || row >= int(n->children.size()))
        return {};
    return createIndex(row, 0, n->children[row].get());
}

QModelIndex FolderTreeModel::parent(const QModelIndex &index) const {
    return index.isValid() ? indexOf(node(index)->parent) : QModelIndex();
}

int FolderTreeModel::rowCount(const QModelIndex &parent) const {
    return int(node(parent)->children.size());
}

bool FolderTreeModel::hasChildren(const QModelIndex &parent) const {
    const Node *n = node(parent);
    if (n->read || !n->children.empty())
        return !n->children.empty();
    return m_options.hidden && m_options.filter.showHidden ? n->folder.anyInside : n->folder.anyVisibleInside;
}

bool FolderTreeModel::canFetchMore(const QModelIndex &parent) const {
    const Node *n = node(parent);
    return !n->read && !n->reading;
}

void FolderTreeModel::fetchMore(const QModelIndex &parent) {
    if (canFetchMore(parent))
        startRead(node(parent));
}

QVariant FolderTreeModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid())
        return {};
    const Node *n = node(index);
    switch (role) {
    case Qt::DisplayRole: return n->name;
    // Asked for each time, as its colour is the theme's.
    case Qt::DecorationRole: return n->isGroup ? Icons::star(QGuiApplication::palette()) : n->icon;
    case StartsGroupRole:
        return n->parent == m_root.get() && index.row() > 0 &&
               !(n->drive && m_root->children[index.row() - 1]->drive);
    case Qt::ToolTipRole:
    case PathRole: return n->path;
    default: return {};
    }
}
