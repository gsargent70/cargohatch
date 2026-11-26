#pragma once

#include <QSortFilterProxyModel>

#include "globalfilter.h"
#include "quickfilter.h"
#include "viewformat.h"

class FileModel;

// Sorts a folder's items (SORT-01 to SORT-03): by any column, either way,
// with folders before files, after them or mixed in. Folders stay in their
// place when the order is reversed, as people expect. Items hidden by the
// global filters or the filter bar are left out (FILT-03, FILT-01).
class FileSortProxy : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit FileSortProxy(FileModel *model, QObject *parent = nullptr);

    FileModel *fileModel() const { return m_model; }

    ViewFormat::FolderOrder folderOrder() const { return m_folderOrder; }
    void setFolderOrder(ViewFormat::FolderOrder order);
    // Folders sort by name A to Z whatever the column (SORT-02).
    bool keepsFoldersAlphabetical() const { return m_foldersAlphabetical; }
    void setKeepFoldersAlphabetical(bool keep);

    const GlobalFilter &globalFilter() const { return m_filter; }
    void setGlobalFilter(const GlobalFilter &filter);

    const QuickFilter &quickFilter() const { return m_quick; }
    void setQuickFilter(const QuickFilter &filter);
    // Why the item in the model's `row` isn't showing.
    bool isHidden(int row) const;
    bool isFilteredOut(int row) const;

    // Re-sorts after the name comparison rules change.
    void resort();

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;

private:
    FileModel *m_model;
    ViewFormat::FolderOrder m_folderOrder = ViewFormat::FolderOrder::FoldersFirst;
    bool m_foldersAlphabetical = false;
    GlobalFilter m_filter;
    GlobalFilter::Matcher m_matcher;
    QuickFilter m_quick;
    QuickFilter::Matcher m_quickMatcher;
};
