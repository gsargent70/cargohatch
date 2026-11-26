#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// The columns a file list can show (VIEW-02). The order here is the model's
// column order; what the user sees is set by the format.
enum class Column {
    Name,
    Extension,
    Size,
    Type,
    Modified,
    Created,
    Accessed,
    Permissions,
    Owner,
    Group,
    LinkTarget,
    FullPath,
    Count
};

namespace Columns {

constexpr int count() { return int(Column::Count); }
// Stable names, used in the settings file.
QString id(Column column);
Column fromId(const QString &id, bool *ok = nullptr);
// The heading, translated.
QString title(Column column);
int defaultWidth(Column column);
bool rightAligned(Column column);
// Sizes and dates sort biggest and newest first the first time their
// heading is clicked; names sort A to Z (SORT-01).
bool sortsDescendingFirst(Column column);

} // namespace Columns

// How one tab shows its folder: the view mode (VIEW-01), the columns in
// Details and Power modes, and the sort (SORT-01, SORT-02). Each tab has its
// own; new tabs start from the default format.
struct ViewFormat {
    enum class Mode { LargeIcons, SmallIcons, List, Details, Power, Thumbnails, Tiles, Count };
    enum class FolderOrder { FoldersFirst, FilesFirst, Mixed };

    struct ColumnState {
        Column column;
        int width;
    };

    Mode mode = Mode::Details;
    // Visible columns, left to right. Name is always among them.
    QList<ColumnState> columns = {{Column::Name, 320}, {Column::Size, 90}, {Column::Type, 160}, {Column::Modified, 150}};
    Column sortColumn = Column::Name;
    bool reverse = false;
    FolderOrder folderOrder = FolderOrder::FoldersFirst;

    bool hasColumn(Column column) const;
    void setColumnVisible(Column column, bool visible);

    // A short text form for the settings file, e.g.
    // "mode=details;columns=name:320,size:90;sort=size;reverse=1".
    QString toString() const;
    // Unknown or broken parts keep their defaults.
    static ViewFormat fromString(const QString &text);

    static QString modeId(Mode mode);
    static QString modeTitle(Mode mode);
    bool isDetailed() const { return mode == Mode::Details || mode == Mode::Power; }
};
