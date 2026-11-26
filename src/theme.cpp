// Theme watching follows Omacut (MIT, David Heinemeier Hansson).
#include "theme.h"

#include <algorithm>

#include <QPainter>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOption>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

Theme::Theme(const QString &directory, QObject *parent)
    : QObject(parent),
      m_directory(directory.isEmpty() ? defaultDirectory() : directory) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Theme::reload);
    reload();
}

QHash<QString, QColor> Theme::fallbackColors() {
    return {
        {"accent", QColor("#FFD60A")},
        {"selection", QColor("#3a3a40")},
        {"muted", QColor("#8a8a90")},
        {"background", QColor("#0e0e10")},
        {"dark_background", QColor("#09090b")},
        {"lighter_background", QColor("#1c1c20")},
        {"foreground", QColor("#e6e6e6")},
        {"dark_foreground", QColor("#8a8a90")},
        {"red", QColor("#ED5B5A")},
        {"orange", QColor("#eb8b54")},
        {"yellow", QColor("#E9BB4F")},
        {"green", QColor("#7fb069")},
        {"cyan", QColor("#5fb3c4")},
        {"blue", QColor("#6c8ee6")},
        {"magenta", QColor("#c47fd5")},
    };
}

QString Theme::defaultDirectory() {
    return QDir::homePath() + "/.local/state/omarchy/current";
}

QStringList Theme::hueNames() {
    return {"red", "orange", "yellow", "green", "cyan", "blue", "magenta"};
}

QHash<QString, QColor> Theme::readColors(const QString &path) {
    QHash<QString, QColor> colors;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return colors;
    static const QRegularExpression pair(R"re(^\s*([a-z_]+)\s*=\s*["'](#[0-9a-fA-F]{6})["'])re",
                                         QRegularExpression::MultilineOption);
    auto it = pair.globalMatch(QString::fromUtf8(file.readAll()));
    while (it.hasNext()) {
        const auto match = it.next();
        colors.insert(match.captured(1), QColor(match.captured(2)));
    }
    return colors;
}

QColor Theme::color(const QString &name) const {
    // A theme that leaves out a color still gets a readable one.
    const auto value = m_colors.value(name);
    return value.isValid() ? value : fallbackColors().value(name, QColor("#808080"));
}

bool Theme::isDark() const {
    if (m_mode == "light")
        return false;
    if (m_mode == "dark")
        return true;
    return color("background").lightnessF() < 0.5;
}

QPalette Theme::palette() const {
    const QColor background = color("background");
    const QColor base = color("dark_background");
    const QColor raised = color("lighter_background");
    const QColor text = color("foreground");
    const QColor muted = color("dark_foreground");

    // The selection color is the theme's own, with the normal foreground on
    // it; the accent is kept for focus and links so it stands out.
    QPalette p;
    p.setColor(QPalette::Window, background);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, raised);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::PlaceholderText, muted);
    p.setColor(QPalette::Button, raised);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, color("red"));
    p.setColor(QPalette::Highlight, color("selection"));
    p.setColor(QPalette::HighlightedText, text);
    p.setColor(QPalette::ToolTipBase, raised);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Link, accent());
    p.setColor(QPalette::LinkVisited, accent().darker(120));
    p.setColor(QPalette::Accent, accent());
    p.setColor(QPalette::Light, raised.lighter(130));
    p.setColor(QPalette::Midlight, raised);
    p.setColor(QPalette::Mid, raised.darker(130));
    p.setColor(QPalette::Dark, base);
    p.setColor(QPalette::Shadow, Qt::black);
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, muted);
    return p;
}

void Theme::reload() {
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);

    // A theme switch replaces symlinks and files, so watch the parents too and
    // re-arm after every change. Watching the nearest existing ancestor means
    // installing Omarchy later works without restarting the app.
    QString ancestor = m_directory;
    while (!QFileInfo::exists(ancestor) && ancestor != "/")
        ancestor = QFileInfo(ancestor).absolutePath();
    QStringList candidates{ancestor, QFileInfo(ancestor).absolutePath(), m_directory,
                           m_directory + "/theme", m_directory + "/theme/colors.toml"};
    candidates.removeDuplicates();
    for (const auto &path : candidates)
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);

    const QString file = m_directory + "/theme/colors.toml";
    auto colors = readColors(file);
    QString mode;
    QFile toml(file);
    if (toml.open(QIODevice::ReadOnly)) {
        static const QRegularExpression modeLine(R"re(^\s*mode\s*=\s*["'](\w+)["'])re",
                                                 QRegularExpression::MultilineOption);
        mode = modeLine.match(QString::fromUtf8(toml.readAll())).captured(1);
    }
    if (colors.isEmpty())
        colors = fallbackColors();

    if (colors != m_colors || mode != m_mode) {
        m_colors = colors;
        m_mode = mode;
        emit changed();
    }
}

namespace {

class AppStyle : public QProxyStyle {
public:
    AppStyle() : QProxyStyle(QStyleFactory::create("Fusion")) {}

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget) const override {
        // A menu gets an outline in the theme's accent, as Omarchy outlines
        // windows, so it stands apart from the window behind it.
        if (element == PE_FrameMenu || element == PE_PanelMenu) {
            if (element == PE_PanelMenu)
                QProxyStyle::drawPrimitive(element, option, painter, widget);
            painter->save();
            painter->setPen(QPen(option->palette.color(QPalette::Accent), 1));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5));
            painter->restore();
            return;
        }
        QProxyStyle::drawPrimitive(element, option, painter, widget);
        if (element != PE_IndicatorCheckBox && element != PE_IndicatorRadioButton)
            return;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        QColor edge = option->palette.color(QPalette::PlaceholderText);
        if (!(option->state & State_Enabled))
            edge.setAlphaF(0.5);
        painter->setPen(QPen(edge, 1));
        painter->setBrush(Qt::NoBrush);
        const QRectF r = QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5);
        if (element == PE_IndicatorRadioButton)
            painter->drawEllipse(r);
        else
            painter->drawRoundedRect(r, 2, 2);
        painter->restore();
    }

    int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const override {
        // Room for the outline, so items don't paint over it.
        if (metric == PM_MenuPanelWidth)
            return std::max(1, QProxyStyle::pixelMetric(metric, option, widget));
        return QProxyStyle::pixelMetric(metric, option, widget);
    }

    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget) const override {
        // Fusion's separators vanish on a dark theme; a faint line keeps the
        // groups of a menu apart.
        const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
        if (element == CE_MenuItem && item && item->menuItemType == QStyleOptionMenuItem::Separator &&
            item->text.isEmpty()) {
            painter->save();
            QColor line = option->palette.color(QPalette::PlaceholderText);
            line.setAlphaF(0.5);
            painter->setPen(QPen(line, 1));
            const int y = option->rect.center().y();
            painter->drawLine(option->rect.left() + 6, y, option->rect.right() - 6, y);
            painter->restore();
            return;
        }
        QProxyStyle::drawControl(element, option, painter, widget);
    }
};

} // namespace

QStyle *createAppStyle() {
    return new AppStyle;
}
