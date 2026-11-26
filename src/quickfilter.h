#pragma once

#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QWidget>

class QAction;
class QLabel;
class QLineEdit;
class QMenu;
class QToolButton;

// What the filter bar shows (FILT-01): only items whose names match.
struct QuickFilter {
    // Words, or wildcards (* and ?); several alternatives separated by ";".
    QString text;

    struct Mode {
        bool ignoreAccents = true;
        bool ignoreExtensions = false;
        // Spaces separate words, any of which may match.
        bool anyWord = false;
        // Anywhere in the name; otherwise the whole name must match.
        bool partial = true;
        bool regex = false;
        bool operator==(const Mode &o) const;
    } mode;

    bool isActive() const { return !text.trimmed().isEmpty(); }
    bool operator==(const QuickFilter &o) const { return text == o.text && mode == o.mode; }
    // What's wrong with it, or empty.
    QString error() const;

    class Matcher {
    public:
        Matcher() = default;
        explicit Matcher(const QuickFilter &filter);
        bool matches(const QString &name, bool isDir) const;

    private:
        Mode m_mode;
        // Plain words need no expressions, which is much quicker.
        QList<QString> m_words;
        QList<QRegularExpression> m_expressions;
        bool m_all = true;
    };

private:
    QStringList terms() const;
    friend class Matcher;
};

// The bar at the bottom of the file list that the filter is typed in.
class FilterBar : public QWidget {
    Q_OBJECT

public:
    explicit FilterBar(QWidget *parent = nullptr);

    QLineEdit *editor() const { return m_edit; }
    QuickFilter filter() const;
    // Sets the ways of matching this bar starts with when it is opened.
    void setDefaultMode(const QuickFilter::Mode &mode);
    // Filters on every keystroke, or only on Enter.
    void setRealTime(bool realTime) { m_realTime = realTime; }

    // Shows the bar, with the keyboard in it.
    void open();
    // Clears the filter and hides the bar.
    void close();
    QMenu *optionsMenu() const { return m_menu; }

signals:
    void filterChanged(const QuickFilter &filter);
    // Enter or an arrow key: the keyboard goes to the file list.
    void leaveRequested();
    void closed();
    // The user chose to start with these ways of matching from now on.
    void defaultsRequested(const QuickFilter::Mode &mode);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void apply();
    void setMode(const QuickFilter::Mode &mode);
    QuickFilter::Mode mode() const;
    void retranslate();
    void updateIcons();

    QLabel *m_label;
    QLineEdit *m_edit;
    QLabel *m_note;
    QToolButton *m_options;
    QToolButton *m_close;
    QMenu *m_menu;
    QAction *m_accents, *m_extensions, *m_anyWord, *m_partial, *m_regex, *m_defaults;
    QuickFilter::Mode m_defaultMode;
    QuickFilter m_applied;
    bool m_realTime = true;
};
