#pragma once

#include <QFrame>

class QTreeWidget;

// The Ctrl+Tab popup (TAB-19, LST-26): every tab in the window, most recently
// used first. Holding Ctrl and pressing Tab moves down the list; letting go
// of Ctrl switches to the highlighted tab, as Alt+Tab does for windows.
class TabSwitcher : public QFrame {
    Q_OBJECT

public:
    struct Entry {
        QString title;
        QString path;
        QString side; // empty in a single display
    };

    explicit TabSwitcher(QWidget *parent);

    // Shows the list centred over the parent window with row `start`
    // highlighted.
    void open(const QList<Entry> &entries, int start);
    int currentRow() const;
    void step(int delta);
    // Closes the list and emits chosen() for the highlighted row.
    void commit();

signals:
    void chosen(int row);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    bool handleKey(QKeyEvent *event, bool press);

    QTreeWidget *m_list;
};
