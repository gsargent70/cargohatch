#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include "history.h"

class QSettings;

// Everything needed to bring a Lister window back: its folders and tabs on
// both sides with their histories, dual display, layout and size. Used for
// the default window that is remembered on close (LST-20) and for reopening
// closed windows (LST-21).
struct ListerState {
    struct Tab {
        static Tab at(const QString &path, const History &history = History()) {
            Tab tab;
            tab.path = path;
            tab.history = history;
            return tab;
        }
        QString path;
        History history;
        QString label;
        int lock = 0; // FolderTab::Lock
        QString lockedPath;
        QString home;
        QString color;
        // The index of the linked tab on the other side, or -1.
        int link = -1;
        // The tab's view format (ViewFormat::toString()), or empty for the
        // default.
        QString format;
        // The tab's own expanded branches of the folder tree, when tabs
        // remember them (TREE-04).
        QStringList tree;
    };
    struct Side {
        QList<Tab> tabs;
        int current = 0;
    };

    QByteArray geometry;
    bool dual = false;
    bool stacked = false;
    QByteArray splitter;
    int active = 0;
    Side sides[2];

    // The folder tree (LST-08): whether it shows, how wide the window's and
    // each side's are, and their expanded branches.
    struct Tree {
        int width = 0;
        QStringList expanded;
    };
    bool treeShown = false;
    Tree trees[3];

    // The viewer pane (LST-09): whether it shows, below the files or beside
    // them, its width or height (0 for the default), and whether it's locked.
    struct Viewer {
        bool shown = false;
        bool below = false;
        int size = 0;
        bool locked = false;
    };
    Viewer viewer;

    bool isEmpty() const { return sides[0].tabs.isEmpty() && sides[1].tabs.isEmpty(); }
    int tabCount() const { return int(sides[0].tabs.size() + sides[1].tabs.size()); }
    // The folder shown in the active side's current tab.
    QString activePath() const;

    // `state` with `folder` shown in its active tab, as when CargoHatch is
    // started on a folder: the remembered tabs stay and the active one goes
    // there. An empty state gets a single tab.
    static ListerState startingIn(ListerState state, const QString &folder);

    // Stored under `group` (e.g. "lister"); a missing group loads empty.
    // Window geometry and layout are left out when `tabsOnly`, as for tab
    // groups.
    void save(QSettings &settings, const QString &group, bool tabsOnly = false) const;
    static ListerState load(QSettings &settings, const QString &group);
};
