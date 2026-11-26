#include "savedsets.h"

#include <QCollator>
#include <QSettings>
#include <QUrl>

namespace SavedSets {
namespace {

// A user's name may hold "/" or other characters settings keys treat
// specially.
QString key(const char *root, const QString &name) {
    return QString("%1/%2").arg(root, QString::fromLatin1(QUrl::toPercentEncoding(name)));
}

QStringList names(QSettings &settings, const char *root) {
    settings.beginGroup(root);
    QStringList list;
    for (const QString &group : settings.childGroups())
        list << QUrl::fromPercentEncoding(group.toLatin1());
    settings.endGroup();
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(list.begin(), list.end(), collator);
    return list;
}

} // namespace

QStringList tabGroupNames(QSettings &settings) {
    return names(settings, "tabGroups");
}

bool hasTabGroup(QSettings &settings, const QString &name) {
    return tabGroupNames(settings).contains(name);
}

void saveTabGroup(QSettings &settings, const QString &name, const ListerState &tabs) {
    settings.remove(key("tabGroups", name));
    tabs.save(settings, key("tabGroups", name), true);
    settings.sync();
}

ListerState loadTabGroup(QSettings &settings, const QString &name) {
    return ListerState::load(settings, key("tabGroups", name));
}

void removeTabGroup(QSettings &settings, const QString &name) {
    settings.remove(key("tabGroups", name));
    settings.sync();
}

QStringList layoutNames(QSettings &settings) {
    return names(settings, "layouts");
}

bool hasLayout(QSettings &settings, const QString &name) {
    return layoutNames(settings).contains(name);
}

void saveLayout(QSettings &settings, const QString &name, const QList<ListerState> &windows) {
    const QString group = key("layouts", name);
    settings.remove(group);
    settings.setValue(group + "/windows", int(windows.size()));
    for (int i = 0; i < windows.size(); ++i)
        windows.at(i).save(settings, QString("%1/window%2").arg(group).arg(i));
    settings.sync();
}

QList<ListerState> loadLayout(QSettings &settings, const QString &name) {
    const QString group = key("layouts", name);
    QList<ListerState> windows;
    const int count = settings.value(group + "/windows", 0).toInt();
    for (int i = 0; i < count; ++i) {
        const ListerState state = ListerState::load(settings, QString("%1/window%2").arg(group).arg(i));
        if (!state.isEmpty())
            windows << state;
    }
    return windows;
}

void removeLayout(QSettings &settings, const QString &name) {
    settings.remove(key("layouts", name));
    settings.sync();
}

} // namespace SavedSets
