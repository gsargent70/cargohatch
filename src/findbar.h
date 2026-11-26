#pragma once

#include <QList>
#include <QPair>
#include <QScrollBar>
#include <QWidget>

class QLabel;
class QLineEdit;

// How find-as-you-type looks for names (SEL-11).
struct FindOptions {
    // Anywhere in the name, though a name that starts with the text is
    // still preferred; otherwise only at the start.
    bool anywhere = true;
    bool ignoreAccents = true;
    bool ignoreExtensions = false;
    // Jump to the shortest matching name rather than the next one.
    bool preferShorter = false;
    // Pressing one letter again and again steps through the names starting
    // with it.
    bool repeatFirst = true;
    bool highlight = true;
    bool scrollMarks = true;
    // Highlights stay after the field closes, until Escape.
    bool keepHighlights = false;
    bool enterOpens = true;

    bool operator==(const FindOptions &o) const;
};

// Looks for one text in names.
class FindMatcher {
public:
    FindMatcher(const QString &text, const FindOptions &options);

    // Where the match is in `name` (position and length), or -1 and 0.
    QPair<int, int> match(const QString &name, bool isDir) const;

private:
    QString m_text;
    FindOptions m_options;
};

// The field that opens at the bottom of the file list when the user types.
// It only holds the text; the folder tab does the finding.
class FindBar : public QWidget {
    Q_OBJECT

public:
    explicit FindBar(QWidget *parent = nullptr);

    // Shows the field with `text` in it and the keyboard in it.
    void open(const QString &text);
    QString text() const;
    QLineEdit *editor() const { return m_edit; }
    // Says which match this is; a total of 0 means nothing matches.
    void setResult(int number, int total);

signals:
    void textChanged(const QString &text);
    // Arrow keys: the next match (1) or the one before (-1).
    void stepRequested(int step);
    // Enter.
    void accepted();
    // Escape or Tab, or the keyboard went somewhere else. `refocus` is
    // whether it should go to the file list.
    void closeRequested(bool refocus);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void retranslate();

    QLabel *m_label;
    QLineEdit *m_edit;
    QLabel *m_result;
    int m_number = 0;
    int m_total = 0;
};

// A scroll bar with ticks where the matches are.
class MatchScrollBar : public QScrollBar {
    Q_OBJECT

public:
    explicit MatchScrollBar(Qt::Orientation orientation, QWidget *parent = nullptr);
    // From 0 (the top or left) to 1.
    void setMarks(const QList<double> &marks);
    QList<double> marks() const { return m_marks; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QList<double> m_marks;
};
