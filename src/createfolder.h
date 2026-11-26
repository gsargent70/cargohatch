#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;

// Making new folders (FCMD-01).
namespace CreateFolder {

// What to do with the first new folder afterwards.
enum class Then { Select, Open, NewTab, OtherSide, Count };
QString thenId(Then then);
Then thenFromId(const QString &id);

// How many folders one line or one box may ask for.
constexpr int kMax = 1000;

// "2026/{Jan,Feb}" gives 2026/Jan and 2026/Feb; "Disc {01..03}" gives Disc 01,
// Disc 02 and Disc 03. Braces that aren't a list or a range stay as they are.
QStringList expand(const QString &line);
// The paths, relative to the folder they go in, that `text` asks for: one
// name or, if `several`, one per line with braces expanded.
QStringList parse(const QString &text, bool several);
// What's wrong with one path, or empty.
QString error(const QString &path);

// The first of "New Folder", "New Folder (2)"… that is free in `base`.
QString defaultName(const QString &base);

struct Result {
    // The paths that were made, and those already there as folders.
    QStringList created;
    QStringList existing;
    QStringList errors;
};
// Makes each path inside `base`, with the folders leading to it.
Result create(const QString &base, const QStringList &paths);

} // namespace CreateFolder

class CreateFolderDialog : public QDialog {
    Q_OBJECT

public:
    struct Setup {
        QString base;
        // Names to offer besides the default one, best first.
        QStringList suggestions;
        bool several = false;
        CreateFolder::Then then = CreateFolder::Then::Select;
    };
    CreateFolderDialog(const Setup &setup, QWidget *parent = nullptr);

    bool isSeveral() const;
    CreateFolder::Then then() const;
    // What was made, once accepted.
    const CreateFolder::Result &result() const { return m_result; }

    // Makes the folders; the dialog stays open, saying why, if none could be.
    void accept() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QString text() const;
    void validate();

    QString m_base;
    QComboBox *m_name;
    QPlainTextEdit *m_names;
    QCheckBox *m_several;
    QComboBox *m_then;
    QLabel *m_note;
    QPushButton *m_ok;
    CreateFolder::Result m_result;
};
