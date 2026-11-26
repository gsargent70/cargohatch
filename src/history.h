#pragma once

#include <QStringList>

// The back/forward list of one folder tab (NAV-01).
//
// Visiting a folder drops everything ahead of the current entry, like a web
// browser. Moving back or forward is done in two steps: peek() gives the
// folder a move would reach, and go() commits the move once that folder has
// actually been opened, so a folder that has since been deleted doesn't
// leave the history pointing at nothing.
class History {
public:
    explicit History(int maxSize = 100);

    void visit(const QString &path);

    QString current() const;
    bool canGoBack() const { return m_index > 0; }
    bool canGoForward() const { return m_index >= 0 && m_index < m_entries.size() - 1; }

    // The folder `steps` entries away (negative is back), or an empty string
    // if that is outside the history.
    QString peek(int steps) const;
    void go(int steps);

    // Most recent first, so the menus list the nearest folder at the top.
    QStringList backList() const;
    QStringList forwardList() const;

    int size() const { return m_entries.size(); }

    // For saving a tab's history between runs (LST-20).
    QStringList entries() const { return m_entries; }
    int index() const { return m_index; }
    // An index outside the entries is clamped to the last one.
    static History fromEntries(const QStringList &entries, int index, int maxSize = 100);

private:
    QStringList m_entries;
    int m_index = -1;
    int m_maxSize;
};
