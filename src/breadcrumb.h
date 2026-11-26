#pragma once

#include <QList>
#include <QSet>
#include <QWidget>

#include "globalfilter.h"
#include "pathcompleter.h"

class QHBoxLayout;
class QLineEdit;
class QMenu;
class QStackedLayout;
class QToolButton;

// One clickable piece of a path in the location bar.
struct Crumb {
    QString label;
    QString path;
    // A ghost crumb is a folder below the current one that the user came up
    // from; it stays visible, dimmed, so they can step back down (NAV-02).
    bool ghost = false;
};

// The breadcrumb location bar (NAV-02). Each folder in the path is a button;
// the arrow after it lists that folder's sub-folders. Clicking the empty part
// of the bar, or calling startEditing(), swaps it for a text field, where paths
// are completed (NAV-03) and may start with an alias (NAV-07).
class Breadcrumb : public QWidget {
    Q_OBJECT

public:
    explicit Breadcrumb(QWidget *parent = nullptr);

    void setPath(const QString &path, const QString &ghost = {});
    QString path() const { return m_path; }

    // Leaves hidden folders out of the sub-folder menus (FILT-03).
    void setGlobalFilter(const GlobalFilter &filter);
    void setCompletionOptions(const PathCompleter::Options &options) { m_completer->setOptions(options); }
    PathCompleter *completer() const { return m_completer; }

    void startEditing();
    void stopEditing();
    bool isEditing() const;
    QLineEdit *editor() const { return m_edit; }

    // How many crumbs are shown, and whether the leading ones were folded into
    // the overflow menu because the bar is too narrow.
    int visibleCrumbCount() const;
    bool isOverflowing() const;

    static QList<Crumb> crumbs(const QString &path, const QString &ghost);
    // The sub-folders of `path`, naturally sorted: visible ones first, then
    // hidden ones (names starting with a dot).
    static QStringList subfolders(const QString &path, QStringList *hidden = nullptr);

signals:
    void pathRequested(const QString &path);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct CrumbWidgets {
        Crumb crumb;
        QToolButton *name;
        QToolButton *arrow;
    };

    void rebuild();
    void fitToWidth();
    void fillFolderMenu(QMenu *menu, const QString &folder, const QString &highlight);
    void fillOverflowMenu();
    void retranslate();

    QStackedLayout *m_stack;
    QWidget *m_crumbArea;
    QHBoxLayout *m_crumbLayout;
    QToolButton *m_overflow;
    QLineEdit *m_edit;
    PathCompleter *m_completer;
    QList<CrumbWidgets> m_widgets;
    QString m_path;
    QString m_ghost;
    GlobalFilter m_filter;
};
