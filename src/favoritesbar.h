#pragma once

#include <QToolBar>

#include "favorites.h"

class QMenu;

// Menus of favourites, for the bar's branches and the Favourites menu.
namespace FavoriteMenus {

class Receiver {
public:
    virtual ~Receiver() = default;
    // `newTab` and `otherSide` as Ctrl or Shift were held.
    virtual void openFavorite(const Favorite &favorite, bool newTab, bool otherSide) = 0;
    virtual void openAll(const QList<Favorite> &favorites) = 0;
};

// Appends `list` to `menu`, branches as sub-menus. Returns what was added.
QList<QAction *> fill(QMenu *menu, const QList<Favorite> &list, Receiver *receiver);
// A branch's own menu, with Open All in Tabs at the end.
void fillBranch(QMenu *menu, const Favorite &branch, Receiver *receiver);
QIcon icon(const Favorite &favorite);

} // namespace FavoriteMenus

// The favourites bar (FAV-01), under the menu bar.
class FavoritesBar : public QToolBar {
    Q_OBJECT

public:
    FavoritesBar(FavoriteMenus::Receiver *receiver, QWidget *parent = nullptr);
    void setFavorites(const QList<Favorite> &list);

signals:
    // A right click on the favourite at `index`, or beside them all (-1).
    void menuRequested(int index, const QPoint &position);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void changeEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void build();

    FavoriteMenus::Receiver *m_receiver;
    QList<Favorite> m_list;
    bool m_built = false;
};
