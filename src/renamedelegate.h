#pragma once

#include <QStyledItemDelegate>

#include <functional>
#include <memory>

class FindMatcher;

class QLineEdit;

// Edits a file's name in place (REN-01). The editor starts with part of the
// name selected, by default the name without its extension; pressing F2
// again moves on to the next part: whole name, extension, name only.
// The rename itself is left to a callback, which reports any error.
class RenameDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    enum class Selection { Stem, Whole, Extension, Start, End };

    explicit RenameDelegate(QObject *parent = nullptr);

    void setSelection(Selection selection) { m_selection = selection; }
    Selection selection() const { return m_selection; }
    // Called with the item's index (in the view's model) and the new name.
    void setRenamer(std::function<void(const QModelIndex &, const QString &)> renamer) { m_renamer = std::move(renamer); }

    // Marks the part of each name this finds (SEL-11); null for none.
    void setFindMatcher(std::shared_ptr<const FindMatcher> matcher) { m_matcher = std::move(matcher); }

    // Items are no wider than this, so a long name is cut short rather than
    // running into the next one; 0 for no limit.
    void setMaximumWidth(int width) { m_maximumWidth = width; }
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    // Selects `part` of `name` in `editor`.
    static void select(QLineEdit *editor, const QString &name, Selection part);
    // Where the extension starts, or -1: "photo.jpg" 5, ".bashrc" -1.
    static int extensionStart(const QString &name);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    // Frames the cursor's item when it isn't selected, so it can be seen where
    // the style draws nothing for it (NAV-08).
    static void drawCursor(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index);

protected:
    // Dims items that have been cut (CLIP-01).
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;
    // Draws the mark for the match in `index`, if there is one, over a name
    // that was drawn with these settings in `rect`.
    void drawMatch(QPainter *painter, const QRect &rect, const QModelIndex &index, const QFont &font,
                   Qt::Alignment alignment, Qt::TextElideMode elide, bool wrap, const QColor &color) const;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    Selection m_selection = Selection::Stem;
    std::shared_ptr<const FindMatcher> m_matcher;
    int m_maximumWidth = 0;
    std::function<void(const QModelIndex &, const QString &)> m_renamer;
};
