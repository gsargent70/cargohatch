#include "batchrename.h"

#include "foldertab.h"
#include "renamedelegate.h"

#include <QCoreApplication>
#include <QHash>
#include <QUuid>

namespace {

QString tr(const char *text) {
    return QCoreApplication::translate("BatchRename", text);
}

// Letters after these start a new word; an apostrophe doesn't ("Don't").
bool startsWord(QChar before) {
    return !before.isLetterOrNumber() && before != '\'' && before != QChar(0x2019);
}

} // namespace

QString BatchRename::changeCase(const QString &text, Case how) {
    switch (how) {
    case Case::Keep:
        return text;
    case Case::Lower:
        return text.toLower();
    case Case::Upper:
        return text.toUpper();
    case Case::FirstWord: {
        QString out = text.toLower();
        for (QChar &c : out)
            if (c.isLetter()) {
                c = c.toUpper();
                break;
            }
        return out;
    }
    case Case::EveryWord: {
        QString out = text.toLower();
        for (int i = 0; i < out.size(); ++i)
            if (out.at(i).isLetter() && (i == 0 || startsWord(out.at(i - 1))))
                out[i] = out.at(i).toUpper();
        return out;
    }
    }
    return text;
}

QString BatchRename::apply(const QString &name, bool isDir, bool *matched) const {
    // Folders have no extension: "Photos.2026" is all name.
    const int dot = isDir ? -1 : RenameDelegate::extensionStart(name);
    const bool split = ignoreExtension && dot >= 0;
    QString stem = split ? name.left(dot) : name;
    const QString extension = split ? name.mid(dot) : QString();
    const Qt::CaseSensitivity cs = matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
    const bool found = find.isEmpty() || stem.contains(find, cs);
    if (matched)
        *matched = found;
    if (!found && !applyWithoutMatch)
        return name;
    if (found && !find.isEmpty())
        stem.replace(find, replace, cs);
    QString result = changeCase(stem, nameCase) + extension;
    if (!isDir && extensionCase != ExtensionCase::Keep) {
        const int newDot = RenameDelegate::extensionStart(result);
        if (newDot >= 0) {
            const QString ext = result.mid(newDot);
            result = result.left(newDot) + (extensionCase == ExtensionCase::Upper ? ext.toUpper() : ext.toLower());
        }
    }
    return result;
}

QList<BatchRename::Item> BatchRename::plan(const QList<QPair<QString, bool>> &items, const QSet<QString> &others) const {
    QList<Item> out;
    QHash<QString, int> count;
    for (const auto &[name, isDir] : items) {
        Item item;
        item.oldName = name;
        item.isDir = isDir;
        item.newName = apply(name, isDir, &item.matched).trimmed();
        if (item.newName == name.trimmed())
            item.newName = name;
        ++count[item.newName];
        out << item;
    }
    for (Item &item : out) {
        if (!item.changes())
            continue;
        if (item.newName.isEmpty())
            item.problem = tr("A name can't be empty.");
        else if (item.newName.contains('/'))
            item.problem = tr("A name can't contain /.");
        else if (item.newName == "." || item.newName == "..")
            item.problem = tr("%1 can't be used as a name.").arg(item.newName);
        else if (item.newName.startsWith('.') && !item.oldName.startsWith('.'))
            item.problem = tr("%1 would be hidden, as it would start with a dot.").arg(item.newName);
        else if (count.value(item.newName) > 1)
            item.problem = tr("Two items would both be called %1.").arg(item.newName);
        else if (others.contains(item.newName))
            item.problem = tr("%1 already exists.").arg(item.newName);
    }
    return out;
}

QStringList BatchRename::perform(const QString &folder, const QList<Item> &items, QStringList *renamed) {
    QList<Item> changing;
    QSet<QString> leaving;
    for (const Item &item : items)
        if (item.changes() && item.problem.isEmpty()) {
            changing << item;
            leaving.insert(item.oldName);
        }
    // A new name that another item is leaving, or one that differs only in
    // case (the same file on some filesystems), needs a step in between.
    bool twoSteps = false;
    for (const Item &item : changing)
        if (leaving.contains(item.newName) || item.newName.compare(item.oldName, Qt::CaseInsensitive) == 0)
            twoSteps = true;

    QStringList errors;
    QStringList current;
    for (const Item &item : changing)
        current << item.oldName;
    if (twoSteps) {
        const QString tag = QUuid::createUuid().toString(QUuid::Id128).left(12);
        for (int i = 0; i < changing.size(); ++i) {
            const QString temporary = QString(".cargohatch-rename-%1-%2").arg(tag).arg(i);
            const QString error = FolderTab::renameOnDisk(folder, current.at(i), temporary);
            if (error.isEmpty())
                current[i] = temporary;
            else
                errors << error;
        }
    }
    for (int i = 0; i < changing.size(); ++i) {
        const Item &item = changing.at(i);
        if (twoSteps && current.at(i) == item.oldName)
            continue; // it never left, and its error is listed
        const QString error = FolderTab::renameOnDisk(folder, current.at(i), item.newName);
        if (error.isEmpty()) {
            if (renamed)
                *renamed << item.newName;
            continue;
        }
        errors << error;
        // Back to where it was rather than left under a temporary name.
        if (current.at(i) != item.oldName && !FolderTab::renameOnDisk(folder, current.at(i), item.oldName).isEmpty())
            errors << tr("%1 is now called %2.").arg(item.oldName, current.at(i));
    }
    return errors;
}
