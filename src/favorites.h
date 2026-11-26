#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

// A favourite folder or file, a branch holding more, or a line between them
// (FAV-01).
struct Favorite {
    enum class Type { Folder, File, Branch, Separator };
    Type type = Type::Folder;
    QString name;
    // May start with ~ or an alias.
    QString path;
    QList<Favorite> children;

    bool operator==(const Favorite &o) const;
    // The name, or the folder's own if it has none.
    QString title() const;
    QString realPath() const;

    static Favorite folder(const QString &path, const QString &name = {});
    static Favorite branch(const QString &name, const QList<Favorite> &children = {});
    static Favorite separator();
};

// All of them: those on the favourites bar, and the rest.
struct FavoriteList {
    QList<Favorite> bar;
    QList<Favorite> items;

    bool operator==(const FavoriteList &o) const { return bar == o.bar && items == o.items; }
    bool isEmpty() const { return bar.isEmpty() && items.isEmpty(); }

    QByteArray toJson() const;
    static FavoriteList fromJson(const QByteArray &json);

    // Branches first, then by name; separators stay where they are, and
    // each stretch between them is sorted by itself.
    static void sort(QList<Favorite> *list, bool inside = false);
};
