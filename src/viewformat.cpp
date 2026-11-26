#include "viewformat.h"

#include <QCoreApplication>

namespace Columns {
namespace {

const char *const kIds[] = {"name",     "ext",         "size",  "type",  "modified", "created",
                            "accessed", "permissions", "owner", "group", "target",   "path"};

} // namespace

QString id(Column column) {
    return kIds[int(column)];
}

Column fromId(const QString &id, bool *ok) {
    for (int i = 0; i < count(); ++i)
        if (id == kIds[i]) {
            if (ok)
                *ok = true;
            return Column(i);
        }
    if (ok)
        *ok = false;
    return Column::Name;
}

QString title(Column column) {
    switch (column) {
    case Column::Name: return QCoreApplication::translate("Columns", "Name");
    case Column::Extension: return QCoreApplication::translate("Columns", "Extension");
    case Column::Size: return QCoreApplication::translate("Columns", "Size");
    case Column::Type: return QCoreApplication::translate("Columns", "Type");
    case Column::Modified: return QCoreApplication::translate("Columns", "Modified");
    case Column::Created: return QCoreApplication::translate("Columns", "Created");
    case Column::Accessed: return QCoreApplication::translate("Columns", "Accessed");
    case Column::Permissions: return QCoreApplication::translate("Columns", "Permissions");
    case Column::Owner: return QCoreApplication::translate("Columns", "Owner");
    case Column::Group: return QCoreApplication::translate("Columns", "Group");
    case Column::LinkTarget: return QCoreApplication::translate("Columns", "Link Target");
    case Column::FullPath: return QCoreApplication::translate("Columns", "Full Path");
    case Column::Count: break;
    }
    return {};
}

int defaultWidth(Column column) {
    switch (column) {
    case Column::Name: return 320;
    case Column::Extension: return 80;
    case Column::Size: return 90;
    case Column::Type: return 160;
    case Column::Permissions: return 100;
    case Column::Owner:
    case Column::Group: return 90;
    case Column::LinkTarget:
    case Column::FullPath: return 300;
    default: return 150; // dates
    }
}

bool rightAligned(Column column) {
    return column == Column::Size;
}

bool sortsDescendingFirst(Column column) {
    return column == Column::Size || column == Column::Modified || column == Column::Created ||
           column == Column::Accessed;
}

} // namespace Columns

namespace {

const char *const kModes[] = {"large-icons", "small-icons", "list", "details", "power", "thumbnails", "tiles"};
const char *const kOrders[] = {"folders-first", "files-first", "mixed"};

} // namespace

bool ViewFormat::hasColumn(Column column) const {
    return std::any_of(columns.begin(), columns.end(), [column](const ColumnState &c) { return c.column == column; });
}

void ViewFormat::setColumnVisible(Column column, bool visible) {
    if (column == Column::Name || visible == hasColumn(column))
        return;
    if (visible)
        columns << ColumnState{column, Columns::defaultWidth(column)};
    else
        columns.removeIf([column](const ColumnState &c) { return c.column == column; });
}

QString ViewFormat::modeId(Mode mode) {
    return kModes[int(mode)];
}

QString ViewFormat::modeTitle(Mode mode) {
    switch (mode) {
    case Mode::LargeIcons: return QCoreApplication::translate("ViewFormat", "Large Icons");
    case Mode::SmallIcons: return QCoreApplication::translate("ViewFormat", "Small Icons");
    case Mode::List: return QCoreApplication::translate("ViewFormat", "List");
    case Mode::Details: return QCoreApplication::translate("ViewFormat", "Details");
    case Mode::Power: return QCoreApplication::translate("ViewFormat", "Power");
    case Mode::Thumbnails: return QCoreApplication::translate("ViewFormat", "Thumbnails");
    case Mode::Tiles: return QCoreApplication::translate("ViewFormat", "Tiles");
    case Mode::Count: break;
    }
    return {};
}

QString ViewFormat::toString() const {
    QStringList cols;
    for (const ColumnState &c : columns)
        cols << QString("%1:%2").arg(Columns::id(c.column)).arg(c.width);
    return QString("mode=%1;columns=%2;sort=%3;reverse=%4;folders=%5")
        .arg(modeId(mode), cols.join(','), Columns::id(sortColumn))
        .arg(reverse ? 1 : 0)
        .arg(kOrders[int(folderOrder)]);
}

ViewFormat ViewFormat::fromString(const QString &text) {
    ViewFormat format;
    for (const QString &part : text.split(';', Qt::SkipEmptyParts)) {
        const QString key = part.section('=', 0, 0).trimmed();
        const QString value = part.section('=', 1).trimmed();
        if (key == "mode") {
            for (int i = 0; i < int(Mode::Count); ++i)
                if (value == kModes[i])
                    format.mode = Mode(i);
        } else if (key == "columns") {
            QList<ColumnState> cols;
            for (const QString &entry : value.split(',', Qt::SkipEmptyParts)) {
                bool ok = false;
                const Column column = Columns::fromId(entry.section(':', 0, 0), &ok);
                if (!ok || std::any_of(cols.begin(), cols.end(), [column](auto &c) { return c.column == column; }))
                    continue;
                bool widthOk = false;
                const int width = entry.section(':', 1).toInt(&widthOk);
                cols << ColumnState{column, widthOk && width > 10 ? width : Columns::defaultWidth(column)};
            }
            if (!cols.isEmpty()) {
                if (!std::any_of(cols.begin(), cols.end(), [](auto &c) { return c.column == Column::Name; }))
                    cols.prepend({Column::Name, Columns::defaultWidth(Column::Name)});
                format.columns = cols;
            }
        } else if (key == "sort") {
            bool ok = false;
            const Column column = Columns::fromId(value, &ok);
            if (ok)
                format.sortColumn = column;
        } else if (key == "reverse") {
            format.reverse = value == "1";
        } else if (key == "folders") {
            for (int i = 0; i < 3; ++i)
                if (value == kOrders[i])
                    format.folderOrder = FolderOrder(i);
        }
    }
    return format;
}
