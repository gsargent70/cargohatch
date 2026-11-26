#include "tabswitcher.h"

#include <QFileIconProvider>
#include <QHeaderView>
#include <QKeyEvent>
#include <QTreeWidget>
#include <QVBoxLayout>

TabSwitcher::TabSwitcher(QWidget *parent) : QFrame(parent, Qt::Popup), m_list(new QTreeWidget(this)) {
    setFrameShape(QFrame::StyledPanel);
    m_list->setColumnCount(3);
    m_list->setHeaderHidden(true);
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->header()->setStretchLastSection(false);
    m_list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_list->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_list->installEventFilter(this);
    connect(m_list, &QTreeWidget::itemClicked, this, [this] { commit(); });

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(m_list);
}

void TabSwitcher::open(const QList<Entry> &entries, int start) {
    m_list->clear();
    const QIcon folder = QFileIconProvider().icon(QFileIconProvider::Folder);
    const QColor dim = palette().color(QPalette::PlaceholderText);
    for (const Entry &entry : entries) {
        auto *item = new QTreeWidgetItem(m_list, {entry.title, entry.path, entry.side});
        item->setIcon(0, folder);
        item->setForeground(1, dim);
        item->setForeground(2, dim);
        item->setToolTip(1, entry.path);
    }
    m_list->setColumnHidden(2, std::all_of(entries.begin(), entries.end(),
                                           [](const Entry &e) { return e.side.isEmpty(); }));
    if (!entries.isEmpty())
        m_list->setCurrentItem(m_list->topLevelItem(qBound(0, start, int(entries.size()) - 1)));

    QWidget *window = parentWidget()->window();
    const int rowHeight = qMax(m_list->sizeHintForRow(0), fontMetrics().height() + 6);
    const int width = qMin(640, window->width() - 40);
    const int height = qMin(int(entries.size()) * rowHeight + 12, window->height() - 40);
    resize(width, height);
    move(window->mapToGlobal(window->rect().center()) - QPoint(width / 2, height / 2));
    show();
    m_list->setFocus();
}

int TabSwitcher::currentRow() const {
    return m_list->indexOfTopLevelItem(m_list->currentItem());
}

void TabSwitcher::step(int delta) {
    const int count = m_list->topLevelItemCount();
    if (count > 0)
        m_list->setCurrentItem(m_list->topLevelItem((currentRow() + delta + count) % count));
}

void TabSwitcher::commit() {
    if (!isVisible())
        return;
    const int row = currentRow();
    hide();
    if (row >= 0)
        emit chosen(row);
}

bool TabSwitcher::handleKey(QKeyEvent *event, bool press) {
    if (!press) {
        // Letting go of Ctrl picks the highlighted tab.
        if (event->key() == Qt::Key_Control) {
            commit();
            return true;
        }
        return false;
    }
    switch (event->key()) {
    case Qt::Key_Tab:
    case Qt::Key_Down:
        step(1);
        return true;
    case Qt::Key_Backtab:
    case Qt::Key_Up:
        step(-1);
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        commit();
        return true;
    case Qt::Key_Escape:
        hide();
        return true;
    default:
        return false;
    }
}

bool TabSwitcher::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_list && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) &&
        handleKey(static_cast<QKeyEvent *>(event), event->type() == QEvent::KeyPress))
        return true;
    return QFrame::eventFilter(watched, event);
}

void TabSwitcher::keyPressEvent(QKeyEvent *event) {
    if (!handleKey(event, true))
        QFrame::keyPressEvent(event);
}

void TabSwitcher::keyReleaseEvent(QKeyEvent *event) {
    if (!handleKey(event, false))
        QFrame::keyReleaseEvent(event);
}
