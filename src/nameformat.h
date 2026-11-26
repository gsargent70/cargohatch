#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

class QComboBox;
class QLabel;
class QPushButton;

// Turning items into text for the clipboard (CLIP-03).
namespace NameFormat {

enum class Style { FullPath, Name, Stem, Folder, Url, Quoted, Count };

QString title(Style style);
// The custom format that gives `style`.
QString format(Style style);

// `path` written out by `format`, with {path}, {name}, {stem}, {ext},
// {folder}, {parent}, {alias}, {url}, {quoted}, {size}, {bytes} and {date} filled in,
// and \n and \t for new line and tab. Unknown codes are left as they are.
QString apply(const QString &format, const QString &path);
// Every item, on a line each, or on one line with spaces between.
QString applyAll(const QString &format, const QStringList &paths, bool oneLine);

// `text` quoted so a shell takes it as one word.
QString quoted(const QString &text);

// The codes, each with what it stands for.
QList<QPair<QString, QString>> codes();

} // namespace NameFormat

// Asks for a custom format, showing what it gives.
class NameFormatDialog : public QDialog {
    Q_OBJECT

public:
    NameFormatDialog(const QStringList &recent, const QStringList &paths, bool oneLine, QWidget *parent = nullptr);
    QString format() const;

private:
    void showPreview();

    QStringList m_paths;
    bool m_oneLine;
    QComboBox *m_format;
    QLabel *m_preview;
    QPushButton *m_ok;
};
