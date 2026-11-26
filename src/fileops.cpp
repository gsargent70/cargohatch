#include "fileops.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QThread>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr qint64 kChunk = 1 << 20;

const char *const kIds[] = {"ask", "skip", "replace", "newer", "renamenew", "renameold", "resume", "trashold"};

struct Stat {
    bool ok = false;
    struct stat st {};
    bool isDir() const { return ok && S_ISDIR(st.st_mode); }
    bool isLink() const { return ok && S_ISLNK(st.st_mode); }
    bool isFile() const { return ok && S_ISREG(st.st_mode); }
};

// lstat: a symbolic link is looked at, never followed.
Stat statOf(const QString &path) {
    Stat s;
    s.ok = ::lstat(QFile::encodeName(path).constData(), &s.st) == 0;
    return s;
}

QDateTime modified(const Stat &s) {
    return QDateTime::fromSecsSinceEpoch(s.st.st_mtim.tv_sec);
}

QString systemError() {
    return QString::fromLocal8Bit(std::strerror(errno));
}

QStringList entriesOf(const QString &folder) {
    return QDir(folder).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                                  QDir::Name);
}

// Copies the permissions and modification time of `from` onto `to`.
void keepAttributes(const Stat &from, const QString &to) {
    const QByteArray path = QFile::encodeName(to);
    ::chmod(path.constData(), from.st.st_mode & 07777);
    const struct timespec times[2] = {from.st.st_atim, from.st.st_mtim};
    ::utimensat(AT_FDCWD, path.constData(), times, AT_SYMLINK_NOFOLLOW);
}

qint64 sizeOf(const QString &path) {
    const Stat s = statOf(path);
    if (!s.ok)
        return 0;
    if (!s.isDir())
        return s.isFile() ? s.st.st_size : 0;
    qint64 total = 0;
    for (const QString &name : entriesOf(path))
        total += sizeOf(path + '/' + name);
    return total;
}

// Everything but folders: the items "file 3 of 12" counts.
int filesIn(const QString &path) {
    const Stat s = statOf(path);
    if (!s.isDir())
        return s.ok ? 1 : 0;
    int total = 0;
    for (const QString &name : entriesOf(path))
        total += filesIn(path + '/' + name);
    return total;
}

qint64 countOf(const QString &path) {
    const Stat s = statOf(path);
    qint64 total = 1;
    if (s.isDir())
        for (const QString &name : entriesOf(path))
            total += countOf(path + '/' + name);
    return total;
}

bool isInside(const QString &path, const QString &folder) {
    return path == folder || path.startsWith(folder.endsWith('/') ? folder : folder + '/');
}

QString tr(const char *text) {
    return QCoreApplication::translate("FileOperation", text);
}

} // namespace

QString WhenExistsNames::id(WhenExists policy) {
    return kIds[int(policy)];
}

WhenExists WhenExistsNames::fromId(const QString &id) {
    for (int i = 0; i < int(WhenExists::Count); ++i)
        if (id == kIds[i])
            return WhenExists(i);
    return WhenExists::Ask;
}

QString WhenExistsNames::title(WhenExists policy) {
    switch (policy) {
    case WhenExists::Ask: return tr("Ask");
    case WhenExists::Skip: return tr("Skip");
    case WhenExists::Replace: return tr("Replace");
    case WhenExists::KeepNewer: return tr("Replace if Newer");
    case WhenExists::RenameNew: return tr("Rename the Copy");
    case WhenExists::RenameOld: return tr("Rename the Existing Item");
    case WhenExists::Resume: return tr("Resume");
    case WhenExists::TrashOld: return tr("Move the Existing Item to Trash");
    case WhenExists::Count: break;
    }
    return {};
}

FileOperation::FileOperation(Kind kind, const QStringList &sources, const QString &destination, QObject *parent)
    : QObject(parent), m_kind(kind), m_sources(sources), m_destination(destination) {
}

QString FileOperation::freeName(const QString &folder, const QString &name) {
    // "archive.tar.gz" becomes "archive.tar (2).gz"; ".bashrc" has no
    // extension and becomes ".bashrc (2)".
    const int dot = name.lastIndexOf('.');
    const bool hasExtension = dot > 0;
    const QString stem = hasExtension ? name.left(dot) : name;
    const QString extension = hasExtension ? name.mid(dot) : QString();
    for (int n = 2;; ++n) {
        const QString candidate = QStringLiteral("%1 (%2)%3").arg(stem).arg(n).arg(extension);
        if (!statOf(folder + '/' + candidate).ok)
            return candidate;
    }
}

void FileOperation::fail(const QString &message) {
    m_errors << message;
}

void FileOperation::advance(qint64 amount, const QString &name) {
    m_done += amount;
    // A progress signal for every small file would flood the GUI.
    if (!m_clock.isValid() || m_clock.elapsed() > 50 || m_done >= m_total) {
        m_clock.start();
        emit progress(m_done, m_total, m_fileDone, m_fileSize, name);
    }
}

void FileOperation::startFile(const QString &path, qint64 size) {
    ++m_fileNumber;
    m_fileDone = 0;
    m_fileSize = size;
    if (!m_fileClock.isValid() || m_fileClock.elapsed() > 50 || m_fileNumber == 1 || m_fileNumber >= m_files) {
        m_fileClock.start();
        emit fileStarted(m_fileNumber, m_files, path);
    }
}

bool FileOperation::waitWhilePaused() {
    while (m_paused && !m_cancelled)
        QThread::msleep(50);
    return !m_cancelled;
}

bool FileOperation::confirmDelete(const QString &path, DeleteQuestion::Reason reason) {
    if (m_deleteAll[int(reason)])
        return true;
    if (m_unattended || !m_deleteResolver) {
        m_skipped << tr("%1 wasn't deleted, because it needed confirming.").arg(path);
        return false;
    }
    switch (m_deleteResolver({path, reason})) {
    case DeleteAnswer::DeleteAll:
        m_deleteAll[int(reason)] = true;
        return true;
    case DeleteAnswer::Delete:
        return true;
    case DeleteAnswer::Skip:
        return false;
    case DeleteAnswer::Cancel:
        cancel();
        return false;
    }
    return false;
}

void FileOperation::run() {
    m_errors.clear();
    m_created.clear();
    m_skipped.clear();
    m_done = 0;
    m_total = 0;
    m_fileNumber = 0;

    if (m_kind == Kind::Trash || m_kind == Kind::Delete) {
        for (const QString &source : m_sources)
            m_total += m_kind == Kind::Trash ? 1 : countOf(source);
        m_files = m_kind == Kind::Trash ? int(m_sources.size()) : int(m_total);
        emit progress(0, m_total, 0, 0, {});
        for (const QString &source : m_sources) {
            if (!waitWhilePaused())
                break;
            const QString name = QFileInfo(source).fileName();
            const Stat s = statOf(source);
            if (!s.ok) {
                fail(tr("%1 is no longer there.").arg(name));
                continue;
            }
            if (m_kind == Kind::Delete) {
                const bool folder = s.isDir();
                if ((folder ? m_deleteConfirm.eachFolder : m_deleteConfirm.eachFile) &&
                    !confirmDelete(source, folder ? DeleteQuestion::Reason::Folder : DeleteQuestion::Reason::File)) {
                    advance(countOf(source), name);
                    continue;
                }
            }
            if (m_kind == Kind::Trash) {
                startFile(source, 0);
                // A failed trash is reported, never quietly turned into a
                // permanent delete.
                if (!QFile::moveToTrash(source))
                    fail(tr("Couldn't move %1 to the trash. The trash may not be available on this drive.")
                             .arg(name));
                advance(1, name);
            } else {
                removeTree(source);
            }
        }
        emit progress(m_done, m_total, 0, 0, {});
        return;
    }

    m_files = 0;
    for (const QString &source : m_sources) {
        if (m_kind == Kind::Link) {
            ++m_total;
            ++m_files;
            continue;
        }
        m_total += sizeOf(source);
        m_files += filesIn(source);
    }
    emit progress(0, m_total, 0, 0, {});

    const QString destination = QDir::cleanPath(m_destination);
    if (!m_duplicate) {
        const Stat d = statOf(destination);
        if (!d.ok && !QDir().mkpath(destination)) {
            fail(tr("Couldn't create the folder %1.").arg(destination));
            return;
        }
        if (d.ok && !QFileInfo(destination).isDir()) {
            fail(tr("%1 is not a folder.").arg(destination));
            return;
        }
    }
    for (const QString &source : m_sources) {
        if (m_cancelled)
            break;
        const QString clean = QDir::cleanPath(source);
        if (m_kind == Kind::Link) {
            if (waitWhilePaused())
                makeLink(clean, destination);
            continue;
        }
        transfer(clean, m_duplicate ? QFileInfo(clean).path() : destination, true);
    }
    emit progress(m_done, m_total, m_fileDone, m_fileSize, {});
}

FileOperation::Resolution FileOperation::ask(const Conflict &conflict) {
    Resolution r;
    if (m_unattended || !m_resolver || m_cancelled) {
        // Nobody to ask: leave it, and say so at the end.
        r.action = WhenExists::Skip;
        if (!m_cancelled)
            m_skipped << tr("%1 was left alone because the name is taken.").arg(conflict.target);
        return r;
    }
    r = m_resolver(conflict);
    if (r.cancel)
        cancel();
    return r;
}

bool FileOperation::resolve(const QString &source, QString *target, bool *append) {
    const Stat s = statOf(source);
    const Stat t = statOf(*target);
    Conflict c;
    c.source = source;
    c.target = *target;
    c.sourceIsDir = s.isDir();
    c.targetIsDir = t.isDir();
    c.sourceSize = s.isDir() ? 0 : s.st.st_size;
    c.targetSize = t.isDir() ? 0 : t.st.st_size;
    c.sourceModified = modified(s);
    c.targetModified = modified(t);
    c.targetReadOnly = t.isFile() && ::access(QFile::encodeName(*target).constData(), W_OK) != 0;
    const bool folders = c.sourceIsDir && c.targetIsDir;

    WhenExists action = m_policy;
    QString newName;
    bool asked = false;
    // Two folders merge unless the user wants to be asked first.
    if (folders)
        action = m_askMerge && !m_mergeForAll ? WhenExists::Ask : m_mergeAction;
    if (action == WhenExists::Ask && (folders || !m_policyForAll)) {
        const Resolution r = ask(c);
        if (r.cancel)
            return false;
        asked = true;
        action = r.action;
        newName = r.newName;
        if (r.forAll && newName.isEmpty()) {
            if (folders) {
                m_mergeAction = action;
                m_mergeForAll = true;
            } else {
                m_policy = action;
                m_policyForAll = true;
            }
        }
    }

    const QString folder = QFileInfo(*target).path();
    const QString name = QFileInfo(*target).fileName();
    switch (action) {
    case WhenExists::Ask:
    case WhenExists::Skip:
    case WhenExists::Count:
        return false;
    case WhenExists::KeepNewer:
        if (!folders && modified(s) <= modified(t))
            return false;
        [[fallthrough]];
    case WhenExists::Replace:
        if (folders) {
            *append = true; // tells transfer() to merge
            return true;
        }
        if (t.isDir()) {
            fail(tr("Couldn't replace the folder %1 with a file.").arg(*target));
            return false;
        }
        if (s.isDir()) {
            fail(tr("Couldn't replace the file %1 with a folder.").arg(*target));
            return false;
        }
        // A read-only file gets a second look, unless the user has just
        // looked at it.
        if (c.targetReadOnly && m_askReadOnly && !asked && !m_readOnlyForAll) {
            const Resolution r = ask(c);
            if (r.cancel || r.action != WhenExists::Replace)
                return false;
            m_readOnlyForAll = r.forAll;
        }
        return true;
    case WhenExists::RenameNew:
        if (!newName.isEmpty()) {
            if (newName.contains('/') || newName == "." || newName == "..") {
                fail(tr("%1 can't be used as a name.").arg(newName));
                return false;
            }
            if (statOf(folder + '/' + newName).ok) {
                fail(tr("%1 already exists too.").arg(folder + '/' + newName));
                return false;
            }
            *target = folder + '/' + newName;
            return true;
        }
        *target = folder + '/' + freeName(folder, name);
        return true;
    case WhenExists::RenameOld: {
        const QString moved = folder + '/' + freeName(folder, name);
        if (::rename(QFile::encodeName(*target).constData(), QFile::encodeName(moved).constData()) != 0) {
            fail(tr("Couldn't rename %1 out of the way: %2").arg(*target, systemError()));
            return false;
        }
        return true;
    }
    case WhenExists::TrashOld:
        if (!QFile::moveToTrash(*target)) {
            fail(tr("Couldn't move %1 to the trash.").arg(*target));
            return false;
        }
        return true;
    case WhenExists::Resume:
        if (folders) {
            *append = true;
            return true;
        }
        if (!s.isFile() || !t.isFile()) {
            fail(tr("Only files can be resumed: %1").arg(*target));
            return false;
        }
        if (t.st.st_size >= s.st.st_size) {
            // Nothing left to copy, or the other file isn't an earlier part
            // of this one; either way there is nothing to resume.
            if (t.st.st_size > s.st.st_size)
                fail(tr("%1 is larger than the file being copied, so it can't be resumed.").arg(*target));
            return false;
        }
        *append = true;
        return true;
    }
    return false;
}

void FileOperation::transfer(const QString &source, const QString &folder, bool topLevel) {
    if (!waitWhilePaused())
        return;
    const Stat s = statOf(source);
    const QString name = QFileInfo(source).fileName();
    if (!s.ok) {
        fail(tr("%1 is no longer there.").arg(source));
        return;
    }
    const qint64 size = s.isFile() ? s.st.st_size : 0;
    QString target = folder + '/' + name;
    if (m_duplicate && topLevel)
        target = folder + '/' + freeName(folder, name);

    if (target == source && m_kind == Kind::Copy && m_copyOntoItself)
        target = folder + '/' + freeName(folder, name);
    if (target == source) {
        // Moving something to where it already is does nothing.
        if (m_kind == Kind::Copy)
            fail(tr("%1 is already in %2.").arg(name, folder));
        advance(sizeOf(source), name);
        return;
    }
    if (s.isDir() && isInside(target, source)) {
        fail(tr("Couldn't put the folder %1 inside itself.").arg(name));
        advance(sizeOf(source), name);
        return;
    }

    const Stat t = statOf(target);
    bool append = false;
    if (t.ok && s.isDir() && t.isDir()) {
        if (!resolve(source, &target, &append)) {
            advance(sizeOf(source), name);
            return;
        }
        // Still taken: the answer was to merge (rather than a new name or
        // moving the old folder away).
        if (append && statOf(target).isDir()) {
            // Folders merge, and their contents follow the policy.
            const int failuresBefore = int(m_errors.size());
            for (const QString &child : entriesOf(source))
                transfer(source + '/' + child, target, false);
            if (m_kind == Kind::Move && int(m_errors.size()) == failuresBefore && !m_cancelled)
                ::rmdir(QFile::encodeName(source).constData()); // left alone if something was skipped
            if (topLevel)
                m_created << target;
            return;
        }
        append = false;
    } else if (t.ok) {
        if (!resolve(source, &target, &append)) {
            advance(sizeOf(source), name);
            return;
        }
    }

    if (!s.isDir())
        startFile(source, size);
    // Within one filesystem a move is just a rename.
    if (m_kind == Kind::Move && !append) {
        if (::rename(QFile::encodeName(source).constData(), QFile::encodeName(target).constData()) == 0) {
            // A whole folder moves at once; count its files as done.
            if (s.isDir())
                m_fileNumber += filesIn(target);
            advance(sizeOf(target), name);
            if (topLevel)
                m_created << target;
            return;
        }
        if (errno != EXDEV) {
            fail(tr("Couldn't move %1: %2").arg(source, systemError()));
            advance(sizeOf(source), name);
            return;
        }
    }

    bool ok = false;
    if (s.isLink()) {
        ok = copyLink(source, target);
        advance(0, name);
    } else if (s.isDir()) {
        if (::mkdir(QFile::encodeName(target).constData(), 0700) != 0 && errno != EEXIST) {
            fail(tr("Couldn't create the folder %1: %2").arg(target, systemError()));
            advance(sizeOf(source), name);
            return;
        }
        const int failuresBefore = int(m_errors.size());
        for (const QString &child : entriesOf(source))
            transfer(source + '/' + child, target, false);
        keepAttributes(s, target);
        ok = int(m_errors.size()) == failuresBefore && !m_cancelled;
        if (ok && m_kind == Kind::Move && ::rmdir(QFile::encodeName(source).constData()) != 0) {
            fail(tr("Couldn't remove %1 after moving it: %2").arg(source, systemError()));
            ok = false;
        }
        if (topLevel && statOf(target).ok)
            m_created << target;
        return;
    } else if (s.isFile()) {
        ok = copyFile(source, target, append);
    } else {
        fail(tr("%1 is not a regular file, so it wasn't copied.").arg(source));
        advance(size, name);
        return;
    }

    if (ok && m_kind == Kind::Move && ::unlink(QFile::encodeName(source).constData()) != 0)
        fail(tr("Couldn't remove %1 after moving it: %2").arg(source, systemError()));
    if (ok && topLevel)
        m_created << target;
}

bool FileOperation::copyFile(const QString &source, const QString &target, bool append) {
    const Stat s = statOf(source);
    const QString name = QFileInfo(source).fileName();
    QFile in(source);
    if (!in.open(QIODevice::ReadOnly)) {
        fail(tr("Couldn't read %1: %2").arg(source, in.errorString()));
        advance(s.st.st_size, name);
        return false;
    }

    // A new or replaced file is written beside the target and renamed into
    // place at the end, so a failed copy never leaves half a file behind
    // or destroys the one it was replacing.
    QSaveFile save(target);
    QFile appendFile(target);
    QIODevice *out = &save;
    qint64 skipped = 0;
    if (append) {
        out = &appendFile;
        if (!appendFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
            fail(tr("Couldn't write %1: %2").arg(target, appendFile.errorString()));
            advance(s.st.st_size, name);
            return false;
        }
        skipped = appendFile.size();
        in.seek(skipped);
        advance(skipped, name);
    } else {
        // Replacing a read-only file has been agreed to by now; the new
        // file is renamed over it, but QSaveFile wants it writable first.
        const QByteArray path = QFile::encodeName(target);
        const Stat t = statOf(target);
        if (t.isFile() && ::access(path.constData(), W_OK) != 0)
            ::chmod(path.constData(), (t.st.st_mode & 07777) | S_IWUSR);
    }
    if (!append && !save.open(QIODevice::WriteOnly)) {
        fail(tr("Couldn't write %1: %2").arg(target, save.errorString()));
        advance(s.st.st_size, name);
        return false;
    }

    QByteArray buffer;
    qint64 copied = skipped;
    m_fileDone = skipped;
    while (!in.atEnd()) {
        waitWhilePaused();
        if (m_skipCurrent.exchange(false) && !m_cancelled) {
            if (!append)
                save.cancelWriting();
            m_skipped << tr("%1 was skipped part way through.").arg(source);
            advance(s.st.st_size - copied, name);
            return false;
        }
        if (m_cancelled) {
            if (!append)
                save.cancelWriting();
            advance(s.st.st_size - copied, name);
            return false;
        }
        buffer = in.read(kChunk);
        if (buffer.isEmpty() && in.error() != QFile::NoError) {
            fail(tr("Couldn't read %1: %2").arg(source, in.errorString()));
            if (!append)
                save.cancelWriting();
            advance(s.st.st_size - copied, name);
            return false;
        }
        if (out->write(buffer) != buffer.size()) {
            fail(tr("Couldn't write %1: %2").arg(target, out->errorString()));
            if (!append)
                save.cancelWriting();
            advance(s.st.st_size - copied, name);
            return false;
        }
        copied += buffer.size();
        m_fileDone = copied;
        advance(buffer.size(), name);
    }
    if (append) {
        appendFile.close();
    } else if (!save.commit()) {
        fail(tr("Couldn't write %1: %2").arg(target, save.errorString()));
        return false;
    }
    keepAttributes(s, target);
    return true;
}

bool FileOperation::copyLink(const QString &source, const QString &target) {
    // The link is copied as it is, relative or absolute, not what it
    // points to.
    QByteArray link(4096, '\0');
    const ssize_t length = ::readlink(QFile::encodeName(source).constData(), link.data(), link.size());
    if (length < 0) {
        fail(tr("Couldn't read the link %1: %2").arg(source, systemError()));
        return false;
    }
    link.truncate(length);
    const QByteArray path = QFile::encodeName(target);
    if (statOf(target).ok && ::unlink(path.constData()) != 0) {
        fail(tr("Couldn't replace %1: %2").arg(target, systemError()));
        return false;
    }
    if (::symlink(link.constData(), path.constData()) != 0) {
        fail(tr("Couldn't create the link %1: %2").arg(target, systemError()));
        return false;
    }
    return true;
}

void FileOperation::makeLink(const QString &source, const QString &folder) {
    const QString name = QFileInfo(source).fileName();
    startFile(source, 0);
    const Stat s = statOf(source);
    if (!s.ok) {
        fail(tr("%1 is no longer there.").arg(source));
        advance(1, name);
        return;
    }
    const QString from = QFileInfo(QFileInfo(source).path()).canonicalFilePath();
    const QString to = QFileInfo(folder).canonicalFilePath();
    const QString item = from == "/" ? from + name : from + '/' + name;
    QString linkName = from == to ? tr("Link to %1").arg(name) : name;
    if (statOf(to + '/' + linkName).ok)
        linkName = freeName(to, linkName);
    const QByteArray target = QFile::encodeName(to + '/' + linkName);
    int result = -1;
    switch (m_linkType) {
    case LinkType::Symbolic:
        result = ::symlink(QFile::encodeName(item).constData(), target.constData());
        break;
    case LinkType::Relative:
        result = ::symlink(QFile::encodeName(QDir(to).relativeFilePath(item)).constData(), target.constData());
        break;
    case LinkType::Hard:
        if (s.isDir()) {
            fail(tr("%1 is a folder, which can't have hard links.").arg(name));
            advance(1, name);
            return;
        }
        result = ::link(QFile::encodeName(item).constData(), target.constData());
        break;
    }
    const int error = errno;
    if (result == 0)
        m_created << to + '/' + linkName;
    else if (m_linkType == LinkType::Hard && error == EXDEV)
        fail(tr("%1 is on another drive, so it can't have a hard link here.").arg(name));
    else
        fail(tr("Couldn't create a link to %1: %2").arg(name, QString::fromLocal8Bit(std::strerror(error))));
    advance(1, name);
}

bool FileOperation::removeTree(const QString &path) {
    const Stat s = statOf(path);
    const QString name = QFileInfo(path).fileName();
    if (!s.isDir())
        startFile(path, 0);
    bool ok = true;
    if (s.isDir()) {
        for (const QString &child : entriesOf(path)) {
            if (!waitWhilePaused())
                return false;
            ok = removeTree(path + '/' + child) && ok;
        }
        if (ok && ::rmdir(QFile::encodeName(path).constData()) != 0) {
            fail(tr("Couldn't delete %1: %2").arg(path, systemError()));
            ok = false;
        }
    } else if (s.isFile() && !m_deleteConfirm.readOnlyWithoutAsking &&
               ::access(QFile::encodeName(path).constData(), W_OK) != 0 &&
               !confirmDelete(path, DeleteQuestion::Reason::ReadOnly)) {
        ok = false; // kept, so the folders above it stay too
    } else if (::unlink(QFile::encodeName(path).constData()) != 0) {
        fail(tr("Couldn't delete %1: %2").arg(path, systemError()));
        ok = false;
    }
    advance(1, name);
    return ok;
}
