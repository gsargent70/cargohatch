#include "tiledelegate.h"

#include <QApplication>
#include <QPainter>

#include "filemodel.h"
#include "filesortproxy.h"

TileDelegate::TileDelegate(FileSortProxy *proxy, QObject *parent) : RenameDelegate(parent), m_proxy(proxy) {}

QSize TileDelegate::tileSize(const QFontMetrics &metrics) {
    return QSize(kIconSize + 8 + metrics.averageCharWidth() * 26, qMax(kIconSize + 12, metrics.height() * 3 + 12));
}

QSize TileDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const {
    return tileSize(option.fontMetrics);
}

void TileDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    const QWidget *widget = opt.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    // The selection and hover background, as the style draws it.
    opt.text.clear();
    opt.icon = QIcon();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);

    const QRect r = option.rect.adjusted(6, 6, -6, -6);
    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    const QRect iconRect(r.left(), r.top() + (r.height() - kIconSize) / 2, kIconSize, kIconSize);
    icon.paint(painter, iconRect, Qt::AlignCenter,
               opt.state & QStyle::State_Enabled ? QIcon::Normal : QIcon::Disabled);

    const int row = m_proxy->mapToSource(index).row();
    const FileModel *model = m_proxy->fileModel();
    const QStringList lines = {model->text(row, Column::Name), model->text(row, Column::Type),
                               model->entry(row).isDir ? model->text(row, Column::Modified)
                                                       : model->text(row, Column::Size)};
    const bool selected = opt.state & QStyle::State_Selected;
    const QPalette::ColorGroup group = opt.state & QStyle::State_Enabled ? QPalette::Active : QPalette::Disabled;
    const QColor main = option.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text);
    const QColor dim = selected || group == QPalette::Disabled ? main : option.palette.color(group, QPalette::PlaceholderText);

    QRect text(iconRect.right() + 8, r.top(), r.right() - iconRect.right() - 8, r.height());
    const int lineHeight = option.fontMetrics.height();
    int y = text.top() + (text.height() - lineHeight * 3) / 2;
    painter->save();
    for (int i = 0; i < lines.size(); ++i) {
        QFont font = option.font;
        font.setBold(i == 0);
        painter->setFont(font);
        painter->setPen(i == 0 ? main : dim);
        const QString shown = QFontMetrics(font).elidedText(lines.at(i), Qt::ElideMiddle, text.width());
        const QRect line(text.left(), y, text.width(), lineHeight);
        painter->drawText(line, Qt::AlignLeft | Qt::AlignVCenter, shown);
        if (i == 0)
            drawMatch(painter, line, index, font, Qt::AlignLeft | Qt::AlignVCenter, Qt::ElideMiddle, false,
                      option.palette.color(QPalette::Accent));
        y += lineHeight;
    }
    painter->restore();
    drawCursor(painter, option, index);
}

void TileDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const {
    // The same layout as paint(): the name is the first of three lines
    // beside the icon.
    const QRect r = option.rect.adjusted(6, 6, -6, -6);
    const int left = r.left() + kIconSize + 8;
    const int lineHeight = option.fontMetrics.height();
    const int nameMiddle = r.top() + (r.height() - lineHeight * 3) / 2 + lineHeight / 2;
    const int height = editor->sizeHint().height();
    editor->setGeometry(left - 3, nameMiddle - height / 2, r.right() - left + 3, height);
}
