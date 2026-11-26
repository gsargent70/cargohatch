// CargoHatch: a power-user file manager for Linux.

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QSettings>
#include <QStyleFactory>

#include "folderaliases.h"
#include "listermanager.h"
#include "taboptions.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("cargohatch");
    app.setApplicationDisplayName("CargoHatch");
    app.setApplicationVersion("0.1.0");

    // Matches the window to cargohatch.desktop, so the compositor (Wayland
    // app_id) and the launcher pick up the installed icon.
    app.setDesktopFileName("cargohatch");
    // Built in as well, for when the program is run without being installed.
    QIcon icon;
    for (const int size : {16, 32, 48, 64, 128, 256, 512})
        icon.addFile(QStringLiteral(":/icons/%1.png").arg(size), QSize(size, size));
    app.setWindowIcon(QIcon::fromTheme("cargohatch", icon));

    // Fusion draws everything from the palette, so the Omarchy theme colors
    // reach every widget.
    app.setStyle(createAppStyle());

    Translator translator;
    app.installTranslator(&translator);
    // Widgets retranslate on QEvent::LanguageChange, which Qt only sends when
    // a translator is installed, so reinstall it whenever the language moves.
    QObject::connect(&translator, &Translator::changed, &app, [&] {
        QApplication::removeTranslator(&translator);
        QApplication::installTranslator(&translator);
    });

    Theme theme;
    QApplication::setPalette(theme.palette());
    QObject::connect(&theme, &Theme::changed, &app, [&] { QApplication::setPalette(theme.palette()); });

    QCommandLineParser parser;
    parser.setApplicationDescription(QApplication::translate("main", "A power-user file manager."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("folder", QApplication::translate("main", "Folder to open."), "[folder]");
    const QCommandLineOption layoutOption(
        "layout", QApplication::translate("main", "Open the windows saved as layout <name>."), "name");
    parser.addOption(layoutOption);
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    // ~/.config/cargohatch/cargohatch.conf
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "cargohatch", "cargohatch");
    FolderAliases::setUser(TabOptions::load(settings).aliases);
    ListerManager listers(&settings);
    // The remembered window comes back as it was; a folder given on the
    // command line opens in its active tab.
    QString folder;
    if (!args.isEmpty()) {
        const QFileInfo info(FolderAliases::expand(args.first(), QDir::currentPath()));
        folder = info.exists() ? info.canonicalFilePath() : args.first();
    }
    const QString layout = parser.value(layoutOption);
    if (layout.isEmpty() || !listers.openLayout(layout)) {
        if (!layout.isEmpty())
            qWarning().noquote() << QApplication::translate("main", "There is no layout called “%1”.").arg(layout);
        listers.openDefault(folder);
    }

    return app.exec();
}
