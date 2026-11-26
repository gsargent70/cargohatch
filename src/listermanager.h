#pragma once

#include <QObject>
#include <QPointer>

#include "filedisplay.h"
#include "listerstate.h"
#include "taboptions.h"

class MainWindow;
class QSettings;

// Keeps track of every Lister window (LST-01): opening new ones from the
// remembered default (LST-20), reopening closed ones (LST-21), sharing the
// tab options, and pairing single-display windows as source and
// destination (LST-04).
class ListerManager : public QObject {
    Q_OBJECT

public:
    // `settings` holds the default window and the options; without it
    // nothing is remembered.
    explicit ListerManager(QSettings *settings = nullptr, QObject *parent = nullptr);
    ~ListerManager() override;

    QSettings *settings() const { return m_settings; }
    // The open windows, leaving out any that are closing.
    QList<MainWindow *> windows() const;

    MainWindow *openWindow(const ListerState &state);
    // The default window, showing `folder` in its active tab if given.
    MainWindow *openDefault(const QString &folder = {});
    MainWindow *duplicateWindow(MainWindow *window);
    // Brings back closedWindows()[i].
    MainWindow *reopenClosedWindow(int i = 0);
    // Most recently closed first. The last window isn't listed: closing it
    // quits, and it is what the next start opens.
    const QList<ListerState> &closedWindows() const { return m_closed; }
    static constexpr int kMaxClosedWindows = 10;

    // Layouts (LST-19): every open window, saved under a name. Opening one
    // replaces the open windows, which can be reopened from the closed list.
    void saveLayout(const QString &name);
    bool openLayout(const QString &name);

    ListerState defaultState() const;
    void saveAsDefault(MainWindow *window);
    bool remembersOnClose() const { return m_rememberOnClose; }
    void setRemembersOnClose(bool remember);

    // Whether two or more single-display windows act as source and
    // destination for each other.
    bool pairsWindows() const { return m_pairWindows; }
    void setPairsWindows(bool pair);
    MainWindow *sourceWindow() const;
    MainWindow *destinationWindow() const;
    FileDisplay::Role windowLock(MainWindow *window) const;

    const TabOptions &tabOptions() const { return m_tabOptions; }
    void setTabOptions(const TabOptions &options);

    // Called by the windows.
    void windowActivated(MainWindow *window);
    void lockWindow(MainWindow *window, FileDisplay::Role role);
    void windowDualChanged(MainWindow *window);
    void windowClosing(MainWindow *window);
    // Closes every window, remembering `from` as the default.
    void quit(MainWindow *from);

signals:
    // The windows, their roles or the closed list changed.
    void windowsChanged();

private:
    QList<MainWindow *> singleWindows() const;
    void updateRoles();

    QSettings *m_settings;
    QList<QPointer<MainWindow>> m_windows;
    QList<ListerState> m_closed;
    QPointer<MainWindow> m_source;
    QPointer<MainWindow> m_destination;
    QHash<MainWindow *, FileDisplay::Role> m_locks;
    TabOptions m_tabOptions;
    bool m_rememberOnClose = true;
    bool m_pairWindows = true;
    bool m_quitting = false;
};
