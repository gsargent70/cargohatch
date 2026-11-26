#include "createfolder.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextBlock>
#include <QVBoxLayout>

#include <cerrno>
#include <cstring>
#include <sys/stat.h>

#include "fileops.h"

namespace {

QString tr(const char *text) {
    return QCoreApplication::translate("CreateFolder", text);
}

const char *const kThenIds[] = {"select", "open", "tab", "other"};

// Where the brace opened at `open` closes, or -1.
int closingBrace(const QString &s, int open) {
    int depth = 0;
    for (int i = open; i < s.size(); ++i) {
        if (s.at(i) == '{')
            ++depth;
        else if (s.at(i) == '}' && --depth == 0)
            return i;
    }
    return -1;
}

// What a brace's contents stand for: the parts of "a,b" or the numbers of
// "01..10". Empty if it is neither.
QStringList alternatives(const QString &inside) {
    static const QRegularExpression range(QStringLiteral("\\A(\\d{1,9})\\.\\.(\\d{1,9})\\z"));
    const QRegularExpressionMatch m = range.match(inside);
    if (m.hasMatch()) {
        const int from = m.captured(1).toInt();
        const int to = m.captured(2).toInt();
        // Zeros in front set the width: {01..10}.
        const bool padded = (m.captured(1).size() > 1 && m.captured(1).startsWith('0')) ||
                            (m.captured(2).size() > 1 && m.captured(2).startsWith('0'));
        const int width = padded ? int(qMax(m.captured(1).size(), m.captured(2).size())) : 0;
        QStringList numbers;
        const int step = from <= to ? 1 : -1;
        for (int n = from; numbers.size() <= CreateFolder::kMax; n += step) {
            numbers << QStringLiteral("%1").arg(n, width, 10, QChar('0'));
            if (n == to)
                break;
        }
        return numbers;
    }
    QStringList parts;
    int depth = 0, start = 0;
    for (int i = 0; i < inside.size(); ++i) {
        if (inside.at(i) == '{')
            ++depth;
        else if (inside.at(i) == '}')
            --depth;
        else if (inside.at(i) == ',' && depth == 0) {
            parts << inside.mid(start, i - start);
            start = i + 1;
        }
    }
    if (parts.isEmpty())
        return {};
    parts << inside.mid(start);
    return parts;
}

} // namespace

QString CreateFolder::thenId(Then then) {
    return kThenIds[int(then)];
}

CreateFolder::Then CreateFolder::thenFromId(const QString &id) {
    for (int i = 0; i < int(Then::Count); ++i)
        if (id == kThenIds[i])
            return Then(i);
    return Then::Select;
}

QStringList CreateFolder::expand(const QString &line) {
    for (int open = int(line.indexOf('{')); open >= 0; open = int(line.indexOf('{', open + 1))) {
        const int close = closingBrace(line, open);
        if (close < 0)
            break;
        const QStringList parts = alternatives(line.mid(open + 1, close - open - 1));
        if (parts.isEmpty())
            continue;
        const QString before = line.left(open);
        QStringList all;
        const QStringList tails = expand(line.mid(close + 1));
        for (const QString &part : parts)
            for (const QString &middle : expand(part))
                for (const QString &tail : tails) {
                    if (all.size() > kMax)
                        return all;
                    all << before + middle + tail;
                }
        return all;
    }
    return {line};
}

QStringList CreateFolder::parse(const QString &text, bool several) {
    QStringList paths;
    auto add = [&paths](const QString &raw) {
        // Spaces around the whole name are slips of the hand; a slash at
        // the end means nothing.
        QString path = raw.trimmed();
        while (path.size() > 1 && path.endsWith('/'))
            path.chop(1);
        if (!path.isEmpty() && !paths.contains(path))
            paths << path;
    };
    if (!several) {
        add(text);
        return paths;
    }
    for (const QString &line : text.split('\n'))
        for (const QString &path : expand(line.trimmed())) {
            if (paths.size() > kMax)
                return paths;
            add(path);
        }
    return paths;
}

QString CreateFolder::error(const QString &path) {
    if (path.isEmpty())
        return tr("Type a name for the folder.");
    if (path.startsWith('/') || path == "~" || path.startsWith("~/"))
        return tr("Type a name or a path inside this folder.");
    for (const QString &part : path.split('/')) {
        if (part.trimmed().isEmpty())
            return tr("%1 has an empty name in it.").arg(path);
        if (part == "." || part == "..")
            return tr("%1 can't be used as a name.").arg(part);
        if (QFile::encodeName(part).size() > 255)
            return tr("That name is too long.");
    }
    return {};
}

QString CreateFolder::defaultName(const QString &base) {
    const QString name = tr("New Folder");
    const QFileInfo info(base + '/' + name);
    return info.exists() || info.isSymLink() ? FileOperation::freeName(base, name) : name;
}

CreateFolder::Result CreateFolder::create(const QString &base, const QStringList &paths) {
    Result result;
    for (const QString &path : paths) {
        const QString bad = error(path);
        if (!bad.isEmpty()) {
            result.errors << bad;
            continue;
        }
        if (QFileInfo(base + '/' + path).isDir()) {
            result.existing << path;
            continue;
        }
        QString at = base;
        QString failure;
        for (const QString &part : path.split('/')) {
            at += '/' + part;
            if (::mkdir(QFile::encodeName(at).constData(), 0777) == 0)
                continue;
            const int why = errno;
            if (why == EEXIST) {
                if (QFileInfo(at).isDir())
                    continue;
                failure = tr("%1 is already there, and it isn't a folder.").arg(at);
            } else {
                failure = tr("Couldn't create %1: %2").arg(at, QString::fromLocal8Bit(std::strerror(why)));
            }
            break;
        }
        if (failure.isEmpty())
            result.created << path;
        else
            result.errors << failure;
    }
    return result;
}

CreateFolderDialog::CreateFolderDialog(const Setup &setup, QWidget *parent)
    : QDialog(parent),
      m_base(setup.base),
      m_name(new QComboBox(this)),
      m_names(new QPlainTextEdit(this)),
      m_several(new QCheckBox(tr("Create &several, one on each line"), this)),
      m_then(new QComboBox(this)),
      m_note(new QLabel(this)) {
    setWindowTitle(tr("Create Folder"));
    const int width = fontMetrics().averageCharWidth() * 48;
    const QString first = CreateFolder::defaultName(m_base);
    m_name->setEditable(true);
    m_name->setInsertPolicy(QComboBox::NoInsert);
    m_name->addItem(first);
    for (const QString &name : setup.suggestions)
        if (m_name->findText(name, Qt::MatchExactly | Qt::MatchCaseSensitive) < 0)
            m_name->addItem(name);
    m_name->setFixedWidth(width);
    // The default completer changes the case of what is typed to match.
    m_name->setCompleter(nullptr);
    m_name->lineEdit()->selectAll();
    m_names->setFixedSize(width, fontMetrics().lineSpacing() * 8 + 12);
    m_names->setTabChangesFocus(true);
    m_names->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_names->installEventFilter(this);
    m_several->setChecked(setup.several);
    m_then->addItems({tr("Select it"), tr("Open it"), tr("Open it in a new tab"), tr("Open it on the other side")});
    m_then->setCurrentIndex(int(setup.then));
    m_note->setWordWrap(true);
    m_note->setTextFormat(Qt::PlainText);
    m_note->setForegroundRole(QPalette::PlaceholderText);
    m_note->setFixedWidth(width);
    m_note->setMinimumHeight(fontMetrics().lineSpacing() * 2);
    m_note->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    auto *where = new QLabel(fontMetrics().elidedText(tr("In %1").arg(m_base), Qt::ElideMiddle, width), this);
    where->setTextFormat(Qt::PlainText);
    where->setToolTip(m_base);

    auto *after = new QFormLayout;
    after->addRow(tr("&Then:"), m_then);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    m_ok->setText(tr("Create"));
    m_ok->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &CreateFolderDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize); // see ChooseFolderDialog
    layout->addWidget(where);
    layout->addWidget(m_name);
    layout->addWidget(m_names);
    layout->addWidget(m_note);
    layout->addWidget(m_several);
    layout->addLayout(after);
    layout->addWidget(buttons);

    auto showMode = [this] {
        m_name->setVisible(!m_several->isChecked());
        m_names->setVisible(m_several->isChecked());
        (m_several->isChecked() ? static_cast<QWidget *>(m_names) : m_name)->setFocus();
        validate();
    };
    connect(m_several, &QCheckBox::toggled, this, [this, showMode](bool several) {
        // What has been typed carries over, unless it is only the default.
        if (several) {
            if (m_name->currentText() != m_name->itemText(0))
                m_names->setPlainText(m_name->currentText());
        } else if (const QString line = m_names->toPlainText().section('\n', 0, 0); !line.trimmed().isEmpty()) {
            m_name->setCurrentText(line);
        }
        showMode();
    });
    connect(m_name, &QComboBox::currentTextChanged, this, &CreateFolderDialog::validate);
    connect(m_names, &QPlainTextEdit::textChanged, this, &CreateFolderDialog::validate);
    showMode();
}

bool CreateFolderDialog::isSeveral() const {
    return m_several->isChecked();
}

CreateFolder::Then CreateFolderDialog::then() const {
    return CreateFolder::Then(m_then->currentIndex());
}

QString CreateFolderDialog::text() const {
    return isSeveral() ? m_names->toPlainText() : m_name->currentText();
}

void CreateFolderDialog::validate() {
    const QStringList paths = CreateFolder::parse(text(), isSeveral());
    QString problem = paths.isEmpty() && !isSeveral() ? CreateFolder::error({}) : QString();
    int existing = 0;
    for (const QString &path : paths) {
        if (problem.isEmpty())
            problem = CreateFolder::error(path);
        if (QFileInfo::exists(m_base + '/' + path)) {
            ++existing;
            if (!isSeveral() && problem.isEmpty())
                problem = tr("%1 already exists.").arg(path);
        }
    }
    if (paths.size() > CreateFolder::kMax)
        problem = tr("That's more than %1 folders.").arg(CreateFolder::kMax);

    QString note = problem;
    if (note.isEmpty() && !isSeveral()) {
        note = tr("a/b/c makes folders inside each other.");
    } else if (note.isEmpty() && paths.isEmpty()) {
        note = tr("2026/{Jan,Feb} or Disc {01..12} makes a set of folders. Shift+Enter, or Enter on an empty line, creates them.");
    } else if (note.isEmpty()) {
        note = tr("%n folders: %1", "", int(paths.size() - existing));
        QStringList fresh;
        for (const QString &path : paths)
            if (!QFileInfo::exists(m_base + '/' + path))
                fresh << path;
        note = fresh.isEmpty() ? tr("All of these already exist.")
               : fresh.size() <= 3
                   ? note.arg(fresh.join(QStringLiteral(", ")))
                   : note.arg(fresh.mid(0, 2).join(QStringLiteral(", ")) + QStringLiteral(" … ") + fresh.last());
        if (existing && !fresh.isEmpty())
            note += QStringLiteral("  ·  ") + tr("%n already exist", "", existing);
    }
    // Long names mustn't make the fixed-size dialog grow line by line.
    m_note->setText(fontMetrics().elidedText(note, Qt::ElideRight, m_note->width() * 2 - 60));
    m_ok->setEnabled(problem.isEmpty() && paths.size() > existing);
}

void CreateFolderDialog::accept() {
    if (!m_ok->isEnabled())
        return;
    m_result = CreateFolder::create(m_base, CreateFolder::parse(text(), isSeveral()));
    if (m_result.created.isEmpty() && !m_result.errors.isEmpty()) {
        m_note->setText(m_result.errors.first());
        return;
    }
    QDialog::accept();
}

bool CreateFolderDialog::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_names && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            const QTextCursor cursor = m_names->textCursor();
            const bool onEmptyLastLine = cursor.atEnd() && cursor.block().text().trimmed().isEmpty();
            if (key->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier) ||
                (onEmptyLastLine && !m_names->toPlainText().trimmed().isEmpty())) {
                accept();
                return true;
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}
