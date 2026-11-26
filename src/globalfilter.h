#pragma once

#include <QDialog>
#include <QList>
#include <QRegularExpression>
#include <QString>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

// What is hidden in every folder of every window (FILT-03).
struct GlobalFilter {
    // Names that start with a dot.
    bool showHidden = false;
    // Names listed in a folder's own ".hidden" file, which other Linux file
    // managers hide too.
    bool hideListed = true;
    // Names matching these patterns: wildcards separated by ";", or a
    // regular expression that must match the whole name.
    bool hideNames = false;
    QString files;
    bool filesRegex = false;
    QString folders;
    bool foldersRegex = false;

    bool operator==(const GlobalFilter &o) const;
    bool operator!=(const GlobalFilter &o) const { return !(*this == o); }

    // What's wrong with one of the patterns, or empty.
    static QString patternError(const QString &pattern, bool regex);

    // A filter ready for use, with its patterns compiled once.
    class Matcher {
    public:
        explicit Matcher(const GlobalFilter &filter);
        // `listed` is whether the folder's .hidden file names it.
        bool hides(const QString &name, bool isDir, bool listed) const;

    private:
        bool m_showHidden;
        bool m_hideListed;
        QList<QRegularExpression> m_files;
        QList<QRegularExpression> m_folders;
    };
};

// View > Global Filters…
class GlobalFilterDialog : public QDialog {
    Q_OBJECT

public:
    explicit GlobalFilterDialog(const GlobalFilter &filter, QWidget *parent = nullptr);
    GlobalFilter filter() const;

private:
    void validate();

    QCheckBox *m_hideHidden;
    QCheckBox *m_hideListed;
    QCheckBox *m_hideNames;
    QLineEdit *m_files;
    QCheckBox *m_filesRegex;
    QLineEdit *m_folders;
    QCheckBox *m_foldersRegex;
    QLabel *m_note;
    QPushButton *m_ok;
};
