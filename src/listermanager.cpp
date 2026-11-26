#include "listermanager.h"

#include <QSettings>

#include "mainwindow.h"
#include "savedsets.h"

using Role = FileDisplay::Role;

ListerManager::ListerManager(QSettings *settings, QObject *parent) : QObject(parent), m_settings(settings) {
    if (m_settings) {
        m_tabOptions = TabOptions::load(*m_settings);
        m_rememberOnClose = m_settings->value("listers/rememberOnClose", true).toBool();
        m_pairWindows = m_settings->value("listers/pairWindows", true).toBool();
    }
}

ListerManager::~ListerManager() {
    // Windows left open (as in tests) go with the manager, without being
    // remembered.
    m_quitting = true;
    for (const QPointer<MainWindow> &window : m_windows) {
        if (window)
            disconnect(window, nullptr, this, nullptr);
        delete window.data();
    }
}

QList<MainWindow *> ListerManager::windows() const {
    QList<MainWindow *> list;
    for (const QPointer<MainWindow> &window : m_windows)
        if (window && !window->isClosing())
            list << window.data();
    return list;
}

MainWindow *ListerManager::openWindow(const ListerState &state) {
    auto *window = new MainWindow(state, m_settings, this);
    window->setAttribute(Qt::WA_DeleteOnClose);
    m_windows << window;
    connect(window, &QObject::destroyed, this, [this] {
        if (m_quitting)
            return;
        m_windows.removeIf([](const QPointer<MainWindow> &w) { return !w; });
        m_locks.removeIf([this](auto it) { return !windows().contains(it.key()); });
        updateRoles();
    });
    window->show();
    // The new window is where the user is about to work.
    windowActivated(window);
    return window;
}

MainWindow *ListerManager::openDefault(const QString &folder) {
    return openWindow(ListerState::startingIn(defaultState(), folder));
}

MainWindow *ListerManager::duplicateWindow(MainWindow *window) {
    return openWindow(window->state());
}

MainWindow *ListerManager::reopenClosedWindow(int i) {
    if (i < 0 || i >= m_closed.size())
        return nullptr;
    const ListerState state = m_closed.takeAt(i);
    emit windowsChanged();
    return openWindow(state);
}

void ListerManager::saveLayout(const QString &name) {
    if (!m_settings)
        return;
    QList<ListerState> states;
    for (MainWindow *window : windows())
        states << window->state();
    SavedSets::saveLayout(*m_settings, name, states);
}

bool ListerManager::openLayout(const QString &name) {
    if (!m_settings)
        return false;
    const QList<ListerState> states = SavedSets::loadLayout(*m_settings, name);
    if (states.isEmpty())
        return false;
    const QList<MainWindow *> old = windows();
    for (const ListerState &state : states)
        openWindow(state);
    for (MainWindow *window : old)
        window->close();
    return true;
}

ListerState ListerManager::defaultState() const {
    return m_settings ? ListerState::load(*m_settings, "lister") : ListerState();
}

void ListerManager::saveAsDefault(MainWindow *window) {
    if (!m_settings)
        return;
    m_settings->remove("lister");
    window->state().save(*m_settings, "lister");
    m_settings->sync();
}

void ListerManager::setRemembersOnClose(bool remember) {
    m_rememberOnClose = remember;
    if (m_settings)
        m_settings->setValue("listers/rememberOnClose", remember);
    emit windowsChanged();
}

void ListerManager::setPairsWindows(bool pair) {
    m_pairWindows = pair;
    if (m_settings)
        m_settings->setValue("listers/pairWindows", pair);
    updateRoles();
}

void ListerManager::setTabOptions(const TabOptions &options) {
    m_tabOptions = options;
    if (m_settings) {
        options.save(*m_settings);
        m_settings->sync();
    }
    for (MainWindow *window : windows())
        window->applyTabOptions(options);
}

MainWindow *ListerManager::sourceWindow() const { return m_source; }

MainWindow *ListerManager::destinationWindow() const { return m_destination; }

Role ListerManager::windowLock(MainWindow *window) const {
    return m_locks.value(window, Role::None);
}

QList<MainWindow *> ListerManager::singleWindows() const {
    QList<MainWindow *> list;
    for (MainWindow *window : windows())
        if (!window->isDual())
            list << window;
    return list;
}

void ListerManager::windowActivated(MainWindow *window) {
    if (!window || window->isDual() || !windows().contains(window)) {
        updateRoles();
        return;
    }
    const Role lock = windowLock(window);
    if (lock == Role::Source) {
        m_source = window;
    } else if (lock == Role::None && window != m_source) {
        // An unlocked window becomes the source and the old source its
        // destination, unless something else is locked into those roles.
        if (m_source && windowLock(m_source) == Role::Source) {
            if (!m_destination || windowLock(m_destination) != Role::Destination)
                m_destination = window;
        } else {
            if (!m_destination || windowLock(m_destination) != Role::Destination)
                m_destination = m_source;
            m_source = window;
        }
    }
    updateRoles();
}

void ListerManager::lockWindow(MainWindow *window, Role role) {
    if (role == Role::None) {
        m_locks.remove(window);
    } else {
        // Only one window can be locked into each role.
        m_locks.removeIf([role](auto it) { return it.value() == role; });
        m_locks.insert(window, role);
        if (role == Role::Source) {
            if (m_destination == window)
                m_destination = m_source;
            m_source = window;
        } else {
            if (m_source == window)
                m_source = m_destination;
            m_destination = window;
        }
    }
    updateRoles();
}

void ListerManager::windowDualChanged(MainWindow *window) {
    // A dual display is its own source and destination, so it leaves the
    // pairing; going single it joins in again.
    if (window->isDual()) {
        m_locks.remove(window);
        updateRoles();
    } else {
        // It is the window in use, so it comes back as the source.
        windowActivated(window);
    }
}

void ListerManager::updateRoles() {
    const QList<MainWindow *> singles = singleWindows();
    if (!singles.contains(m_source))
        m_source = nullptr;
    if (!singles.contains(m_destination) || m_destination == m_source)
        m_destination = nullptr;
    if (!m_source && !singles.isEmpty())
        m_source = m_destination ? m_destination.data() : singles.first();
    if (m_destination == m_source)
        m_destination = nullptr;
    if (!m_destination)
        for (MainWindow *window : singles)
            if (window != m_source) {
                m_destination = window;
                break;
            }

    const bool paired = m_pairWindows && singles.size() >= 2;
    for (MainWindow *window : windows()) {
        Role role = Role::None;
        if (paired && window == m_source)
            role = Role::Source;
        else if (paired && window == m_destination)
            role = Role::Destination;
        window->setWindowRole(role, paired && m_locks.contains(window));
    }
    emit windowsChanged();
}

void ListerManager::windowClosing(MainWindow *window) {
    if (m_quitting)
        return;
    const ListerState state = window->state();
    if (m_rememberOnClose)
        saveAsDefault(window);
    // The window is already off the list; if it was the last one, the app
    // quits and the default is what comes back.
    if (!windows().isEmpty()) {
        m_closed.prepend(state);
        while (m_closed.size() > kMaxClosedWindows)
            m_closed.removeLast();
    }
    m_locks.remove(window);
    updateRoles();
}

void ListerManager::quit(MainWindow *from) {
    if (m_rememberOnClose && from)
        saveAsDefault(from);
    m_quitting = true;
    for (MainWindow *window : windows())
        window->close();
    m_quitting = false;
}
