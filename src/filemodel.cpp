#include "filemodel.h"

#include <QDir>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImageReader>
#include <QLocale>
#include <QMimeDatabase>
#include <QPainter>
#include <QtConcurrent/QtConcurrentRun>

#include "fileclipboard.h"

#include <grp.h>
#include <pwd.h>

namespace {

QString permissionText(const FileEntry &entry) {
    const QFile::Permissions p = entry.permissions;
    QString text = entry.isLink ? "l" : entry.isDir ? "d" : "-";
    const std::pair<QFile::Permission, char> bits[] = {
        {QFile::ReadOwner, 'r'}, {QFile::WriteOwner, 'w'}, {QFile::ExeOwner, 'x'},
        {QFile::ReadGroup, 'r'}, {QFile::WriteGroup, 'w'}, {QFile::ExeGroup, 'x'},
        {QFile::ReadOther, 'r'}, {QFile::WriteOther, 'w'}, {QFile::ExeOther, 'x'},
    };
    for (const auto &[bit, letter] : bits)
        text += p.testFlag(bit) ? QChar(letter) : QChar('-');
    return text;
}

QString dateText(const QDateTime &date) {
    return date.isValid() ? QLocale().toString(date, QLocale::ShortFormat) : QString();
}

} // namespace

QString FileEntry::extension() const {
    if (isDir)
        return {};
    const int dot = name.lastIndexOf('.');
    return dot > 0 ? name.mid(dot + 1) : QString();
}

QString FileEntry::baseName() const {
    const QString ext = extension();
    return ext.isEmpty() ? name : name.left(name.size() - ext.size() - 1);
}

bool FileEntry::sameAs(const FileEntry &o) const {
    return name == o.name && isDir == o.isDir && isLink == o.isLink &&
           isListedHidden == o.isListedHidden && size == o.size && modified == o.modified &&
           created == o.created && accessed == o.accessed && permissions == o.permissions &&
           ownerId == o.ownerId && groupId == o.groupId && mimeName == o.mimeName && linkTarget == o.linkTarget;
}

FileModel::FileModel(QObject *parent) : QAbstractTableModel(parent) {
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);
    m_collator.setNumericMode(true);
    // Changes on disk often come in bursts (a copy, an unpack), so they are
    // gathered before the folder is read again.
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(250);
    connect(&m_refreshTimer, &QTimer::timeout, this, &FileModel::refresh);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_refreshTimer.start(); });
}

QList<FileEntry> FileModel::readFolder(const QString &path) {
    QList<FileEntry> entries;
    QMimeDatabase mimes;
    const QFileInfoList infos =
        QDir(path).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::NoSort);
    entries.reserve(infos.size());
    const QSet<QString> listed = hiddenNames(path);
    for (const QFileInfo &info : infos) {
        entries << readEntry(info, mimes);
        entries.last().isListedHidden = listed.contains(entries.last().name);
    }
    return entries;
}

QSet<QString> FileModel::hiddenNames(const QString &folder) {
    QSet<QString> names;
    const QString path = folder + "/.hidden";
    // Only a plain file, so a pipe or device by that name can't hang this.
    if (!QFileInfo(path).isFile())
        return names;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return names;
    // Nobody writes a list this long; something else has the name.
    for (const QByteArray &line : file.read(1 << 20).split('\n')) {
        const QString name = QString::fromUtf8(line).trimmed();
        if (!name.isEmpty())
            names.insert(name);
    }
    return names;
}

FileEntry FileModel::readEntry(const QFileInfo &info, const QMimeDatabase &mimes) {
    {
        FileEntry e;
        e.name = info.fileName();
        e.isLink = info.isSymLink();
        // A link to a folder is shown as a folder.
        e.isDir = info.isDir();
        e.isHidden = e.name.startsWith('.');
        e.size = e.isDir ? 0 : info.size();
        e.modified = info.lastModified();
        e.created = info.birthTime();
        e.accessed = info.lastRead();
        e.permissions = info.permissions();
        e.ownerId = info.ownerId();
        e.groupId = info.groupId();
        e.linkTarget = e.isLink ? info.symLinkTarget() : QString();
        e.mimeName = e.isDir ? QStringLiteral("inode/directory")
                             : mimes.mimeTypeForFile(info, QMimeDatabase::MatchExtension).name();
        return e;
    }
}

void FileModel::renameEntry(int row, const QString &newName) {
    if (row < 0 || row >= m_entries.size())
        return;
    m_thumbnails.remove(m_entries.at(row).name);
    m_rows.remove(m_entries.at(row).name);
    m_entries[row] = readEntry(QFileInfo(m_folder + '/' + newName), QMimeDatabase());
    m_entries[row].isListedHidden = hiddenNames(m_folder).contains(newName);
    m_rows.insert(newName, row);
    emit dataChanged(index(row, 0), index(row, columnCount() - 1));
}

Qt::ItemFlags FileModel::flags(const QModelIndex &index) const {
    Qt::ItemFlags f = QAbstractTableModel::flags(index);
    if (index.isValid())
        f |= Qt::ItemIsDragEnabled;
    if (index.isValid() && index.column() == int(Column::Name))
        f |= Qt::ItemIsEditable;
    return f;
}

void FileModel::setFolder(const QString &path) {
    if (!m_folder.isEmpty())
        m_watcher.removePath(m_folder);
    beginResetModel();
    m_folder = path;
    m_entries.clear();
    m_rows.clear();
    m_thumbnails.clear();
    m_thumbnailsPending.clear();
    ++m_thumbnailGeneration;
    endResetModel();
    ++m_generation;
    if (!path.isEmpty())
        m_watcher.addPath(path);
    startRead(false);
}

void FileModel::refresh() {
    if (m_loading) {
        m_rereadWanted = true;
        return;
    }
    startRead(true);
}

void FileModel::startRead(bool merge) {
    m_loading = true;
    m_rereadWanted = false;
    const int generation = m_generation;
    auto *watcher = new QFutureWatcher<QList<FileEntry>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation, merge] {
        watcher->deleteLater();
        // A newer folder has been asked for in the meantime.
        if (generation != m_generation)
            return;
        m_loading = false;
        apply(watcher->result(), merge);
        if (m_rereadWanted)
            refresh();
        else
            emit settled();
    });
    watcher->setFuture(QtConcurrent::run(&FileModel::readFolder, m_folder));
}

void FileModel::apply(const QList<FileEntry> &fresh, bool merge) {
    if (!merge) {
        beginResetModel();
        m_entries = fresh;
        m_rows.clear();
        for (int i = 0; i < m_entries.size(); ++i)
            m_rows.insert(m_entries.at(i).name, i);
        endResetModel();
        emit loaded();
        return;
    }

    QHash<QString, int> freshRows;
    for (int i = 0; i < fresh.size(); ++i)
        freshRows.insert(fresh.at(i).name, i);

    // Gone: removed from the end, a run of neighbours at a time.
    for (int row = int(m_entries.size()) - 1; row >= 0;) {
        if (freshRows.contains(m_entries.at(row).name)) {
            --row;
            continue;
        }
        int first = row;
        while (first > 0 && !freshRows.contains(m_entries.at(first - 1).name))
            --first;
        beginRemoveRows({}, first, row);
        for (int r = first; r <= row; ++r)
            m_thumbnails.remove(m_entries.at(r).name);
        m_entries.remove(first, row - first + 1);
        endRemoveRows();
        row = first - 1;
    }

    // Changed: updated in place.
    for (int row = 0; row < m_entries.size(); ++row) {
        const FileEntry &now = fresh.at(freshRows.value(m_entries.at(row).name));
        if (!m_entries.at(row).sameAs(now)) {
            m_thumbnails.remove(now.name);
            m_entries[row] = now;
            emit dataChanged(index(row, 0), index(row, columnCount() - 1));
        }
    }

    // New: added at the end; the sort puts them in place.
    QSet<QString> known;
    for (const FileEntry &e : std::as_const(m_entries))
        known.insert(e.name);
    QList<FileEntry> added;
    for (const FileEntry &e : fresh)
        if (!known.contains(e.name))
            added << e;
    if (!added.isEmpty()) {
        beginInsertRows({}, int(m_entries.size()), int(m_entries.size() + added.size() - 1));
        m_entries << added;
        endInsertRows();
    }

    m_rows.clear();
    for (int i = 0; i < m_entries.size(); ++i)
        m_rows.insert(m_entries.at(i).name, i);
}

QString FileModel::filePath(int row) const {
    const QString &name = m_entries.at(row).name;
    return m_folder.endsWith('/') ? m_folder + name : m_folder + '/' + name;
}

int FileModel::rowOf(const QString &name) const {
    return m_rows.value(name, -1);
}

void FileModel::setSortOptions(const SortOptions &options) {
    m_sortOptions = options;
    m_collator.setNumericMode(options.natural);
    for (const FileEntry &e : std::as_const(m_entries)) {
        e.nameKey.reset();
        e.baseKey.reset();
        e.extensionKey.reset();
    }
}

const QCollatorSortKey &FileModel::key(const FileEntry &entry, int which) const {
    std::optional<QCollatorSortKey> &slot = which == 0 ? entry.nameKey : which == 1 ? entry.baseKey : entry.extensionKey;
    if (!slot) {
        QString text = which == 0 ? entry.name : which == 1 ? entry.baseName() : entry.extension();
        if (m_sortOptions.wordSort)
            text.remove('-').remove('\'').remove(QChar(0x2019));
        slot = m_collator.sortKey(text);
    }
    return *slot;
}

int FileModel::compareNames(int a, int b) const {
    const FileEntry &x = m_entries.at(a);
    const FileEntry &y = m_entries.at(b);
    int result = 0;
    if (m_sortOptions.extensionsSeparately) {
        result = key(x, 1).compare(key(y, 1));
        if (result == 0)
            result = key(x, 2).compare(key(y, 2));
    } else {
        result = key(x, 0).compare(key(y, 0));
    }
    // Names that only differ in case still get a fixed order.
    return result != 0 ? result : x.name.compare(y.name);
}

void FileModel::setThumbnailSize(int size) {
    if (size == m_thumbnailSize)
        return;
    m_thumbnailSize = size;
    m_thumbnails.clear();
    m_thumbnailsPending.clear();
    ++m_thumbnailGeneration; // drops thumbnails still being made at the old size
    if (!m_entries.isEmpty())
        emit dataChanged(index(0, 0), index(int(m_entries.size()) - 1, 0), {Qt::DecorationRole});
}

void FileModel::requestThumbnail(int row) const {
    const FileEntry &e = m_entries.at(row);
    if (m_thumbnailsPending.contains(e.name))
        return;
    m_thumbnailsPending.insert(e.name);
    auto *self = const_cast<FileModel *>(this);
    const QString path = filePath(row);
    const QString name = e.name;
    const int size = m_thumbnailSize;
    const int generation = m_thumbnailGeneration;
    const QString folder = m_folder;
    auto *watcher = new QFutureWatcher<QImage>(self);
    connect(watcher, &QFutureWatcherBase::finished, self, [self, watcher, name, generation, folder] {
        watcher->deleteLater();
        if (generation != self->m_thumbnailGeneration || folder != self->m_folder)
            return;
        self->m_thumbnailsPending.remove(name);
        const QImage image = watcher->result();
        if (image.isNull())
            return;
        self->m_thumbnails.insert(name, QIcon(QPixmap::fromImage(image)));
        const int at = self->rowOf(name);
        if (at >= 0)
            emit self->dataChanged(self->index(at, 0), self->index(at, 0), {Qt::DecorationRole});
    });
    watcher->setFuture(QtConcurrent::run([path, size] {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        const QSize full = reader.size();
        if (full.isValid())
            reader.setScaledSize(full.scaled(size, size, Qt::KeepAspectRatio));
        QImage image = reader.read();
        if (image.isNull())
            return image;
        if (image.width() > size || image.height() > size)
            image = image.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        // Centred on a square, so every item's label lines up in the grid.
        QImage square(size, size, QImage::Format_ARGB32_Premultiplied);
        square.fill(Qt::transparent);
        QPainter painter(&square);
        painter.drawImage((size - image.width()) / 2, (size - image.height()) / 2, image);
        painter.end();
        return square;
    }));
}

int FileModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(m_entries.size());
}

int FileModel::columnCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : Columns::count();
}

QIcon FileModel::iconFor(const FileEntry &entry) const {
    auto it = m_icons.constFind(entry.mimeName);
    if (it != m_icons.constEnd())
        return *it;
    QIcon icon;
    if (entry.isDir) {
        icon = QFileIconProvider().icon(QFileIconProvider::Folder);
    } else {
        // The most specific icon that also comes in a large size: some icon
        // themes only ship a small fallback for a type (Qt quietly trims
        // "image-png" to a 16px "image"), and large icon modes would show a
        // speck. The generic icon for the type usually has every size.
        const QMimeDatabase mimes;
        const QMimeType mime = mimes.mimeTypeForName(entry.mimeName);
        QStringList names{mime.iconName(), mime.genericIconName()};
        for (const QString &parent : mime.allAncestors())
            names << mimes.mimeTypeForName(parent).iconName() << mimes.mimeTypeForName(parent).genericIconName();
        names << "text-x-generic" << "application-x-generic";
        QIcon smallOnly;
        for (const QString &name : names) {
            const QIcon candidate = QIcon::fromTheme(name);
            if (candidate.isNull())
                continue;
            if (candidate.actualSize(QSize(128, 128)).width() >= 48) {
                icon = candidate;
                break;
            }
            if (smallOnly.isNull())
                smallOnly = candidate;
        }
        if (icon.isNull() && !smallOnly.isNull()) {
            // Nothing bigger anywhere: enlarge the small one.
            const QPixmap source = smallOnly.pixmap(smallOnly.actualSize(QSize(128, 128)));
            icon.addPixmap(source);
            for (int size : {48, 128})
                icon.addPixmap(source.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        if (icon.isNull())
            icon = QFileIconProvider().icon(QFileIconProvider::File);
    }
    m_icons.insert(entry.mimeName, icon);
    return icon;
}

QString FileModel::typeName(const FileEntry &entry) const {
    auto it = m_typeNames.constFind(entry.mimeName);
    if (it != m_typeNames.constEnd())
        return *it;
    const QString name = entry.isDir ? tr("Folder") : QMimeDatabase().mimeTypeForName(entry.mimeName).comment();
    m_typeNames.insert(entry.mimeName, name);
    return name;
}

QString FileModel::userName(uint id) const {
    auto it = m_users.constFind(id);
    if (it != m_users.constEnd())
        return *it;
    QString name = QString::number(id);
    passwd pw;
    passwd *found = nullptr;
    char buffer[4096];
    if (getpwuid_r(id, &pw, buffer, sizeof buffer, &found) == 0 && found)
        name = QString::fromLocal8Bit(found->pw_name);
    m_users.insert(id, name);
    return name;
}

QString FileModel::groupName(uint id) const {
    auto it = m_groups.constFind(id);
    if (it != m_groups.constEnd())
        return *it;
    QString name = QString::number(id);
    group gr;
    group *found = nullptr;
    char buffer[4096];
    if (getgrgid_r(id, &gr, buffer, sizeof buffer, &found) == 0 && found)
        name = QString::fromLocal8Bit(found->gr_name);
    m_groups.insert(id, name);
    return name;
}

QString FileModel::text(int row, Column column) const {
    const FileEntry &e = m_entries.at(row);
    switch (column) {
    case Column::Name: return e.name;
    case Column::Extension: return e.extension();
    case Column::Size: return e.isDir ? QString() : sizeText(e.size);
    case Column::Type: return typeName(e);
    case Column::Modified: return dateText(e.modified);
    case Column::Created: return dateText(e.created);
    case Column::Accessed: return dateText(e.accessed);
    case Column::Permissions: return permissionText(e);
    case Column::Owner: return userName(e.ownerId);
    case Column::Group: return groupName(e.groupId);
    case Column::LinkTarget: return e.linkTarget;
    case Column::FullPath: return filePath(row);
    case Column::Count: break;
    }
    return {};
}

QVariant FileModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    const Column column = Column(index.column());
    const FileEntry &e = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return text(index.row(), column);
    case Qt::EditRole:
        return column == Column::Name ? e.name : text(index.row(), column);
    case Qt::DecorationRole:
        if (column != Column::Name)
            return {};
        if (m_thumbnailSize > 0 && e.mimeName.startsWith("image/")) {
            auto it = m_thumbnails.constFind(e.name);
            if (it != m_thumbnails.constEnd())
                return *it;
            requestThumbnail(index.row());
        }
        return iconFor(e);
    case Qt::TextAlignmentRole:
        return Columns::rightAligned(column) ? QVariant(Qt::AlignRight | Qt::AlignVCenter) : QVariant();
    case FolderRole:
        return e.isDir;
    case CutRole:
        return FileClipboard::instance()->isCut(filePath(index.row()));
    case Qt::ToolTipRole:
        return column == Column::Name && e.isLink ? tr("Link to %1").arg(e.linkTarget) : QVariant();
    default:
        return {};
    }
}

QVariant FileModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || section < 0 || section >= Columns::count())
        return {};
    if (role == Qt::DisplayRole)
        return Columns::title(Column(section));
    if (role == Qt::TextAlignmentRole && Columns::rightAligned(Column(section)))
        return QVariant(Qt::AlignRight | Qt::AlignVCenter);
    return {};
}

QString FileModel::sizeText(qint64 bytes) {
    // Qt says "1 bytes"; small sizes get a proper plural.
    if (bytes < 1000)
        return tr("%n bytes", "", int(bytes));
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}
