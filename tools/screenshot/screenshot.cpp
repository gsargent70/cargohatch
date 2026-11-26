// Takes the README screenshot (bin/screenshot): made-up files in a private
// /home/sam, the default theme, and a window showing off the main features.
#include <QAction>
#include <QApplication>
#include <QMenuBar>
#include <QTimer>
#include <QSettings>
#include "favorites.h"
#include "filedisplay.h"
#include "foldertab.h"
#include "mainwindow.h"
#include "theme.h"

static QAction *find(QMenuBar *bar, const QString &menu, const QString &text) {
    for (QAction *m : bar->actions())
        if (m->text().remove('&') == menu)
            for (QAction *a : m->menu()->actions())
                if (a->text().section('\t', 0, 0) == text)
                    return a;
    return nullptr;
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setStyle(createAppStyle());
    Theme theme(QStringLiteral("/nonexistent"));
    app.setPalette(theme.palette());
    const QString home = QStringLiteral("/home/sam");
    QSettings settings(home + "/.config/cargohatch/cargohatch.conf", QSettings::IniFormat);
    MainWindow w(home + "/Projects", &settings);
    w.resize(1440, 470);
    TabOptions options = w.tabOptions();
    options.favoritesBar = true;
    options.tree.drives = false; // the real machine's mounts would show
    options.tree.favorites = false;
    options.favorites.bar = {Favorite::folder(home + "/Projects"), Favorite::folder(home + "/Photos"),
                             Favorite::folder(home + "/Documents"), Favorite::folder(home + "/Downloads"),
                             Favorite::folder(home + "/Music")};
    w.setTabOptions(options);
    w.show();

    FileDisplay *left = w.display(0);
    left->currentTab()->setColorName("blue");
    FolderTab *invoices = left->addTab(home + "/Documents/Invoices");
    invoices->setLock(FolderTab::Lock::Locked);
    left->addTab(home + "/Music/Albums")->setColorName("magenta");
    left->setCurrentIndex(0);
    w.setDual(true);
    FileDisplay *right = w.display(1);
    right->currentTab()->openPath(home + "/Photos/2026 Summer");
    right->currentTab()->setColorName("green");
    right->addTab(home + "/Recipes");
    right->setCurrentIndex(0);
    find(w.menuBar(), "View", "Folder Tree")->trigger();

    QTimer::singleShot(800, [&] {
        right->currentTab()->selectNames({"harbour.jpg", "lighthouse.jpg", "sunset.jpg"});
        left->currentTab()->selectNames({});
        left->currentTab()->sizeColumnsToFit();
        right->currentTab()->sizeColumnsToFit();
        w.activateSide(1);
        right->currentTab()->setFocus();
    });
    QTimer::singleShot(1400, [&] {
        w.grab().save(argv[1]);
        app.quit();
    });
    return app.exec();
}
