#pragma once

#include <QList>
#include <QStringList>

#include "listerstate.h"

class QSettings;

// Named, saved sets the user can come back to: tab groups (TAB-13), the tabs
// of one window, and layouts (LST-19), every window at once. Names can be
// anything the user types; they are encoded for the settings file.
namespace SavedSets {

QStringList tabGroupNames(QSettings &settings);
bool hasTabGroup(QSettings &settings, const QString &name);
void saveTabGroup(QSettings &settings, const QString &name, const ListerState &tabs);
ListerState loadTabGroup(QSettings &settings, const QString &name);
void removeTabGroup(QSettings &settings, const QString &name);

QStringList layoutNames(QSettings &settings);
bool hasLayout(QSettings &settings, const QString &name);
void saveLayout(QSettings &settings, const QString &name, const QList<ListerState> &windows);
QList<ListerState> loadLayout(QSettings &settings, const QString &name);
void removeLayout(QSettings &settings, const QString &name);

} // namespace SavedSets
