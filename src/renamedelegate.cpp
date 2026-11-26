#include "renamedelegate.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPainter>
#include <QTextLayout>
#include <QTimer>

#include "filemodel.h"
#include "findbar.h"

namespace {

const char *const kPartProperty = "cargohatchRenamePart";

} // namespace

RenameDelegate::RenameDelegate(QObject *parent) : QStyledItemDelegate(parent) {
}

int RenameDelegate::extensionStart(const QString &name) {
    const int dot = name.lastIndexOf('.');
    return dot > 0 ? dot : -1;
}

void RenameDelegate::select(QLineEdit *editor, const QString &name, Selection part) {
    const int dot = extensionStart(name);
    switch (part) {
    case Selection::Stem:
        editor->setSelection(0, dot < 0 ? int(name.size()) : dot);
        break;
    case Selection::Whole:
        editor->selectAll();
        break;
    case Selection::Extension:
        if (dot < 0)
            editor->setCursorPosition(int(name.size()));
        else
            editor->setSelection(dot + 1, int(name.size()) - dot - 1);
        break;
    case Selection::Start:
        editor->setCursorPosition(0);
        break;
    case Selection::End:
        editor->setCursorPosition(int(name.size()));
        break;
    }
    editor->setProperty(kPartProperty, int(part));
}

void RenameDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const {
    QStyledItemDelegate::initStyleOption(option, index);
    if (index.data(FileModel::CutRole).toBool())
        option->state &= ~QStyle::State_Enabled;
}

QSize RenameDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    if (m_maximumWidth > 0)
        size.setWidth(qMin(size.width(), m_maximumWidth));
    return size;
}

void RenameDelegate::drawCursor(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) {
    // Asked of the view: not every view says so in the option.
    const auto *view = qobject_cast<const QAbstractItemView *>(option.widget);
    if (!view || view->currentIndex() != index.siblingAtColumn(view->currentIndex().column()) ||
        !view->hasFocus() || option.state & QStyle::State_Selected || index.column() != int(Column::Name))
        return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(option.palette.color(QPalette::Accent), 1.5));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(QRectF(option.rect).adjusted(1, 1, -1, -1), 3, 3);
    painter->restore();
}

void RenameDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QStyledItemDelegate::paint(painter, option, index);
    drawCursor(painter, option, index);
    if (!m_matcher || index.column() != int(Column::Name))
        return;
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    const QWidget *widget = opt.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    const int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, widget) + 1;
    const QRect text = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, widget).adjusted(margin, 0, -margin, 0);
    drawMatch(painter, text, index, opt.font, opt.displayAlignment, opt.textElideMode,
              opt.features & QStyleOptionViewItem::WrapText, opt.palette.color(QPalette::Accent));
}

void RenameDelegate::drawMatch(QPainter *painter, const QRect &rect, const QModelIndex &index, const QFont &font,
                               Qt::Alignment alignment, Qt::TextElideMode elide, bool wrap,
                               const QColor &color) const {
    if (!m_matcher)
        return;
    const QString name = index.data(Qt::EditRole).toString();
    const auto [start, length] = m_matcher->match(name, index.data(FileModel::FolderRole).toBool());
    if (start < 0)
        return;
    // The same lines the style breaks the name into.
    const QFontMetrics metrics(font);
    QTextLayout layout(name, font);
    QTextOption textOption;
    textOption.setWrapMode(wrap ? QTextOption::WordWrap : QTextOption::ManualWrap);
    layout.setTextOption(textOption);
    QList<QPair<int, int>> lines;
    double height = 0;
    layout.beginLayout();
    for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
        line.setLineWidth(rect.width());
        lines.append({line.textStart(), line.textLength()});
        height += line.height();
    }
    layout.endLayout();

    painter->save();
    painter->setClipRect(rect, Qt::IntersectClip);
    QColor fill = color;
    fill.setAlphaF(0.35f);
    const double lineHeight = height / qMax(1, int(lines.size()));
    double y = alignment & Qt::AlignVCenter ? rect.top() + qMax(0.0, (rect.height() - height) / 2)
               : alignment & Qt::AlignBottom ? rect.bottom() + 1 - height
                                             : rect.top();
    for (const auto &[lineStart, lineLength] : std::as_const(lines)) {
        QString text = name.mid(lineStart, lineLength);
        while (text.endsWith(' '))
            text.chop(1);
        int from = qBound(0, start - lineStart, int(text.size()));
        int to = qBound(0, start + length - lineStart, int(text.size()));
        const bool touches = to > from;
        // A name cut short with "…" shows its start, its end, or both. The
        // parts of the match that are still there are marked.
        const QString shown = metrics.elidedText(text, elide, rect.width());
        QList<QPair<int, int>> ranges;
        if (shown == text) {
            ranges.append({from, to});
        } else {
            const int dots = int(shown.indexOf(QChar(0x2026)));
            const int tail = int(shown.size()) - dots - 1;
            if (from < dots)
                ranges.append({from, qMin(to, dots)});
            if (to > text.size() - tail) {
                const int shift = int(shown.size() - text.size());
                ranges.append({qMax(from + shift, dots + 1), to + shift});
            }
            // The match is all in the part that isn't shown.
            if (touches && ranges.isEmpty())
                ranges.append({dots, dots + 1});
        }
        const int width = metrics.horizontalAdvance(shown);
        const double left = alignment & Qt::AlignHCenter ? rect.left() + (rect.width() - width) / 2.0
                            : alignment & Qt::AlignRight ? rect.right() + 1 - width
                                                         : rect.left();
        for (const auto &[a, b] : std::as_const(ranges)) {
            if (!touches || b <= a)
                continue;
            const double x1 = left + metrics.horizontalAdvance(shown, a);
            const double x2 = left + metrics.horizontalAdvance(shown, b);
            painter->fillRect(QRectF(x1, y, x2 - x1, lineHeight), fill);
            painter->fillRect(QRectF(x1, y + lineHeight - 2, x2 - x1, 2), color);
        }
        y += lineHeight;
    }
    painter->restore();
}

QWidget *RenameDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const {
    auto *editor = new QLineEdit(parent);
    editor->setFrame(true);
    editor->installEventFilter(const_cast<RenameDelegate *>(this));
    return editor;
}

void RenameDelegate::setEditorData(QWidget *widget, const QModelIndex &index) const {
    auto *editor = static_cast<QLineEdit *>(widget);
    // Only the first time: the view calls this again when the model
    // changes, which mustn't throw away what is being typed.
    if (editor->property(kPartProperty).isValid())
        return;
    const QString name = index.data(Qt::EditRole).toString();
    editor->setText(name);
    select(editor, name, m_selection);
    // The view selects all of a line edit's text once it has opened it, so
    // choose the part again after that.
    const Selection part = m_selection;
    QTimer::singleShot(0, editor, [editor, name, part] { select(editor, name, part); });
}

void RenameDelegate::setModelData(QWidget *widget, QAbstractItemModel *, const QModelIndex &index) const {
    const QString name = static_cast<QLineEdit *>(widget)->text();
    if (m_renamer && name != index.data(Qt::EditRole).toString())
        m_renamer(index, name);
}

void RenameDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const {
    QStyledItemDelegate::updateEditorGeometry(editor, option, index);
    // Long names need more room than a narrow column or icon label gives.
    QRect r = editor->geometry();
    const int wanted = editor->fontMetrics().horizontalAdvance(index.data(Qt::EditRole).toString()) + 24;
    if (r.width() < wanted && editor->parentWidget())
        r.setWidth(qMin(wanted, editor->parentWidget()->width() - r.left()));
    r.setHeight(qMax(r.height(), editor->sizeHint().height()));
    editor->setGeometry(r);
}

bool RenameDelegate::eventFilter(QObject *watched, QEvent *event) {
    auto *editor = qobject_cast<QLineEdit *>(watched);
    // While a name is being typed, keys belong to the editor rather than
    // the window's shortcuts: F2 must cycle the selection here instead of
    // starting the rename again, and keys like F5 or Ctrl+D mustn't act on
    // files mid-rename.
    if (editor && event->type() == QEvent::ShortcutOverride) {
        event->accept();
        return true;
    }
    if (editor && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_F2 && key->modifiers() == Qt::NoModifier) {
            // Name, then whole name, then extension, then name again.
            const auto current = Selection(editor->property(kPartProperty).toInt());
            const Selection next = current == Selection::Stem    ? Selection::Whole
                                   : current == Selection::Whole ? Selection::Extension
                                                                 : Selection::Stem;
            select(editor, editor->text(), next);
            return true;
        }
    }
    return QStyledItemDelegate::eventFilter(watched, event);
}
