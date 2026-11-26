#include "nameformat.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QPushButton>
#include <QRegularExpression>
#include <QUrl>
#include <QVBoxLayout>

#include "filemodel.h"
#include "folderaliases.h"
#include "renamedelegate.h"

namespace {

QString tr(const char *text) {
    return QCoreApplication::translate("NameFormat", text);
}

QString folderOf(const QString &path) {
    return QFileInfo(path).path();
}

// The value of one code, or a null string if there's no such code.
QString value(const QString &code, const QString &path) {
    const QFileInfo info(path);
    const QString name = info.fileName().isEmpty() ? path : info.fileName();
    // Folders have no extension, even with a dot in the name.
    const int dot = info.isDir() ? -1 : RenameDelegate::extensionStart(name);
    auto text = [](const QString &s) { return s.isNull() ? QStringLiteral("") : s; };
    if (code == "path")
        return text(path);
    if (code == "name")
        return text(name);
    if (code == "stem")
        return text(dot < 0 ? name : name.left(dot));
    if (code == "ext")
        return text(dot < 0 ? QString() : name.mid(dot + 1));
    if (code == "folder")
        return text(folderOf(path));
    if (code == "parent")
        return text(QFileInfo(folderOf(path)).fileName());
    if (code == "url")
        return QString::fromLatin1(QUrl::fromLocalFile(path).toEncoded());
    if (code == "alias")
        return FolderAliases::shorten(path);
    if (code == "quoted")
        return NameFormat::quoted(path);
    if (code == "size")
        return text(info.isDir() ? QString() : FileModel::sizeText(info.size()));
    if (code == "bytes")
        return text(info.isDir() ? QString() : QString::number(info.size()));
    if (code == "date")
        return text(QLocale().toString(info.lastModified(), QLocale::ShortFormat));
    return {};
}

} // namespace

QString NameFormat::title(Style style) {
    switch (style) {
    case Style::FullPath: return tr("Full Paths");
    case Style::Name: return tr("Names");
    case Style::Stem: return tr("Names Without Extensions");
    case Style::Folder: return tr("Folder Paths");
    case Style::Url: return tr("URLs");
    case Style::Quoted: return tr("Paths Quoted for the Shell");
    case Style::Count: break;
    }
    return {};
}

QString NameFormat::format(Style style) {
    switch (style) {
    case Style::FullPath: return QStringLiteral("{path}");
    case Style::Name: return QStringLiteral("{name}");
    case Style::Stem: return QStringLiteral("{stem}");
    case Style::Folder: return QStringLiteral("{folder}");
    case Style::Url: return QStringLiteral("{url}");
    case Style::Quoted: return QStringLiteral("{quoted}");
    case Style::Count: break;
    }
    return {};
}

QList<QPair<QString, QString>> NameFormat::codes() {
    return {{"path", tr("Full path")},          {"name", tr("Name")},
            {"stem", tr("Name, no extension")}, {"ext", tr("Extension")},
            {"folder", tr("Folder's path")},    {"parent", tr("Folder's name")},
            {"alias", tr("Path with a folder alias")},
            {"url", tr("URL")},                 {"quoted", tr("Path quoted for the shell")},
            {"size", tr("Size")},               {"bytes", tr("Size in bytes")},
            {"date", tr("Date modified")}};
}

QString NameFormat::quoted(const QString &text) {
    static const QRegularExpression plain(QStringLiteral("\\A[A-Za-z0-9_@%+=:,./-]+\\z"));
    if (plain.match(text).hasMatch() && !text.startsWith('-'))
        return text;
    QString escaped = text;
    escaped.replace('\'', QStringLiteral("'\\''"));
    return '\'' + escaped + '\'';
}

QString NameFormat::apply(const QString &format, const QString &path) {
    QString out;
    for (int i = 0; i < format.size(); ++i) {
        const QChar c = format.at(i);
        if (c == '{') {
            const int end = int(format.indexOf('}', i));
            const QString filled = end < 0 ? QString() : value(format.mid(i + 1, end - i - 1).toLower(), path);
            if (!filled.isNull()) {
                out += filled;
                i = end;
                continue;
            }
        } else if (c == '\\' && i + 1 < format.size()) {
            const QChar next = format.at(i + 1);
            if (next == 'n' || next == 't' || next == '\\') {
                out += next == 'n' ? QChar('\n') : next == 't' ? QChar('\t') : QChar('\\');
                ++i;
                continue;
            }
        }
        out += c;
    }
    return out;
}

QString NameFormat::applyAll(const QString &format, const QStringList &paths, bool oneLine) {
    QStringList lines;
    for (const QString &path : paths) {
        const QString line = apply(format, path);
        // Several items in one folder give its path once.
        if (format == NameFormat::format(Style::Folder) && lines.contains(line))
            continue;
        lines << line;
    }
    return lines.join(oneLine ? QChar(' ') : QChar('\n'));
}

NameFormatDialog::NameFormatDialog(const QStringList &recent, const QStringList &paths, bool oneLine, QWidget *parent)
    : QDialog(parent), m_paths(paths), m_oneLine(oneLine), m_format(new QComboBox(this)), m_preview(new QLabel(this)) {
    setWindowTitle(tr("Copy Names As"));
    const int width = fontMetrics().averageCharWidth() * 56;
    m_format->setEditable(true);
    m_format->setInsertPolicy(QComboBox::NoInsert);
    m_format->addItems(recent.isEmpty() ? QStringList{"{name}\\t{size}", "[{name}]({url})"} : recent);
    m_format->setFixedWidth(width);
    m_format->lineEdit()->selectAll();

    auto *codes = new QPushButton(tr("&Insert Code"), this);
    auto *menu = new QMenu(codes);
    for (const auto &[code, meaning] : NameFormat::codes()) {
        const QString text = '{' + code + '}';
        connect(menu->addAction(QStringLiteral("%1\t%2").arg(meaning, text)), &QAction::triggered, this, [this, text] {
            m_format->lineEdit()->insert(text);
            m_format->setFocus();
        });
    }
    codes->setMenu(menu);

    m_preview->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_preview->setTextFormat(Qt::PlainText);
    m_preview->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_preview->setFrameShape(QFrame::StyledPanel);
    m_preview->setMargin(6);
    m_preview->setFixedWidth(width);
    m_preview->setFixedHeight(QFontMetrics(m_preview->font()).lineSpacing() * 5 + 16);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    m_ok->setText(tr("Copy"));
    m_ok->setDefault(true);
    buttons->addButton(codes, QDialogButtonBox::ResetRole);
    codes->setAutoDefault(false);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *label = new QLabel(tr("&Format:"), this);
    label->setBuddy(m_format);
    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize); // see ChooseFolderDialog
    layout->addWidget(label);
    layout->addWidget(m_format);
    layout->addWidget(new QLabel(tr("This gives:"), this));
    layout->addWidget(m_preview);
    layout->addWidget(buttons);
    connect(m_format, &QComboBox::currentTextChanged, this, &NameFormatDialog::showPreview);
    showPreview();
}

QString NameFormatDialog::format() const {
    return m_format->currentText();
}

void NameFormatDialog::showPreview() {
    // The first few items are enough to judge by.
    QStringList lines = NameFormat::applyAll(format(), m_paths.mid(0, 5), m_oneLine).split('\n');
    const bool more = lines.size() > 5 || m_paths.size() > 5;
    lines = lines.mid(0, more ? 4 : 5);
    if (more)
        lines << QStringLiteral("…");
    const QFontMetrics metrics(m_preview->font());
    for (QString &line : lines)
        line = metrics.elidedText(line, Qt::ElideRight, m_preview->width() - 20);
    m_preview->setText(lines.join('\n'));
    m_ok->setEnabled(!format().isEmpty());
}
