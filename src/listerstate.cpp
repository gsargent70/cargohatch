#include "listerstate.h"

#include <QSettings>

QString ListerState::activePath() const {
    const Side &side = sides[qBound(0, active, 1)];
    if (side.tabs.isEmpty())
        return {};
    return side.tabs.at(qBound(0, side.current, int(side.tabs.size()) - 1)).path;
}

ListerState ListerState::startingIn(ListerState state, const QString &folder) {
    if (folder.isEmpty())
        return state;
    state.active = qBound(0, state.active, 1);
    if (state.sides[state.active].tabs.isEmpty() && !state.sides[1 - state.active].tabs.isEmpty())
        state.active = 1 - state.active;
    Side &side = state.sides[state.active];
    if (side.tabs.isEmpty()) {
        Tab tab;
        tab.path = folder;
        tab.history.visit(folder);
        side.tabs << tab;
        side.current = 0;
        return state;
    }
    side.current = qBound(0, side.current, int(side.tabs.size()) - 1);
    Tab &tab = side.tabs[side.current];
    tab.path = folder;
    tab.history.visit(folder);
    return state;
}

void ListerState::save(QSettings &settings, const QString &group, bool tabsOnly) const {
    settings.beginGroup(group);
    settings.setValue("dual", dual);
    if (!tabsOnly) {
        settings.setValue("geometry", geometry);
        settings.setValue("layout", stacked ? "stacked" : "side-by-side");
        if (!splitter.isEmpty())
            settings.setValue("splitter", splitter);
        settings.setValue("tree", treeShown);
        for (int t = 0; t < 3; ++t) {
            const QString key = QString("tree%1").arg(t);
            settings.remove(key);
            if (trees[t].width > 0)
                settings.setValue(key + "/width", trees[t].width);
            if (!trees[t].expanded.isEmpty())
                settings.setValue(key + "/expanded", trees[t].expanded);
        }
        settings.setValue("viewer/shown", viewer.shown);
        settings.setValue("viewer/below", viewer.below);
        settings.setValue("viewer/size", viewer.size);
        settings.setValue("viewer/locked", viewer.locked);
    }
    settings.setValue("active", active);
    for (int s = 0; s < 2; ++s) {
        const QString prefix = QString("side%1").arg(s);
        settings.setValue(prefix + "/current", sides[s].current);
        settings.beginWriteArray(prefix + "/tabs", int(sides[s].tabs.size()));
        for (int i = 0; i < sides[s].tabs.size(); ++i) {
            settings.setArrayIndex(i);
            const Tab &tab = sides[s].tabs.at(i);
            settings.setValue("path", tab.path);
            settings.setValue("history", tab.history.entries());
            settings.setValue("historyIndex", tab.history.index());
            // Only what is set, to keep the file readable.
            auto optional = [&settings](const char *key, const QVariant &value, bool set) {
                if (set)
                    settings.setValue(key, value);
            };
            optional("label", tab.label, !tab.label.isEmpty());
            optional("lock", tab.lock, tab.lock != 0);
            optional("lockedPath", tab.lockedPath, !tab.lockedPath.isEmpty());
            optional("home", tab.home, !tab.home.isEmpty());
            optional("color", tab.color, !tab.color.isEmpty());
            optional("link", tab.link, tab.link >= 0);
            optional("tree", tab.tree, !tab.tree.isEmpty());
            optional("format", tab.format, !tab.format.isEmpty());
        }
        settings.endArray();
    }
    settings.endGroup();
}

ListerState ListerState::load(QSettings &settings, const QString &group) {
    ListerState state;
    settings.beginGroup(group);
    state.geometry = settings.value("geometry").toByteArray();
    state.dual = settings.value("dual", false).toBool();
    state.stacked = settings.value("layout").toString() == "stacked";
    state.splitter = settings.value("splitter").toByteArray();
    state.treeShown = settings.value("tree", false).toBool();
    for (int t = 0; t < 3; ++t) {
        const QString key = QString("tree%1").arg(t);
        state.trees[t].width = settings.value(key + "/width", 0).toInt();
        state.trees[t].expanded = settings.value(key + "/expanded").toStringList();
    }
    state.viewer.shown = settings.value("viewer/shown", false).toBool();
    state.viewer.below = settings.value("viewer/below", false).toBool();
    state.viewer.size = qMax(0, settings.value("viewer/size", 0).toInt());
    state.viewer.locked = settings.value("viewer/locked", false).toBool();
    state.active = qBound(0, settings.value("active", 0).toInt(), 1);
    for (int s = 0; s < 2; ++s) {
        const QString prefix = QString("side%1").arg(s);
        state.sides[s].current = settings.value(prefix + "/current", 0).toInt();
        const int count = settings.beginReadArray(prefix + "/tabs");
        for (int i = 0; i < count; ++i) {
            settings.setArrayIndex(i);
            const QString path = settings.value("path").toString();
            if (path.isEmpty())
                continue;
            History history = History::fromEntries(settings.value("history").toStringList(),
                                                   settings.value("historyIndex", -1).toInt());
            if (history.current() != path)
                history.visit(path);
            Tab tab;
            tab.path = path;
            tab.history = history;
            tab.label = settings.value("label").toString();
            tab.lock = qBound(0, settings.value("lock", 0).toInt(), 3);
            tab.lockedPath = settings.value("lockedPath").toString();
            tab.home = settings.value("home").toString();
            tab.color = settings.value("color").toString();
            tab.link = settings.value("link", -1).toInt();
            tab.format = settings.value("format").toString();
            tab.tree = settings.value("tree").toStringList();
            state.sides[s].tabs << tab;
        }
        settings.endArray();
    }
    settings.endGroup();
    return state;
}
