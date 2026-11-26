#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "globalfilter.h"

class QCompleter;
class QKeyEvent;
class QLineEdit;
class QModelIndex;
class QStandardItemModel;

// Completes folder paths typed into a line edit (NAV-03): sub-folders of
// the folder typed so far, and aliases after a ~. Folders are read on a
// worker thread, so a slow drive never holds up the typing.
class PathCompleter : public QObject {
    Q_OBJECT

public:
    struct Options {
        bool enabled = true;
        // Not on network drives.
        bool localOnly = false;
        // A list of the folders; otherwise the arrow keys and Tab step
        // through them in the field.
        bool popup = true;
        // Only when an arrow key asks for it.
        bool onArrows = false;
    };

    explicit PathCompleter(QLineEdit *edit);

    void setOptions(const Options &options);
    void setGlobalFilter(const GlobalFilter &filter) { m_filter = filter; }
    // The folder that relative paths are in.
    void setBase(const QString &base) { m_base = base; }
    // Puts the list away and forgets what was typed.
    void reset();

    bool isPopupVisible() const;
    QCompleter *completer() const { return m_completer; }
    // What would be offered now, as it would read in the field.
    QStringList completions() const;

    // The sub-folders of `folder`, in name order, hidden ones included.
    // Nothing for a network drive if `localOnly`.
    static QStringList foldersIn(const QString &folder, bool localOnly);

signals:
    // A folder was clicked in the list; it is in the field by now.
    void chosen(const QString &path);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    // What to do with the folder list once it has been read.
    enum class Then { Nothing, Show, Complete, Next, Previous };

    bool handleKey(QKeyEvent *key, bool inPopup);
    void update(Then then);
    void listed(Then then);
    void showPopup();
    void hidePopup();
    void complete();
    void step(int by);
    void take(int row);
    QString rowText(int row) const;

    QLineEdit *m_edit;
    QCompleter *m_completer;
    QStandardItemModel *m_model;
    Options m_options;
    GlobalFilter m_filter;
    QString m_base;
    // The text as typed, which stepping through the list shows over.
    QString m_typed;
    // The part of it before the name being completed, and that name.
    QString m_prefix;
    QString m_fragment;
    int m_row = -1;
    // The folder whose sub-folders are in m_folders.
    QString m_listedFolder;
    QStringList m_folders;
    int m_generation = 0;
    // A folder is being read; the list still shows what came before.
    bool m_reading = false;
};
