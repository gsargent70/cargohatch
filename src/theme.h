#pragma once

#include <QColor>

class QStyle;
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QPalette>
#include <QTimer>

// Follows the current Omarchy theme live. Omarchy writes a full set of named
// colors to theme/colors.toml, so a widget application can build a complete
// palette from it instead of tinting a fixed dark style with the accent.
class Theme : public QObject {
    Q_OBJECT

public:
    // The directory is ~/.local/state/omarchy/current unless a test passes a
    // temporary one.
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr);

    QColor color(const QString &name) const;
    QColor accent() const { return color("accent"); }
    bool isDark() const;
    QPalette palette() const;

    // Every `name = "#rrggbb"` pair in a colors.toml file.
    static QHash<QString, QColor> readColors(const QString &path);
    // The colors used when Omarchy isn't installed: a neutral dark scheme with
    // Omarchy's default yellow accent.
    static QHash<QString, QColor> fallbackColors();
    // Where Omarchy keeps the current theme.
    static QString defaultDirectory();
    // The hues a tab can be coloured with, by their names in colors.toml.
    static QStringList hueNames();

signals:
    void changed();

private:
    void reload();

    QString m_directory;
    QHash<QString, QColor> m_colors;
    QString m_mode;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};

// Fusion, with the frames of check boxes and radio buttons drawn in a
// colour that shows on dark themes, where Fusion's own frame (a darker
// shade of the window) disappears.
QStyle *createAppStyle();
