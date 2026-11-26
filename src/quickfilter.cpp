#include "quickfilter.h"

#include <QAction>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QToolButton>

#include "icons.h"
#include "namematch.h"

namespace {

bool hasWildcards(const QString &term) {
    return term.contains('*') || term.contains('?');
}

// Only * and ? are special; brackets are common in names.
QRegularExpression wildcard(const QString &term, bool partial) {
    QString pattern;
    for (const QChar c : term)
        pattern += c == '*' ? QStringLiteral(".*") : c == '?' ? QStringLiteral(".") : QRegularExpression::escape(QString(c));
    // Typing a wildcard says where the rest of the name may go, so
    // "*.c" means names ending in .c, not .cpp, even when matching part of
    // the name.
    if (!partial || hasWildcards(term))
        pattern = QRegularExpression::anchoredPattern(pattern);
    return QRegularExpression(pattern, QRegularExpression::DotMatchesEverythingOption);
}

} // namespace

bool QuickFilter::Mode::operator==(const Mode &o) const {
    return ignoreAccents == o.ignoreAccents && ignoreExtensions == o.ignoreExtensions && anyWord == o.anyWord &&
           partial == o.partial && regex == o.regex;
}

QStringList QuickFilter::terms() const {
    QStringList terms;
    for (const QString &part : text.split(';')) {
        if (mode.anyWord)
            terms << part.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        else if (!part.trimmed().isEmpty())
            terms << part.trimmed();
    }
    return terms;
}

QString QuickFilter::error() const {
    if (!isActive())
        return {};
    if (mode.regex && !QRegularExpression(text).isValid())
        return QCoreApplication::translate("FilterBar", "Not a valid regular expression yet");
    return {};
}

QuickFilter::Matcher::Matcher(const QuickFilter &filter) : m_mode(filter.mode) {
    if (!filter.isActive())
        return;
    auto plain = [this](const QString &s) { return NameMatch::strip(s, m_mode.ignoreAccents).text; };
    if (m_mode.regex) {
        const QString text = plain(filter.text);
        m_expressions << QRegularExpression(m_mode.partial ? text : QRegularExpression::anchoredPattern(text));
    } else {
        for (const QString &term : filter.terms()) {
            if (m_mode.partial && !hasWildcards(term))
                m_words << plain(term);
            else
                m_expressions << wildcard(plain(term), m_mode.partial);
        }
    }
    for (QRegularExpression &re : m_expressions)
        re.setPatternOptions(re.patternOptions() | QRegularExpression::CaseInsensitiveOption |
                             QRegularExpression::UseUnicodePropertiesOption);
    // Only a ";" so far, say: that filters nothing.
    m_all = m_words.isEmpty() && m_expressions.isEmpty();
}

bool QuickFilter::Matcher::matches(const QString &fullName, bool isDir) const {
    if (m_all)
        return true;
    const QString name = NameMatch::strip(
        m_mode.ignoreExtensions ? fullName.left(NameMatch::lengthWithoutExtension(fullName, isDir)) : fullName,
        m_mode.ignoreAccents).text;
    for (const QString &word : m_words)
        if (name.contains(word, Qt::CaseInsensitive))
            return true;
    for (const QRegularExpression &re : m_expressions)
        if (re.match(name).hasMatch())
            return true;
    return false;
}

FilterBar::FilterBar(QWidget *parent)
    : QWidget(parent),
      m_label(new QLabel(this)),
      m_edit(new QLineEdit(this)),
      m_note(new QLabel(this)),
      m_options(new QToolButton(this)),
      m_close(new QToolButton(this)),
      m_menu(new QMenu(this)) {
    m_edit->installEventFilter(this);
    m_label->setBuddy(m_edit);
    m_note->setForegroundRole(QPalette::PlaceholderText);
    m_note->hide();
    for (QAction **a : {&m_accents, &m_extensions, &m_anyWord, &m_partial, &m_regex}) {
        *a = m_menu->addAction(QString());
        (*a)->setCheckable(true);
        connect(*a, &QAction::triggered, this, &FilterBar::apply);
    }
    m_menu->addSeparator();
    m_defaults = m_menu->addAction(QString());
    connect(m_defaults, &QAction::triggered, this, [this] { emit defaultsRequested(mode()); });
    m_options->setAutoRaise(true);
    m_options->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(m_options, &QToolButton::clicked, this, [this] {
        // Above the button, as the bar is at the bottom of the window.
        const QPoint corner = m_options->mapToGlobal(QPoint(m_options->width() - m_menu->sizeHint().width(), 0));
        m_menu->popup(corner - QPoint(0, m_menu->sizeHint().height()));
    });
    m_options->setFocusPolicy(Qt::NoFocus);
    m_close->setAutoRaise(true);
    m_close->setFocusPolicy(Qt::NoFocus);
    connect(m_close, &QToolButton::clicked, this, &FilterBar::close);
    connect(m_edit, &QLineEdit::textEdited, this, [this] {
        // Emptying the field always shows everything again.
        if (m_realTime || m_edit->text().isEmpty())
            apply();
    });

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 3, 3, 3);
    layout->addWidget(m_label);
    layout->addWidget(m_edit, 1);
    layout->addWidget(m_note);
    layout->addWidget(m_options);
    layout->addWidget(m_close);
    setMode({});
    retranslate();
    updateIcons();
    hide();
}

QuickFilter::Mode FilterBar::mode() const {
    QuickFilter::Mode m;
    m.ignoreAccents = m_accents->isChecked();
    m.ignoreExtensions = m_extensions->isChecked();
    m.anyWord = m_anyWord->isChecked();
    m.partial = m_partial->isChecked();
    m.regex = m_regex->isChecked();
    return m;
}

void FilterBar::setMode(const QuickFilter::Mode &m) {
    m_accents->setChecked(m.ignoreAccents);
    m_extensions->setChecked(m.ignoreExtensions);
    m_anyWord->setChecked(m.anyWord);
    m_partial->setChecked(m.partial);
    m_regex->setChecked(m.regex);
    m_anyWord->setEnabled(!m.regex);
}

void FilterBar::setDefaultMode(const QuickFilter::Mode &mode) {
    m_defaultMode = mode;
    // A bar in use keeps what its own menu says.
    if (isHidden())
        setMode(mode);
}

QuickFilter FilterBar::filter() const {
    QuickFilter f;
    f.text = m_edit->text();
    f.mode = mode();
    return f;
}

void FilterBar::open() {
    if (isHidden())
        setMode(m_defaultMode);
    show();
    m_edit->setFocus();
    m_edit->selectAll();
}

void FilterBar::close() {
    m_edit->clear();
    setMode(m_defaultMode);
    apply();
    hide();
    emit closed();
}

void FilterBar::apply() {
    const QuickFilter f = filter();
    m_anyWord->setEnabled(!f.mode.regex);
    const QString error = f.error();
    m_note->setText(error);
    m_note->setVisible(!error.isEmpty());
    // While a pattern is half typed, the last whole one stays.
    if (!error.isEmpty() || f == m_applied)
        return;
    m_applied = f;
    emit filterChanged(f);
}

bool FilterBar::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        switch (key->key()) {
        case Qt::Key_Escape:
            close();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (key->modifiers() & ~Qt::KeypadModifier)
                break;
            apply();
            if (filter().error().isEmpty())
                emit leaveRequested();
            return true;
        case Qt::Key_Down:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            emit leaveRequested();
            return true;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FilterBar::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    else if (event->type() == QEvent::PaletteChange)
        updateIcons();
    QWidget::changeEvent(event);
}

void FilterBar::updateIcons() {
    m_close->setIcon(Icons::close(palette()));
    m_options->setIcon(Icons::chevron(palette()));
}

void FilterBar::retranslate() {
    m_label->setText(tr("Filter:"));
    m_edit->setPlaceholderText(tr("Part of a name, or *.jpg;*.png"));
    m_options->setText(tr("Options"));
    m_close->setToolTip(tr("Show everything again (Esc)"));
    m_accents->setText(tr("Ignore Accents"));
    m_extensions->setText(tr("Ignore Extensions"));
    m_anyWord->setText(tr("Match Any Word"));
    m_partial->setText(tr("Match Part of the Name"));
    m_regex->setText(tr("Regular Expression"));
    m_defaults->setText(tr("Make These the Defaults"));
}
