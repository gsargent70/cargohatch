#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "favorites.h"
#include "findbar.h"
#include "folderaliases.h"
#include "globalfilter.h"
#include "pathcompleter.h"
#include "quickfilter.h"
#include "treeoptions.h"
#include "renamedelegate.h"

class QSettings;

// How folder tabs look and behave (TAB-02, TAB-04, TAB-07, TAB-19). Chosen
// from View > Tab Bar until there is a preferences window.
struct TabOptions {
    enum class Position { Top, Bottom, Left, Right };

    // Tabs showing a folder that matches `pattern` (a path, with * and ?
    // wildcards) or is inside it get the theme hue `color` (TAB-10).
    struct ColorRule {
        QString pattern;
        QString color;
        bool matches(const QString &path) const;
    };

    Position position = Position::Top;
    bool newTabButton = true;
    // New tabs open beside the current one rather than at the end.
    bool openNextToCurrent = false;
    bool closeButtons = true;
    bool doubleClickCloses = false;
    // Closing the last tab closes the window, or in a dual display, that side.
    bool closeWindowWithLastTab = false;
    // Ctrl+Tab shows the tab switcher; otherwise it steps to the next tab.
    bool ctrlTabSwitcher = true;
    // Opening a folder in a new tab switches to a tab already showing it
    // on that side instead (TAB-15).
    bool findExistingTab = false;
    QList<ColorRule> colorRules;

    // The view format new tabs start with (ViewFormat::toString()).
    QString defaultFormat;
    // How names sort (SORT-02, SORT-03).
    bool naturalSort = true;
    bool wordSort = false;
    bool extensionsSeparately = false;
    bool foldersAlphabetical = false;

    // What a copy or move does about names already taken (COPY-02), as a
    // WhenExistsNames id.
    QString whenExists = QStringLiteral("ask");
    // What dropping files does when no key is held, as one of
    // FileDrag::behaviours().
    QString dropAction = QStringLiteral("auto");
    // Delete moves to the trash and Shift+Delete deletes for good; off,
    // the other way round (DEL-01).
    bool useTrash = true;
    // COPY-03: ask before merging two folders; ask before replacing a
    // read-only file even when the policy says replace.
    bool askBeforeMerging = false;
    bool askBeforeReplacingReadOnly = true;
    // DEL-02.
    bool confirmTrash = false;
    bool confirmDelete = true;
    bool confirmEachFile = false;
    bool confirmEachFolder = false;
    bool deleteReadOnlyWithoutAsking = false;
    bool selectNextAfterDelete = true;
    // REN-01: "stem", "whole", "extension", "start" or "end".
    QString renameSelect = QStringLiteral("stem");
    RenameDelegate::Selection renameSelection() const;
    static QString renameSelectId(RenameDelegate::Selection selection);

    // Patterns recently used to select by name, newest first (SEL-02).
    QStringList selectPatterns;

    // What every folder hides (FILT-03).
    GlobalFilter filter;
    // FILT-01: how the filter bar matches to begin with, and how it acts.
    QuickFilter::Mode filterMode;
    bool filterRealTime = true;
    bool filterClearOnNavigate = true;
    bool filterSelectFirst = true;
    // SEL-11; see FindOptions.
    bool findAnywhere = true;
    bool findIgnoreAccents = true;
    bool findIgnoreExtensions = false;
    bool findPreferShorter = false;
    bool findRepeatFirst = true;
    bool findHighlight = true;
    bool findScrollMarks = true;
    bool findKeepHighlights = false;
    bool findEnterOpens = true;
    FindOptions findOptions() const;
    TreeOptions tree;
    // FAV-01.
    FavoriteList favorites;
    bool favoritesBar = false;
    bool favoritesAutoSort = false;
    // The user's own folder aliases (NAV-07).
    QList<FolderAliases::Alias> aliases;
    // NAV-03; see PathCompleter::Options.
    bool completePaths = true;
    bool completeLocalOnly = false;
    bool completePopup = true;
    bool completeOnArrows = false;
    PathCompleter::Options completionOptions() const {
        return {completePaths, completeLocalOnly, completePopup, completeOnArrows};
    }
    // FCMD-01: what to do after creating a folder (a CreateFolder::Then id),
    // whether several are made at once, and names used before.
    QString createThen = QStringLiteral("select");
    bool createSeveral = false;
    QStringList createNames;
    // CLIP-03.
    bool copyNamesOneLine = false;
    QStringList nameFormats;

    // The hue for `path` from the first matching rule, or empty.
    QString colorFor(const QString &path) const;
    // Adds or replaces the rule for exactly `pattern`; an empty color
    // removes it.
    void setColorRule(const QString &pattern, const QString &color);

    // Missing keys keep their defaults.
    static TabOptions load(QSettings &settings);
    void save(QSettings &settings) const;
};
