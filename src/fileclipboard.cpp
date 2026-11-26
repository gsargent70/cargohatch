#include "fileclipboard.h"

#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QMimeData>
#include <QPointer>
#include <QUrl>

namespace {

const QString kGnome = QStringLiteral("x-special/gnome-copied-files");
const QString kKdeCut = QStringLiteral("application/x-kde-cutselection");

// The same file by one fixed path: links in the folders above it are
// followed, but if the item is itself a link, it stays one.
// `folders` keeps what each folder turned out to be.
QString fixedPath(const QString &path, QHash<QString, QString> *folders) {
    const QFileInfo info(path);
    auto known = folders->constFind(info.path());
    if (known == folders->constEnd())
        known = folders->insert(info.path(), QFileInfo(info.path()).canonicalFilePath());
    const QString folder = *known;
    if (folder.isEmpty() || info.fileName().isEmpty())
        return path;
    return folder == "/" ? folder + info.fileName() : folder + '/' + info.fileName();
}

bool isCutData(const QMimeData *mime) {
    if (mime->hasFormat(kGnome))
        return mime->data(kGnome).startsWith("cut");
    return mime->hasFormat(kKdeCut) && mime->data(kKdeCut).startsWith('1');
}

} // namespace

FileClipboard *FileClipboard::instance() {
    static QPointer<FileClipboard> clipboard;
    if (!clipboard)
        clipboard = new FileClipboard(qApp);
    return clipboard;
}

FileClipboard::FileClipboard(QObject *parent) : QObject(parent) {
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &FileClipboard::update);
    update();
}

FileClipboard::Files FileClipboard::files() const {
    return read(QGuiApplication::clipboard()->mimeData());
}

FileClipboard::Files FileClipboard::read(const QMimeData *mime) {
    Files files;
    if (!mime)
        return files;
    QList<QUrl> urls = mime->urls();
    if (urls.isEmpty() && mime->hasFormat(kGnome)) {
        // "copy" or "cut", then a URL on each line.
        const QList<QByteArray> lines = mime->data(kGnome).split('\n');
        for (int i = 1; i < lines.size(); ++i)
            if (!lines.at(i).trimmed().isEmpty())
                urls << QUrl::fromEncoded(lines.at(i).trimmed());
    }
    QHash<QString, QString> folders;
    QSet<QString> seen;
    for (const QUrl &url : std::as_const(urls)) {
        if (!url.isLocalFile()) {
            ++files.remote;
            continue;
        }
        QString path = url.toLocalFile();
        while (path.size() > 1 && path.endsWith('/'))
            path.chop(1);
        path = fixedPath(path, &folders);
        if (path != "/" && !seen.contains(path)) {
            seen.insert(path);
            files.paths << path;
        }
    }
    files.cut = !files.paths.isEmpty() && isCutData(mime);
    return files;
}

void FileClipboard::setFiles(const QStringList &paths, bool cut) {
    QList<QUrl> urls;
    QByteArray gnome = cut ? "cut" : "copy";
    for (const QString &path : paths) {
        urls << QUrl::fromLocalFile(path);
        gnome += '\n' + urls.last().toEncoded();
    }
    auto *mime = new QMimeData;
    mime->setUrls(urls);
    mime->setData(kGnome, gnome);
    if (cut)
        mime->setData(kKdeCut, "1");
    // Pasted into a text editor or terminal, it is the paths.
    mime->setText(paths.join('\n'));
    QGuiApplication::clipboard()->setMimeData(mime);
}

void FileClipboard::setText(const QString &text) {
    QGuiApplication::clipboard()->setText(text);
}

void FileClipboard::clear() {
    QGuiApplication::clipboard()->clear();
}

void FileClipboard::movedAway(const Files &pasted) {
    if (!(files() == pasted))
        return;
    QStringList left;
    for (const QString &path : pasted.paths) {
        const QFileInfo info(path);
        if (info.exists() || info.isSymLink())
            left << path;
    }
    if (left.isEmpty())
        clear();
    else if (left != pasted.paths)
        setFiles(left, true);
}

void FileClipboard::update() {
    QSet<QString> cut;
    // Only looks further when the clipboard holds cut files, so copying a
    // large picture elsewhere costs nothing here.
    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    if (mime && isCutData(mime)) {
        const QStringList paths = files().paths;
        cut = QSet<QString>(paths.begin(), paths.end());
    }
    if (cut == m_cut)
        return;
    m_cut = cut;
    emit cutChanged();
}
