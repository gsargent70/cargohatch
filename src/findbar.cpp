#include "findbar.h"

#include <QFocusEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QStyleOptionSlider>

#include "namematch.h"

bool FindOptions::operator==(const FindOptions &o) const {
    return anywhere == o.anywhere && ignoreAccents == o.ignoreAccents && ignoreExtensions == o.ignoreExtensions &&
           preferShorter == o.preferShorter && repeatFirst == o.repeatFirst && highlight == o.highlight &&
           scrollMarks == o.scrollMarks && keepHighlights == o.keepHighlights && enterOpens == o.enterOpens;
}

FindMatcher::FindMatcher(const QString &text, const FindOptions &options)
    : m_text(NameMatch::strip(text, options.ignoreAccents).text), m_options(options) {
}

QPair<int, int> FindMatcher::match(const QString &name, bool isDir) const {
    if (m_text.isEmpty())
        return {-1, 0};
    const QString searched =
        m_options.ignoreExtensions ? name.left(NameMatch::lengthWithoutExtension(name, isDir)) : name;
    const NameMatch::Stripped s = NameMatch::strip(searched, m_options.ignoreAccents);
    const bool starts = s.text.startsWith(m_text, Qt::CaseInsensitive);
    const int at = starts ? 0 : m_options.anywhere ? int(s.text.indexOf(m_text, 0, Qt::CaseInsensitive)) : -1;
    if (at < 0)
        return {-1, 0};
    const int start = s.original(at);
    return {start, s.original(at + int(m_text.size())) - start};
}

FindBar::FindBar(QWidget *parent)
    : QWidget(parent), m_label(new QLabel(this)), m_edit(new QLineEdit(this)), m_result(new QLabel(this)) {
    m_edit->installEventFilter(this);
    m_result->setForegroundRole(QPalette::PlaceholderText);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 3, 6, 3);
    layout->addWidget(m_label);
    layout->addWidget(m_edit, 1);
    layout->addWidget(m_result);
    connect(m_edit, &QLineEdit::textEdited, this, &FindBar::textChanged);
    retranslate();
    hide();
}

void FindBar::open(const QString &text) {
    show();
    m_edit->setText(text);
    m_edit->setFocus();
    m_edit->deselect();
    m_edit->end(false);
    emit textChanged(text);
}

QString FindBar::text() const {
    return m_edit->text();
}

void FindBar::setResult(int number, int total) {
    m_number = number;
    m_total = total;
    if (m_edit->text().isEmpty())
        m_result->clear();
    else if (total == 0)
        m_result->setText(tr("No match"));
    else
        m_result->setText(number > 0 ? tr("%1 of %2").arg(number).arg(total) : QString::number(total));
}

bool FindBar::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::ShortcutOverride) {
        auto *key = static_cast<QKeyEvent *>(event);
        // With no text selected to copy, these are for the file just found.
        if ((key->matches(QKeySequence::Copy) || key->matches(QKeySequence::Cut)) && !m_edit->hasSelectedText()) {
            event->ignore();
            return true;
        }
    } else if (event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        switch (key->key()) {
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            emit closeRequested(true);
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (key->modifiers() & ~Qt::KeypadModifier)
                break;
            emit accepted();
            return true;
        case Qt::Key_Down:
            emit stepRequested(1);
            return true;
        case Qt::Key_Up:
            emit stepRequested(-1);
            return true;
        default:
            break;
        }
    } else if (event->type() == QEvent::FocusOut) {
        // Going to another window or a menu and back leaves it open.
        const Qt::FocusReason reason = static_cast<QFocusEvent *>(event)->reason();
        if (reason != Qt::ActiveWindowFocusReason && reason != Qt::PopupFocusReason &&
            reason != Qt::MenuBarFocusReason)
            emit closeRequested(false);
    }
    return QWidget::eventFilter(watched, event);
}

void FindBar::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

void FindBar::retranslate() {
    m_label->setText(tr("Find:"));
    setResult(m_number, m_total);
}

MatchScrollBar::MatchScrollBar(Qt::Orientation orientation, QWidget *parent) : QScrollBar(orientation, parent) {
}

void MatchScrollBar::setMarks(const QList<double> &marks) {
    if (marks == m_marks)
        return;
    m_marks = marks;
    update();
}

void MatchScrollBar::paintEvent(QPaintEvent *event) {
    QScrollBar::paintEvent(event);
    if (m_marks.isEmpty())
        return;
    QStyleOptionSlider option;
    initStyleOption(&option);
    const QRect groove = style()->subControlRect(QStyle::CC_ScrollBar, &option, QStyle::SC_ScrollBarGroove, this);
    QPainter painter(this);
    const QColor color = palette().color(QPalette::Accent);
    const bool vertical = orientation() == Qt::Vertical;
    const int length = (vertical ? groove.height() : groove.width()) - 2;
    // Thousands of matches still only need one tick for each pixel.
    int last = -1;
    for (const double mark : m_marks) {
        const int at = int(mark * length);
        if (at == last)
            continue;
        last = at;
        if (vertical)
            painter.fillRect(groove.left() + 3, groove.top() + at, groove.width() - 6, 2, color);
        else
            painter.fillRect(groove.left() + at, groove.top() + 3, 2, groove.height() - 6, color);
    }
}
