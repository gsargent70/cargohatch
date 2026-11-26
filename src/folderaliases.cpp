#include "folderaliases.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#include <pwd.h>

namespace {

QList<FolderAliases::Alias> &userAliases() {
    static QList<FolderAliases::Alias> aliases;
    return aliases;
}

// The folders named in the desktop's user-dirs file, which may have more
// than the standard ones.
QList<FolderAliases::Alias> xdgUserDirs() {
    QList<FolderAliases::Alias> list;
    QFile file(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/user-dirs.dirs");
    if (!file.open(QIODevice::ReadOnly))
        return list;
    static const QRegularExpression line(QStringLiteral("^\\s*XDG_([A-Z0-9_]+)_DIR\\s*=\\s*\"(.*)\"\\s*$"),
                                         QRegularExpression::MultilineOption);
    auto it = line.globalMatch(QString::fromUtf8(file.read(1 << 16)));
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        QString name = m.captured(1).toLower().replace('_', '-');
        if (name == "download")
            name = QStringLiteral("downloads");
        else if (name == "publicshare")
            name = QStringLiteral("public");
        QString path = m.captured(2);
        path.replace(QRegularExpression(QStringLiteral("\\\\(.)")), QStringLiteral("\\1"));
        if (path == "$HOME" || path.startsWith("$HOME/"))
            path = QDir::homePath() + path.mid(5);
        path = QDir::cleanPath(path);
        // Pointing one at the home folder itself is how it is turned off.
        if (QDir::isAbsolutePath(path) && path != QDir::homePath())
            list.append({name, path});
    }
    return list;
}

bool contains(const QList<FolderAliases::Alias> &list, const QString &name) {
    for (const auto &a : list)
        if (a.name.compare(name, Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

// The path `name` stands for, or a null string.
QString pathFor(const QString &name) {
    for (const auto &a : FolderAliases::user())
        if (a.name.compare(name, Qt::CaseInsensitive) == 0)
            return a.path;
    for (const auto &a : FolderAliases::builtIn())
        if (a.name.compare(name, Qt::CaseInsensitive) == 0)
            return a.path;
    return {};
}

QString homeOf(const QString &user) {
    passwd pw;
    passwd *found = nullptr;
    char buffer[4096];
    if (getpwnam_r(user.toLocal8Bit().constData(), &pw, buffer, sizeof buffer, &found) == 0 && found)
        return QFile::decodeName(found->pw_dir);
    return {};
}

// Fills in a ~ at the start. `depth` stops aliases that go in circles.
QString fillIn(const QString &path, int depth) {
    if (!path.startsWith('~'))
        return path;
    const int slash = int(path.indexOf('/'));
    const QString name = path.mid(1, slash < 0 ? -1 : slash - 1);
    const QString rest = slash < 0 ? QString() : path.mid(slash);
    if (name.isEmpty())
        return QDir::homePath() + rest;
    QString target = pathFor(name);
    if (!target.isNull())
        target = depth > 0 ? fillIn(target, depth - 1) : QString();
    else
        target = homeOf(name);
    if (target.isEmpty())
        return path;
    return target == "/" && !rest.isEmpty() ? rest : target + rest;
}

constexpr int kMaxDepth = 8;

} // namespace

QList<FolderAliases::Alias> FolderAliases::builtIn() {
    using P = QStandardPaths;
    QList<Alias> list{{QStringLiteral("home"), QDir::homePath()}};
    for (const Alias &a : xdgUserDirs())
        if (!contains(list, a.name))
            list << a;
    const QString data = P::writableLocation(P::GenericDataLocation);
    const std::pair<const char *, QString> others[] = {
        {"desktop", P::writableLocation(P::DesktopLocation)},
        {"documents", P::writableLocation(P::DocumentsLocation)},
        {"downloads", P::writableLocation(P::DownloadLocation)},
        {"music", P::writableLocation(P::MusicLocation)},
        {"pictures", P::writableLocation(P::PicturesLocation)},
        {"videos", P::writableLocation(P::MoviesLocation)},
        {"config", P::writableLocation(P::GenericConfigLocation)},
        {"data", data},
        {"cache", P::writableLocation(P::GenericCacheLocation)},
        {"trash", data + "/Trash/files"},
        {"temp", P::writableLocation(P::TempLocation)},
        {"media", "/run/media/" + qEnvironmentVariable("USER")},
    };
    for (const auto &[name, path] : others)
        // Qt gives the home folder for those that are turned off.
        if (!path.isEmpty() && path != QDir::homePath() && !contains(list, name))
            list.append({name, QDir::cleanPath(path)});
    return list;
}

QList<FolderAliases::Alias> FolderAliases::user() {
    return userAliases();
}

void FolderAliases::setUser(const QList<Alias> &aliases) {
    userAliases() = aliases;
}

QList<FolderAliases::Alias> FolderAliases::all() {
    QList<Alias> list = user();
    for (const Alias &a : builtIn())
        if (!contains(list, a.name))
            list << a;
    std::sort(list.begin(), list.end(),
              [](const Alias &a, const Alias &b) { return a.name.compare(b.name, Qt::CaseInsensitive) < 0; });
    return list;
}

QString FolderAliases::nameError(const QString &name) {
    if (name.isEmpty())
        return QCoreApplication::translate("FolderAliases", "Type a name for the alias.");
    static const QRegularExpression good(QStringLiteral("\\A[\\w.-]+\\z"),
                                         QRegularExpression::UseUnicodePropertiesOption);
    if (!good.match(name).hasMatch() || name.startsWith('.'))
        return QCoreApplication::translate("FolderAliases",
                                           "A name can have letters, digits, hyphens, underscores and dots.");
    return {};
}

QString FolderAliases::suggestName(const QString &path) {
    QString name = QFileInfo(path).fileName().toLower();
    name.replace(QRegularExpression(QStringLiteral("[^\\w.-]+"), QRegularExpression::UseUnicodePropertiesOption),
                 QStringLiteral("-"));
    while (name.startsWith('-') || name.startsWith('.'))
        name.remove(0, 1);
    while (name.endsWith('-'))
        name.chop(1);
    return name;
}

QString FolderAliases::expand(const QString &typed, const QString &base) {
    QString path = typed.trimmed();
    if (path.size() > 1 && (path.startsWith('"') || path.startsWith('\'')) && path.endsWith(path.at(0)))
        path = path.mid(1, path.size() - 2);
    if (path.startsWith("file://"))
        path = QUrl(path).toLocalFile();
    if (path.isEmpty())
        return {};
    if (path.startsWith('~')) {
        const QString filled = fillIn(path, kMaxDepth);
        if (filled != path)
            return filled;
        // Not an alias: a folder here might really be called that.
    } else if (path.startsWith('/')) {
        const int slash = int(path.indexOf('/', 1));
        const QString name = path.mid(1, slash < 0 ? -1 : slash - 1);
        const QFileInfo real('/' + name);
        if (!name.isEmpty() && !real.exists() && !real.isSymLink() && !pathFor(name).isNull()) {
            const QString filled = fillIn('~' + path.mid(1), kMaxDepth);
            if (!filled.startsWith('~'))
                return filled;
        }
        return path;
    }
    return base.isEmpty() ? path : base == "/" ? '/' + path : base + '/' + path;
}

QString FolderAliases::shorten(const QString &path) {
    QString best, name;
    for (const Alias &a : all()) {
        const QString folder = QDir::cleanPath(expand(a.path));
        if (folder.size() > best.size() && folder != "/" && (path == folder || path.startsWith(folder + '/'))) {
            best = folder;
            name = a.name;
        }
    }
    if (best.isEmpty())
        return path;
    // ~ says it for the home folder.
    return (best == QDir::homePath() ? QStringLiteral("~") : '~' + name) + path.mid(best.size());
}
