#pragma once

#include <QDialog>

#include "favorites.h"

class FolderPicker;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

// Adds a favourite, choosing its name and branch, or edits one.
class FavoriteDialog : public QDialog {
    Q_OBJECT

public:
    // Adds to `list`, sorting the branch afterwards if asked.
    FavoriteDialog(const FavoriteList &list, const Favorite &favorite, bool sort, FolderPicker *picker,
                   QWidget *parent = nullptr);
    // Edits one by itself.
    FavoriteDialog(const Favorite &favorite, FolderPicker *picker, QWidget *parent = nullptr);

    Favorite favorite() const;
    // The list with the favourite in it.
    FavoriteList list() const;
    bool isOnBar() const;

private:
    void setUp(const Favorite &favorite, FolderPicker *picker);
    void fillBranches();
    void addBranch();
    void validate();
    // The place the chosen branch is in `list`.
    QList<Favorite> *chosen(FavoriteList *list) const;

    FavoriteList m_list;
    Favorite m_favorite;
    bool m_sort = false;
    QLineEdit *m_path = nullptr;
    QLineEdit *m_name = nullptr;
    QTreeWidget *m_branches = nullptr;
    QLabel *m_note = nullptr;
    QPushButton *m_ok = nullptr;
};

// Favourites > Edit Favourites…
class FavoritesDialog : public QDialog {
    Q_OBJECT

public:
    FavoritesDialog(const FavoriteList &list, bool autoSort, FolderPicker *picker, QWidget *parent = nullptr);
    FavoriteList list() const;
    bool autoSort() const;
    QTreeWidget *tree() const { return m_tree; }

public slots:
    void addFolder();
    void addBranch();
    void addSeparator();
    void editItem();
    void deleteItem();
    void moveItem(int by);
    void sortItems();

private:
    QTreeWidgetItem *makeItem(const Favorite &favorite) const;
    void setItem(QTreeWidgetItem *item, const Favorite &favorite) const;
    Favorite read(const QTreeWidgetItem *item) const;
    bool isBar(const QTreeWidgetItem *item) const { return item && item == m_bar; }
    // Puts a new item after the current one, or inside the current branch.
    void insert(QTreeWidgetItem *item);
    void updateButtons();

    FolderPicker *m_picker;
    QTreeWidget *m_tree;
    QTreeWidgetItem *m_bar;
    QCheckBox *m_autoSort;
    QPushButton *m_edit, *m_delete, *m_up, *m_down;
};
