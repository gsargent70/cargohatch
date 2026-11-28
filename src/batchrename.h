#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

// Renaming several items at once (REN-03 in part): find and replace in the
// names, change their capitalisation (REN-05), with the options of REN-10.
// Working out the new names is separate from renaming on disk, so the dialog
// can show what will happen before anything does.
struct BatchRename {
    enum class Case { Keep, Lower, Upper, FirstWord, EveryWord };
    enum class ExtensionCase { Keep, Lower, Upper };

    struct Item {
        QString oldName;
        bool isDir = false;
        QString newName;
        // Find matched the name (always, when Find is empty).
        bool matched = false;
        // Why the item can't be renamed this way; empty if it can.
        QString problem;
        bool changes() const { return newName != oldName; }
    };

    // Replaced wherever it occurs; empty leaves the names to the case rules.
    QString find;
    QString replace;
    bool matchCase = false;
    // Find, replace and the name's capitalisation leave a file's extension
    // alone; the extension has its own capitalisation.
    bool ignoreExtension = true;
    // The capitalisation changes even names that Find doesn't match.
    bool applyWithoutMatch = false;
    Case nameCase = Case::Keep;
    ExtensionCase extensionCase = ExtensionCase::Keep;

    // The new name for `name`, and whether Find matched it.
    QString apply(const QString &name, bool isDir, bool *matched = nullptr) const;
    // The new names for `items` (old names and whether each is a folder),
    // with the problems that stop them: an empty or impossible name, two
    // items ending up with one name, or a name taken by another item in the
    // folder (`others`, the names not being renamed).
    QList<Item> plan(const QList<QPair<QString, bool>> &items, const QSet<QString> &others) const;

    // Renames the items in `folder` that change, never over anything. Names
    // that trade places, or differ only in case, go by way of a temporary
    // name. Returns an error for each item that couldn't be renamed (those
    // keep their old names); `renamed` gets the new names of those that were.
    static QStringList perform(const QString &folder, const QList<Item> &items, QStringList *renamed = nullptr);

    static QString changeCase(const QString &text, Case how);
};
