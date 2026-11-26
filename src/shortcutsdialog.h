#pragma once

#include <QDialog>
#include <QList>

class QLineEdit;
class QMenuBar;
class QTreeWidget;

// Every key the window answers to (CUST-17), read from its menus so the
// list can't fall behind them, plus the few keys that have no menu entry.
class ShortcutsDialog : public QDialog {
    Q_OBJECT

public:
    struct Row {
        QString section;
        QString command;
        QString keys;
    };
    // The menu entries with keys, in menu order, then the keys of the file
    // list and of dragging.
    static QList<Row> rows(const QMenuBar *bar);

    explicit ShortcutsDialog(const QMenuBar *bar, QWidget *parent = nullptr);

    // Shows only the commands or keys containing `text`.
    void setFilter(const QString &text);
    int shownCount() const;

private:
    QLineEdit *m_filter;
    QTreeWidget *m_list;
};
