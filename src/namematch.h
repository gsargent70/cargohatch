#pragma once

#include <QList>
#include <QString>

// Comparing names without their accents, so "resume" finds "Résumé".
namespace NameMatch {

struct Stripped {
    QString text;
    // Where each character of `text` was in the original, with the length
    // of the original at the end. Empty if nothing was taken out.
    QList<int> at;

    // The original position for position `i` in `text`.
    int original(int i) const { return at.isEmpty() ? i : at.at(i); }
};

// `name` as it is, or without accents.
Stripped strip(const QString &name, bool withoutAccents);

// How much of a name is compared when extensions are ignored: up to the
// last dot of a file's name, or all of a folder's.
int lengthWithoutExtension(const QString &name, bool isDir);

} // namespace NameMatch
