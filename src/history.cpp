#include "history.h"

#include <algorithm>

History::History(int maxSize) : m_maxSize(std::max(2, maxSize)) {}

void History::visit(const QString &path) {
    if (path.isEmpty() || path == current())
        return;
    while (m_entries.size() > m_index + 1)
        m_entries.removeLast();
    m_entries.append(path);
    m_index = m_entries.size() - 1;
    while (m_entries.size() > m_maxSize) {
        m_entries.removeFirst();
        --m_index;
    }
}

QString History::current() const {
    return m_index >= 0 ? m_entries.at(m_index) : QString();
}

QString History::peek(int steps) const {
    const int target = m_index + steps;
    if (m_index < 0 || target < 0 || target >= m_entries.size())
        return {};
    return m_entries.at(target);
}

void History::go(int steps) {
    const int target = m_index + steps;
    if (m_index >= 0 && target >= 0 && target < m_entries.size())
        m_index = target;
}

QStringList History::backList() const {
    QStringList list;
    for (int i = m_index - 1; i >= 0; --i)
        list.append(m_entries.at(i));
    return list;
}

QStringList History::forwardList() const {
    QStringList list;
    if (m_index < 0)
        return list;
    for (int i = m_index + 1; i < m_entries.size(); ++i)
        list.append(m_entries.at(i));
    return list;
}

History History::fromEntries(const QStringList &entries, int index, int maxSize) {
    History history(maxSize);
    history.m_entries = entries.mid(qMax(0, int(entries.size()) - maxSize));
    const int dropped = int(entries.size() - history.m_entries.size());
    history.m_index = history.m_entries.isEmpty() ? -1 : qBound(0, index - dropped, int(history.m_entries.size()) - 1);
    return history;
}
