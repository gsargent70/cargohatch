#include "taboptions.h"

#include <QRegularExpression>
#include <QSettings>

namespace {

const char *const kPositions[] = {"top", "bottom", "left", "right"};
const char *const kRenameSelections[] = {"stem", "whole", "extension", "start", "end"};

} // namespace

TabOptions TabOptions::load(QSettings &settings) {
    TabOptions o;
    const QString position = settings.value("tabs/position", "top").toString();
    for (int i = 0; i < 4; ++i)
        if (position == kPositions[i])
            o.position = Position(i);
    o.newTabButton = settings.value("tabs/newTabButton", o.newTabButton).toBool();
    o.openNextToCurrent = settings.value("tabs/openNextToCurrent", o.openNextToCurrent).toBool();
    o.closeButtons = settings.value("tabs/closeButtons", o.closeButtons).toBool();
    o.doubleClickCloses = settings.value("tabs/doubleClickCloses", o.doubleClickCloses).toBool();
    o.closeWindowWithLastTab = settings.value("tabs/closeWindowWithLastTab", o.closeWindowWithLastTab).toBool();
    o.ctrlTabSwitcher = settings.value("tabs/ctrlTabSwitcher", o.ctrlTabSwitcher).toBool();
    o.findExistingTab = settings.value("tabs/findExistingTab", o.findExistingTab).toBool();
    o.defaultFormat = settings.value("format/default").toString();
    o.naturalSort = settings.value("sorting/natural", o.naturalSort).toBool();
    o.wordSort = settings.value("sorting/wordSort", o.wordSort).toBool();
    o.extensionsSeparately = settings.value("sorting/extensionsSeparately", o.extensionsSeparately).toBool();
    o.foldersAlphabetical = settings.value("sorting/foldersAlphabetical", o.foldersAlphabetical).toBool();
    o.whenExists = settings.value("files/whenExists", o.whenExists).toString();
    o.dropAction = settings.value("files/dropAction", o.dropAction).toString();
    o.useTrash = settings.value("files/useTrash", o.useTrash).toBool();
    o.selectPatterns = settings.value("select/patterns").toStringList();
    o.askBeforeMerging = settings.value("files/askMerge", o.askBeforeMerging).toBool();
    o.askBeforeReplacingReadOnly = settings.value("files/askReadOnly", o.askBeforeReplacingReadOnly).toBool();
    o.confirmTrash = settings.value("delete/confirmTrash", o.confirmTrash).toBool();
    o.confirmDelete = settings.value("delete/confirmDelete", o.confirmDelete).toBool();
    o.confirmEachFile = settings.value("delete/eachFile", o.confirmEachFile).toBool();
    o.confirmEachFolder = settings.value("delete/eachFolder", o.confirmEachFolder).toBool();
    o.deleteReadOnlyWithoutAsking = settings.value("delete/readOnlyWithoutAsking", o.deleteReadOnlyWithoutAsking).toBool();
    o.selectNextAfterDelete = settings.value("delete/selectNext", o.selectNextAfterDelete).toBool();
    o.renameSelect = settings.value("rename/select", o.renameSelect).toString();
    GlobalFilter &f = o.filter;
    f.showHidden = settings.value("filters/showHidden", f.showHidden).toBool();
    f.hideListed = settings.value("filters/hideListed", f.hideListed).toBool();
    f.hideNames = settings.value("filters/hideNames", f.hideNames).toBool();
    f.files = settings.value("filters/files").toString();
    f.filesRegex = settings.value("filters/filesRegex", f.filesRegex).toBool();
    f.folders = settings.value("filters/folders").toString();
    f.foldersRegex = settings.value("filters/foldersRegex", f.foldersRegex).toBool();
    QuickFilter::Mode &m = o.filterMode;
    m.ignoreAccents = settings.value("filterBar/ignoreAccents", m.ignoreAccents).toBool();
    m.ignoreExtensions = settings.value("filterBar/ignoreExtensions", m.ignoreExtensions).toBool();
    m.anyWord = settings.value("filterBar/anyWord", m.anyWord).toBool();
    m.partial = settings.value("filterBar/partial", m.partial).toBool();
    m.regex = settings.value("filterBar/regex", m.regex).toBool();
    o.filterRealTime = settings.value("filterBar/realTime", o.filterRealTime).toBool();
    o.filterClearOnNavigate = settings.value("filterBar/clearOnNavigate", o.filterClearOnNavigate).toBool();
    o.filterSelectFirst = settings.value("filterBar/selectFirst", o.filterSelectFirst).toBool();
    o.findAnywhere = settings.value("find/anywhere", o.findAnywhere).toBool();
    o.findIgnoreAccents = settings.value("find/ignoreAccents", o.findIgnoreAccents).toBool();
    o.findIgnoreExtensions = settings.value("find/ignoreExtensions", o.findIgnoreExtensions).toBool();
    o.findPreferShorter = settings.value("find/preferShorter", o.findPreferShorter).toBool();
    o.findRepeatFirst = settings.value("find/repeatFirst", o.findRepeatFirst).toBool();
    o.findHighlight = settings.value("find/highlight", o.findHighlight).toBool();
    o.findScrollMarks = settings.value("find/scrollMarks", o.findScrollMarks).toBool();
    o.findKeepHighlights = settings.value("find/keepHighlights", o.findKeepHighlights).toBool();
    o.findEnterOpens = settings.value("find/enterOpens", o.findEnterOpens).toBool();
    o.tree = TreeOptions::load(settings);
    o.favorites = FavoriteList::fromJson(settings.value("favorites/list").toString().toUtf8());
    o.favoritesBar = settings.value("favorites/bar", o.favoritesBar).toBool();
    o.favoritesAutoSort = settings.value("favorites/autoSort", o.favoritesAutoSort).toBool();
    o.completePaths = settings.value("completion/enabled", o.completePaths).toBool();
    o.completeLocalOnly = settings.value("completion/localOnly", o.completeLocalOnly).toBool();
    o.completePopup = settings.value("completion/popup", o.completePopup).toBool();
    o.completeOnArrows = settings.value("completion/onArrows", o.completeOnArrows).toBool();
    o.viewerKeepLast = settings.value("viewer/keepLast", o.viewerKeepLast).toBool();
    o.viewerWrap = settings.value("viewer/wrap", o.viewerWrap).toBool();
    const int aliases = settings.beginReadArray("aliases");
    for (int i = 0; i < aliases; ++i) {
        settings.setArrayIndex(i);
        const FolderAliases::Alias alias{settings.value("name").toString(), settings.value("path").toString()};
        if (FolderAliases::nameError(alias.name).isEmpty() && !alias.path.isEmpty())
            o.aliases << alias;
    }
    settings.endArray();
    o.createThen = settings.value("createFolder/then", o.createThen).toString();
    o.createSeveral = settings.value("createFolder/several", o.createSeveral).toBool();
    o.createNames = settings.value("createFolder/names").toStringList();
    o.copyNamesOneLine = settings.value("clipboard/oneLine", o.copyNamesOneLine).toBool();
    o.nameFormats = settings.value("clipboard/nameFormats").toStringList();
    const int rules = settings.beginReadArray("tabColorRules");
    for (int i = 0; i < rules; ++i) {
        settings.setArrayIndex(i);
        const ColorRule rule{settings.value("pattern").toString(), settings.value("color").toString()};
        if (!rule.pattern.isEmpty() && !rule.color.isEmpty())
            o.colorRules << rule;
    }
    settings.endArray();
    return o;
}

void TabOptions::save(QSettings &settings) const {
    settings.setValue("tabs/position", kPositions[int(position)]);
    settings.setValue("tabs/newTabButton", newTabButton);
    settings.setValue("tabs/openNextToCurrent", openNextToCurrent);
    settings.setValue("tabs/closeButtons", closeButtons);
    settings.setValue("tabs/doubleClickCloses", doubleClickCloses);
    settings.setValue("tabs/closeWindowWithLastTab", closeWindowWithLastTab);
    settings.setValue("tabs/ctrlTabSwitcher", ctrlTabSwitcher);
    settings.setValue("tabs/findExistingTab", findExistingTab);
    if (!defaultFormat.isEmpty())
        settings.setValue("format/default", defaultFormat);
    settings.setValue("sorting/natural", naturalSort);
    settings.setValue("sorting/wordSort", wordSort);
    settings.setValue("sorting/extensionsSeparately", extensionsSeparately);
    settings.setValue("sorting/foldersAlphabetical", foldersAlphabetical);
    settings.setValue("files/whenExists", whenExists);
    settings.setValue("files/dropAction", dropAction);
    settings.setValue("files/useTrash", useTrash);
    settings.setValue("select/patterns", selectPatterns);
    settings.setValue("files/askMerge", askBeforeMerging);
    settings.setValue("files/askReadOnly", askBeforeReplacingReadOnly);
    settings.setValue("delete/confirmTrash", confirmTrash);
    settings.setValue("delete/confirmDelete", confirmDelete);
    settings.setValue("delete/eachFile", confirmEachFile);
    settings.setValue("delete/eachFolder", confirmEachFolder);
    settings.setValue("delete/readOnlyWithoutAsking", deleteReadOnlyWithoutAsking);
    settings.setValue("delete/selectNext", selectNextAfterDelete);
    settings.setValue("rename/select", renameSelect);
    settings.setValue("filters/showHidden", filter.showHidden);
    settings.setValue("filters/hideListed", filter.hideListed);
    settings.setValue("filters/hideNames", filter.hideNames);
    settings.setValue("filters/files", filter.files);
    settings.setValue("filters/filesRegex", filter.filesRegex);
    settings.setValue("filters/folders", filter.folders);
    settings.setValue("filters/foldersRegex", filter.foldersRegex);
    settings.setValue("filterBar/ignoreAccents", filterMode.ignoreAccents);
    settings.setValue("filterBar/ignoreExtensions", filterMode.ignoreExtensions);
    settings.setValue("filterBar/anyWord", filterMode.anyWord);
    settings.setValue("filterBar/partial", filterMode.partial);
    settings.setValue("filterBar/regex", filterMode.regex);
    settings.setValue("filterBar/realTime", filterRealTime);
    settings.setValue("filterBar/clearOnNavigate", filterClearOnNavigate);
    settings.setValue("filterBar/selectFirst", filterSelectFirst);
    settings.setValue("find/anywhere", findAnywhere);
    settings.setValue("find/ignoreAccents", findIgnoreAccents);
    settings.setValue("find/ignoreExtensions", findIgnoreExtensions);
    settings.setValue("find/preferShorter", findPreferShorter);
    settings.setValue("find/repeatFirst", findRepeatFirst);
    settings.setValue("find/highlight", findHighlight);
    settings.setValue("find/scrollMarks", findScrollMarks);
    settings.setValue("find/keepHighlights", findKeepHighlights);
    settings.setValue("find/enterOpens", findEnterOpens);
    tree.save(settings);
    settings.setValue("favorites/list", QString::fromUtf8(favorites.toJson()));
    settings.setValue("favorites/bar", favoritesBar);
    settings.setValue("favorites/autoSort", favoritesAutoSort);
    settings.setValue("completion/enabled", completePaths);
    settings.setValue("completion/localOnly", completeLocalOnly);
    settings.setValue("completion/popup", completePopup);
    settings.setValue("completion/onArrows", completeOnArrows);
    settings.setValue("viewer/keepLast", viewerKeepLast);
    settings.setValue("viewer/wrap", viewerWrap);
    settings.remove("aliases");
    settings.beginWriteArray("aliases", int(aliases.size()));
    for (int i = 0; i < aliases.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("name", aliases.at(i).name);
        settings.setValue("path", aliases.at(i).path);
    }
    settings.endArray();
    settings.setValue("createFolder/then", createThen);
    settings.setValue("createFolder/several", createSeveral);
    settings.setValue("createFolder/names", createNames);
    settings.setValue("clipboard/oneLine", copyNamesOneLine);
    settings.setValue("clipboard/nameFormats", nameFormats);
    settings.remove("tabColorRules");
    settings.beginWriteArray("tabColorRules", int(colorRules.size()));
    for (int i = 0; i < colorRules.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("pattern", colorRules.at(i).pattern);
        settings.setValue("color", colorRules.at(i).color);
    }
    settings.endArray();
}

bool TabOptions::ColorRule::matches(const QString &path) const {
    // * may cross folder boundaries, so "~/Work/*" covers everything below.
    const auto options = QRegularExpression::NonPathWildcardConversion;
    QString base = pattern.startsWith('~') ? FolderAliases::expand(pattern) : pattern;
    if (base.endsWith('/') && base.size() > 1)
        base.chop(1);
    for (const QString &candidate : {base, base == "/" ? QString("/*") : base + "/*"}) {
        const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(candidate, options));
        if (re.match(path).hasMatch())
            return true;
    }
    return false;
}

QString TabOptions::colorFor(const QString &path) const {
    for (const ColorRule &rule : colorRules)
        if (rule.matches(path))
            return rule.color;
    return {};
}

void TabOptions::setColorRule(const QString &pattern, const QString &color) {
    colorRules.removeIf([&](const ColorRule &rule) { return rule.pattern == pattern; });
    if (!color.isEmpty())
        colorRules.prepend({pattern, color});
}

RenameDelegate::Selection TabOptions::renameSelection() const {
    for (int i = 0; i < 5; ++i)
        if (renameSelect == kRenameSelections[i])
            return RenameDelegate::Selection(i);
    return RenameDelegate::Selection::Stem;
}

QString TabOptions::renameSelectId(RenameDelegate::Selection selection) {
    return kRenameSelections[int(selection)];
}

FindOptions TabOptions::findOptions() const {
    FindOptions f;
    f.anywhere = findAnywhere;
    f.ignoreAccents = findIgnoreAccents;
    f.ignoreExtensions = findIgnoreExtensions;
    f.preferShorter = findPreferShorter;
    f.repeatFirst = findRepeatFirst;
    f.highlight = findHighlight;
    f.scrollMarks = findScrollMarks;
    f.keepHighlights = findKeepHighlights;
    f.enterOpens = findEnterOpens;
    return f;
}
