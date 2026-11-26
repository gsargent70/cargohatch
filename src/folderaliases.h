#pragma once

#include <QList>
#include <QString>

// Short names for folders (NAV-07), used wherever a path is typed: ~downloads
// for the downloads folder, as zsh's named folders work. Plain ~ and ~user
// are home folders as usual.
namespace FolderAliases {

struct Alias {
    QString name;
    // May itself start with an alias.
    QString path;
    bool operator==(const Alias &o) const { return name == o.name && path == o.path; }
};

// The ones that are always there: home, the XDG user folders (downloads,
// documents and any of the user's own), config, data, cache, trash, temp…
QList<Alias> builtIn();

// The user's own, which are the same in every window.
QList<Alias> user();
void setUser(const QList<Alias> &aliases);
// Both, in name order. The user's replace built-in ones of the same name.
QList<Alias> all();

// What's wrong with a name, or empty.
QString nameError(const QString &name);
// A name that could stand for `path`.
QString suggestName(const QString &path);

// A typed path made into a real one: aliases and home folders are filled
// in, quotes and file:// addresses taken off. A relative path is taken from
// `base`, if there is one. /downloads works too, unless there really is a
// folder by that name.
QString expand(const QString &typed, const QString &base = {});

// `path` with the longest alias that fits in front, or as it is.
QString shorten(const QString &path);

} // namespace FolderAliases
