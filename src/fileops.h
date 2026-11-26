#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QObject>
#include <QStringList>

#include <atomic>
#include <functional>

// What to do when a copied or moved item's name is already taken in the
// destination (COPY-02). Folders that meet folders are always merged; the
// policy decides for the files inside them.
enum class WhenExists {
    Ask,
    Skip,
    Replace,
    // Replace only if the incoming file is newer.
    KeepNewer,
    // Give the incoming item a free name: "name (2).ext".
    RenameNew,
    // Give the existing item a free name, then copy.
    RenameOld,
    // Carry on from where a shorter, earlier copy stopped.
    Resume,
    // Move the existing item to the trash, then copy.
    TrashOld,
    Count,
};

namespace WhenExistsNames {
QString id(WhenExists policy);
WhenExists fromId(const QString &id);
QString title(WhenExists policy);
} // namespace WhenExistsNames

// Copies, moves, links, trashes or deletes files (COPY-01, COPY-02, DEL-01,
// CLIP-01).
//
// run() does the work and blocks, so the GUI calls it on a worker thread
// and listens to progress(); tests call it directly. Errors don't stop the
// operation: each one is recorded and the rest carry on.
class FileOperation : public QObject {
    Q_OBJECT

public:
    enum class Kind { Copy, Move, Trash, Delete, Link };
    // What a link made by Kind::Link is.
    enum class LinkType { Symbolic, Relative, Hard };

    // An existing item in the way of an incoming one.
    struct Conflict {
        QString source;
        QString target;
        bool sourceIsDir = false;
        bool targetIsDir = false;
        qint64 sourceSize = 0;
        qint64 targetSize = 0;
        QDateTime sourceModified;
        QDateTime targetModified;
        // The existing file can't be written to, so replacing it deserves
        // a second thought (COPY-03).
        bool targetReadOnly = false;
    };
    // The answer to a conflict. `forAll` applies it to the rest as well.
    // For two folders, Replace means merge. A `newName` given with
    // RenameNew is used instead of a free name.
    struct Resolution {
        WhenExists action = WhenExists::Skip;
        bool forAll = false;
        bool cancel = false;
        QString newName;
    };
    using Resolver = std::function<Resolution(const Conflict &)>;

    // A permanent delete asking about one item (DEL-02).
    struct DeleteQuestion {
        enum class Reason { File, Folder, ReadOnly };
        QString path;
        Reason reason = Reason::File;
    };
    enum class DeleteAnswer { Delete, DeleteAll, Skip, Cancel };
    using DeleteResolver = std::function<DeleteAnswer(const DeleteQuestion &)>;
    struct DeleteConfirmation {
        bool eachFile = false;
        bool eachFolder = false;
        // Files the user can't write to are deleted without asking.
        bool readOnlyWithoutAsking = false;
    };

    // `destination` is the folder to copy or move into; it is created if it
    // doesn't exist. Trash and Delete ignore it.
    FileOperation(Kind kind, const QStringList &sources, const QString &destination = {}, QObject *parent = nullptr);

    Kind kind() const { return m_kind; }
    QStringList sources() const { return m_sources; }
    QString destination() const { return m_destination; }

    void setWhenExists(WhenExists policy) { m_policy = policy; }
    // Asked when the policy is Ask. Without one, Ask skips.
    void setResolver(Resolver resolver) { m_resolver = std::move(resolver); }
    // Copies into the items' own folder under free names (COPY-01 "here").
    void setDuplicate(bool duplicate) { m_duplicate = duplicate; }
    // Copying an item into the folder it is already in makes a copy under a
    // free name, instead of being an error. For pasting (CLIP-01).
    void setCopyOntoItself(bool copy) { m_copyOntoItself = copy; }
    void setLinkType(LinkType type) { m_linkType = type; }
    // Ask before merging two folders, instead of merging them quietly.
    void setAskBeforeMerging(bool ask) { m_askMerge = ask; }
    // Ask before replacing a file that is read-only, even when the policy
    // says to replace.
    void setAskBeforeReplacingReadOnly(bool ask) { m_askReadOnly = ask; }
    void setDeleteConfirmation(const DeleteConfirmation &confirm) { m_deleteConfirm = confirm; }
    void setDeleteResolver(DeleteResolver resolver) { m_deleteResolver = std::move(resolver); }

    void run();
    // Safe to call from any thread; stops at the next file or chunk.
    void cancel() { m_cancelled = true; }
    bool isCancelled() const { return m_cancelled; }
    // COPY-08. Also safe from any thread. Pausing holds the work at the
    // next chunk; skipping drops the file being copied.
    void setPaused(bool paused) { m_paused = paused; }
    bool isPaused() const { return m_paused; }
    void skipCurrent() { m_skipCurrent = true; }
    // Unattended, anything that would ask is skipped instead, and listed
    // in skipped().
    void setUnattended(bool unattended) { m_unattended = unattended; }
    bool isUnattended() const { return m_unattended; }

    // One line per item that failed, saying why.
    QStringList errors() const { return m_errors; }
    // The top-level items that now exist in the destination.
    QStringList created() const { return m_created; }
    // Items left alone without the user choosing to, because the operation
    // was unattended or the file was skipped from the progress window.
    QStringList skipped() const { return m_skipped; }

    // A name like `name` that isn't taken in `folder`: "photo (2).jpg".
    static QString freeName(const QString &folder, const QString &name);

signals:
    // `done` and `total` are bytes for copies and moves, items otherwise;
    // `fileDone` and `fileSize` are for the file being copied.
    void progress(qint64 done, qint64 total, qint64 fileDone, qint64 fileSize, const QString &name);
    // Another file has started: `number` of `count`, counting from 1.
    void fileStarted(int number, int count, const QString &path);

private:
    void transfer(const QString &source, const QString &folder, bool topLevel);
    bool copyFile(const QString &source, const QString &target, bool append);
    bool copyLink(const QString &source, const QString &target);
    // Links never replace anything: a taken name gets a number.
    void makeLink(const QString &source, const QString &folder);
    bool removeTree(const QString &path);
    // Decides what to do about `target` being taken. Returns false to skip,
    // with `target` possibly changed to a free name and `append` set for a
    // resume.
    bool resolve(const QString &source, QString *target, bool *append);
    void fail(const QString &message);
    void advance(qint64 amount, const QString &name);
    void startFile(const QString &path, qint64 size);
    // Waits while paused; false if cancelled meanwhile.
    bool waitWhilePaused();
    Resolution ask(const Conflict &conflict);
    // Whether a permanent delete may go ahead with `path`.
    bool confirmDelete(const QString &path, DeleteQuestion::Reason reason);

    Kind m_kind;
    QStringList m_sources;
    QString m_destination;
    WhenExists m_policy = WhenExists::Ask;
    bool m_policyForAll = false;
    Resolver m_resolver;
    bool m_duplicate = false;
    bool m_copyOntoItself = false;
    LinkType m_linkType = LinkType::Symbolic;
    std::atomic<bool> m_cancelled{false};
    QStringList m_errors;
    QStringList m_created;
    qint64 m_total = 0;
    qint64 m_done = 0;
    QElapsedTimer m_clock;
    QElapsedTimer m_fileClock;
    bool m_askMerge = false;
    bool m_askReadOnly = true;
    bool m_readOnlyForAll = false;
    WhenExists m_mergeAction = WhenExists::Replace;
    bool m_mergeForAll = false;
    DeleteConfirmation m_deleteConfirm;
    DeleteResolver m_deleteResolver;
    bool m_deleteAll[3] = {false, false, false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_skipCurrent{false};
    std::atomic<bool> m_unattended{false};
    QStringList m_skipped;
    int m_files = 0;
    int m_fileNumber = 0;
    qint64 m_fileDone = 0;
    qint64 m_fileSize = 0;
};
