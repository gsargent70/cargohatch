#include "favorites.h"

#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "folderaliases.h"

namespace {

const char *const kTypes[] = {"folder", "file", "branch", "separator"};
// Branches inside branches, far more than anyone makes by hand.
constexpr int kMaxDepth = 32;

QJsonArray write(const QList<Favorite> &list) {
    QJsonArray array;
    for (const Favorite &f : list) {
        QJsonObject o{{"type", kTypes[int(f.type)]}};
        if (!f.name.isEmpty())
            o["name"] = f.name;
        if (!f.path.isEmpty())
            o["path"] = f.path;
        if (f.type == Favorite::Type::Branch)
            o["children"] = write(f.children);
        array << o;
    }
    return array;
}

QList<Favorite> read(const QJsonArray &array, int depth = 0) {
    QList<Favorite> list;
    for (const QJsonValue &value : array) {
        const QJsonObject o = value.toObject();
        Favorite f;
        bool known = false;
        for (int i = 0; i < 4; ++i)
            if (o["type"].toString() == QLatin1String(kTypes[i])) {
                f.type = Favorite::Type(i);
                known = true;
            }
        f.name = o["name"].toString();
        f.path = o["path"].toString();
        if (f.type == Favorite::Type::Branch && depth < kMaxDepth)
            f.children = read(o["children"].toArray(), depth + 1);
        const bool place = f.type == Favorite::Type::Folder || f.type == Favorite::Type::File;
        if (known && !(place && f.path.isEmpty()))
            list << f;
    }
    return list;
}

} // namespace

bool Favorite::operator==(const Favorite &o) const {
    return type == o.type && name == o.name && path == o.path && children == o.children;
}

QString Favorite::title() const {
    if (!name.isEmpty() || type == Type::Separator)
        return name;
    const QString last = QFileInfo(path).fileName();
    return last.isEmpty() ? path : last;
}

QString Favorite::realPath() const {
    return QDir::cleanPath(FolderAliases::expand(path));
}

Favorite Favorite::folder(const QString &path, const QString &name) {
    Favorite f;
    f.path = path;
    f.name = name;
    return f;
}

Favorite Favorite::branch(const QString &name, const QList<Favorite> &children) {
    Favorite f;
    f.type = Type::Branch;
    f.name = name;
    f.children = children;
    return f;
}

Favorite Favorite::separator() {
    Favorite f;
    f.type = Type::Separator;
    return f;
}

QByteArray FavoriteList::toJson() const {
    return QJsonDocument(QJsonObject{{"bar", write(bar)}, {"items", write(items)}}).toJson(QJsonDocument::Compact);
}

FavoriteList FavoriteList::fromJson(const QByteArray &json) {
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    FavoriteList list;
    list.bar = read(o["bar"].toArray());
    list.items = read(o["items"].toArray());
    return list;
}

void FavoriteList::sort(QList<Favorite> *list, bool inside) {
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    auto before = [&collator](const Favorite &a, const Favorite &b) {
        const bool branchA = a.type == Favorite::Type::Branch;
        const bool branchB = b.type == Favorite::Type::Branch;
        if (branchA != branchB)
            return branchA;
        return collator.compare(a.title(), b.title()) < 0;
    };
    auto start = list->begin();
    for (auto it = list->begin();; ++it) {
        if (it == list->end() || it->type == Favorite::Type::Separator) {
            std::stable_sort(start, it, before);
            if (it == list->end())
                break;
            start = it + 1;
        }
    }
    if (inside)
        for (Favorite &f : *list)
            sort(&f.children, true);
}
