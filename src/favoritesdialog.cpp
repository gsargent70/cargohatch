#include "favoritesdialog.h"

#include <QAction>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "favoritesbar.h"
#include "folderpicker.h"
#include "pathcompleter.h"

namespace {

constexpr int kTypeRole = Qt::UserRole;
constexpr int kNameRole = Qt::UserRole + 1;
constexpr int kPathRole = Qt::UserRole + 2;

QIcon iconFor(const Favorite &f) {
    return f.type == Favorite::Type::Separator ? QIcon() : FavoriteMenus::icon(f);
}

QString askBranch(QWidget *parent, const QString &name = {}) {
    bool ok = false;
    const QString text = QInputDialog::getText(parent, FavoritesDialog::tr("Branch"), FavoritesDialog::tr("Name:"),
                                               QLineEdit::Normal, name, &ok).trimmed();
    return ok ? text : QString();
}

} // namespace

FavoriteDialog::FavoriteDialog(const FavoriteList &list, const Favorite &favorite, bool sort, FolderPicker *picker,
                               QWidget *parent)
    : QDialog(parent), m_list(list), m_sort(sort), m_branches(new QTreeWidget(this)) {
    setWindowTitle(tr("Add to Favourites"));
    setUp(favorite, picker);
}

FavoriteDialog::FavoriteDialog(const Favorite &favorite, FolderPicker *picker, QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Edit Favourite"));
    setUp(favorite, picker);
}

void FavoriteDialog::setUp(const Favorite &favorite, FolderPicker *picker) {
    m_favorite = favorite;
    const int width = fontMetrics().averageCharWidth() * 46;
    m_path = new QLineEdit(favorite.path, this);
    m_path->setMinimumWidth(width);
    new PathCompleter(m_path);
    m_name = new QLineEdit(favorite.name, this);
    m_note = new QLabel(this);
    m_note->setForegroundRole(QPalette::PlaceholderText);
    m_note->setTextFormat(Qt::PlainText);

    auto *browse = new QPushButton(tr("Browse…"), this);
    browse->setAutoDefault(false);
    browse->setVisible(picker && favorite.type == Favorite::Type::Folder);
    connect(browse, &QPushButton::clicked, this, [this, picker] {
        QString error;
        const QString chosen = picker->pickFolder(this, windowTitle(), this->favorite().realPath(), &error);
        if (!chosen.isEmpty())
            m_path->setText(chosen);
        else if (!error.isEmpty())
            m_note->setText(error);
    });
    auto *path = new QHBoxLayout;
    path->addWidget(m_path, 1);
    path->addWidget(browse);
    auto *pathLabel = new QLabel(favorite.type == Favorite::Type::File ? tr("&File:") : tr("&Folder:"), this);
    pathLabel->setBuddy(m_path);
    auto *form = new QFormLayout;
    form->addRow(pathLabel, path);
    form->addRow(tr("&Name:"), m_name);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize); // see ChooseFolderDialog
    layout->addLayout(form);
    if (m_branches) {
        m_branches->setHeaderHidden(true);
        m_branches->setFixedHeight(fontMetrics().lineSpacing() * 9);
        auto *label = new QLabel(tr("Create &in:"), this);
        label->setBuddy(m_branches);
        layout->addWidget(label);
        layout->addWidget(m_branches);
        auto *branch = new QPushButton(tr("New &Branch…"), this);
        branch->setAutoDefault(false);
        connect(branch, &QPushButton::clicked, this, &FavoriteDialog::addBranch);
        buttons->addButton(branch, QDialogButtonBox::ResetRole);
        fillBranches();
        connect(m_branches, &QTreeWidget::itemSelectionChanged, this, &FavoriteDialog::validate);
    }
    layout->addWidget(m_note);
    layout->addWidget(buttons);
    connect(m_path, &QLineEdit::textChanged, this, &FavoriteDialog::validate);
    m_name->setFocus();
    m_name->selectAll();
    validate();
}

void FavoriteDialog::fillBranches() {
    m_branches->clear();
    auto add = [](auto &&self, QTreeWidgetItem *parent, const QList<Favorite> &list) -> void {
        for (int i = 0; i < list.size(); ++i) {
            if (list.at(i).type != Favorite::Type::Branch)
                continue;
            auto *item = new QTreeWidgetItem(parent, {list.at(i).title()});
            item->setIcon(0, iconFor(list.at(i)));
            // Where it is among its parent's favourites.
            item->setData(0, Qt::UserRole, i);
            self(self, item, list.at(i).children);
        }
    };
    auto *all = new QTreeWidgetItem(m_branches, {tr("Favourites")});
    auto *bar = new QTreeWidgetItem(m_branches, {tr("Favourites Bar")});
    for (QTreeWidgetItem *item : {all, bar})
        item->setIcon(0, FavoriteMenus::icon(Favorite::branch({})));
    add(add, all, m_list.items);
    add(add, bar, m_list.bar);
    m_branches->expandAll();
    m_branches->setCurrentItem(all);
}

QList<Favorite> *FavoriteDialog::chosen(FavoriteList *list) const {
    QList<int> steps;
    QTreeWidgetItem *item = m_branches->currentItem();
    for (; item && item->parent(); item = item->parent())
        steps.prepend(item->data(0, Qt::UserRole).toInt());
    QList<Favorite> *at = item && m_branches->indexOfTopLevelItem(item) == 1 ? &list->bar : &list->items;
    for (const int step : std::as_const(steps))
        at = &(*at)[step].children;
    return at;
}

bool FavoriteDialog::isOnBar() const {
    QTreeWidgetItem *item = m_branches ? m_branches->currentItem() : nullptr;
    while (item && item->parent())
        item = item->parent();
    return item && m_branches->indexOfTopLevelItem(item) == 1;
}

void FavoriteDialog::addBranch() {
    const QString name = askBranch(this);
    if (name.isEmpty())
        return;
    QList<Favorite> *at = chosen(&m_list);
    at->append(Favorite::branch(name));
    auto *item = new QTreeWidgetItem(m_branches->currentItem(), {name});
    item->setIcon(0, iconFor(at->last()));
    item->setData(0, Qt::UserRole, int(at->size()) - 1);
    m_branches->setCurrentItem(item);
}

Favorite FavoriteDialog::favorite() const {
    Favorite f = m_favorite;
    f.path = m_path->text().trimmed();
    while (f.path.size() > 1 && f.path.endsWith('/'))
        f.path.chop(1);
    f.name = m_name->text().trimmed();
    return f;
}

FavoriteList FavoriteDialog::list() const {
    FavoriteList list = m_list;
    if (m_branches) {
        QList<Favorite> *at = chosen(&list);
        at->append(favorite());
        if (m_sort)
            FavoriteList::sort(at);
    }
    return list;
}

void FavoriteDialog::validate() {
    const Favorite f = favorite();
    QString note;
    bool ok = !f.path.isEmpty();
    if (!ok)
        note = f.type == Favorite::Type::File ? tr("Type the file's path.") : tr("Type the folder.");
    else if (!QDir::isAbsolutePath(f.realPath())) {
        note = tr("This needs a full path, or one starting with ~.");
        ok = false;
    } else if (m_branches) {
        for (const Favorite &other : *chosen(&m_list))
            if ((other.type == Favorite::Type::Folder || other.type == Favorite::Type::File) &&
                other.realPath() == f.realPath())
                note = tr("It's there already, as %1.").arg(other.title());
    }
    m_note->setText(note);
    m_ok->setEnabled(ok);
}

FavoritesDialog::FavoritesDialog(const FavoriteList &list, bool autoSort, FolderPicker *picker, QWidget *parent)
    : QDialog(parent),
      m_picker(picker),
      m_tree(new QTreeWidget(this)),
      m_autoSort(new QCheckBox(tr("&Sort automatically when adding"), this)),
      m_edit(new QPushButton(tr("&Edit…"), this)),
      m_delete(new QPushButton(tr("&Delete"), this)),
      m_up(new QPushButton(tr("&Up"), this)),
      m_down(new QPushButton(tr("Do&wn"), this)) {
    setWindowTitle(tr("Favourites"));
    const int em = fontMetrics().averageCharWidth();
    m_tree->setHeaderLabels({tr("Name"), tr("Folder")});
    m_tree->setFixedSize(em * 66, fontMetrics().lineSpacing() * 20);
    m_tree->header()->resizeSection(0, em * 28);
    m_tree->setTextElideMode(Qt::ElideMiddle);
    m_tree->setDragDropMode(QAbstractItemView::InternalMove);
    m_tree->setUniformRowHeights(true);
    m_tree->setExpandsOnDoubleClick(false);

    m_bar = makeItem(Favorite::branch(tr("Favourites Bar"), list.bar));
    // Its place doesn't matter, as it isn't in a place among the others.
    m_bar->setFlags(m_bar->flags() & ~Qt::ItemIsDragEnabled);
    QFont bold = m_bar->font(0);
    bold.setBold(true);
    m_bar->setFont(0, bold);
    m_tree->addTopLevelItem(m_bar);
    for (const Favorite &f : list.items)
        m_tree->addTopLevelItem(makeItem(f));
    m_tree->expandAll();
    m_tree->setCurrentItem(m_tree->topLevelItem(m_tree->topLevelItemCount() - 1));
    m_autoSort->setChecked(autoSort);

    auto *add = new QPushButton(tr("&Add Folder…"), this);
    auto *branch = new QPushButton(tr("New &Branch…"), this);
    auto *line = new QPushButton(tr("Add Separa&tor"), this);
    auto *sort = new QPushButton(tr("S&ort"), this);
    auto *side = new QVBoxLayout;
    for (QPushButton *b : {add, branch, line, m_edit, m_delete, m_up, m_down, sort}) {
        b->setAutoDefault(false);
        side->addWidget(b);
    }
    side->addStretch(1);
    auto *middle = new QHBoxLayout;
    middle->addWidget(m_tree);
    middle->addLayout(side);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize); // see ChooseFolderDialog
    layout->addLayout(middle);
    layout->addWidget(m_autoSort);
    layout->addWidget(buttons);

    connect(add, &QPushButton::clicked, this, &FavoritesDialog::addFolder);
    connect(branch, &QPushButton::clicked, this, &FavoritesDialog::addBranch);
    connect(line, &QPushButton::clicked, this, &FavoritesDialog::addSeparator);
    connect(m_edit, &QPushButton::clicked, this, &FavoritesDialog::editItem);
    connect(m_delete, &QPushButton::clicked, this, &FavoritesDialog::deleteItem);
    connect(m_up, &QPushButton::clicked, this, [this] { moveItem(-1); });
    connect(m_down, &QPushButton::clicked, this, [this] { moveItem(1); });
    connect(sort, &QPushButton::clicked, this, &FavoritesDialog::sortItems);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this] { editItem(); });
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &FavoritesDialog::updateButtons);
    auto key = [this](const QKeySequence &keys, auto slot) {
        auto *a = new QAction(m_tree);
        a->setShortcut(keys);
        a->setShortcutContext(Qt::WidgetShortcut);
        connect(a, &QAction::triggered, this, slot);
        m_tree->addAction(a);
    };
    key(QKeySequence(Qt::Key_Delete), [this] { deleteItem(); });
    key(QKeySequence(Qt::Key_F2), [this] { editItem(); });
    key(QKeySequence(Qt::ALT | Qt::Key_Up), [this] { moveItem(-1); });
    key(QKeySequence(Qt::ALT | Qt::Key_Down), [this] { moveItem(1); });
    updateButtons();
}

void FavoritesDialog::setItem(QTreeWidgetItem *item, const Favorite &f) const {
    const bool line = f.type == Favorite::Type::Separator;
    item->setData(0, kTypeRole, int(f.type));
    item->setData(0, kNameRole, f.name);
    item->setData(0, kPathRole, f.path);
    item->setText(0, line ? QString(16, QChar(0x2500)) : f.title());
    item->setText(1, f.path);
    item->setToolTip(1, f.path);
    item->setIcon(0, iconFor(f));
    if (line)
        item->setForeground(0, palette().color(QPalette::PlaceholderText));
    Qt::ItemFlags flags = item->flags() | Qt::ItemIsDragEnabled;
    flags.setFlag(Qt::ItemIsDropEnabled, f.type == Favorite::Type::Branch);
    item->setFlags(flags);
}

QTreeWidgetItem *FavoritesDialog::makeItem(const Favorite &f) const {
    auto *item = new QTreeWidgetItem;
    setItem(item, f);
    for (const Favorite &child : f.children)
        item->addChild(makeItem(child));
    return item;
}

Favorite FavoritesDialog::read(const QTreeWidgetItem *item) const {
    Favorite f;
    f.type = Favorite::Type(item->data(0, kTypeRole).toInt());
    f.name = item->data(0, kNameRole).toString();
    f.path = item->data(0, kPathRole).toString();
    for (int i = 0; i < item->childCount(); ++i)
        f.children << read(item->child(i));
    return f;
}

FavoriteList FavoritesDialog::list() const {
    FavoriteList list;
    list.bar = read(m_bar).children;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        if (!isBar(m_tree->topLevelItem(i)))
            list.items << read(m_tree->topLevelItem(i));
    return list;
}

bool FavoritesDialog::autoSort() const {
    return m_autoSort->isChecked();
}

void FavoritesDialog::insert(QTreeWidgetItem *item) {
    QTreeWidgetItem *current = m_tree->currentItem();
    if (!current) {
        m_tree->addTopLevelItem(item);
    } else if (Favorite::Type(current->data(0, kTypeRole).toInt()) == Favorite::Type::Branch) {
        current->addChild(item);
        current->setExpanded(true);
    } else {
        QTreeWidgetItem *parent = current->parent() ? current->parent() : m_tree->invisibleRootItem();
        parent->insertChild(parent->indexOfChild(current) + 1, item);
    }
    m_tree->setCurrentItem(item);
}

void FavoritesDialog::addFolder() {
    FavoriteDialog dialog(Favorite::folder({}), m_picker, this);
    dialog.setWindowTitle(tr("Add Favourite"));
    if (dialog.exec() == QDialog::Accepted)
        insert(makeItem(dialog.favorite()));
}

void FavoritesDialog::addBranch() {
    const QString name = askBranch(this);
    if (!name.isEmpty())
        insert(makeItem(Favorite::branch(name)));
}

void FavoritesDialog::addSeparator() {
    insert(makeItem(Favorite::separator()));
}

void FavoritesDialog::editItem() {
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item || isBar(item))
        return;
    Favorite f = read(item);
    switch (f.type) {
    case Favorite::Type::Separator:
        return;
    case Favorite::Type::Branch: {
        const QString name = askBranch(this, f.name);
        if (name.isEmpty())
            return;
        f.name = name;
        break;
    }
    case Favorite::Type::Folder:
    case Favorite::Type::File: {
        FavoriteDialog dialog(f, m_picker, this);
        if (dialog.exec() != QDialog::Accepted)
            return;
        f = dialog.favorite();
        break;
    }
    }
    setItem(item, f);
}

void FavoritesDialog::deleteItem() {
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item || isBar(item))
        return;
    if (item->childCount() > 0) {
        QMessageBox box(QMessageBox::Question, tr("Delete Branch"),
                        tr("Delete the branch %1 and everything in it?").arg(item->text(0)), QMessageBox::Cancel, this);
        // The folders themselves aren't touched, which is worth saying here.
        box.setInformativeText(tr("The folders themselves stay as they are."));
        QPushButton *remove = box.addButton(tr("Delete"), QMessageBox::DestructiveRole);
        box.exec();
        if (box.clickedButton() != remove)
            return;
    }
    delete item;
    updateButtons();
}

void FavoritesDialog::moveItem(int by) {
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item || isBar(item))
        return;
    QTreeWidgetItem *parent = item->parent() ? item->parent() : m_tree->invisibleRootItem();
    const int from = parent->indexOfChild(item);
    const int to = from + by;
    // The bar's branch stays first.
    const int first = parent == m_tree->invisibleRootItem() && isBar(parent->child(0)) ? 1 : 0;
    if (to < first || to >= parent->childCount())
        return;
    const bool expanded = item->isExpanded();
    parent->takeChild(from);
    parent->insertChild(to, item);
    item->setExpanded(expanded);
    m_tree->setCurrentItem(item);
}

void FavoritesDialog::sortItems() {
    // The branch the current item is in, or the current branch itself.
    QTreeWidgetItem *item = m_tree->currentItem();
    const bool branch = item && Favorite::Type(item->data(0, kTypeRole).toInt()) == Favorite::Type::Branch;
    QTreeWidgetItem *parent = branch ? item : item ? item->parent() : nullptr;
    QTreeWidgetItem *root = parent ? parent : m_tree->invisibleRootItem();
    QList<Favorite> list;
    for (int i = 0; i < root->childCount(); ++i)
        if (!isBar(root->child(i)))
            list << read(root->child(i));
    FavoriteList::sort(&list);
    for (int i = root->childCount() - 1; i >= 0; --i)
        if (!isBar(root->child(i)))
            delete root->takeChild(i);
    for (const Favorite &f : std::as_const(list)) {
        QTreeWidgetItem *made = makeItem(f);
        root->addChild(made);
        made->setExpanded(true);
    }
    updateButtons();
}

void FavoritesDialog::updateButtons() {
    const QTreeWidgetItem *item = m_tree->selectedItems().value(0);
    const bool some = item && !isBar(item);
    const bool line = some && Favorite::Type(item->data(0, kTypeRole).toInt()) == Favorite::Type::Separator;
    m_edit->setEnabled(some && !line);
    m_delete->setEnabled(some);
    m_up->setEnabled(some);
    m_down->setEnabled(some);
}
