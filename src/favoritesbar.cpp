#include "favoritesbar.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QFileIconProvider>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QToolButton>

#include "icons.h"

namespace {

const char *const kIndex = "cargohatchFavorite";

QString escaped(const QString &text) {
    return QString(text).replace('&', "&&");
}

} // namespace

QIcon FavoriteMenus::icon(const Favorite &f) {
    const QIcon folder = QFileIconProvider().icon(QFileIconProvider::Folder);
    switch (f.type) {
    case Favorite::Type::File: return QIcon::fromTheme("text-x-generic", QFileIconProvider().icon(QFileIconProvider::File));
    case Favorite::Type::Branch: return Icons::star(QGuiApplication::palette());
    default: return folder;
    }
}

void FavoriteMenus::fillBranch(QMenu *menu, const Favorite &branch, Receiver *receiver) {
    fill(menu, branch.children, receiver);
    if (branch.children.isEmpty())
        menu->addAction(FavoritesBar::tr("Empty"))->setEnabled(false);
    int folders = 0;
    for (const Favorite &child : branch.children)
        folders += child.type == Favorite::Type::Folder;
    if (folders < 2)
        return;
    menu->addSeparator();
    const QList<Favorite> children = branch.children;
    QObject::connect(menu->addAction(FavoritesBar::tr("Open All in Tabs")), &QAction::triggered, menu,
                     [receiver, children] { receiver->openAll(children); });
}

QList<QAction *> FavoriteMenus::fill(QMenu *menu, const QList<Favorite> &list, Receiver *receiver) {
    QList<QAction *> added;
    for (const Favorite &f : list) {
        switch (f.type) {
        case Favorite::Type::Separator:
            added << menu->addSeparator();
            break;
        case Favorite::Type::Branch: {
            QMenu *sub = menu->addMenu(icon(f), escaped(f.title()));
            fillBranch(sub, f, receiver);
            added << sub->menuAction();
            break;
        }
        case Favorite::Type::Folder:
        case Favorite::Type::File: {
            QAction *a = menu->addAction(icon(f), escaped(f.title()));
            a->setToolTip(f.path);
            QObject::connect(a, &QAction::triggered, menu, [receiver, f] {
                const Qt::KeyboardModifiers keys = QGuiApplication::keyboardModifiers();
                receiver->openFavorite(f, keys & Qt::ControlModifier, keys & Qt::ShiftModifier);
            });
            added << a;
            break;
        }
        }
    }
    return added;
}

FavoritesBar::FavoritesBar(FavoriteMenus::Receiver *receiver, QWidget *parent) : QToolBar(parent), m_receiver(receiver) {
    setMovable(false);
    setFloatable(false);
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setIconSize(QSize(16, 16));
    setFocusPolicy(Qt::NoFocus);
}

void FavoritesBar::setFavorites(const QList<Favorite> &list) {
    if (m_built && list == m_list)
        return;
    m_list = list;
    build();
}

void FavoritesBar::build() {
    m_built = true;
    for (QMenu *menu : findChildren<QMenu *>("branch", Qt::FindDirectChildrenOnly))
        menu->deleteLater();
    clear();
    if (m_list.isEmpty()) {
        auto *hint = new QLabel(tr("Right-click here to add the current folder."), this);
        hint->setForegroundRole(QPalette::PlaceholderText);
        hint->setContentsMargins(8, 2, 8, 2);
        hint->setAttribute(Qt::WA_TransparentForMouseEvents);
        addWidget(hint);
        return;
    }
    for (int i = 0; i < m_list.size(); ++i) {
        const Favorite &f = m_list.at(i);
        if (f.type == Favorite::Type::Separator) {
            addSeparator();
            continue;
        }
        const QString title = fontMetrics().elidedText(f.title(), Qt::ElideMiddle, fontMetrics().averageCharWidth() * 24);
        QAction *a = addAction(FavoriteMenus::icon(f), escaped(title));
        // Said outright, since Qt makes one for buttons by taking "…" and
        // such out of the action's text.
        a->setIconText(escaped(title) + (f.type == Favorite::Type::Branch ? QStringLiteral(" \u25BE") : QString()));
        a->setToolTip(f.type == Favorite::Type::Branch ? f.title() : f.path);
        if (f.type == Favorite::Type::Branch) {
            auto *menu = new QMenu(this);
            menu->setObjectName("branch");
            FavoriteMenus::fillBranch(menu, f, m_receiver);
            a->setMenu(menu);
        } else {
            connect(a, &QAction::triggered, this, [this, f] {
                const Qt::KeyboardModifiers keys = QGuiApplication::keyboardModifiers();
                m_receiver->openFavorite(f, keys & Qt::ControlModifier, keys & Qt::ShiftModifier);
            });
        }
        if (auto *button = qobject_cast<QToolButton *>(widgetForAction(a))) {
            button->setFocusPolicy(Qt::NoFocus);
            button->setPopupMode(QToolButton::InstantPopup);
            button->setStyleSheet("QToolButton::menu-indicator { image: none; width: 0; }");
            button->setProperty(kIndex, i);
            button->installEventFilter(this);
        }
    }
}

bool FavoritesBar::eventFilter(QObject *watched, QEvent *event) {
    const QVariant index = watched->property(kIndex);
    if (index.isValid() && index.toInt() < m_list.size()) {
        const Favorite f = m_list.at(index.toInt());
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
            if (static_cast<QMouseEvent *>(event)->button() == Qt::MiddleButton) {
                if (event->type() == QEvent::MouseButtonRelease && f.type != Favorite::Type::Branch)
                    m_receiver->openFavorite(f, true, false);
                return true;
            }
        } else if (event->type() == QEvent::ContextMenu) {
            emit menuRequested(index.toInt(), static_cast<QContextMenuEvent *>(event)->globalPos());
            return true;
        }
    }
    return QToolBar::eventFilter(watched, event);
}

void FavoritesBar::contextMenuEvent(QContextMenuEvent *event) {
    emit menuRequested(-1, event->globalPos());
    event->accept();
}

void FavoritesBar::paintEvent(QPaintEvent *) {
    // No border: Fusion's stands out far too much on dark themes.
}

void FavoritesBar::changeEvent(QEvent *event) {
    // The stars are in the theme's colour.
    if ((event->type() == QEvent::LanguageChange || event->type() == QEvent::PaletteChange) && m_built)
        build();
    QToolBar::changeEvent(event);
}
