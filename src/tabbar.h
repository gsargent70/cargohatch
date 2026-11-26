#pragma once

#include <QTabBar>

// The folder tab bar. On the left or right of a display its tabs keep their
// text horizontal, so folder names stay readable, rather than turning them on
// their side as Qt does (TAB-02).
class FolderTabBar : public QTabBar {
    Q_OBJECT

public:
    explicit FolderTabBar(QWidget *parent = nullptr);

    bool isVertical() const;
    // Tabs are as wide as their names, up to a limit; past it the name is
    // shortened in the middle.
    static constexpr int kMaxVerticalWidth = 240;
    static constexpr int kMaxWidth = 220;
    // When the bar is short of room tabs shrink to this before it scrolls,
    // so a few letters of each name stay readable.
    static constexpr int kMinWidth = 130;

    QSize minimumSizeHint() const override;

    // A tab's colour, drawn as a band along its outer edge (TAB-10). An
    // invalid colour clears it. Stored as tab data, so it moves with the tab.
    void setTabColor(int index, const QColor &color);
    QColor tabColor(int index) const;

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize tabSizeHint(int index) const override;
    QSize minimumTabSizeHint(int index) const override;
};
