#include "foldertree.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>

#include "filedrag.h"
#include "icons.h"

namespace {

// Room above each group of top-level items but the first, with a line in it.
constexpr int kRootSpacing = 9;
// The cursor must rest this long after a key before its folder opens, so
// holding an arrow down doesn't read every folder on the way.
constexpr int kKeyDelayMs = 350;
// Starts the keys of expanded branches in Favourites.
const QString kGroupKey = QStringLiteral("favorites:");

bool isUnder(const QString &path, const QString &folder) {
    return path == folder || folder == "/" || path.startsWith(folder + '/');
}

// Makes rows taller, and without full-row selection, items only as wide as
// their names.
class TreeDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    TreeOptions options;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.rheight() += options.extraHeight;
        if (options.separateRoots && index.data(FolderTreeModel::StartsGroupRole).toBool())
            size.rheight() += kRootSpacing;
        return size;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyleOptionViewItem opt = option;
        if (!options.fullRow)
            opt.rect.setWidth(qMin(opt.rect.width(), QStyledItemDelegate::sizeHint(option, index).width() + 4));
        QStyledItemDelegate::paint(painter, opt, index);
    }
};

} // namespace

FolderTreeView::FolderTreeView(QWidget *parent) : QTreeView(parent) {
    setHeaderHidden(true);
    setFrameShape(QFrame::NoFrame);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setTextElideMode(Qt::ElideRight);
    setItemDelegate(new TreeDelegate(this));
    setIconSize(QSize(16, 16));
    m_keyTimer.setSingleShot(true);
    m_keyTimer.setInterval(kKeyDelayMs);
    connect(&m_keyTimer, &QTimer::timeout, this, [this] { emit keyOpened(currentIndex()); });
}

void FolderTreeView::setTreeOptions(const TreeOptions &options) {
    m_options = options;
    static_cast<TreeDelegate *>(itemDelegate())->options = options;
    setIndentation(options.indent);
    setAllColumnsShowFocus(options.fullRow);
    scheduleDelayedItemsLayout();
    viewport()->update();
}

bool FolderTreeView::event(QEvent *event) {
    if (event->type() == QEvent::ShortcutOverride) {
        // These keys belong to whatever has the keyboard. Left to the
        // window, they would act on files selected in the list, which is
        // not what someone looking at the tree means.
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->matches(QKeySequence::Copy) || key->matches(QKeySequence::Cut) || key->matches(QKeySequence::Paste) ||
            key->key() == Qt::Key_Delete || key->key() == Qt::Key_F2 || key->key() == Qt::Key_Return ||
            key->key() == Qt::Key_Enter ||
            (key->modifiers() == Qt::ControlModifier && (key->key() == Qt::Key_D || key->key() == Qt::Key_A))) {
            event->accept();
            return true;
        }
    }
    return QTreeView::event(event);
}

void FolderTreeView::keyPressEvent(QKeyEvent *event) {
    const QModelIndex index = currentIndex();
    if (event->matches(QKeySequence::Copy)) {
        emit copyRequested(index);
        return;
    }
    if (event->matches(QKeySequence::Cut)) {
        emit cutRequested(index);
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
        emit pasteRequested(index);
        return;
    }
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        m_keyTimer.stop();
        if (event->modifiers() & Qt::ControlModifier)
            emit newTabRequested(index);
        else if (event->modifiers() & Qt::ShiftModifier)
            emit otherSideRequested(index);
        else
            emit keyOpened(index);
        return;
    case Qt::Key_Escape:
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        m_keyTimer.stop();
        emit leaveRequested();
        return;
    case Qt::Key_Delete:
    case Qt::Key_F2:
        emit refused();
        return;
    default:
        if (event->modifiers() == Qt::ControlModifier && (event->key() == Qt::Key_D || event->key() == Qt::Key_A)) {
            emit refused();
            return;
        }
    }
    QTreeView::keyPressEvent(event);
    if (currentIndex() != index)
        m_keyTimer.start();
}

void FolderTreeView::mousePressEvent(QMouseEvent *event) {
    const QModelIndex index = indexAt(event->position().toPoint());
    m_keyTimer.stop();
    if (event->button() == Qt::MiddleButton) {
        if (index.isValid())
            emit newTabRequested(index);
        return;
    }
    // The right button opens a menu about a folder without going there.
    if (event->button() == Qt::RightButton)
        return;
    QTreeView::mousePressEvent(event);
}

void FolderTreeView::contextMenuEvent(QContextMenuEvent *event) {
    const bool mouse = event->reason() == QContextMenuEvent::Mouse;
    const QModelIndex index = mouse ? indexAt(viewport()->mapFromGlobal(event->globalPos())) : currentIndex();
    if (index.isValid())
        emit menuRequested(index, mouse ? event->globalPos()
                                        : viewport()->mapToGlobal(visualRect(index).bottomLeft()));
}

void FolderTreeView::drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QStyleOptionViewItem opt = option;
    if (m_options.separateRoots && index.data(FolderTreeModel::StartsGroupRole).toBool()) {
        painter->save();
        painter->setPen(palette().color(QPalette::Mid));
        const int y = opt.rect.top() + kRootSpacing / 2;
        painter->drawLine(6, y, viewport()->width() - 6, y);
        painter->restore();
        opt.rect.setTop(opt.rect.top() + kRootSpacing);
    }
    QTreeView::drawRow(painter, opt, index);
}

void FolderTreeView::drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const {
    painter->save();
    const bool selected = selectionModel()->isSelected(index);
    // The style has painted the whole row as selected.
    if (selected && !m_options.fullRow)
        painter->fillRect(rect, palette().brush(QPalette::Base));
    const int step = indentation();
    const int depth = qMax(1, rect.width() / step);
    auto middle = [&](int level) {
        const int x = isRightToLeft() ? rect.right() - step * level - step / 2 : rect.left() + step * level + step / 2;
        return QPoint(x, rect.center().y());
    };
    const QPoint at = middle(depth - 1);
    const bool branch = model()->hasChildren(index);
    const QColor faint = palette().color(QPalette::PlaceholderText);
    painter->setPen(faint);

    if (m_options.style == TreeOptions::Style::Lines) {
        // A line down from each folder above that has more to come, and
        // across to this one. Top-level items stand on their own.
        QModelIndex up = index;
        for (int level = depth - 1; level >= 1 && up.isValid(); --level, up = up.parent()) {
            const bool more = up.siblingAtRow(up.row() + 1).isValid();
            const QPoint p = middle(level);
            if (level == depth - 1) {
                painter->drawLine(p.x(), rect.top(), p.x(), more ? rect.bottom() : p.y());
                painter->drawLine(p.x(), p.y(), isRightToLeft() ? p.x() - step / 2 : p.x() + step / 2, p.y());
            } else if (more) {
                painter->drawLine(p.x(), rect.top(), p.x(), rect.bottom());
            }
        }
    }
    if (branch) {
        const QColor color = selected && m_options.fullRow ? palette().color(QPalette::HighlightedText)
                                                           : palette().color(QPalette::Text);
        if (m_options.style == TreeOptions::Style::Arrows) {
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->translate(at + QPointF(0.5, 0.5));
            if (isExpanded(index))
                painter->rotate(90);
            else if (isRightToLeft())
                painter->rotate(180);
            painter->drawPolyline(QPolygonF{{-2, -4}, {2, 0}, {-2, 4}});
        } else {
            const QRect box(at.x() - 4, at.y() - 4, 8, 8);
            painter->setPen(faint);
            painter->setBrush(palette().color(QPalette::Base));
            painter->drawRect(box);
            painter->setPen(palette().color(QPalette::Text));
            painter->drawLine(box.left() + 2, at.y(), box.right() - 1, at.y());
            if (!isExpanded(index))
                painter->drawLine(at.x(), box.top() + 2, at.x(), box.bottom() - 1);
        }
    }
    painter->restore();
}

FolderTree::FolderTree(QWidget *parent)
    : QWidget(parent),
      m_model(new FolderTreeModel(this)),
      m_view(new FolderTreeView(this)),
      m_header(new QWidget(this)),
      m_title(new QLabel(m_header)),
      m_close(new QToolButton(m_header)) {
    m_view->setModel(m_model);
    m_close->setAutoRaise(true);
    m_close->setFocusPolicy(Qt::NoFocus);
    m_close->setIconSize(QSize(14, 14));
    connect(m_close, &QToolButton::clicked, this, &FolderTree::closeRequested);
    auto *row = new QHBoxLayout(m_header);
    row->setContentsMargins(8, 2, 2, 2);
    row->addWidget(m_title, 1);
    row->addWidget(m_close);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_header);
    layout->addWidget(m_view, 1);
    setFocusProxy(m_view);
    setMinimumWidth(90);
    new FileDropTarget(m_view->viewport(), [this](const QPoint &pos) -> FileDropTarget::Place {
        const QModelIndex index = m_view->indexAt(pos);
        return {m_model->path(index), m_view->visualRect(index)};
    });

    connect(m_view, &QTreeView::clicked, this, &FolderTree::open);
    connect(m_view, &FolderTreeView::keyOpened, this, &FolderTree::open);
    connect(m_view, &FolderTreeView::leaveRequested, this, &FolderTree::leaveRequested);
    connect(m_view, &FolderTreeView::refused, this, &FolderTree::refused);
    auto pass = [this](auto from, auto to) {
        connect(m_view, from, this, [this, to](const QModelIndex &index) {
            if (!m_model->path(index).isEmpty())
                emit (this->*to)(m_model->path(index));
        });
    };
    pass(&FolderTreeView::newTabRequested, &FolderTree::newTabRequested);
    pass(&FolderTreeView::otherSideRequested, &FolderTree::otherSideRequested);
    pass(&FolderTreeView::cutRequested, &FolderTree::cutRequested);
    pass(&FolderTreeView::copyRequested, &FolderTree::copyRequested);
    pass(&FolderTreeView::pasteRequested, &FolderTree::pasteRequested);
    connect(m_view, &FolderTreeView::menuRequested, this, [this](const QModelIndex &index, const QPoint &position) {
        if (!m_model->path(index).isEmpty())
            emit menuRequested(m_model->path(index), position);
    });
    connect(m_view, &QTreeView::expanded, this, [this](const QModelIndex &index) {
        m_model->setWatched(index, true);
        m_model->fetchMore(index);
        restoreBranches(index);
    });
    connect(m_view, &QTreeView::collapsed, this, [this](const QModelIndex &index) { m_model->setWatched(index, false); });
    connect(m_model, &FolderTreeModel::childrenRead, this, [this](const QModelIndex &index) {
        // Whether branches can be opened may have changed.
        m_view->relayout();
        restoreBranches(index);
        advance();
    });
    retranslate();
    updateIcons();
}

void FolderTree::setOptions(const TreeOptions &options, const GlobalFilter &filter,
                            const QList<Favorite> &favorites) {
    const bool expandNow = options.autoExpand && !m_options.autoExpand;
    m_options = options;
    m_header->setVisible(options.header);
    m_view->setTreeOptions(options);
    FolderTreeModel::Options contents;
    contents.hidden = options.hidden;
    contents.system = options.system;
    contents.drives = options.drives;
    contents.trash = options.trash;
    contents.filter = filter;
    if (options.favorites)
        contents.favorites = favorites;
    // Favourites are all put in afresh, and their branches expanded again.
    const QStringList branches = expanded();
    m_model->setOptions(contents);
    const QStringList now = expanded();
    for (const QString &branch : branches)
        if (branch.startsWith(kGroupKey) && !now.contains(branch))
            m_toExpand.insert(branch);
    restoreBranches({});
    // The folder shown may just have been taken away, or may now be expanded
    // to.
    if (!m_target.isEmpty() && (expandNow || !m_current.isValid())) {
        m_arrived = false;
        m_from = QPersistentModelIndex();
        advance();
    }
}

void FolderTree::setPreferredWidth(int width) {
    m_preferredWidth = qBound(90, width, 2000);
    updateGeometry();
}

QSize FolderTree::sizeHint() const {
    return {m_preferredWidth, 400};
}

QString FolderTree::currentPath() const {
    return m_arrived ? m_target : QString();
}

void FolderTree::follow(const QString &path) {
    if (path == m_target)
        return;
    // Opening a sub-folder, or going up, stays in the same part of the
    // tree: someone browsing the file system into their home folder isn't
    // thrown over to Home. Anywhere else starts from the nearest top-level
    // item.
    m_from = QPersistentModelIndex();
    if (m_current.isValid()) {
        const QString here = m_model->path(m_current);
        const QModelIndex root = m_model->rootOf(m_current);
        const bool up = isUnder(here, path) && isUnder(path, m_model->path(root));
        const bool down = isUnder(path, here) && path.mid(here == "/" ? 1 : here.size() + 1).count('/') == 0;
        if (up || down)
            m_from = root;
    }
    m_target = path;
    m_arrived = false;
    advance();
}

void FolderTree::advance() {
    if (m_arrived || m_target.isEmpty())
        return;
    QModelIndex at = m_from.isValid() ? QModelIndex(m_from) : m_model->rootFor(m_target);
    if (!at.isValid())
        return;
    while (m_model->path(at) != m_target) {
        // Without expanding to it, as far as is open already will do.
        if (!m_options.autoExpand && !m_view->isExpanded(at))
            break;
        QModelIndex next = m_model->childTowards(at, m_target);
        if (!next.isValid()) {
            if (!m_model->isRead(at)) {
                m_model->read(at); // childrenRead() comes back here
                return;
            }
            next = m_model->addTowards(at, m_target);
            if (!next.isValid())
                break; // gone already
        }
        at = next;
    }
    arrived(at);
}

void FolderTree::arrived(const QModelIndex &index) {
    m_arrived = true;
    const QPersistentModelIndex target = index;
    m_current = target;
    if (m_options.removeDynamic)
        m_model->removeAdded(m_target);
    if (m_options.autoExpand) {
        for (QModelIndex up = target.parent(); up.isValid(); up = up.parent())
            m_view->expand(up);
        if (m_options.expandSelected)
            m_view->expand(target);
    }
    if (m_options.collapseOthers) {
        const QStringList open =
            m_options.collapseUnlessOpen && m_openFolders ? m_openFolders() : QStringList();
        QSet<QModelIndex> keep;
        for (QModelIndex up = target; up.isValid(); up = up.parent())
            keep.insert(up);
        QList<QModelIndex> branches{QModelIndex()};
        QList<QModelIndex> close;
        while (!branches.isEmpty()) {
            const QModelIndex parent = branches.takeFirst();
            for (int row = 0; row < m_model->rowCount(parent); ++row) {
                const QModelIndex child = m_model->index(row, 0, parent);
                if (!m_view->isExpanded(child))
                    continue;
                branches << child;
                // Favourites aren't a place that was left behind.
                bool wanted = keep.contains(child) || m_model->path(child).isEmpty();
                for (const QString &folder : open)
                    // Open in a tab, in the part of the tree it would show.
                    wanted |= folder != m_model->path(child) && isUnder(folder, m_model->path(child)) &&
                              m_model->rootOf(child) == m_model->rootFor(folder);
                if (!wanted)
                    close << child;
            }
        }
        for (const QModelIndex &child : std::as_const(close))
            m_view->collapse(child);
    }
    m_view->selectionModel()->setCurrentIndex(target, QItemSelectionModel::ClearAndSelect);
    m_view->scrollTo(target);
}

void FolderTree::open(const QModelIndex &index) {
    if (!index.isValid())
        return;
    const QString path = m_model->path(index);
    // Favourites and its branches aren't folders, only something to open up.
    if (path.isEmpty()) {
        m_view->setExpanded(index, !m_view->isExpanded(index));
        return;
    }
    // This is the place in the tree, so following the file list there won't
    // look for another.
    m_target = path;
    arrived(index);
    emit openRequested(path);
}

QString FolderTree::key(const QModelIndex &index) const {
    if (m_model->path(index).isEmpty()) {
        // By name, as there's no path.
        QStringList names;
        for (QModelIndex at = index; at.isValid(); at = at.parent())
            names.prepend(at.data().toString());
        return kGroupKey + names.join('\t');
    }
    const QModelIndex root = m_model->rootOf(index);
    // A favourite's branch is kept apart from the same folder elsewhere.
    const QString place = root.parent().isValid() ? kGroupKey : QString();
    return place + m_model->path(root) + '\t' + m_model->path(index);
}

QStringList FolderTree::expanded() const {
    QStringList list;
    QList<QModelIndex> branches{QModelIndex()};
    while (!branches.isEmpty()) {
        const QModelIndex parent = branches.takeFirst();
        for (int row = 0; row < m_model->rowCount(parent); ++row) {
            const QModelIndex child = m_model->index(row, 0, parent);
            if (m_view->isExpanded(child)) {
                list << key(child);
                branches << child;
            }
        }
    }
    // Those still waiting to be read count as well.
    for (const QString &waiting : m_toExpand)
        if (!list.contains(waiting))
            list << waiting;
    return list;
}

void FolderTree::setExpanded(const QStringList &branches) {
    m_view->collapseAll();
    m_toExpand = QSet<QString>(branches.begin(), branches.end());
    restoreBranches({});
    // Back to where it was, which may be in a branch just collapsed.
    if (!m_target.isEmpty()) {
        m_arrived = false;
        m_from = QPersistentModelIndex(m_current.isValid() ? m_model->rootOf(m_current) : QModelIndex());
        advance();
    }
}

void FolderTree::restoreBranches(const QModelIndex &parent) {
    if (m_toExpand.isEmpty())
        return;
    for (int row = 0; row < m_model->rowCount(parent); ++row) {
        const QModelIndex child = m_model->index(row, 0, parent);
        if (m_toExpand.remove(key(child)))
            m_view->expand(child); // reads it, and this comes round again
    }
}

void FolderTree::showCurrent() {
    if (m_current.isValid())
        m_view->scrollTo(m_current);
}

void FolderTree::collapseAll() {
    m_toExpand.clear();
    m_view->collapseAll();
}

void FolderTree::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    else if (event->type() == QEvent::PaletteChange)
        updateIcons();
    QWidget::changeEvent(event);
}

void FolderTree::updateIcons() {
    m_close->setIcon(Icons::close(palette()));
}

void FolderTree::retranslate() {
    m_title->setText(tr("Folders"));
    m_close->setToolTip(tr("Hide the folder tree (F9)"));
}
