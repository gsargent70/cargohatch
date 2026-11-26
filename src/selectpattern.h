#pragma once

#include <QDialog>
#include <QRegularExpression>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;

// Choosing items by name (SEL-02).
struct SelectPattern {
    enum class Action {
        // Add the matching items to the selection.
        Select,
        // Take them out of it.
        Deselect,
        // Select the matching items and nothing else.
        SelectOnly,
    };
    enum class Scope { All, FilesOnly, FoldersOnly };

    // Wildcards (* and ?) by default, several separated by ";", like
    // "*.jpg;*.png". As a regular expression it must match the whole name.
    QString text;
    bool regex = false;
    bool matchCase = false;
    Action action = Action::Select;
    Scope scope = Scope::All;

    // Empty if the pattern is usable, else what's wrong with it.
    QString error() const;
    bool matches(const QString &name) const;
    bool applies(bool isDir) const {
        return scope == Scope::All || (scope == Scope::FoldersOnly) == isDir;
    }
    // The pattern compiled, for matching many names.
    QList<QRegularExpression> expressions() const;
};

// Asks for a pattern, with the last few used to pick from.
class SelectPatternDialog : public QDialog {
    Q_OBJECT

public:
    explicit SelectPatternDialog(const QStringList &recent, QWidget *parent = nullptr);
    SelectPattern pattern() const;

private:
    void validate();

    QComboBox *m_text;
    QCheckBox *m_regex;
    QCheckBox *m_matchCase;
    QComboBox *m_scope;
    QRadioButton *m_select;
    QRadioButton *m_deselect;
    QRadioButton *m_selectOnly;
    QLabel *m_error;
    QPushButton *m_ok;
};
