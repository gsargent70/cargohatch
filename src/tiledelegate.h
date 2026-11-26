#pragma once

#include "renamedelegate.h"

class FileSortProxy;

// Draws an item in Tiles mode (VIEW-01): a large icon with the name, type
// and size beside it.
class TileDelegate : public RenameDelegate {
    Q_OBJECT

public:
    explicit TileDelegate(FileSortProxy *proxy, QObject *parent = nullptr);

    static constexpr int kIconSize = 48;
    static QSize tileSize(const QFontMetrics &metrics);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    // Renaming edits over the name line only.
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    FileSortProxy *m_proxy;
};
