#include "filesortproxy.h"

#include "filemodel.h"

FileSortProxy::FileSortProxy(FileModel *model, QObject *parent) :
    QSortFilterProxyModel(parent), m_model(model), m_matcher(m_filter) {
    setSourceModel(model);
    setDynamicSortFilter(true);
}

void FileSortProxy::setFolderOrder(ViewFormat::FolderOrder order) {
    if (order == m_folderOrder)
        return;
    m_folderOrder = order;
    resort();
}

void FileSortProxy::setKeepFoldersAlphabetical(bool keep) {
    if (keep == m_foldersAlphabetical)
        return;
    m_foldersAlphabetical = keep;
    resort();
}

void FileSortProxy::setGlobalFilter(const GlobalFilter &filter) {
    if (filter == m_filter)
        return;
    beginFilterChange();
    m_filter = filter;
    m_matcher = GlobalFilter::Matcher(filter);
    endFilterChange();
}

void FileSortProxy::setQuickFilter(const QuickFilter &filter) {
    if (filter == m_quick)
        return;
    beginFilterChange();
    m_quick = filter;
    m_quickMatcher = QuickFilter::Matcher(filter);
    endFilterChange();
}

bool FileSortProxy::isHidden(int row) const {
    const FileEntry &e = m_model->entry(row);
    return m_matcher.hides(e.name, e.isDir, e.isListedHidden);
}

bool FileSortProxy::isFilteredOut(int row) const {
    const FileEntry &e = m_model->entry(row);
    return !m_quickMatcher.matches(e.name, e.isDir);
}

bool FileSortProxy::filterAcceptsRow(int row, const QModelIndex &) const {
    return !isHidden(row) && !isFilteredOut(row);
}

void FileSortProxy::resort() {
    invalidate();
    if (sortColumn() >= 0)
        sort(sortColumn(), sortOrder());
}

bool FileSortProxy::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    const int a = left.row();
    const int b = right.row();
    const FileEntry &x = m_model->entry(a);
    const FileEntry &y = m_model->entry(b);
    // Qt sorts descending by asking lessThan(right, left), so anything that
    // must not flip with the order is answered the other way round then.
    const bool descending = sortOrder() == Qt::DescendingOrder;

    if (x.isDir != y.isDir && m_folderOrder != ViewFormat::FolderOrder::Mixed) {
        const bool xFirst = (m_folderOrder == ViewFormat::FolderOrder::FoldersFirst) == x.isDir;
        return descending ? !xFirst : xFirst;
    }
    if (x.isDir && y.isDir && m_foldersAlphabetical && m_folderOrder != ViewFormat::FolderOrder::Mixed) {
        const int byName = m_model->compareNames(a, b);
        return descending ? byName > 0 : byName < 0;
    }

    int result = 0;
    switch (Column(left.column())) {
    case Column::Name:
        return m_model->compareNames(a, b) < 0;
    case Column::Size:
        result = x.size < y.size ? -1 : x.size > y.size ? 1 : 0;
        break;
    case Column::Modified:
        result = x.modified < y.modified ? -1 : x.modified > y.modified ? 1 : 0;
        break;
    case Column::Created:
        result = x.created < y.created ? -1 : x.created > y.created ? 1 : 0;
        break;
    case Column::Accessed:
        result = x.accessed < y.accessed ? -1 : x.accessed > y.accessed ? 1 : 0;
        break;
    case Column::Permissions:
        result = int(x.permissions.toInt()) - int(y.permissions.toInt());
        break;
    default:
        // Text columns compare the way names do.
        result = QString::localeAwareCompare(m_model->text(a, Column(left.column())),
                                             m_model->text(b, Column(left.column())));
        break;
    }
    // Equal values fall back to the name, A to Z either way.
    if (result == 0) {
        const int byName = m_model->compareNames(a, b);
        return descending ? byName > 0 : byName < 0;
    }
    return result < 0;
}
