#pragma once

#include <QIcon>

class QPalette;

// Small line icons drawn in the palette's button text colors, so they follow
// the Omarchy theme. Icon themes often ship dark glyphs that vanish on a dark
// background, and the style's own arrows are tiny.
namespace Icons {

QIcon arrow(Qt::ArrowType direction, const QPalette &palette);
QIcon plus(const QPalette &palette);
QIcon close(const QPalette &palette);
// A downward chevron, for the list of all tabs.
QIcon chevron(const QPalette &palette);
// Marks a locked tab.
QIcon padlock(const QPalette &palette);
// Marks a tab linked with one on the other side.
QIcon link(const QPalette &palette);
// Favourites and their branches, in the accent colour.
QIcon star(const QPalette &palette);

} // namespace Icons
