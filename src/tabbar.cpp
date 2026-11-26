#include "tabbar.h"

#include <QPainter>
#include <QProxyStyle>
#include <QStyleOptionTab>

namespace {

bool verticalShape(QTabBar::Shape shape) {
    return shape == QTabBar::RoundedWest || shape == QTabBar::RoundedEast || shape == QTabBar::TriangularWest ||
           shape == QTabBar::TriangularEast;
}

// Lays out and draws the inside of a vertical tab as if the tab were on top:
// text left to right, buttons at the right-hand end. The tab's outline is
// still drawn for its real side.
class HorizontalLabelStyle : public QProxyStyle {
public:
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget) const override {
        if (element == CE_TabBarTabLabel) {
            if (const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option);
                tab && verticalShape(tab->shape)) {
                QStyleOptionTab flat(*tab);
                flat.shape = QTabBar::RoundedNorth;
                QProxyStyle::drawControl(element, &flat, painter, widget);
                return;
            }
        }
        QProxyStyle::drawControl(element, option, painter, widget);
    }

    QRect subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget) const override {
        if (element == SE_TabBarTabText || element == SE_TabBarTabLeftButton ||
            element == SE_TabBarTabRightButton) {
            if (const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option);
                tab && verticalShape(tab->shape)) {
                QStyleOptionTab flat(*tab);
                flat.shape = QTabBar::RoundedNorth;
                return QProxyStyle::subElementRect(element, &flat, widget);
            }
        }
        return QProxyStyle::subElementRect(element, option, widget);
    }
};

} // namespace

FolderTabBar::FolderTabBar(QWidget *parent) : QTabBar(parent) {
    auto *style = new HorizontalLabelStyle;
    style->setParent(this);
    setStyle(style);
}

bool FolderTabBar::isVertical() const {
    return verticalShape(shape());
}

void FolderTabBar::setTabColor(int index, const QColor &color) {
    if (tabColor(index) == color)
        return;
    setTabData(index, color);
    update();
}

QColor FolderTabBar::tabColor(int index) const {
    return tabData(index).value<QColor>();
}

void FolderTabBar::paintEvent(QPaintEvent *event) {
    QTabBar::paintEvent(event);
    QPainter painter(this);
    constexpr int band = 3;
    for (int i = 0; i < count(); ++i) {
        const QColor color = tabColor(i);
        if (!color.isValid())
            continue;
        // Along the edge away from the folder, so it reads like a tab's
        // coloured top.
        const QRect r = tabRect(i).adjusted(1, 1, -1, -1);
        QRect edge;
        switch (shape()) {
        case RoundedSouth:
        case TriangularSouth:
            edge = QRect(r.left(), r.bottom() - band + 1, r.width(), band);
            break;
        case RoundedWest:
        case TriangularWest:
            edge = QRect(r.left(), r.top(), band, r.height());
            break;
        case RoundedEast:
        case TriangularEast:
            edge = QRect(r.right() - band + 1, r.top(), band, r.height());
            break;
        default:
            edge = QRect(r.left(), r.top(), r.width(), band);
            break;
        }
        painter.fillRect(edge, color);
    }
}

QSize FolderTabBar::minimumSizeHint() const {
    // Room for the scroll buttons is only needed when the tabs don't fit;
    // a short row of tabs shouldn't ask for more than itself.
    QSize size = QTabBar::minimumSizeHint();
    const QSize wanted = sizeHint();
    if (isVertical())
        size.setHeight(qMin(size.height(), wanted.height()));
    else
        size.setWidth(qMin(size.width(), wanted.width()));
    return size;
}

QSize FolderTabBar::tabSizeHint(int index) const {
    QSize size = QTabBar::tabSizeHint(index);
    if (isVertical()) {
        // Qt sizes a vertical tab for turned text: its length along the bar
        // is the text width. With the text level, that is the tab's width.
        size.transpose();
        size.setWidth(qMin(size.width(), kMaxVerticalWidth));
    } else {
        size.setWidth(qMin(size.width(), kMaxWidth));
    }
    return size;
}

QSize FolderTabBar::minimumTabSizeHint(int index) const {
    const QSize full = tabSizeHint(index);
    if (isVertical())
        return full;
    return QSize(qMin(full.width(), kMinWidth), full.height());
}
