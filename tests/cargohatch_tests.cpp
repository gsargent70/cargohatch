#include <QDir>
#include <QFile>
#include <QImage>
#include <QListView>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSplitter>
#include <QStorageInfo>
#include <QTabBar>
#include <QMenu>
#include <QMenuBar>
#include <QToolButton>
#include <QTemporaryDir>
#include <QTreeView>
#include <QtTest>

#include "aboutdialog.h"
#include "shortcutsdialog.h"
#include "breadcrumb.h"
#include "filedisplay.h"
#include "filemodel.h"
#include "fileops.h"
#include "createfolder.h"
#include "fileclipboard.h"
#include "filedrag.h"
#include "globalfilter.h"
#include "aliasdialog.h"
#include "favorites.h"
#include "favoritesbar.h"
#include "favoritesdialog.h"
#include "folderaliases.h"
#include "foldertree.h"
#include "foldertreemodel.h"
#include "treeoptions.h"
#include "folderpicker.h"
#include "pathcompleter.h"
#include <QCompleter>
#include <QContextMenuEvent>
#include <QMessageBox>
#include <QInputDialog>
#include <QTreeWidget>
#include "findbar.h"
#include "namematch.h"
#include "quickfilter.h"
#include "nameformat.h"
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <sys/stat.h>
#include <unistd.h>
#include "operationrunner.h"
#include "progresswindow.h"
#include "renamedelegate.h"
#include "replacedialog.h"
#include <QtConcurrent>
#include "selectpattern.h"
#include "filesortproxy.h"
#include "viewformat.h"
#include "foldertab.h"
#include "history.h"
#include "listermanager.h"
#include "listerstate.h"
#include "savedsets.h"
#include "mainwindow.h"
#include "tabbar.h"
#include "taboptions.h"
#include "tabswitcher.h"
#include "terminal.h"
#include "theme.h"
#include "translator.h"
#include "preview.h"
#include "batchrename.h"
#include "renamedialog.h"
#include "viewerpane.h"
#include <QPainter>
#include <QPdfWriter>

using Role = FileDisplay::Role;

class CargoHatchTests : public QObject {
    Q_OBJECT

private slots:
    // --- Theme and language --------------------------------------------

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QColor("#FFD60A"));
        QVERIFY(theme.isDark());
    }

    void themeReadsColorsAndMode()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml",
              "mode = \"light\"\naccent = \"#112233\"\nselection = \"#445566\"\nbackground = \"#ffffff\"\n");

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QColor("#112233"));
        QVERIFY(!theme.isDark());
        const QPalette palette = theme.palette();
        QCOMPARE(palette.color(QPalette::Highlight), QColor("#445566"));
        QCOMPARE(palette.color(QPalette::Window), QColor("#ffffff"));
        QCOMPARE(theme.color("foreground"), QColor("#e6e6e6"));
    }

    void themeFollowsChanges()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");
        Theme theme(dir.path());
        write(dir.path() + "/theme/colors.toml", "accent = \"#abcdef\"\n");
        QTRY_COMPARE(theme.accent(), QColor("#abcdef"));
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("MainWindow", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("MainWindow", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("MainWindow", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("MainWindow", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/en.json", R"({"%n items": {"one": "%n item", "other": "%n items"}})");
        write(dir.path() + "/ru.json",
              R"({"%n items": {"one": "%n элемент", "few": "%n элемента", "many": "%n элементов", "other": "%n элемента"}})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("MainWindow", "%n items", nullptr, 21), QString("%n элемент"));
        QCOMPARE(translator.translate("MainWindow", "%n items", nullptr, 25), QString("%n элементов"));
    }

    // --- History (NAV-01) ------------------------------------------------

    void historyGoesBackAndForward()
    {
        History h;
        h.visit("/a");
        h.visit("/b");
        h.visit("/c");
        QVERIFY(h.canGoBack());
        QVERIFY(!h.canGoForward());
        QCOMPARE(h.peek(-2), QString("/a"));
        QCOMPARE(h.current(), QString("/c")); // peek doesn't move
        h.go(-2);
        QCOMPARE(h.current(), QString("/a"));
        QCOMPARE(h.forwardList(), (QStringList{"/b", "/c"}));
        h.go(1);
        QCOMPARE(h.backList(), QStringList{"/a"});
        QCOMPARE(h.forwardList(), QStringList{"/c"});
        QVERIFY(h.peek(5).isEmpty());
    }

    void historyVisitDropsTheForwardEntries()
    {
        History h;
        h.visit("/a");
        h.visit("/b");
        h.visit("/c");
        h.go(-2);
        h.visit("/d");
        QCOMPARE(h.backList(), QStringList{"/a"});
        QVERIFY(!h.canGoForward());
        h.visit("/d"); // revisiting the current folder adds nothing
        QCOMPARE(h.size(), 2);
    }

    void historyIsCapped()
    {
        History h(3);
        for (const char *p : {"/1", "/2", "/3", "/4"})
            h.visit(p);
        QCOMPARE(h.size(), 3);
        QCOMPARE(h.backList(), (QStringList{"/3", "/2"}));
    }

    // --- Breadcrumb (NAV-02) ---------------------------------------------

    void crumbsSplitThePath()
    {
        const auto list = Breadcrumb::crumbs("/home/u/docs", {});
        QCOMPARE(list.size(), 4);
        QCOMPARE(list.at(0).path, QString("/"));
        QCOMPARE(list.at(2).label, QString("u"));
        QCOMPARE(list.at(3).path, QString("/home/u/docs"));
        QVERIFY(!list.at(3).ghost);
        QCOMPARE(Breadcrumb::crumbs("/", {}).size(), 1);
    }

    void crumbsAddGhostsBelowThePath()
    {
        const auto list = Breadcrumb::crumbs("/home/u", "/home/u/a/b");
        QCOMPARE(list.size(), 5);
        QVERIFY(list.at(3).ghost);
        QCOMPARE(list.at(4).path, QString("/home/u/a/b"));
        // A ghost that isn't below the path is ignored.
        QCOMPARE(Breadcrumb::crumbs("/home/u", "/etc/x").size(), 3);
        QCOMPARE(Breadcrumb::crumbs("/home/u", "/home/user2").size(), 3);
    }

    void ghostRulesFollowNavigation()
    {
        // Going up leaves the deeper folder as a ghost.
        QCOMPARE(FolderTab::nextGhost("/a/b/c", {}, "/a"), QString("/a/b/c"));
        // Going up further keeps the deepest one.
        QCOMPARE(FolderTab::nextGhost("/a", "/a/b/c", "/"), QString("/a/b/c"));
        // Stepping back down part of the way keeps it; all the way clears it.
        QCOMPARE(FolderTab::nextGhost("/a", "/a/b/c", "/a/b"), QString("/a/b/c"));
        QVERIFY(FolderTab::nextGhost("/a/b", "/a/b/c", "/a/b/c").isEmpty());
        // Going somewhere else clears it.
        QVERIFY(FolderTab::nextGhost("/a", "/a/b/c", "/x").isEmpty());
        QVERIFY(FolderTab::nextGhost("/a", "/a/b/c", "/a/z").isEmpty());
    }

    void subfoldersSortNaturallyWithHiddenLast()
    {
        QTemporaryDir dir;
        QDir d(dir.path());
        for (const char *n : {"b", "a10", "a9", ".cache", "A1"})
            d.mkdir(n);
        write(dir.path() + "/file.txt", "x");
        QStringList hidden;
        QCOMPARE(Breadcrumb::subfolders(dir.path(), &hidden), (QStringList{"A1", "a9", "a10", "b"}));
        QCOMPARE(hidden, QStringList{".cache"});
        QCOMPARE(Breadcrumb::subfolders(dir.path()).last(), QString(".cache"));
    }

    void breadcrumbFoldsLeadingFoldersWhenNarrow()
    {
        Breadcrumb bar;
        bar.resize(2000, 30);
        bar.show(); // hidden widgets get no resize events
        bar.setPath("/one/two/three/four/five/six");
        QCOMPARE(bar.visibleCrumbCount(), 7);
        QVERIFY(!bar.isOverflowing());
        bar.resize(120, 30);
        QVERIFY(bar.isOverflowing());
        QVERIFY(bar.visibleCrumbCount() >= 1);
        QVERIFY(bar.visibleCrumbCount() < 7);
    }

    void breadcrumbEditsAndEscapes()
    {
        Breadcrumb bar;
        QSignalSpy requested(&bar, &Breadcrumb::pathRequested);
        bar.setPath("/tmp");
        bar.startEditing();
        QVERIFY(bar.isEditing());
        QCOMPARE(bar.editor()->text(), QString("/tmp"));
        bar.editor()->setText("/usr");
        QTest::keyClick(bar.editor(), Qt::Key_Return);
        QCOMPARE(requested.count(), 1);
        QCOMPARE(requested.at(0).at(0).toString(), QString("/usr"));
        QTest::keyClick(bar.editor(), Qt::Key_Escape);
        QVERIFY(!bar.isEditing());
    }

    // --- Folder tab ------------------------------------------------------

    void tabListsAFolder()
    {
        QTemporaryDir dir;
        write(dir.path() + "/a.txt", "a");
        write(dir.path() + "/.hidden", "h");
        QDir(dir.path()).mkdir("sub");

        FolderTab tab(dir.path());
        QCOMPARE(tab.currentPath(), canonical(dir.path()));
        QTRY_COMPARE(tab.itemCount(), 2);
        QCOMPARE(tab.statusText(), QString("2 items  ·  1 hidden"));
    }

    void tabRefusesMissingPaths()
    {
        QTemporaryDir dir;
        FolderTab tab(dir.path());
        const QString before = tab.currentPath();
        QVERIFY(!tab.openPath(dir.path() + "/nope"));
        QVERIFY(tab.statusText().contains("does not exist"));
        QCOMPARE(tab.currentPath(), before);
        QCOMPARE(tab.history().size(), 1);
    }

    void filePathsOpenTheirFolderWithTheFileSelected()
    {
        // IDEA-12.
        QTemporaryDir dir;
        const QString base = canonical(dir.path());
        QDir().mkpath(base + "/sub");
        write(base + "/sub/a.txt", "a");
        write(base + "/sub/b.txt", "b");
        write(base + "/sub/.secret", "s");
        FolderTab tab(base);
        QVERIFY(tab.openPath(base + "/sub/b.txt"));
        QCOMPARE(tab.currentPath(), base + "/sub");
        QTRY_COMPARE(tab.selectedPaths(), QStringList{base + "/sub/b.txt"});
        // In the folder already, it is only selected.
        QVERIFY(tab.openPath(base + "/sub/a.txt"));
        QTRY_COMPARE(tab.selectedPaths(), QStringList{base + "/sub/a.txt"});
        // Typed in the location bar, with the folder's own relative path.
        tab.editPath();
        tab.breadcrumb()->editor()->setText("b.txt");
        QTest::keyClick(tab.breadcrumb()->editor(), Qt::Key_Return);
        QTRY_COMPARE(tab.selectedPaths(), QStringList{base + "/sub/b.txt"});
        // A hidden one says why it can't be seen.
        QVERIFY(tab.openPath(base + "/sub/.secret"));
        QTRY_VERIFY(tab.statusText().contains("hidden"));
        // A new window given a file starts there too.
        FolderTab fresh(base + "/sub/a.txt");
        QCOMPARE(fresh.currentPath(), base + "/sub");
        QTRY_COMPARE(fresh.selectedPaths(), QStringList{base + "/sub/a.txt"});
    }

    void goingUpLandsOnTheFolderLeft()
    {
        // NAV-08: by the Up key, the location bar, or Back.
        QTemporaryDir dir;
        const QString base = canonical(dir.path());
        QDir().mkpath(base + "/a/deep/er");
        QDir().mkpath(base + "/b");
        FolderTab tab(base + "/a/deep/er");
        auto current = [&] { return tab.fileView()->currentIndex().data().toString(); };
        tab.goUp();
        QTRY_COMPARE(current(), QString("er"));
        // Marked, not selected, so nothing acts on it by surprise.
        QCOMPARE(tab.selectedCount(), 0);
        QVERIFY(tab.openPath(base));
        QTRY_COMPARE(current(), QString("a"));
        QVERIFY(tab.goBack());
        QTRY_COMPARE(tab.currentPath(), base + "/a/deep");
        QVERIFY(tab.openPath(base + "/b"));
        QVERIFY(tab.goBack());
        QTRY_COMPARE(current(), QString());
    }

    void tabStartingInAMissingFolderFallsBackHomeAndSaysWhy()
    {
        FolderTab tab("/definitely/not/here");
        QCOMPARE(tab.currentPath(), canonical(QDir::homePath()));
        QVERIFY(tab.statusText().contains("/definitely/not/here"));
    }

    void tabNavigatesThroughHistory()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("a/b");
        const QString root = canonical(dir.path());

        FolderTab tab(root);
        QVERIFY(tab.openPath(root + "/a"));
        QVERIFY(tab.openPath(root + "/a/b"));
        QVERIFY(tab.goBack(2));
        QCOMPARE(tab.currentPath(), root);
        QVERIFY(tab.goForward());
        QCOMPARE(tab.currentPath(), root + "/a");
        QVERIFY(!tab.goBack(5));
        QCOMPARE(tab.currentPath(), root + "/a");
    }

    void tabHistorySkipsFoldersThatWereDeleted()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkdir("gone");
        const QString root = canonical(dir.path());

        FolderTab tab(root + "/gone");
        QVERIFY(tab.openPath(root));
        QVERIFY(QDir(root + "/gone").removeRecursively());
        QVERIFY(!tab.goBack());
        QCOMPARE(tab.currentPath(), root);
        QVERIFY(tab.history().canGoBack()); // history wasn't moved
    }

    void tabGoesUpLeavingAGhostAndSelectingTheChild()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("a/b");
        const QString root = canonical(dir.path());

        FolderTab tab(root + "/a/b");
        tab.goUp();
        QCOMPARE(tab.currentPath(), root + "/a");
        QCOMPARE(tab.ghostPath(), root + "/a/b");
        QTRY_VERIFY(tab.fileView()->currentIndex().isValid());
        QCOMPARE(tab.fileView()->currentIndex().data().toString(), QString("b"));
        tab.goUp();
        QCOMPARE(tab.ghostPath(), root + "/a/b");
        QVERIFY(tab.openPath(root + "/a/b"));
        QVERIFY(tab.ghostPath().isEmpty());
    }

    void tabGoesToTheRootOfItsFilesystem()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkdir("x");
        const QString start = canonical(dir.path() + "/x");
        const QString mountRoot = QStorageInfo(start).rootPath();

        FolderTab tab(start);
        tab.goRoot();
        QCOMPARE(tab.currentPath(), mountRoot);
        tab.goRoot(); // already at the top of this filesystem: go to /
        QCOMPARE(tab.currentPath(), QString("/"));
    }

    void tabBreadcrumbRequestsNavigate()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkdir("a");
        const QString root = canonical(dir.path());
        FolderTab tab(root);
        emit tab.breadcrumb()->pathRequested(root + "/a");
        QCOMPARE(tab.currentPath(), root + "/a");
        QCOMPARE(tab.breadcrumb()->path(), root + "/a");
    }

    // --- File display tabs (TAB-01) --------------------------------------

    void displayAddsDuplicatesAndClosesTabs()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkdir("a");
        const QString root = canonical(dir.path());

        FileDisplay display;
        display.addTab(root);
        QVERIFY(!display.closeTab(0)); // the last tab stays
        display.currentTab()->openPath(root + "/a");

        FolderTab *copy = display.duplicateTab(0);
        QCOMPARE(display.tabCount(), 2);
        QCOMPARE(display.currentIndex(), 1);
        QCOMPARE(copy->currentPath(), root + "/a");
        QVERIFY(copy->history().canGoBack()); // the copy keeps the history
        QCOMPARE(display.tabText(1), QString("a"));

        display.addTab(root);
        QCOMPARE(display.tabCount(), 3);
        QCOMPARE(display.currentIndex(), 2);
        display.cycleTab(1);
        QCOMPARE(display.currentIndex(), 0);
        display.cycleTab(-1);
        QCOMPARE(display.currentIndex(), 2);

        QVERIFY(display.closeTab(0));
        QCOMPARE(display.tabCount(), 2);
    }

    void displayTabLabelFollowsTheFolder()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkdir("R&D");
        FileDisplay display;
        display.addTab(dir.path());
        display.currentTab()->openPath(dir.path() + "/R&D");
        QCOMPARE(display.tabText(0), QString("R&&D"));
        QCOMPARE(display.tabToolTip(0), canonical(dir.path() + "/R&D"));
    }

    // --- Lister: dual display and source/destination (LST-02, LST-03) -----

    void roleAssignment()
    {
        int s, d;
        MainWindow::assignRoles(false, 1, -1, Role::None, &s, &d);
        QCOMPARE(s, 1);
        QCOMPARE(d, -1);
        MainWindow::assignRoles(true, 1, -1, Role::None, &s, &d);
        QCOMPARE(s, 1);
        QCOMPARE(d, 0);
        MainWindow::assignRoles(true, 1, 0, Role::Source, &s, &d);
        QCOMPARE(s, 0);
        QCOMPARE(d, 1);
        MainWindow::assignRoles(true, 0, 0, Role::Destination, &s, &d);
        QCOMPARE(s, 1);
        QCOMPARE(d, 0);
    }

    void listerStartsSingle()
    {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        QVERIFY(!w.isDual());
        QVERIFY(w.display(1)->isHidden());
        QCOMPARE(w.sourceTab()->currentPath(), canonical(dir.path()));
        QCOMPARE(w.destinationTab(), nullptr);
        QCOMPARE(w.display(0)->role(), Role::None);
    }

    void listerTogglesDual()
    {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        w.setDual(true);
        QVERIFY(!w.display(1)->isHidden());
        QCOMPARE(w.display(1)->currentTab()->currentPath(), canonical(dir.path()));
        QCOMPARE(w.sourceSide(), 0);
        QCOMPARE(w.destinationSide(), 1);
        QCOMPARE(w.display(0)->role(), Role::Source);
        QCOMPARE(w.display(1)->role(), Role::Destination);

        // Clicking the other side makes it the source.
        w.activateSide(1);
        QCOMPARE(w.sourceSide(), 1);
        QCOMPARE(w.display(0)->role(), Role::Destination);

        // Going single keeps the side in use; its partner keeps its tabs.
        w.display(0)->addTab(dir.path());
        w.setDual(false);
        QVERIFY(w.display(0)->isHidden());
        QVERIFY(!w.display(1)->isHidden());
        QCOMPARE(w.display(1)->role(), Role::None);
        w.setDual(true);
        QCOMPARE(w.display(0)->tabCount(), 2);
    }

    void listerLocksRoles()
    {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        w.setDual(true);
        w.lockActiveSide(Role::Source); // side 0
        w.activateSide(1);
        QCOMPARE(w.activeSide(), 1);
        QCOMPARE(w.sourceSide(), 0); // still the source
        QVERIFY(w.display(0)->isRoleLocked());

        w.lockActiveSide(Role::Destination); // side 1 is now locked as destination
        w.activateSide(0);
        w.activateSide(1);
        QCOMPARE(w.sourceSide(), 0);
        QCOMPARE(w.destinationSide(), 1);

        w.lockActiveSide(Role::None);
        QCOMPARE(w.sourceSide(), 1);
        QVERIFY(!w.display(1)->isRoleLocked());

        // A single display can't be locked, and leaving dual clears locks.
        w.lockActiveSide(Role::Source);
        w.setDual(false);
        w.setDual(true);
        QVERIFY(!w.display(0)->isRoleLocked() && !w.display(1)->isRoleLocked());
    }

    void listerMovesTabsBetweenSides()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkdir("moving");
        MainWindow w(dir.path());
        w.moveTabToOtherSide(0, 0); // nowhere to go in a single display
        QCOMPARE(w.display(0)->tabCount(), 1);

        w.setDual(true);
        w.moveTabToOtherSide(0, 0); // the last tab on a side can't leave it
        QCOMPARE(w.display(0)->tabCount(), 1);

        w.display(0)->addTab(dir.path() + "/moving");
        w.moveTabToOtherSide(0, 1);
        QCOMPARE(w.display(0)->tabCount(), 1);
        QCOMPARE(w.display(1)->tabCount(), 2);
        QCOMPARE(w.display(1)->currentTab()->currentPath(), canonical(dir.path() + "/moving"));
        QCOMPARE(w.activeSide(), 1);

        // The moved tab still reports to its new side.
        QSignalSpy spy(w.display(1), &FileDisplay::tabPathChanged);
        w.display(1)->currentTab()->openPath(dir.path());
        QCOMPARE(spy.count(), 1);
    }

    void listerActivatesSidesByClickAndTab()
    {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        w.setDual(true);
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));

        // Clicking the right side's tab bar (which takes no focus) activates it.
        QTabBar *rightTabs = w.display(1)->tabBar();
        QTest::mouseClick(rightTabs, Qt::LeftButton, {}, rightTabs->tabRect(0).center());
        QCOMPARE(w.activeSide(), 1);
        QCOMPARE(w.sourceSide(), 1);

        // Tab in a file list jumps to the other side's list.
        QAbstractItemView *rightView = w.display(1)->currentTab()->fileView();
        rightView->setFocus();
        QTRY_VERIFY(rightView->hasFocus());
        QTest::keyClick(rightView, Qt::Key_Tab);
        QTRY_VERIFY(w.display(0)->currentTab()->fileView()->hasFocus());
        QCOMPARE(w.activeSide(), 0);
        QCOMPARE(w.sourceSide(), 0);
    }

    void listerSwitchesLayout()
    {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        auto *splitter = w.findChild<QSplitter *>("sides");
        QCOMPARE(splitter->orientation(), Qt::Horizontal);
        w.setLayout(MainWindow::Layout::Stacked);
        QCOMPARE(splitter->orientation(), Qt::Vertical);
    }

    void listerRemembersDualAndLayout()
    {
        QTemporaryDir dir;
        const QString file = dir.path() + "/cargohatch.conf";
        {
            QSettings settings(file, QSettings::IniFormat);
            MainWindow w(dir.path(), &settings);
            w.setDual(true);
            w.setLayout(MainWindow::Layout::Stacked);
            w.saveSettings();
        }
        QSettings settings(file, QSettings::IniFormat);
        MainWindow w(dir.path(), &settings);
        QVERIFY(w.isDual());
        QCOMPARE(w.layout(), MainWindow::Layout::Stacked);
        QCOMPARE(w.display(1)->currentTab()->currentPath(), canonical(dir.path()));
    }

    // --- Tab bar options (TAB-02, TAB-04, TAB-05, TAB-07) ----------------

    void tabOptionsRoundTrip()
    {
        QTemporaryDir dir;
        QSettings settings(dir.path() + "/c.conf", QSettings::IniFormat);
        const TabOptions defaults = TabOptions::load(settings);
        QCOMPARE(defaults.position, TabOptions::Position::Top);
        QVERIFY(defaults.closeButtons && defaults.ctrlTabSwitcher && !defaults.closeWindowWithLastTab);

        TabOptions o;
        o.position = TabOptions::Position::Right;
        o.newTabButton = false;
        o.openNextToCurrent = true;
        o.closeButtons = false;
        o.doubleClickCloses = true;
        o.closeWindowWithLastTab = true;
        o.ctrlTabSwitcher = false;
        o.save(settings);
        const TabOptions back = TabOptions::load(settings);
        QCOMPARE(back.position, TabOptions::Position::Right);
        QVERIFY(!back.newTabButton && back.openNextToCurrent && !back.closeButtons && back.doubleClickCloses);
        QVERIFY(back.closeWindowWithLastTab && !back.ctrlTabSwitcher);
    }

    void displayOpensNewTabsNextToCurrent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkdir("b");
        FileDisplay display;
        display.addTab(dir.path());
        display.addTab(dir.path());
        display.addTab(dir.path());
        display.setCurrentIndex(0);
        display.addTab(dir.path() + "/b"); // at the end by default
        QCOMPARE(display.currentIndex(), 3);

        TabOptions o;
        o.openNextToCurrent = true;
        display.setOptions(o);
        display.setCurrentIndex(0);
        FolderTab *tab = display.addTab(dir.path() + "/b");
        QCOMPARE(display.currentIndex(), 1);
        QCOMPARE(display.indexOf(tab), 1);
        QCOMPARE(display.currentTab(), tab);
    }

    void displayKeepsPagesInStepWithMovedTabs()
    {
        QTemporaryDir dir;
        for (auto name : {"a", "b", "c"})
            QDir(dir.path()).mkdir(name);
        FileDisplay display;
        for (auto name : {"a", "b", "c"})
            display.addTab(dir.path() + "/" + name);
        display.setCurrentIndex(0);
        display.tabBar()->moveTab(0, 2); // as a drag would
        QCOMPARE(display.tab(2)->title(), QString("a"));
        QCOMPARE(display.tabText(2), QString("a"));
        QCOMPARE(display.currentIndex(), 2);
        QCOMPARE(display.currentTab()->title(), QString("a"));

        display.closeTab(0); // "b"
        QCOMPARE(display.tabCount(), 2);
        QCOMPARE(display.currentTab()->title(), QString("a"));
        QCOMPARE(display.tab(0)->title(), QString("c"));
    }

    void displayCloseButtons()
    {
        QTemporaryDir dir;
        FileDisplay display;
        display.addTab(dir.path());
        auto closeButton = [&](int i) {
            const auto side = QTabBar::ButtonPosition(display.style()->styleHint(
                QStyle::SH_TabBar_CloseButtonPosition, nullptr, display.tabBar()));
            return qobject_cast<QToolButton *>(display.tabBar()->tabButton(i, side));
        };
        QVERIFY(closeButton(0));
        QVERIFY(closeButton(0)->isHidden()); // a lone tab can't be closed
        display.setLastTabClosable(true);
        QVERIFY(!closeButton(0)->isHidden());
        display.setLastTabClosable(false);

        display.addTab(dir.path());
        QVERIFY(!closeButton(0)->isHidden());
        closeButton(0)->click();
        QCOMPARE(display.tabCount(), 1);

        TabOptions o;
        o.closeButtons = false;
        display.setOptions(o);
        QVERIFY(!closeButton(0));
        o.closeButtons = true;
        display.setOptions(o);
        QVERIFY(closeButton(0));
    }

    void displayAsksTheWindowAboutTheLastTab()
    {
        QTemporaryDir dir;
        FileDisplay display;
        display.addTab(dir.path());
        QSignalSpy spy(&display, &FileDisplay::lastTabCloseRequested);
        QVERIFY(!display.closeTab(0));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(display.tabCount(), 1);
    }

    void displayPutsTabsOnAnySide()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkdir("a folder with a long name");
        FileDisplay display;
        display.addTab(dir.path() + "/a folder with a long name");
        display.resize(800, 400);
        display.show();
        QVERIFY(QTest::qWaitForWindowExposed(&display));

        TabOptions o;
        o.position = TabOptions::Position::Left;
        display.setOptions(o);
        QCOMPARE(display.tabBar()->shape(), QTabBar::RoundedWest);
        QTRY_VERIFY(display.tabBar()->mapTo(&display, QPoint()).x() < display.currentTab()->mapTo(&display, QPoint()).x());
        // Level text: a vertical tab is wider than it is tall.
        const QRect r = display.tabBar()->tabRect(0);
        QVERIFY2(r.width() > r.height(), qPrintable(QString("%1x%2").arg(r.width()).arg(r.height())));
        QVERIFY(r.width() <= FolderTabBar::kMaxVerticalWidth);

        o.position = TabOptions::Position::Right;
        display.setOptions(o);
        QCOMPARE(display.tabBar()->shape(), QTabBar::RoundedEast);
        QTRY_VERIFY(display.tabBar()->mapTo(&display, QPoint()).x() > display.currentTab()->mapTo(&display, QPoint()).x());

        o.position = TabOptions::Position::Bottom;
        display.setOptions(o);
        QCOMPARE(display.tabBar()->shape(), QTabBar::RoundedSouth);
        QTRY_VERIFY(display.tabBar()->mapTo(&display, QPoint()).y() > display.currentTab()->mapTo(&display, QPoint()).y());
    }

    void displayDoubleClicks()
    {
        QTemporaryDir dir;
        FileDisplay display;
        display.addTab(dir.path());
        display.resize(800, 400);
        display.show();
        QVERIFY(QTest::qWaitForWindowExposed(&display));

        // Empty space beside the tabs opens a new one.
        QWidget *header = display.tabBar()->parentWidget();
        QTest::mouseDClick(header, Qt::LeftButton, {}, QPoint(header->width() / 2, header->height() / 2));
        QCOMPARE(display.tabCount(), 2);

        // On a tab it only closes it when asked to.
        QTabBar *bar = display.tabBar();
        QTest::mouseDClick(bar, Qt::LeftButton, {}, bar->tabRect(0).center());
        QCOMPARE(display.tabCount(), 2);
        TabOptions o;
        o.doubleClickCloses = true;
        display.setOptions(o);
        QTest::mouseDClick(bar, Qt::LeftButton, {}, bar->tabRect(0).center());
        QCOMPARE(display.tabCount(), 1);
    }

    void displayListsTabsWhenTheyOverflow()
    {
        QTemporaryDir dir;
        for (int i = 0; i < 12; ++i)
            QDir(dir.path()).mkdir(QString("folder number %1").arg(i));
        FileDisplay display;
        display.resize(900, 300);
        display.show();
        QVERIFY(QTest::qWaitForWindowExposed(&display));
        display.addTab(dir.path() + "/folder number 0");
        display.addTab(dir.path() + "/folder number 1");
        QTRY_VERIFY(!display.isOverflowing());

        for (int i = 2; i < 12; ++i)
            display.addTab(dir.path() + QString("/folder number %1").arg(i));
        QTRY_VERIFY(display.isOverflowing());

        QMenu *menu = display.tabListMenu();
        QCOMPARE(menu->actions().size(), 12);
        QVERIFY(menu->actions().at(11)->isChecked());
        menu->actions().at(3)->trigger();
        QCOMPARE(display.currentIndex(), 3);

        while (display.tabCount() > 2)
            display.closeTab(0);
        QTRY_VERIFY(!display.isOverflowing());
    }

    // --- Closed tabs, last tab and the tab switcher (TAB-07/12/19, LST-26) --

    void listerReopensClosedTabs()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("a/b");
        MainWindow w(dir.path());
        FileDisplay *display = w.display(0);
        display->addTab(dir.path());
        display->currentTab()->openPath(dir.path() + "/a");
        display->currentTab()->openPath(dir.path() + "/a/b");
        display->addTab(dir.path());
        QVERIFY(w.closedTabs().isEmpty());

        QVERIFY(display->closeTab(1));
        QCOMPARE(w.closedTabs().size(), 1);
        QCOMPARE(w.closedTabs().first().tab.path, canonical(dir.path() + "/a/b"));

        FolderTab *back = w.reopenClosedTab();
        QVERIFY(back);
        QCOMPARE(display->indexOf(back), 1);
        QCOMPARE(back->currentPath(), canonical(dir.path() + "/a/b"));
        QVERIFY(back->goBack()); // with its history
        QCOMPARE(back->currentPath(), canonical(dir.path() + "/a"));
        QVERIFY(w.closedTabs().isEmpty());
        QCOMPARE(w.reopenClosedTab(), nullptr);

        // Only so many are kept.
        for (int i = 0; i < MainWindow::kMaxClosedTabs + 5; ++i) {
            display->addTab(dir.path());
            display->closeTab(display->currentIndex());
        }
        QCOMPARE(w.closedTabs().size(), MainWindow::kMaxClosedTabs);
    }

    void listerClosesWithTheLastTabWhenAsked()
    {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.display(0)->closeTab(0); // off by default: nothing happens
        QVERIFY(w.isVisible());

        TabOptions o;
        o.closeWindowWithLastTab = true;
        w.setTabOptions(o);

        // In a dual display only that side goes.
        w.setDual(true);
        w.display(1)->addTab(dir.path());
        w.display(1)->closeTab(1);
        w.display(1)->closeTab(0);
        QVERIFY(!w.isDual());
        QCOMPARE(w.activeSide(), 0);
        QCOMPARE(w.display(1)->tabCount(), 0);
        QCOMPARE(w.closedTabs().size(), 2);
        w.setDual(true); // comes back fresh
        QCOMPARE(w.display(1)->tabCount(), 1);
        w.setDual(false);

        w.display(0)->closeTab(0);
        QVERIFY(!w.isVisible());
    }

    void listerOrdersTabsByRecentUse()
    {
        QTemporaryDir dir;
        for (auto name : {"a", "b", "c"})
            QDir(dir.path()).mkdir(name);
        MainWindow w(dir.path() + "/a");
        FolderTab *a = w.activeTab();
        FolderTab *b = w.display(0)->addTab(dir.path() + "/b");
        w.setDual(true);
        FolderTab *right = w.display(1)->currentTab();
        FolderTab *c = w.display(1)->addTab(dir.path() + "/c");

        // b was current last on the active side; the right side has never
        // been used, so its tabs follow in screen order.
        QCOMPARE(w.tabsByRecentUse(), (QList<FolderTab *>{b, a, right, c}));

        w.activateSide(1);
        QCOMPARE(w.tabsByRecentUse().first(), c);
        w.display(1)->setCurrentIndex(0);
        QCOMPARE(w.tabsByRecentUse(), (QList<FolderTab *>{right, c, b, a}));

        // Tabs on a hidden side aren't offered.
        w.setDual(false);
        QVERIFY(!w.tabsByRecentUse().contains(a));
    }

    void listerSwitchesTabsWithCtrlTab()
    {
        QTemporaryDir dir;
        for (auto name : {"a", "b", "c"})
            QDir(dir.path()).mkdir(name);
        MainWindow w(dir.path() + "/a");
        w.display(0)->addTab(dir.path() + "/b");
        w.setDual(true);
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        FolderTab *a = w.display(0)->tab(0);

        // Ctrl+Tab highlights the previous tab; another Tab moves on, and
        // letting go of Ctrl switches.
        w.openTabSwitcher(1);
        TabSwitcher *switcher = w.tabSwitcher();
        QVERIFY(switcher->isVisible());
        QCOMPARE(switcher->currentRow(), 1);
        switcher->step(1);
        QCOMPARE(switcher->currentRow(), 2);
        switcher->step(-1);
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_Control, Qt::NoModifier);
        QApplication::sendEvent(switcher, &release);
        QVERIFY(!switcher->isVisible());
        QCOMPARE(w.activeTab(), a);

        // Escape leaves everything as it was.
        w.openTabSwitcher(-1);
        QVERIFY(switcher->isVisible());
        QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(switcher, &escape);
        QVERIFY(!switcher->isVisible());
        QCOMPARE(w.activeTab(), a);

        // Picking a tab on the other side moves there.
        w.openTabSwitcher(-1);
        switcher->commit();
        QCOMPARE(w.activeSide(), 1);
        QCOMPARE(w.sourceSide(), 1);
    }

    void listerRemembersTabOptions()
    {
        QTemporaryDir dir;
        const QString file = dir.path() + "/cargohatch.conf";
        {
            QSettings settings(file, QSettings::IniFormat);
            MainWindow w(dir.path(), &settings);
            TabOptions o;
            o.position = TabOptions::Position::Bottom;
            w.setTabOptions(o);
        }
        QSettings settings(file, QSettings::IniFormat);
        MainWindow w(dir.path(), &settings);
        QCOMPARE(w.tabOptions().position, TabOptions::Position::Bottom);
        QCOMPARE(w.display(0)->tabBar()->shape(), QTabBar::RoundedSouth);
    }

    // --- Windows: state, default, reopen, pairing (LST-01/04/20/21) -------

    void historyRoundTrips()
    {
        History h;
        for (auto p : {"/a", "/b", "/c"})
            h.visit(p);
        h.go(-1);
        const History back = History::fromEntries(h.entries(), h.index());
        QCOMPARE(back.current(), QString("/b"));
        QVERIFY(back.canGoBack() && back.canGoForward());

        const History clamped = History::fromEntries({"/a", "/b"}, 7);
        QCOMPARE(clamped.current(), QString("/b"));
        const History capped = History::fromEntries({"/a", "/b", "/c", "/d"}, 3, 2);
        QCOMPARE(capped.entries(), (QStringList{"/c", "/d"}));
        QCOMPARE(capped.current(), QString("/d"));
        QCOMPARE(History::fromEntries({}, 0).current(), QString());
    }

    void listerStateStartsInAFolder()
    {
        const ListerState empty = ListerState::startingIn(ListerState(), "/x");
        QCOMPARE(empty.tabCount(), 1);
        QCOMPARE(empty.activePath(), QString("/x"));

        ListerState state;
        History h;
        h.visit("/old");
        state.sides[1].tabs << ListerState::Tab::at("/other") << ListerState::Tab::at("/old", h);
        state.sides[1].current = 1;
        state.active = 0; // empty side: the folder goes where the tabs are
        const ListerState moved = ListerState::startingIn(state, "/new");
        QCOMPARE(moved.active, 1);
        QCOMPARE(moved.tabCount(), 2);
        QCOMPARE(moved.activePath(), QString("/new"));
        QCOMPARE(moved.sides[1].tabs.at(1).history.peek(-1), QString("/old"));
    }

    void listerStateRestoresTabsBothSides()
    {
        QTemporaryDir dir;
        for (auto name : {"a/deep", "b", "c"})
            QDir(dir.path()).mkpath(name);
        const QString root = canonical(dir.path());
        ListerState saved;
        {
            MainWindow w(root + "/a");
            w.activeTab()->openPath(root + "/a/deep");
            w.display(0)->addTab(root + "/b");
            w.display(0)->setCurrentIndex(0);
            w.setDual(true);
            w.display(1)->currentTab()->openPath(root + "/c");
            w.activateSide(1);
            w.setLayout(MainWindow::Layout::Stacked);
            saved = w.state();
        }
        QSettings settings(dir.path() + "/c.conf", QSettings::IniFormat);
        saved.save(settings, "lister");
        const ListerState loaded = ListerState::load(settings, "lister");

        MainWindow w(loaded);
        QVERIFY(w.isDual());
        QCOMPARE(w.layout(), MainWindow::Layout::Stacked);
        QCOMPARE(w.activeSide(), 1);
        QCOMPARE(w.display(0)->tabCount(), 2);
        QCOMPARE(w.display(0)->currentIndex(), 0);
        QCOMPARE(w.display(0)->currentTab()->currentPath(), root + "/a/deep");
        QVERIFY(w.display(0)->currentTab()->goBack()); // the history came too
        QCOMPARE(w.display(0)->currentTab()->currentPath(), root + "/a");
        QCOMPARE(w.display(0)->tab(1)->currentPath(), root + "/b");
        QCOMPARE(w.display(1)->currentTab()->currentPath(), root + "/c");
    }

    void listerStateSkipsFoldersThatAreGone()
    {
        QTemporaryDir dir;
        ListerState state;
        state.sides[0].tabs << ListerState::Tab::at(dir.path() + "/gone");
        MainWindow w(state);
        QCOMPARE(w.display(0)->tabCount(), 1);
        QCOMPARE(w.activeTab()->currentPath(), canonical(QDir::homePath()));
    }

    void managerOpensAndReopensWindows()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("a");
        QSettings settings(dir.path() + "/c.conf", QSettings::IniFormat);
        ListerManager manager(&settings);
        MainWindow *first = manager.openDefault(dir.path());
        QCOMPARE(manager.windows().size(), 1);
        QCOMPARE(first->activeTab()->currentPath(), canonical(dir.path()));

        MainWindow *second = manager.duplicateWindow(first);
        second->activeTab()->openPath(dir.path() + "/a");
        second->display(0)->addTab(dir.path());
        QCOMPARE(manager.windows().size(), 2);

        second->close();
        QCOMPARE(manager.windows().size(), 1);
        QCOMPARE(manager.closedWindows().size(), 1);
        QCOMPARE(manager.closedWindows().first().tabCount(), 2);
        // Closing remembered it as the default for new windows.
        QCOMPARE(manager.defaultState().tabCount(), 2);

        MainWindow *back = manager.reopenClosedWindow();
        QVERIFY(back);
        QCOMPARE(manager.closedWindows().size(), 0);
        QCOMPARE(back->display(0)->tabCount(), 2);
        QCOMPARE(back->display(0)->tab(0)->currentPath(), canonical(dir.path() + "/a"));

        // New windows open the default.
        MainWindow *fresh = manager.openDefault();
        QCOMPARE(fresh->display(0)->tabCount(), 2);
        QCOMPARE(manager.windows().size(), 3);
    }

    void managerForgetsWhenAsked()
    {
        QTemporaryDir dir;
        QSettings settings(dir.path() + "/c.conf", QSettings::IniFormat);
        ListerManager manager(&settings);
        manager.setRemembersOnClose(false);
        MainWindow *w = manager.openDefault(dir.path());
        w->display(0)->addTab(dir.path());
        w->close();
        QVERIFY(manager.defaultState().isEmpty());
        QVERIFY(manager.closedWindows().isEmpty()); // the last window isn't listed
        QVERIFY(!ListerManager(&settings).remembersOnClose());
    }

    void managerQuitsRememberingTheWindowQuitFrom()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("a");
        QSettings settings(dir.path() + "/c.conf", QSettings::IniFormat);
        ListerManager manager(&settings);
        MainWindow *a = manager.openDefault(dir.path() + "/a");
        manager.openDefault(dir.path());
        manager.quit(a);
        QVERIFY(manager.windows().isEmpty());
        QVERIFY(manager.closedWindows().isEmpty());
        QCOMPARE(manager.defaultState().activePath(), canonical(dir.path() + "/a"));
    }

    void managerSharesTabOptions()
    {
        QTemporaryDir dir;
        ListerManager manager;
        MainWindow *a = manager.openDefault(dir.path());
        MainWindow *b = manager.openDefault(dir.path());
        TabOptions o;
        o.position = TabOptions::Position::Bottom;
        a->setTabOptions(o);
        QCOMPARE(b->display(0)->tabBar()->shape(), QTabBar::RoundedSouth);
        QCOMPARE(b->tabOptions().position, TabOptions::Position::Bottom);
    }

    void managerPairsSingleWindows()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("b");
        ListerManager manager;
        MainWindow *a = manager.openDefault(dir.path());
        QCOMPARE(a->display(0)->role(), Role::None); // alone: no roles
        QCOMPARE(a->destinationTab(), nullptr);

        MainWindow *b = manager.openDefault(dir.path() + "/b");
        QCOMPARE(manager.sourceWindow(), b); // the newest is the source
        QCOMPARE(manager.destinationWindow(), a);
        QCOMPARE(b->display(0)->role(), Role::Source);
        QCOMPARE(a->display(0)->role(), Role::Destination);
        QCOMPARE(b->destinationTab(), a->activeTab());
        // The destination copies from the source into itself (IDEA-17).
        QCOMPARE(a->sourceTab(), b->activeTab());
        QCOMPARE(a->destinationTab(), a->activeTab());
        QCOMPARE(a->partnerTab(), b->activeTab());

        manager.windowActivated(a);
        QCOMPARE(manager.sourceWindow(), a);
        QCOMPARE(manager.destinationWindow(), b);

        // Locked as the source, a stays it when b is used.
        a->lockActiveSide(Role::Source);
        QVERIFY(a->display(0)->isRoleLocked());
        manager.windowActivated(b);
        QCOMPARE(manager.sourceWindow(), a);
        QCOMPARE(manager.destinationWindow(), b);
        a->lockActiveSide(Role::None);
        manager.windowActivated(b);
        QCOMPARE(manager.sourceWindow(), b);

        // A dual display is its own pair.
        b->setDual(true);
        QCOMPARE(a->display(0)->role(), Role::None);
        QCOMPARE(b->display(0)->role(), Role::Source); // its own sides
        b->setDual(false);
        QCOMPARE(a->display(0)->role(), Role::Destination);

        manager.setPairsWindows(false);
        QCOMPARE(a->display(0)->role(), Role::None);
        QCOMPARE(b->destinationTab(), nullptr);
    }

    void destinationWindowCopiesFromTheSource()
    {
        // IDEA-17: in the window locked as the destination, F5 takes what the
        // source window has selected.
        QTemporaryDir dir;
        const QString base = canonical(dir.path());
        QDir().mkpath(base + "/here");
        QDir().mkpath(base + "/there");
        write(base + "/there/t.txt", "t");
        ListerManager manager;
        MainWindow *dest = manager.openDefault(base + "/here");
        MainWindow *source = manager.openDefault(base + "/there");
        dest->lockActiveSide(Role::Destination);
        manager.windowActivated(dest);
        QCOMPARE(manager.destinationWindow(), dest);
        QAction *copy = nullptr;
        for (QAction *a : dest->findChildren<QAction *>())
            if (a->shortcut() == QKeySequence(Qt::Key_F5))
                copy = a;
        QVERIFY(copy);
        QTRY_COMPARE(source->activeTab()->itemCount(), 1);
        QVERIFY(!copy->isEnabled());
        // Selecting in the source window is heard of in the destination.
        source->activeTab()->selectNames({"t.txt"});
        QVERIFY(copy->isEnabled());
        QCOMPARE(copy->text(), QString("Copy from Source Window"));
        OperationRunner *runner = dest->copySelection(false);
        QVERIFY(runner);
        QSignalSpy done(runner, &OperationRunner::finished);
        QVERIFY(done.wait());
        QCOMPARE(read(base + "/here/t.txt"), QByteArray("t"));
        // The source window still copies to the destination as before.
        QCOMPARE(source->destinationTab(), dest->activeTab());
    }

    // --- The other side and the split (LST-06, LST-07) ------------------

    void listerSwapsAndCopiesFolders()
    {
        QTemporaryDir dir;
        for (auto name : {"a", "b"})
            QDir(dir.path()).mkpath(name);
        const QString root = canonical(dir.path());
        MainWindow w(root + "/a");
        w.swapFolders(); // nothing to swap with
        QCOMPARE(w.activeTab()->currentPath(), root + "/a");

        // With no other side, showing the folder there turns dual on.
        w.showFolderOnOtherSide();
        QVERIFY(w.isDual());
        QCOMPARE(w.partnerTab()->currentPath(), root + "/a");

        w.partnerTab()->openPath(root + "/b");
        w.swapFolders();
        QCOMPARE(w.activeTab()->currentPath(), root + "/b");
        QCOMPARE(w.partnerTab()->currentPath(), root + "/a");
        QVERIFY(w.activeTab()->goBack()); // swapping is ordinary navigation
    }

    void listerOpensTheHighlightedFolderOnTheOtherSide()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("inner");
        write(dir.path() + "/file.txt", "x");
        MainWindow w(dir.path());
        FolderTab *tab = w.activeTab();
        QTRY_VERIFY(tab->itemCount() == 2);

        // Nothing highlighted: it says so instead of doing nothing.
        w.openInOtherSide();
        QVERIFY(!w.isDual());
        QVERIFY(tab->statusText().contains("folder"));

        auto *model = tab->fileView()->model();
        const QModelIndex root = tab->fileView()->rootIndex();
        for (int row = 0; row < model->rowCount(root); ++row)
            if (model->index(row, 0, root).data().toString() == "inner")
                tab->fileView()->setCurrentIndex(model->index(row, 0, root));
        w.openInOtherSide();
        QVERIFY(w.isDual());
        QCOMPARE(w.partnerTab()->currentPath(), canonical(dir.path() + "/inner"));
        QCOMPARE(w.activeTab(), tab); // the user stays where they were
    }

    void listerSizesTheSplit()
    {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        w.resize(1000, 600);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.setSplitRatio(0.7); // single display: nothing to split
        w.setDual(true);
        QTRY_VERIFY(qAbs(w.splitRatio() - 0.5) < 0.02);
        w.setSplitRatio(0.7);
        QVERIFY(qAbs(w.splitRatio() - 0.7) < 0.02);
        w.setSplitRatio(0.99);
        // Never squeezed away: at most 90%, less if the other side needs
        // more room.
        QVERIFY(w.splitRatio() > 0.7 && w.splitRatio() < 0.92);

        // Widening works from whichever side is active.
        w.setSplitRatio(0.5);
        w.activateSide(1);
        QTest::keyClick(&w, Qt::Key_BracketRight, Qt::AltModifier);
        QTRY_VERIFY(qAbs(w.splitRatio() - 0.4) < 0.02);
    }

    // --- Tab names, locks, home, colours, links (TAB-08/09/10/11/15/18) ---

    void tabLabelReplacesTheFolderName()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkdir("src");
        FileDisplay display;
        FolderTab *tab = display.addTab(dir.path() + "/src");
        tab->setLabel("  Code & stuff ");
        QCOMPARE(tab->label(), QString("Code & stuff"));
        QCOMPARE(display.tabText(0), QString("Code && stuff"));
        tab->setLabel("");
        QCOMPARE(display.tabText(0), QString("src"));
    }

    void lockedTabOpensOtherFoldersInNewTabs()
    {
        QTemporaryDir dir;
        for (auto name : {"a/inner", "b", "c"})
            QDir(dir.path()).mkpath(name);
        const QString root = canonical(dir.path());
        FileDisplay display;
        FolderTab *locked = display.addTab(root + "/a");
        display.addTab(root); // something after it, to see where new tabs go
        display.setCurrentIndex(0);
        locked->setLock(FolderTab::Lock::Locked);
        QCOMPARE(display.tabBar()->tabIcon(0).isNull(), false); // the padlock

        QVERIFY(!locked->openPath(root + "/b"));
        QCOMPARE(locked->currentPath(), root + "/a");
        QCOMPARE(display.tabCount(), 3);
        QCOMPARE(display.tab(1)->currentPath(), root + "/b"); // right beside it
        QCOMPARE(display.currentIndex(), 1);

        locked->goUp();
        QCOMPARE(locked->currentPath(), root + "/a");
        QCOMPARE(display.tabCount(), 4);
        QVERIFY(locked->openPath(root + "/a") || locked->currentPath() == root + "/a");

        // Locked tabs can't be closed until unlocked.
        QVERIFY(!display.closeTab(0));
        QCOMPARE(display.tab(0), locked);
        QVERIFY(locked->statusText().contains("locked"));
        locked->setLock(FolderTab::Lock::None);
        QVERIFY(display.closeTab(0));
    }

    void reuseLockSendsFoldersToOneTab()
    {
        QTemporaryDir dir;
        for (auto name : {"a", "b", "c"})
            QDir(dir.path()).mkpath(name);
        const QString root = canonical(dir.path());
        FileDisplay display;
        FolderTab *locked = display.addTab(root + "/a");
        locked->setLock(FolderTab::Lock::Reuse);
        locked->openPath(root + "/b");
        locked->openPath(root + "/c");
        QCOMPARE(display.tabCount(), 2);
        QCOMPARE(display.tab(1)->currentPath(), root + "/c");
        QVERIFY(display.tab(1)->goBack()); // the reused tab keeps a history
        QCOMPARE(display.tab(1)->currentPath(), root + "/b");
    }

    void allowChangesLockBrowsesAndReturns()
    {
        QTemporaryDir dir;
        for (auto name : {"a", "b"})
            QDir(dir.path()).mkpath(name);
        const QString root = canonical(dir.path());
        FileDisplay display;
        FolderTab *tab = display.addTab(root + "/a");
        display.addTab(root);
        tab->setLock(FolderTab::Lock::AllowChanges);
        QVERIFY(tab->openPath(root + "/b"));
        QCOMPARE(display.tabCount(), 2);
        QCOMPARE(tab->lockedPath(), root + "/a");
        tab->returnToLockedFolder();
        QCOMPARE(tab->currentPath(), root + "/a");
        QVERIFY(!display.closeTab(display.indexOf(tab)));
    }

    void newTabsCanSwitchToAnOpenTab()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("a");
        FileDisplay display;
        FolderTab *a = display.addTab(dir.path() + "/a");
        display.addTab(dir.path());
        display.openInNewTab(dir.path() + "/a");
        QCOMPARE(display.tabCount(), 3); // off by default

        TabOptions o;
        o.findExistingTab = true;
        display.setOptions(o);
        QCOMPARE(display.openInNewTab(dir.path() + "/a"), a);
        QCOMPARE(display.tabCount(), 3);
        QCOMPARE(display.currentTab(), a);
    }

    void tabGoesHome()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("a/b");
        const QString root = canonical(dir.path());
        FolderTab tab(root + "/a");
        QCOMPARE(tab.homePath(), root + "/a"); // the first folder it opened
        tab.openPath(root + "/a/b");
        tab.openPath(root);
        tab.goHome();
        QCOMPARE(tab.currentPath(), root + "/a");
        tab.setHomePath(root + "/a/b");
        tab.goHome();
        QCOMPARE(tab.currentPath(), root + "/a/b");
    }

    void colorRulesMatchFoldersAndBelow()
    {
        const TabOptions::ColorRule exact{"/home/me/Work", "blue"};
        QVERIFY(exact.matches("/home/me/Work"));
        QVERIFY(exact.matches("/home/me/Work/cargo/src"));
        QVERIFY(!exact.matches("/home/me/Workshop"));
        const TabOptions::ColorRule wild{"/mnt/*/Photos", "green"};
        QVERIFY(wild.matches("/mnt/usb/Photos"));
        QVERIFY(wild.matches("/mnt/usb/Photos/2026"));
        QVERIFY(!wild.matches("/mnt/usb/Music"));
        QVERIFY((TabOptions::ColorRule{"/", "red"}.matches("/etc")));

        TabOptions o;
        o.setColorRule("/a", "red");
        o.setColorRule("/a/b", "green"); // newest first, so the deeper rule wins
        QCOMPARE(o.colorFor("/a/b/c"), QString("green"));
        QCOMPARE(o.colorFor("/a/x"), QString("red"));
        o.setColorRule("/a/b", "");
        QCOMPARE(o.colorFor("/a/b/c"), QString("red"));
        QCOMPARE(o.colorRules.size(), 1);

        QTemporaryDir dir;
        QSettings settings(dir.path() + "/c.conf", QSettings::IniFormat);
        o.save(settings);
        QCOMPARE(TabOptions::load(settings).colorFor("/a/q"), QString("red"));
    }

    void tabColorsComeFromTheTabOrItsFolder()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("work/deep");
        const QString root = canonical(dir.path());
        FileDisplay display;
        FolderTab *tab = display.addTab(root);
        QVERIFY(!display.tabBar()->tabColor(0).isValid());

        TabOptions o;
        o.setColorRule(root + "/work", "green");
        display.setOptions(o);
        tab->openPath(root + "/work/deep");
        QVERIFY(display.tabBar()->tabColor(0).isValid());
        const QColor ruled = display.tabBar()->tabColor(0);

        tab->setColorName("red"); // the tab's own colour wins
        QVERIFY(display.tabBar()->tabColor(0) != ruled);
        tab->setColorName("");
        tab->openPath(root);
        QVERIFY(!display.tabBar()->tabColor(0).isValid());
    }

    void linkedTabsShowTogether()
    {
        QTemporaryDir dir;
        for (auto name : {"l1", "l2", "r1", "r2"})
            QDir(dir.path()).mkpath(name);
        MainWindow w(dir.path() + "/l1");
        w.display(0)->addTab(dir.path() + "/l2");
        w.setDual(true);
        w.display(1)->currentTab()->openPath(dir.path() + "/r1");
        w.display(1)->addTab(dir.path() + "/r2");
        // l1 with r1.
        w.display(0)->setCurrentIndex(0);
        w.display(1)->setCurrentIndex(0);
        w.toggleLink(0, 0);
        FolderTab *l1 = w.display(0)->tab(0);
        FolderTab *r1 = w.display(1)->tab(0);
        QCOMPARE(l1->linkedTab(), r1);
        QCOMPARE(r1->linkedTab(), l1);

        w.display(1)->setCurrentIndex(1);
        w.display(0)->setCurrentIndex(1);
        w.display(0)->setCurrentIndex(0); // choosing l1 brings r1 forward
        QCOMPARE(w.display(1)->currentTab(), r1);

        // Saved and restored with the window.
        MainWindow back(w.state());
        QCOMPARE(back.display(0)->tab(0)->linkedTab(), back.display(1)->tab(0));

        // Moving a tab to the same side as its partner unlinks them.
        w.moveTabToOtherSide(0, 0);
        QCOMPARE(r1->linkedTab(), nullptr);
    }

    void tabDetailsSurviveSavingAndReopening()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("a/b");
        const QString root = canonical(dir.path());
        MainWindow w(root + "/a");
        FolderTab *tab = w.activeTab();
        tab->setLabel("Alpha");
        tab->setColorName("cyan");
        tab->setLock(FolderTab::Lock::AllowChanges);
        tab->openPath(root + "/a/b");
        tab->setHomePath(root);

        QSettings settings(dir.path() + "/c.conf", QSettings::IniFormat);
        w.state().save(settings, "lister");
        MainWindow back(ListerState::load(settings, "lister"));
        FolderTab *t = back.activeTab();
        QCOMPARE(t->label(), QString("Alpha"));
        QCOMPARE(t->colorName(), QString("cyan"));
        QCOMPARE(t->lock(), FolderTab::Lock::AllowChanges);
        QCOMPARE(t->lockedPath(), root + "/a");
        QCOMPARE(t->currentPath(), root + "/a/b");
        QCOMPARE(t->homePath(), root);

        // A reopened tab keeps them too.
        w.display(0)->addTab(root);
        FolderTab *other = w.display(0)->addTab(root);
        other->setLabel("Beta");
        w.display(0)->closeTab(w.display(0)->indexOf(other));
        QCOMPARE(w.reopenClosedTab()->label(), QString("Beta"));
    }

    // --- Tab groups and layouts (TAB-13, LST-19) -------------------------

    void tabGroupsSaveAndOpen()
    {
        QTemporaryDir dir;
        for (auto name : {"a", "b", "c", "keep"})
            QDir(dir.path()).mkpath(name);
        const QString root = canonical(dir.path());
        QSettings settings(dir.path() + "/c.conf", QSettings::IniFormat);

        MainWindow w(root + "/a");
        w.display(0)->addTab(root + "/b");
        w.setDual(true);
        w.display(1)->currentTab()->openPath(root + "/c");
        SavedSets::saveTabGroup(settings, "Work / 2026", w.tabGroup());
        SavedSets::saveTabGroup(settings, "alpha", w.tabGroup());
        QCOMPARE(SavedSets::tabGroupNames(settings), (QStringList{"alpha", "Work / 2026"}));

        MainWindow v(root + "/keep");
        v.activeTab()->setLock(FolderTab::Lock::Locked);
        v.display(0)->addTab(root);
        v.openTabGroup(SavedSets::loadTabGroup(settings, "Work / 2026"), true);
        QVERIFY(v.isDual());
        // The locked tab stays; the other one made way for the group's.
        QCOMPARE(v.display(0)->tabCount(), 3);
        QCOMPARE(v.display(0)->tab(0)->currentPath(), root + "/keep");
        QCOMPARE(v.display(0)->tab(1)->currentPath(), root + "/a");
        QCOMPARE(v.display(0)->tab(2)->currentPath(), root + "/b");
        QCOMPARE(v.display(1)->tabCount(), 1);
        QCOMPARE(v.display(1)->currentTab()->currentPath(), root + "/c");

        v.openTabGroup(SavedSets::loadTabGroup(settings, "alpha"), false);
        QCOMPARE(v.display(0)->tabCount(), 5);

        SavedSets::removeTabGroup(settings, "alpha");
        QCOMPARE(SavedSets::tabGroupNames(settings), QStringList{"Work / 2026"});
    }

    void layoutsSaveAndReopenEveryWindow()
    {
        QTemporaryDir dir;
        for (auto name : {"a", "b", "c"})
            QDir(dir.path()).mkpath(name);
        const QString root = canonical(dir.path());
        QSettings settings(dir.path() + "/c.conf", QSettings::IniFormat);
        ListerManager manager(&settings);
        manager.openDefault(root + "/a");
        MainWindow *b = manager.openDefault(root + "/b");
        b->setDual(true);
        manager.saveLayout("Two windows");
        QCOMPARE(SavedSets::layoutNames(settings), QStringList{"Two windows"});

        MainWindow *c = manager.openDefault(root + "/c");
        Q_UNUSED(c);
        QCOMPARE(manager.windows().size(), 3);
        QVERIFY(manager.openLayout("Two windows"));
        const QList<MainWindow *> now = manager.windows();
        QCOMPARE(now.size(), 2);
        QCOMPARE(now.at(0)->activeTab()->currentPath(), root + "/a");
        QVERIFY(now.at(1)->isDual());
        QCOMPARE(manager.closedWindows().size(), 3); // the replaced ones
        QVERIFY(!manager.openLayout("missing"));
    }

    // --- File model, view modes, columns and sorting (VIEW-01/02, SORT-01..03)

    void viewFormatRoundTrips()
    {
        ViewFormat f;
        f.mode = ViewFormat::Mode::Tiles;
        f.columns = {{Column::Name, 250}, {Column::Modified, 140}, {Column::Extension, 60}};
        f.sortColumn = Column::Modified;
        f.reverse = true;
        f.folderOrder = ViewFormat::FolderOrder::Mixed;
        const ViewFormat back = ViewFormat::fromString(f.toString());
        QCOMPARE(back.toString(), f.toString());
        QCOMPARE(back.columns.at(2).column, Column::Extension);

        // Rubbish is ignored, a missing Name column comes back first, and
        // a column listed twice counts once.
        const ViewFormat odd = ViewFormat::fromString("mode=nope;columns=size:x,bogus:5,size:70;sort=zzz;junk");
        QCOMPARE(odd.mode, ViewFormat::Mode::Details);
        QCOMPARE(odd.columns.size(), 2);
        QCOMPARE(odd.columns.at(0).column, Column::Name);
        QCOMPARE(odd.columns.at(1).width, Columns::defaultWidth(Column::Size));
        QCOMPARE(odd.sortColumn, Column::Name);
    }

    void fileModelReadsAFolder()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkdir("sub");
        write(dir.path() + "/notes.txt", QByteArray(2048, 'x'));
        write(dir.path() + "/.hidden", "h");
        write(dir.path() + "/archive.tar.gz", "z");
        QFile::link(dir.path() + "/notes.txt", dir.path() + "/shortcut");

        FileModel model;
        QSignalSpy loaded(&model, &FileModel::loaded);
        model.setFolder(dir.path());
        QVERIFY(model.isLoading());
        QTRY_COMPARE(loaded.count(), 1);
        QCOMPARE(model.rowCount(), 5);

        const int notes = model.rowOf("notes.txt");
        QCOMPARE(model.entry(notes).size, 2048);
        QCOMPARE(model.text(notes, Column::Extension), QString("txt"));
        QCOMPARE(model.entry(notes).mimeName, QString("text/plain"));
        QVERIFY(!model.text(notes, Column::Size).isEmpty());
        QVERIFY(model.text(notes, Column::Permissions).startsWith("-rw"));
        QVERIFY(!model.text(notes, Column::Owner).isEmpty());
        QCOMPARE(model.text(notes, Column::FullPath), dir.path() + "/notes.txt");

        const int sub = model.rowOf("sub");
        QVERIFY(model.entry(sub).isDir);
        QCOMPARE(model.text(sub, Column::Size), QString());
        QVERIFY(model.text(sub, Column::Permissions).startsWith('d'));
        QCOMPARE(model.text(model.rowOf("archive.tar.gz"), Column::Extension), QString("gz"));
        QCOMPARE(model.entry(model.rowOf(".hidden")).extension(), QString()); // a dot file has no extension
        QVERIFY(model.entry(model.rowOf(".hidden")).isHidden);
        const int link = model.rowOf("shortcut");
        QVERIFY(model.entry(link).isLink);
        QCOMPARE(model.text(link, Column::LinkTarget), dir.path() + "/notes.txt");
        QCOMPARE(model.headerData(int(Column::Size), Qt::Horizontal).toString(), QString("Size"));
    }

    void fileModelMergesChangesKeepingRows()
    {
        QTemporaryDir dir;
        write(dir.path() + "/keep", "1");
        write(dir.path() + "/change", "1");
        write(dir.path() + "/gone", "1");
        FileModel model;
        QSignalSpy loaded(&model, &FileModel::loaded);
        model.setFolder(dir.path());
        QTRY_COMPARE(loaded.count(), 1);
        const QPersistentModelIndex keep = model.index(model.rowOf("keep"), 0);

        QFile::remove(dir.path() + "/gone");
        write(dir.path() + "/change", "grown");
        write(dir.path() + "/new", "1");
        model.refresh();
        QTRY_COMPARE(model.rowOf("new") >= 0 && model.rowOf("gone") < 0, true);
        QCOMPARE(model.rowCount(), 3);
        QVERIFY(keep.isValid()); // an unchanged item keeps its place
        QCOMPARE(keep.data().toString(), QString("keep"));
        QCOMPARE(model.entry(model.rowOf("change")).size, 5);
        QCOMPARE(loaded.count(), 1); // merged, not reloaded
    }

    void fileModelNoticesChangesOnDisk()
    {
        QTemporaryDir dir;
        FileModel model;
        QSignalSpy loaded(&model, &FileModel::loaded);
        model.setFolder(dir.path());
        QTRY_COMPARE(loaded.count(), 1);
        write(dir.path() + "/appeared", "1");
        QTRY_COMPARE(model.rowCount(), 1);
    }

    void sortingFollowsTheRules()
    {
        QTemporaryDir dir;
        for (auto name : {"file10", "file2", "File1", "zeta", "alpha"})
            write(dir.path() + "/" + name, QByteArray(QByteArray(name).size() * 100, 'x'));
        QDir(dir.path()).mkdir("Beta");
        QDir(dir.path()).mkdir("omega");
        FileModel model;
        FileSortProxy proxy(&model);
        QSignalSpy loaded(&model, &FileModel::loaded);
        model.setFolder(dir.path());
        QTRY_COMPARE(loaded.count(), 1);
        auto order = [&] {
            QStringList names;
            for (int r = 0; r < proxy.rowCount(); ++r)
                names << proxy.index(r, 0).data().toString();
            return names;
        };

        proxy.sort(int(Column::Name), Qt::AscendingOrder);
        QCOMPARE(order(), (QStringList{"Beta", "omega", "alpha", "File1", "file2", "file10", "zeta"}));

        // Reversed, folders still come first.
        proxy.sort(int(Column::Name), Qt::DescendingOrder);
        QCOMPARE(order(), (QStringList{"omega", "Beta", "zeta", "file10", "file2", "File1", "alpha"}));

        proxy.setFolderOrder(ViewFormat::FolderOrder::FilesFirst);
        proxy.sort(int(Column::Name), Qt::AscendingOrder);
        QCOMPARE(order().last(), QString("omega"));
        proxy.setFolderOrder(ViewFormat::FolderOrder::Mixed);
        QCOMPARE(order(), (QStringList{"alpha", "Beta", "File1", "file2", "file10", "omega", "zeta"}));

        // Without natural numbers, 10 comes before 2.
        FileModel::SortOptions plain;
        plain.natural = false;
        model.setSortOptions(plain);
        proxy.resort();
        QVERIFY(order().indexOf("file10") < order().indexOf("file2"));

        // By size, biggest first, folders kept in name order if asked.
        model.setSortOptions(FileModel::SortOptions());
        proxy.setFolderOrder(ViewFormat::FolderOrder::FoldersFirst);
        proxy.setKeepFoldersAlphabetical(true);
        proxy.sort(int(Column::Size), Qt::DescendingOrder);
        QCOMPARE(order().mid(0, 2), (QStringList{"Beta", "omega"}));
        QCOMPARE(order().mid(2, 3), (QStringList{"file10", "alpha", "File1"}));
    }

    void wordSortAndExtensionsSeparately()
    {
        QTemporaryDir dir;
        for (auto name : {"co-op", "cook", "a-b.c", "a.c"})
            write(dir.path() + "/" + name, "x");
        FileModel model;
        FileSortProxy proxy(&model);
        QSignalSpy loaded(&model, &FileModel::loaded);
        model.setFolder(dir.path());
        QTRY_COMPARE(loaded.count(), 1);
        auto before = [&](const char *a, const char *b) {
            return model.compareNames(model.rowOf(a), model.rowOf(b)) < 0;
        };
        QVERIFY(before("co-op", "cook")); // the hyphen counts
        FileModel::SortOptions word;
        word.wordSort = true;
        model.setSortOptions(word);
        QVERIFY(before("cook", "co-op")); // read as "coop"

        QVERIFY(before("a-b.c", "a.c") != before("a.c", "a-b.c"));
        const bool wholeName = before("a-b.c", "a.c");
        FileModel::SortOptions separate;
        separate.extensionsSeparately = true;
        model.setSortOptions(separate);
        QVERIFY(before("a.c", "a-b.c")); // "a" before "a-b"
        Q_UNUSED(wholeName);
    }

    void headingClicksSortSmartly()
    {
        QTemporaryDir dir;
        FolderTab tab(dir.path());
        QCOMPARE(tab.format().sortColumn, Column::Name);
        tab.sortByHeading(Column::Size);
        QVERIFY(tab.format().reverse); // biggest first
        QCOMPARE(tab.detailsView()->header()->sortIndicatorOrder(), Qt::DescendingOrder);
        tab.sortByHeading(Column::Size);
        QVERIFY(!tab.format().reverse);
        tab.sortByHeading(Column::Name);
        QVERIFY(!tab.format().reverse); // A to Z first
        tab.sortByHeading(Column::Name);
        QVERIFY(tab.format().reverse);
        // Clicking the real heading does the same.
        QSignalSpy changed(&tab, &FolderTab::formatChanged);
        emit tab.detailsView()->header()->sectionClicked(int(Column::Modified));
        QCOMPARE(tab.format().sortColumn, Column::Modified);
        QVERIFY(tab.format().reverse);
        QVERIFY(changed.count() > 0);
    }

    void columnsCanBeShownMovedAndRestored()
    {
        QTemporaryDir dir;
        FolderTab tab(dir.path());
        QHeaderView *header = tab.detailsView()->header();
        QVERIFY(header->isSectionHidden(int(Column::Extension)));
        tab.setColumnVisible(Column::Extension, true);
        QVERIFY(!header->isSectionHidden(int(Column::Extension)));
        QVERIFY(tab.format().hasColumn(Column::Extension));
        tab.setColumnVisible(Column::Name, false); // Name always stays
        QVERIFY(!header->isSectionHidden(int(Column::Name)));

        ViewFormat f;
        f.columns = {{Column::Name, 200}, {Column::Owner, 77}, {Column::Size, 66}};
        tab.setFormat(f);
        const ViewFormat now = tab.format();
        QCOMPARE(now.columns.size(), 3);
        QCOMPARE(now.columns.at(1).column, Column::Owner);
        QCOMPARE(now.columns.at(1).width, 77);
        QCOMPARE(now.columns.at(2).column, Column::Size);
        QVERIFY(header->isSectionHidden(int(Column::Extension)));
    }

    void viewModesShareTheSelection()
    {
        QTemporaryDir dir;
        for (auto name : {"a", "b", "c"})
            write(dir.path() + "/" + name, "x");
        FolderTab tab(dir.path());
        QTRY_COMPARE(tab.itemCount(), 3);
        QCOMPARE(tab.fileView(), static_cast<QAbstractItemView *>(tab.detailsView()));
        tab.fileView()->setCurrentIndex(tab.proxy()->index(1, 0));

        const std::pair<ViewFormat::Mode, QListView::ViewMode> modes[] = {
            {ViewFormat::Mode::LargeIcons, QListView::IconMode}, {ViewFormat::Mode::SmallIcons, QListView::ListMode},
            {ViewFormat::Mode::List, QListView::ListMode},       {ViewFormat::Mode::Thumbnails, QListView::IconMode},
            {ViewFormat::Mode::Tiles, QListView::ListMode}};
        for (const auto &[mode, listMode] : modes) {
            tab.setViewMode(mode);
            QCOMPARE(tab.fileView(), static_cast<QAbstractItemView *>(tab.iconView()));
            QCOMPARE(tab.iconView()->viewMode(), listMode);
            QCOMPARE(tab.fileView()->currentIndex().data().toString(), QString("b"));
        }
        QCOMPARE(tab.iconView()->flow(), QListView::LeftToRight);
        tab.setViewMode(ViewFormat::Mode::List);
        QCOMPARE(tab.iconView()->flow(), QListView::TopToBottom);
        tab.setViewMode(ViewFormat::Mode::Power);
        QCOMPARE(tab.detailsView()->selectionMode(), QAbstractItemView::MultiSelection);
        tab.setViewMode(ViewFormat::Mode::Details);
        QCOMPARE(tab.detailsView()->selectionMode(), QAbstractItemView::ExtendedSelection);
        QCOMPARE(tab.fileView()->currentIndex().data().toString(), QString("b"));

        // Every mode draws without trouble.
        tab.resize(600, 400);
        for (int m = 0; m < int(ViewFormat::Mode::Count); ++m) {
            tab.setViewMode(ViewFormat::Mode(m));
            QVERIFY(!tab.grab().isNull());
        }
    }

    void thumbnailsShowImages()
    {
        QTemporaryDir dir;
        QImage image(400, 200, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(dir.path() + "/red.png"));
        FolderTab tab(dir.path());
        QTRY_COMPARE(tab.itemCount(), 1);
        const QModelIndex index = tab.model()->index(0, 0);
        const QIcon plain = index.data(Qt::DecorationRole).value<QIcon>();
        tab.setViewMode(ViewFormat::Mode::Thumbnails);
        index.data(Qt::DecorationRole); // asks for the thumbnail
        QTRY_VERIFY(index.data(Qt::DecorationRole).value<QIcon>().cacheKey() != plain.cacheKey());
        const QIcon thumbnail = index.data(Qt::DecorationRole).value<QIcon>();
        QCOMPARE(thumbnail.availableSizes().value(0), QSize(128, 128));
        // Fitted inside keeping its shape (128x64), centred on the square.
        const QImage shown = thumbnail.pixmap(128, 128).toImage();
        QCOMPARE(shown.pixelColor(64, 64), QColor(Qt::red));
        QCOMPARE(shown.pixelColor(64, 10).alpha(), 0);
    }

    void tabFormatsAreKeptAndDefaulted()
    {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        FolderTab *tab = w.activeTab();
        tab->setViewMode(ViewFormat::Mode::Tiles);
        tab->sortByHeading(Column::Modified);

        // Saved with the window, copied by duplicating.
        MainWindow back(w.state());
        QCOMPARE(back.activeTab()->format().mode, ViewFormat::Mode::Tiles);
        QCOMPARE(back.activeTab()->format().sortColumn, Column::Modified);
        FolderTab *copy = w.display(0)->duplicateTab(0);
        QCOMPARE(copy->format().mode, ViewFormat::Mode::Tiles);

        // New tabs start from the default format.
        QCOMPARE(w.display(0)->addTab(dir.path())->format().mode, ViewFormat::Mode::Details);
        TabOptions o = w.tabOptions();
        o.defaultFormat = tab->format().toString();
        w.setTabOptions(o);
        QCOMPARE(w.display(0)->addTab(dir.path())->format().mode, ViewFormat::Mode::Tiles);
    }

    // --- File operations (COPY-01, COPY-02, DEL-01) -----------------------

    void freeNamesKeepTheExtension()
    {
        QTemporaryDir dir;
        write(dir.path() + "/photo.jpg", "x");
        write(dir.path() + "/photo (2).jpg", "x");
        QCOMPARE(FileOperation::freeName(dir.path(), "photo.jpg"), QString("photo (3).jpg"));
        QCOMPARE(FileOperation::freeName(dir.path(), ".bashrc"), QString(".bashrc (2)"));
        QCOMPARE(FileOperation::freeName(dir.path(), "notes"), QString("notes (2)"));
    }

    void copyKeepsTreesLinksAndTimes()
    {
        QTemporaryDir dir;
        const QString src = dir.path() + "/src";
        QDir().mkpath(src + "/sub/deeper");
        write(src + "/a.txt", "alpha");
        write(src + "/sub/deeper/b.txt", "bravo");
        QVERIFY(QFile::link("a.txt", src + "/link"));
        const QDateTime old = QDateTime::currentDateTime().addDays(-3);
        {
            QFile f(src + "/a.txt");
            QVERIFY(f.open(QIODevice::ReadWrite));
            QVERIFY(f.setFileTime(old, QFileDevice::FileModificationTime));
        }

        // The destination doesn't exist yet and is created.
        const QString dest = dir.path() + "/out/new";
        FileOperation op(FileOperation::Kind::Copy, {src}, dest);
        QSignalSpy progress(&op, &FileOperation::progress);
        op.run();
        QVERIFY2(op.errors().isEmpty(), qPrintable(op.errors().join('\n')));
        QCOMPARE(op.created(), QStringList{dest + "/src"});
        QCOMPARE(read(dest + "/src/a.txt"), QByteArray("alpha"));
        QCOMPARE(read(dest + "/src/sub/deeper/b.txt"), QByteArray("bravo"));
        QCOMPARE(QFileInfo(dest + "/src/link").symLinkTarget(), dest + "/src/a.txt");
        QCOMPARE(QFile::symLinkTarget(dest + "/src/link"), QFileInfo(dest + "/src/a.txt").absoluteFilePath());
        QCOMPARE(QFileInfo(dest + "/src/a.txt").lastModified().toSecsSinceEpoch(), old.toSecsSinceEpoch());
        QVERIFY(QFileInfo::exists(src + "/a.txt"));
        QVERIFY(!progress.isEmpty());
        QCOMPARE(progress.last().at(0).toLongLong(), progress.last().at(1).toLongLong());
        QCOMPARE(progress.last().at(1).toLongLong(), qint64(10));
    }

    void moveRenamesAndRefusesItself()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.path() + "/box/inner");
        write(dir.path() + "/box/inner/c.txt", "charlie");
        QDir().mkpath(dir.path() + "/there");

        FileOperation op(FileOperation::Kind::Move, {dir.path() + "/box"}, dir.path() + "/there");
        op.run();
        QVERIFY(op.errors().isEmpty());
        QVERIFY(!QFileInfo::exists(dir.path() + "/box"));
        QCOMPARE(read(dir.path() + "/there/box/inner/c.txt"), QByteArray("charlie"));

        // A folder can't go inside itself.
        FileOperation into(FileOperation::Kind::Copy, {dir.path() + "/there"}, dir.path() + "/there/box");
        into.run();
        QCOMPARE(into.errors().size(), 1);
        QVERIFY(!QFileInfo::exists(dir.path() + "/there/box/there"));
    }

    void duplicateCopiesBesideTheOriginal()
    {
        QTemporaryDir dir;
        write(dir.path() + "/report.txt", "r");
        FileOperation op(FileOperation::Kind::Copy, {dir.path() + "/report.txt"}, dir.path());
        op.setDuplicate(true);
        op.run();
        QVERIFY(op.errors().isEmpty());
        QCOMPARE(read(dir.path() + "/report (2).txt"), QByteArray("r"));
    }

    void whenExistsPolicies()
    {
        QTemporaryDir dir;
        const QString in = dir.path() + "/in", out = dir.path() + "/out";
        QDir().mkpath(in);
        auto reset = [&] {
            QDir(out).removeRecursively();
            QDir().mkpath(out);
            write(in + "/f.txt", "new contents");
            write(out + "/f.txt", "old");
        };
        auto copy = [&](WhenExists policy) {
            FileOperation op(FileOperation::Kind::Copy, {in + "/f.txt"}, out);
            op.setWhenExists(policy);
            op.run();
            return op.errors();
        };

        reset();
        QVERIFY(copy(WhenExists::Skip).isEmpty());
        QCOMPARE(read(out + "/f.txt"), QByteArray("old"));

        reset();
        QVERIFY(copy(WhenExists::Replace).isEmpty());
        QCOMPARE(read(out + "/f.txt"), QByteArray("new contents"));

        reset();
        QVERIFY(copy(WhenExists::RenameNew).isEmpty());
        QCOMPARE(read(out + "/f.txt"), QByteArray("old"));
        QCOMPARE(read(out + "/f (2).txt"), QByteArray("new contents"));

        reset();
        QVERIFY(copy(WhenExists::RenameOld).isEmpty());
        QCOMPARE(read(out + "/f.txt"), QByteArray("new contents"));
        QCOMPARE(read(out + "/f (2).txt"), QByteArray("old"));

        // Newer only: the existing file is made newer, then older.
        reset();
        setModified(out + "/f.txt", QDateTime::currentDateTime().addDays(1));
        QVERIFY(copy(WhenExists::KeepNewer).isEmpty());
        QCOMPARE(read(out + "/f.txt"), QByteArray("old"));
        setModified(out + "/f.txt", QDateTime::currentDateTime().addDays(-1));
        QVERIFY(copy(WhenExists::KeepNewer).isEmpty());
        QCOMPARE(read(out + "/f.txt"), QByteArray("new contents"));

        // Resume carries on from the part already there.
        reset();
        write(out + "/f.txt", "new con");
        QVERIFY(copy(WhenExists::Resume).isEmpty());
        QCOMPARE(read(out + "/f.txt"), QByteArray("new contents"));

        // Ask goes to the resolver; "for all" stops it asking again.
        reset();
        write(in + "/g.txt", "gee");
        write(out + "/g.txt", "old gee");
        int asked = 0;
        FileOperation op(FileOperation::Kind::Copy, {in + "/f.txt", in + "/g.txt"}, out);
        op.setResolver([&](const FileOperation::Conflict &c) {
            ++asked;
            [&] { QCOMPARE(c.target, out + "/f.txt"); }();
            FileOperation::Resolution r;
            r.action = WhenExists::Replace;
            r.forAll = true;
            return r;
        });
        op.run();
        QCOMPARE(asked, 1);
        QCOMPARE(read(out + "/g.txt"), QByteArray("gee"));

        // Cancelling stops before the rest.
        reset();
        write(out + "/g.txt", "old gee");
        FileOperation stop(FileOperation::Kind::Copy, {in + "/f.txt", in + "/g.txt"}, out);
        stop.setResolver([](const FileOperation::Conflict &) {
            FileOperation::Resolution r;
            r.cancel = true;
            return r;
        });
        stop.run();
        QVERIFY(stop.isCancelled());
        QCOMPARE(read(out + "/g.txt"), QByteArray("old gee"));
    }

    void foldersMergeIntoFolders()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.path() + "/in/photos");
        QDir().mkpath(dir.path() + "/out/photos");
        write(dir.path() + "/in/photos/one.jpg", "1");
        write(dir.path() + "/out/photos/two.jpg", "2");
        FileOperation op(FileOperation::Kind::Move, {dir.path() + "/in/photos"}, dir.path() + "/out");
        op.setWhenExists(WhenExists::Skip);
        op.run();
        QVERIFY(op.errors().isEmpty());
        QVERIFY(QFileInfo::exists(dir.path() + "/out/photos/one.jpg"));
        QVERIFY(QFileInfo::exists(dir.path() + "/out/photos/two.jpg"));
        QVERIFY(!QFileInfo::exists(dir.path() + "/in/photos"));
    }

    void deleteRemovesTreesWithoutFollowingLinks()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.path() + "/keep");
        write(dir.path() + "/keep/precious.txt", "p");
        QDir().mkpath(dir.path() + "/gone/sub");
        write(dir.path() + "/gone/sub/x.txt", "x");
        QVERIFY(QFile::link(dir.path() + "/keep", dir.path() + "/gone/link"));
        FileOperation op(FileOperation::Kind::Delete, {dir.path() + "/gone"});
        op.run();
        QVERIFY(op.errors().isEmpty());
        QVERIFY(!QFileInfo::exists(dir.path() + "/gone"));
        QVERIFY(QFileInfo::exists(dir.path() + "/keep/precious.txt"));
    }

    void trashGoesToTheUsersTrash()
    {
        // A home of our own, so nothing lands in the real trash.
        QTemporaryDir dir;
        const QByteArray oldHome = qgetenv("HOME"), oldData = qgetenv("XDG_DATA_HOME");
        qputenv("HOME", QFile::encodeName(dir.path()));
        qputenv("XDG_DATA_HOME", QFile::encodeName(dir.path() + "/.local/share"));
        QDir().mkpath(dir.path() + "/.local/share");
        write(dir.path() + "/junk.txt", "j");
        FileOperation op(FileOperation::Kind::Trash, {dir.path() + "/junk.txt"});
        op.run();
        qputenv("HOME", oldHome);
        if (oldData.isEmpty())
            qunsetenv("XDG_DATA_HOME");
        else
            qputenv("XDG_DATA_HOME", oldData);
        QVERIFY2(op.errors().isEmpty(), qPrintable(op.errors().join('\n')));
        QVERIFY(!QFileInfo::exists(dir.path() + "/junk.txt"));
        QVERIFY(QFileInfo::exists(dir.path() + "/.local/share/Trash/files/junk.txt"));
        QVERIFY(QFileInfo::exists(dir.path() + "/.local/share/Trash/info/junk.txt.trashinfo"));
    }

    void whenExistsIdsRoundTrip()
    {
        for (int i = 0; i < int(WhenExists::Count); ++i)
            QCOMPARE(WhenExistsNames::fromId(WhenExistsNames::id(WhenExists(i))), WhenExists(i));
        QCOMPARE(WhenExistsNames::fromId("nonsense"), WhenExists::Ask);
    }

    void copyingFollowsALockedSource()
    {
        QTemporaryDir dir;
        const QString base = canonical(dir.path());
        QDir().mkpath(base + "/left");
        QDir().mkpath(base + "/right");
        write(base + "/left/l.txt", "l");
        write(base + "/right/r.txt", "r");
        MainWindow w(base + "/left");
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        w.setDual(true);
        FolderTab *left = w.display(0)->currentTab();
        FolderTab *right = w.display(1)->currentTab();
        QVERIFY(right->openPath(base + "/right"));
        QTRY_COMPARE(left->itemCount(), 1);
        QTRY_COMPARE(right->itemCount(), 1);
        const auto withKey = [&](const QKeySequence &key) {
            for (QAction *a : w.findChildren<QAction *>())
                if (a->shortcut() == key)
                    return a;
            return static_cast<QAction *>(nullptr);
        };
        QAction *copy = withKey(Qt::Key_F5);
        QAction *move = withKey(Qt::Key_F6);
        QAction *copyTo = withKey(Qt::SHIFT | Qt::Key_F5);
        QAction *trash = withKey(Qt::Key_Delete);
        QVERIFY(copy && move && copyTo && trash);

        // Not locked: whichever side is in use.
        w.activateSide(0);
        left->selectNames({"l.txt"});
        QVERIFY(copy->isEnabled());
        QCOMPARE(copy->text(), QString("Copy to Other Side"));
        w.activateSide(1);
        QVERIFY(!copy->isEnabled());

        // Locked: the source's selection counts, wherever the user is.
        w.activateSide(0);
        w.lockActiveSide(Role::Source);
        w.activateSide(1);
        QVERIFY(copy->isEnabled() && move->isEnabled() && copyTo->isEnabled());
        QVERIFY(!trash->isEnabled()); // the rest stays with the side in use
        QCOMPARE(copy->text(), QString("Copy from Source Side"));
        QCOMPARE(move->text(), QString("Move from Source Side"));
        left->selectNone();
        QVERIFY(!copy->isEnabled());
        right->selectNames({"r.txt"});
        QVERIFY(!copy->isEnabled() && !move->isEnabled() && !copyTo->isEnabled());
        QVERIFY(trash->isEnabled());
        // What's wrong is said where the user is.
        QVERIFY(!w.copySelection(false));
        QVERIFY(right->statusText().contains("Select the items"));
        QVERIFY(!left->errorId());

        left->selectNames({"l.txt"});
        QTest::keyClick(right->fileView(), Qt::Key_F5);
        QTRY_COMPARE(read(base + "/right/l.txt"), QByteArray("l"));
        QVERIFY(!QFileInfo::exists(base + "/left/r.txt"));

        // Locked the other way round, from the destination.
        w.lockActiveSide(Role::Destination);
        QCOMPARE(copy->text(), QString("Copy from Source Side"));
        w.activateSide(0);
        QCOMPARE(copy->text(), QString("Copy to Other Side"));
        w.activateSide(1);
        w.lockActiveSide(Role::None);
        QCOMPARE(copy->text(), QString("Copy to Other Side"));
        QVERIFY(copy->isEnabled());
    }

    void windowCopiesSelectionToOtherSide()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.path() + "/left");
        QDir().mkpath(dir.path() + "/right");
        write(dir.path() + "/left/a.txt", "a");
        write(dir.path() + "/left/b.txt", "b");
        MainWindow w(dir.path() + "/left");
        w.setDual(true);
        w.display(1)->currentTab()->openPath(dir.path() + "/right");
        FolderTab *left = w.display(0)->currentTab();
        QTRY_VERIFY(left->itemCount() == 2);
        left->selectMatching(pattern("a.txt"));
        QCOMPARE(left->selectedCount(), 1);

        OperationRunner *runner = w.copySelection(false);
        QVERIFY(runner);
        runner->setQuiet(true);
        QSignalSpy finished(runner, &OperationRunner::finished);
        QVERIFY(finished.wait(5000));
        QCOMPARE(read(dir.path() + "/right/a.txt"), QByteArray("a"));
        // The copied item is deselected, and can be selected again.
        QCOMPARE(left->selectedCount(), 0);
        QVERIFY(left->reselectPrevious());
        QCOMPARE(left->selectedPaths(), QStringList{canonical(dir.path() + "/left/a.txt")});

        // Without another side there's nowhere to copy to.
        w.setDual(false);
        QVERIFY(!w.copySelection(false));
        QVERIFY(left->statusText().contains("other side"));
    }

    void windowTrashesWithoutAsking()
    {
        QTemporaryDir dir;
        const QByteArray oldHome = qgetenv("HOME"), oldData = qgetenv("XDG_DATA_HOME");
        qputenv("HOME", QFile::encodeName(dir.path()));
        qputenv("XDG_DATA_HOME", QFile::encodeName(dir.path() + "/.local/share"));
        QDir().mkpath(dir.path() + "/.local/share");
        QDir().mkpath(dir.path() + "/work");
        write(dir.path() + "/work/old.txt", "o");
        MainWindow w(dir.path() + "/work");
        FolderTab *tab = w.activeTab();
        QTRY_VERIFY(tab->itemCount() == 1);
        tab->selectAll();
        OperationRunner *runner = w.deleteSelection(false);
        QVERIFY(runner);
        runner->setQuiet(true);
        QSignalSpy finished(runner, &OperationRunner::finished);
        QVERIFY(finished.wait(5000));
        qputenv("HOME", oldHome);
        if (oldData.isEmpty())
            qunsetenv("XDG_DATA_HOME");
        else
            qputenv("XDG_DATA_HOME", oldData);
        QVERIFY(!QFileInfo::exists(dir.path() + "/work/old.txt"));
        QTRY_COMPARE(tab->itemCount(), 0);
    }

    // --- Selection (SEL-01, SEL-02) --------------------------------------

    void selectCommands()
    {
        QTemporaryDir dir;
        for (const char *name : {"a.jpg", "B.JPG", "c.png", "notes.txt"})
            write(dir.path() + '/' + name, "x");
        QDir().mkpath(dir.path() + "/pics.jpg"); // a folder with an extension
        QDir().mkpath(dir.path() + "/docs");
        FolderTab tab(dir.path());
        QTRY_COMPARE(tab.itemCount(), 6);
        auto names = [&] {
            QStringList list;
            for (const QString &path : tab.selectedPaths())
                list << QFileInfo(path).fileName();
            list.sort();
            return list;
        };

        tab.selectAll();
        QCOMPARE(tab.selectedCount(), 6);
        QVERIFY(tab.statusText().contains("6 selected"));
        tab.selectFolders();
        QCOMPARE(names(), (QStringList{"docs", "pics.jpg"}));
        tab.invertSelection();
        QCOMPARE(names(), (QStringList{"B.JPG", "a.jpg", "c.png", "notes.txt"}));
        tab.selectFiles();
        QCOMPARE(tab.selectedCount(), 4);
        tab.selectNone();
        QCOMPARE(tab.selectedCount(), 0);
        QVERIFY(tab.reselectPrevious());
        QCOMPARE(tab.selectedCount(), 4);

        // Wildcards ignore case unless asked; ";" separates several.
        tab.selectNone();
        QCOMPARE(tab.selectMatching(pattern("*.jpg")), 3);
        QCOMPARE(names(), (QStringList{"B.JPG", "a.jpg", "pics.jpg"}));
        SelectPattern filesOnly = pattern("*.jpg;*.png", SelectPattern::Action::SelectOnly);
        filesOnly.scope = SelectPattern::Scope::FilesOnly;
        tab.selectMatching(filesOnly);
        QCOMPARE(names(), (QStringList{"B.JPG", "a.jpg", "c.png"}));
        SelectPattern exact = pattern("*.jpg", SelectPattern::Action::Deselect);
        exact.matchCase = true;
        tab.selectMatching(exact);
        QCOMPARE(names(), (QStringList{"B.JPG", "c.png"}));
        tab.selectMatching(pattern("notes.*")); // adds
        QCOMPARE(tab.selectedCount(), 3);

        // A regular expression has to match the whole name.
        SelectPattern re = pattern("[a-c]\\.(jpg|png)", SelectPattern::Action::SelectOnly);
        re.regex = true;
        tab.selectMatching(re);
        QCOMPARE(names(), (QStringList{"B.JPG", "a.jpg", "c.png"}));
        re.text = "[a-c";
        QVERIFY(!re.error().isEmpty());
        QVERIFY(!pattern("  ").error().isEmpty());

        // Escape in the list clears the selection.
        QTest::keyClick(tab.fileView(), Qt::Key_Escape);
        QCOMPARE(tab.selectedCount(), 0);
    }

    void editKeysLeaveThePathFieldAlone()
    {
        QTemporaryDir dir;
        write(dir.path() + "/keep.txt", "k");
        MainWindow w(dir.path());
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        FolderTab *tab = w.activeTab();
        QTRY_COMPARE(tab->itemCount(), 1);
        tab->editPath();
        QLineEdit *edit = tab->breadcrumb()->editor();
        QTRY_VERIFY(edit->hasFocus());
        // Ctrl+A and Delete edit the text, not the files.
        QTest::keyClick(edit, Qt::Key_A, Qt::ControlModifier);
        QCOMPARE(tab->selectedCount(), 0);
        QVERIFY(edit->hasSelectedText());
        QTest::keyClick(edit, Qt::Key_Delete);
        QVERIFY(edit->text().isEmpty());
        QVERIFY(QFileInfo::exists(dir.path() + "/keep.txt"));
    }

    // --- COPY-03, COPY-08, DEL-02, REN-01 ---------------------------------

    void foldersCanAskBeforeMerging()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.path() + "/in/photos");
        QDir().mkpath(dir.path() + "/out/photos");
        write(dir.path() + "/in/photos/one.jpg", "1");
        write(dir.path() + "/out/photos/two.jpg", "2");
        QList<FileOperation::Conflict> asked;
        auto copyAnswering = [&](WhenExists answer) {
            FileOperation op(FileOperation::Kind::Copy, {dir.path() + "/in/photos"}, dir.path() + "/out");
            op.setAskBeforeMerging(true);
            op.setResolver([&](const FileOperation::Conflict &c) {
                asked << c;
                FileOperation::Resolution r;
                r.action = answer;
                return r;
            });
            op.run();
            return op.errors();
        };
        QVERIFY(copyAnswering(WhenExists::RenameNew).isEmpty());
        QCOMPARE(asked.size(), 1);
        QVERIFY(asked.first().sourceIsDir && asked.first().targetIsDir);
        QVERIFY(QFileInfo::exists(dir.path() + "/out/photos (2)/one.jpg"));
        QVERIFY(!QFileInfo::exists(dir.path() + "/out/photos/one.jpg"));
        QVERIFY(copyAnswering(WhenExists::Replace).isEmpty()); // merge
        QVERIFY(QFileInfo::exists(dir.path() + "/out/photos/one.jpg"));
        QVERIFY(QFileInfo::exists(dir.path() + "/out/photos/two.jpg"));
    }

    void readOnlyFilesGetASecondLook()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.path() + "/in");
        QDir().mkpath(dir.path() + "/out");
        write(dir.path() + "/in/f.txt", "new");
        write(dir.path() + "/out/f.txt", "old");
        QFile::setPermissions(dir.path() + "/out/f.txt", QFile::ReadOwner);
        int asked = 0;
        auto copy = [&](bool askReadOnly, WhenExists answer) {
            FileOperation op(FileOperation::Kind::Copy, {dir.path() + "/in/f.txt"}, dir.path() + "/out");
            op.setWhenExists(WhenExists::Replace);
            op.setAskBeforeReplacingReadOnly(askReadOnly);
            op.setResolver([&](const FileOperation::Conflict &c) {
                ++asked;
                [&] { QVERIFY(c.targetReadOnly); }();
                FileOperation::Resolution r;
                r.action = answer;
                return r;
            });
            op.run();
        };
        copy(true, WhenExists::Skip);
        QCOMPARE(asked, 1);
        QCOMPARE(read(dir.path() + "/out/f.txt"), QByteArray("old"));
        copy(false, WhenExists::Skip);
        QCOMPARE(asked, 1);
        QCOMPARE(read(dir.path() + "/out/f.txt"), QByteArray("new"));
    }

    void conflictsTakeAChosenName()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.path() + "/in");
        QDir().mkpath(dir.path() + "/out");
        write(dir.path() + "/in/f.txt", "new");
        write(dir.path() + "/out/f.txt", "old");
        FileOperation op(FileOperation::Kind::Copy, {dir.path() + "/in/f.txt"}, dir.path() + "/out");
        op.setResolver([](const FileOperation::Conflict &) {
            FileOperation::Resolution r;
            r.action = WhenExists::RenameNew;
            r.newName = "f from home.txt";
            return r;
        });
        op.run();
        QVERIFY(op.errors().isEmpty());
        QCOMPARE(read(dir.path() + "/out/f from home.txt"), QByteArray("new"));
        QCOMPARE(read(dir.path() + "/out/f.txt"), QByteArray("old"));
    }

    void unattendedSkipsWhatWouldAsk()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.path() + "/in");
        QDir().mkpath(dir.path() + "/out");
        write(dir.path() + "/in/a.txt", "a");
        write(dir.path() + "/in/b.txt", "b");
        write(dir.path() + "/out/a.txt", "old");
        FileOperation op(FileOperation::Kind::Copy, {dir.path() + "/in/a.txt", dir.path() + "/in/b.txt"},
                         dir.path() + "/out");
        bool asked = false;
        op.setResolver([&](const FileOperation::Conflict &) {
            asked = true;
            return FileOperation::Resolution();
        });
        op.setUnattended(true);
        op.run();
        QVERIFY(!asked);
        QCOMPARE(op.skipped().size(), 1);
        QCOMPARE(read(dir.path() + "/out/a.txt"), QByteArray("old"));
        QCOMPARE(read(dir.path() + "/out/b.txt"), QByteArray("b"));
    }

    void copiesCountFilesAndCanSkipOne()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.path() + "/in/sub");
        write(dir.path() + "/in/big.bin", QByteArray(4 << 20, 'b'));
        write(dir.path() + "/in/sub/small.txt", "s");
        write(dir.path() + "/in/zz.txt", "z");
        FileOperation op(FileOperation::Kind::Copy, {dir.path() + "/in"}, dir.path() + "/out");
        QSignalSpy files(&op, &FileOperation::fileStarted);
        // Skip the big file once it is under way.
        connect(&op, &FileOperation::progress, &op,
                [&op](qint64, qint64, qint64 fileDone, qint64 fileSize, const QString &) {
                    if (fileSize > 1000 && fileDone > 0)
                        op.skipCurrent();
                });
        op.run();
        QVERIFY(op.errors().isEmpty());
        QCOMPARE(op.skipped().size(), 1);
        QVERIFY(!QFileInfo::exists(dir.path() + "/out/in/big.bin"));
        QCOMPARE(read(dir.path() + "/out/in/sub/small.txt"), QByteArray("s"));
        QCOMPARE(read(dir.path() + "/out/in/zz.txt"), QByteArray("z"));
        QVERIFY(!files.isEmpty());
        QCOMPARE(files.last().at(0).toInt(), 3);
        QCOMPARE(files.last().at(1).toInt(), 3);
    }

    void pausingHoldsTheWork()
    {
        QTemporaryDir dir;
        write(dir.path() + "/a.txt", "a");
        FileOperation op(FileOperation::Kind::Copy, {dir.path() + "/a.txt"}, dir.path() + "/out");
        op.setPaused(true);
        QFuture<void> work = QtConcurrent::run([&op] { op.run(); });
        QTest::qWait(200);
        QVERIFY(!work.isFinished());
        QVERIFY(!QFileInfo::exists(dir.path() + "/out/a.txt"));
        op.setPaused(false);
        work.waitForFinished();
        QCOMPARE(read(dir.path() + "/out/a.txt"), QByteArray("a"));
    }

    void permanentDeletesAskWhenTold()
    {
        QTemporaryDir dir;
        write(dir.path() + "/keep.txt", "k");
        write(dir.path() + "/lose.txt", "l");
        QDir().mkpath(dir.path() + "/folder");
        write(dir.path() + "/folder/locked.txt", "r");
        QFile::setPermissions(dir.path() + "/folder/locked.txt", QFile::ReadOwner);
        QList<FileOperation::DeleteQuestion> asked;
        FileOperation op(FileOperation::Kind::Delete,
                         {dir.path() + "/keep.txt", dir.path() + "/lose.txt", dir.path() + "/folder"});
        FileOperation::DeleteConfirmation confirm;
        confirm.eachFile = true;
        op.setDeleteConfirmation(confirm);
        op.setDeleteResolver([&](const FileOperation::DeleteQuestion &q) {
            asked << q;
            if (q.reason == FileOperation::DeleteQuestion::Reason::ReadOnly)
                return FileOperation::DeleteAnswer::Skip;
            return q.path.endsWith("keep.txt") ? FileOperation::DeleteAnswer::Skip : FileOperation::DeleteAnswer::Delete;
        });
        op.run();
        QVERIFY2(op.errors().isEmpty(), qPrintable(op.errors().join('\n')));
        QCOMPARE(asked.size(), 3); // two files, then the read-only one
        QCOMPARE(asked.last().reason, FileOperation::DeleteQuestion::Reason::ReadOnly);
        QVERIFY(QFileInfo::exists(dir.path() + "/keep.txt"));
        QVERIFY(!QFileInfo::exists(dir.path() + "/lose.txt"));
        // The read-only file stays, and so does the folder holding it.
        QVERIFY(QFileInfo::exists(dir.path() + "/folder/locked.txt"));

        // Asked to delete read-only files without asking, it does.
        FileOperation again(FileOperation::Kind::Delete, {dir.path() + "/folder"});
        confirm.eachFile = false;
        confirm.readOnlyWithoutAsking = true;
        again.setDeleteConfirmation(confirm);
        again.run();
        QVERIFY(!QFileInfo::exists(dir.path() + "/folder"));
    }

    void renameSelectsThePartAsked()
    {
        QCOMPARE(RenameDelegate::extensionStart("photo.jpg"), 5);
        QCOMPARE(RenameDelegate::extensionStart(".bashrc"), -1);
        QCOMPARE(RenameDelegate::extensionStart("notes"), -1);
        QLineEdit edit("archive.tar.gz");
        RenameDelegate::select(&edit, edit.text(), RenameDelegate::Selection::Stem);
        QCOMPARE(edit.selectedText(), QString("archive.tar"));
        RenameDelegate::select(&edit, edit.text(), RenameDelegate::Selection::Extension);
        QCOMPARE(edit.selectedText(), QString("gz"));
        RenameDelegate::select(&edit, edit.text(), RenameDelegate::Selection::Whole);
        QCOMPARE(edit.selectedText(), edit.text());
        RenameDelegate::select(&edit, edit.text(), RenameDelegate::Selection::End);
        QCOMPARE(edit.cursorPosition(), int(edit.text().size()));
        QVERIFY(!edit.hasSelectedText());
    }

    void renameNeverReplaces()
    {
        QTemporaryDir dir;
        write(dir.path() + "/a.txt", "a");
        write(dir.path() + "/b.txt", "b");
        QVERIFY(FolderTab::renameOnDisk(dir.path(), "a.txt", "b.txt").contains("already exists"));
        QVERIFY(!FolderTab::renameOnDisk(dir.path(), "a.txt", "x/y").isEmpty());
        QVERIFY(!FolderTab::renameOnDisk(dir.path(), "a.txt", "..").isEmpty());
        QVERIFY(!FolderTab::renameOnDisk(dir.path(), "a.txt", "").isEmpty());
        QVERIFY(FolderTab::renameOnDisk(dir.path(), "a.txt", "c.txt").isEmpty());
        QCOMPARE(read(dir.path() + "/c.txt"), QByteArray("a"));
        QCOMPARE(read(dir.path() + "/b.txt"), QByteArray("b"));
    }

    void inlineRenameInTheList()
    {
        QTemporaryDir dir;
        write(dir.path() + "/holiday.jpg", "h");
        write(dir.path() + "/other.txt", "o");
        FolderTab tab(dir.path());
        tab.resize(600, 400);
        tab.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tab));
        QTRY_COMPARE(tab.itemCount(), 2);
        tab.focusName("holiday.jpg");
        tab.startRename();
        QLineEdit *editor = nullptr;
        QTRY_VERIFY((editor = tab.fileView()->viewport()->findChild<QLineEdit *>()));
        QTRY_COMPARE(editor->selectedText(), QString("holiday"));
        QTest::keyClick(editor, Qt::Key_F2); // whole name next
        QCOMPARE(editor->selectedText(), QString("holiday.jpg"));
        QTest::keyClick(editor, Qt::Key_F2);
        QCOMPARE(editor->selectedText(), QString("jpg"));
        QTest::keyClick(editor, Qt::Key_F2);
        QCOMPARE(editor->selectedText(), QString("holiday"));
        QTest::keyClicks(editor, "beach");
        QTest::keyClick(editor, Qt::Key_Return);
        QTRY_VERIFY(QFileInfo::exists(dir.path() + "/beach.jpg"));
        QVERIFY(!QFileInfo::exists(dir.path() + "/holiday.jpg"));
        QCOMPARE(tab.selectedPaths(), QStringList{canonical(dir.path()) + "/beach.jpg"});

        // A taken name is refused and said so.
        QVERIFY(!tab.renameItem("beach.jpg", "other.txt"));
        QVERIFY(tab.statusText().contains("already exists"));
        QVERIFY(QFileInfo::exists(dir.path() + "/beach.jpg"));
    }

    void f2CyclesInTheWindowToo()
    {
        // F2 is also the window's Rename shortcut; inside the editor it must
        // reach the editor instead.
        QTemporaryDir dir;
        write(dir.path() + "/holiday.jpg", "h");
        MainWindow w(dir.path());
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        FolderTab *tab = w.activeTab();
        QTRY_COMPARE(tab->itemCount(), 1);
        tab->focusName("holiday.jpg");
        tab->fileView()->setFocus();
        QTest::keyClick(tab->fileView(), Qt::Key_F2);
        QLineEdit *editor = nullptr;
        QTRY_VERIFY((editor = tab->fileView()->viewport()->findChild<QLineEdit *>()));
        QTRY_COMPARE(editor->selectedText(), QString("holiday"));
        QTest::keyClick(editor, Qt::Key_F2);
        QCOMPARE(editor->selectedText(), QString("holiday.jpg"));
        QTest::keyClick(editor, Qt::Key_F2);
        QCOMPARE(editor->selectedText(), QString("jpg"));
        // Other window keys don't act on files while typing.
        QTest::keyClick(editor, Qt::Key_D, Qt::ControlModifier);
        QTest::keyClick(editor, Qt::Key_Escape);
        QTest::qWait(100);
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files), QStringList{"holiday.jpg"});
    }

    void deletingLandsOnTheNextItem()
    {
        QTemporaryDir dir;
        for (const char *name : {"a.txt", "b.txt", "c.txt", "d.txt"})
            write(dir.path() + '/' + name, "x");
        FolderTab tab(dir.path());
        QTRY_COMPARE(tab.itemCount(), 4);
        tab.selectNames({"b.txt", "c.txt"});
        QCOMPARE(tab.nameAfterSelection(), QString("d.txt"));
        tab.selectNames({"c.txt", "d.txt"});
        QCOMPARE(tab.nameAfterSelection(), QString("b.txt"));
        tab.focusName("a.txt");
        QCOMPARE(tab.selectedPaths(), QStringList{canonical(dir.path()) + "/a.txt"});
    }

    void replaceDialogShowsBothItems()
    {
        QTemporaryDir dir;
        QImage image(200, 100, QImage::Format_RGB32);
        image.fill(Qt::blue);
        QVERIFY(image.save(dir.path() + "/pic.png"));
        const QPixmap preview = ReplaceDialog::preview(dir.path() + "/pic.png", false, 96);
        QCOMPARE(preview.width(), 96);
        QCOMPARE(preview.height(), 48);
        QVERIFY(!ReplaceDialog::preview(dir.path(), true, 96).isNull());

        FileOperation::Conflict c;
        c.source = dir.path() + "/pic.png";
        c.target = dir.path() + "/pic.png";
        c.sourceSize = 10;
        c.targetSize = 5;
        c.sourceModified = QDateTime::currentDateTime();
        c.targetModified = c.sourceModified.addDays(-1);
        ReplaceDialog dialog(c);
        QStringList texts;
        for (QLabel *label : dialog.findChildren<QLabel *>())
            texts << label->text();
        QVERIFY(texts.join('|').contains("newer"));
        QVERIFY(texts.join('|').contains("larger"));
        // Closing without choosing cancels.
        QVERIFY(dialog.resolution().cancel);
        // Resume is offered because the existing file is smaller.
        bool resume = false;
        for (QPushButton *b : dialog.findChildren<QPushButton *>())
            resume |= b->text().contains("ume");
        QVERIFY(resume);
    }

    void progressWindowSaysWhatIsHappening()
    {
        QCOMPARE(ProgressWindow::timeLeft(3), QString("a few seconds left"));
        QCOMPARE(ProgressWindow::timeLeft(42), QString("about 45 s left"));
        QCOMPARE(ProgressWindow::timeLeft(150), QString("about 3 min left"));
        QCOMPARE(ProgressWindow::timeLeft(3720), QString("about 1 h 2 min left"));

        FileOperation op(FileOperation::Kind::Copy, {"/a/b/song.flac"}, "/c");
        ProgressWindow window(&op);
        window.setFile(2, 5, "/a/b/song.flac");
        window.setProgress(250, 1000, 10, 20, "song.flac");
        QCOMPARE(window.windowTitle(), QString("25% · Copying"));
        QStringList texts;
        for (QLabel *label : window.findChildren<QLabel *>())
            texts << label->text();
        QVERIFY(texts.join('|').contains("item 2 of 5"));
        QVERIFY(texts.join('|').contains("song.flac"));
        // Pause reaches the operation.
        QPushButton *pause = nullptr;
        for (QPushButton *b : window.findChildren<QPushButton *>())
            if (b->text().contains("Pause"))
                pause = b;
        QVERIFY(pause);
        pause->click();
        QVERIFY(op.isPaused());
        window.reject();
        QVERIFY(op.isCancelled());
    }

    // --- FILT-03 ----------------------------------------------------------

    void globalFilterDecides()
    {
        GlobalFilter f;
        auto hides = [&f](const char *name, bool dir = false, bool listed = false) {
            return GlobalFilter::Matcher(f).hides(name, dir, listed);
        };
        QVERIFY(hides(".bashrc"));
        QVERIFY(hides("snap", true, true));
        QVERIFY(!hides("notes.txt"));
        f.showHidden = true;
        QVERIFY(!hides(".bashrc"));
        QVERIFY(!hides("snap", true, true));
        // Patterns hide whether or not hidden files show, files and folders apart.
        f.files = "*.bak; *~";
        f.folders = "node_modules";
        QVERIFY(!hides("a.bak")); // not turned on
        f.hideNames = true;
        QVERIFY(hides("a.BAK"));
        QVERIFY(hides("a.txt~"));
        QVERIFY(!hides("a.bak", true));
        QVERIFY(hides("node_modules", true));
        QVERIFY(!hides("node_modules"));
        f.files = "IMG_\\d+\\.jpg";
        f.filesRegex = true;
        QVERIFY(hides("IMG_12.jpg"));
        QVERIFY(!hides("xIMG_12.jpg"));
        // A broken pattern hides nothing.
        f.files = "(";
        QVERIFY(!hides("("));
        QVERIFY(!GlobalFilter::patternError("(", true).isEmpty());
        QVERIFY(GlobalFilter::patternError("", false).isEmpty());
    }

    void globalFilterAppliesToEveryTab()
    {
        QTemporaryDir dir, config;
        write(dir.path() + "/a.txt", "a");
        write(dir.path() + "/b.bak", "b");
        write(dir.path() + "/.secret", "s");
        write(dir.path() + "/.hidden", "listed.txt\n\n");
        write(dir.path() + "/listed.txt", "l");
        QSettings settings(config.path() + "/c.conf", QSettings::IniFormat);
        MainWindow w(dir.path(), &settings);
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        FolderTab *tab = w.activeTab();
        QTRY_COMPARE(tab->itemCount(), 2);
        QCOMPARE(tab->hiddenCount(), 3);
        QVERIFY(tab->statusText().contains("3 hidden"));
        tab->selectAll(); // never the hidden ones
        QCOMPARE(tab->selectedCount(), 2);

        tab->fileView()->setFocus();
        QTest::keyClick(tab->fileView(), Qt::Key_H, Qt::ControlModifier);
        QCOMPARE(tab->itemCount(), 5);
        QCOMPARE(tab->hiddenCount(), 0);
        QVERIFY(!tab->statusText().contains("hidden"));
        FolderTab *second = w.display(0)->addTab(dir.path());
        QTRY_COMPARE(second->itemCount(), 5);

        TabOptions options = w.tabOptions();
        options.filter.showHidden = false;
        options.filter.hideNames = true;
        options.filter.files = "*.bak";
        w.setTabOptions(options);
        QCOMPARE(tab->itemCount(), 1);
        QCOMPARE(second->itemCount(), 1);
        // Remembered.
        QVERIFY(TabOptions::load(settings).filter == options.filter);

        // A hidden file appearing is counted, not shown.
        write(dir.path() + "/.another", "x");
        QTRY_COMPARE(tab->hiddenCount(), 5);
        QCOMPARE(tab->itemCount(), 1);
        // Renaming something to a hidden name says where it went.
        QVERIFY(tab->renameItem("a.txt", ".a.txt"));
        QCOMPARE(tab->itemCount(), 0);
        QVERIFY(tab->statusText().contains("hidden here"));
    }

    void goingUpFromAHiddenFolderIsFine()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.path() + "/.config/app");
        FolderTab tab(dir.path() + "/.config");
        QTRY_COMPARE(tab.itemCount(), 1);
        tab.goUp();
        QTRY_COMPARE(tab.itemCount(), 0);
        QCOMPARE(tab.hiddenCount(), 1);
        QVERIFY(tab.selectedPaths().isEmpty());
    }

    void globalFilterDialogEdits()
    {
        GlobalFilter f;
        f.hideNames = true;
        f.files = "*.o";
        GlobalFilterDialog dialog(f);
        QVERIFY(dialog.filter() == f);
        QLineEdit *files = dialog.findChildren<QLineEdit *>().first();
        QCOMPARE(files->text(), QString("*.o"));
        QPushButton *ok = dialog.findChildren<QPushButton *>().first();
        for (QCheckBox *box : dialog.findChildren<QCheckBox *>())
            if (box->text().contains("Regular"))
                box->setChecked(true);
        QVERIFY(!ok->isEnabled()); // *.o isn't a regular expression
        files->setText(".*\\.o");
        QVERIFY(ok->isEnabled());
    }

    // --- FCMD-01 ----------------------------------------------------------

    void folderNamesExpand()
    {
        QCOMPARE(CreateFolder::expand("plain"), QStringList{"plain"});
        QCOMPARE(CreateFolder::expand("2026/{Jan,Feb}"), QStringList({"2026/Jan", "2026/Feb"}));
        QCOMPARE(CreateFolder::expand("Disc {08..10}"), QStringList({"Disc 08", "Disc 09", "Disc 10"}));
        QCOMPARE(CreateFolder::expand("{3..1}"), QStringList({"3", "2", "1"}));
        QCOMPARE(CreateFolder::expand("{a,b{1,2}}/x"), QStringList({"a/x", "b1/x", "b2/x"}));
        QCOMPARE(CreateFolder::expand("{a,b}-{c,d}"), QStringList({"a-c", "a-d", "b-c", "b-d"}));
        // Not a list, so just a name.
        QCOMPARE(CreateFolder::expand("{draft}"), QStringList{"{draft}"});
        QCOMPARE(CreateFolder::expand("a{b"), QStringList{"a{b"});
        // Far too many stops instead of eating all the memory.
        QVERIFY(CreateFolder::parse("{1..999999999}", true).size() <= CreateFolder::kMax + 1);
        QVERIFY(CreateFolder::parse("{1..999}/{1..999}/{1..999}", true).size() <= CreateFolder::kMax + 1);
        // One name is taken as it is.
        QCOMPARE(CreateFolder::parse(" {a,b}/ ", false), QStringList{"{a,b}"});
        QCOMPARE(CreateFolder::parse("a\n\n b \na\n", true), QStringList({"a", "b"}));
    }

    void folderNamesAreChecked()
    {
        QVERIFY(CreateFolder::error("a/b").isEmpty());
        QVERIFY(CreateFolder::error("...").isEmpty());
        for (const char *bad : {"", "..", "a/../../b", "/tmp/x", "~/x", "a//b", "."})
            QVERIFY2(!CreateFolder::error(bad).isEmpty(), bad);
        QVERIFY(!CreateFolder::error(QString(300, 'x')).isEmpty());
        QVERIFY(CreateFolder::error(QString(120, 'x') + '/' + QString(200, 'y')).isEmpty());
    }

    void foldersAreCreated()
    {
        QTemporaryDir dir;
        write(dir.path() + "/file", "f");
        QDir().mkpath(dir.path() + "/old");
        const CreateFolder::Result r =
            CreateFolder::create(dir.path(), {"a/b/c", "old", "file", "file/x", "../out", "old/new"});
        QCOMPARE(r.created, QStringList({"a/b/c", "old/new"}));
        QCOMPARE(r.existing, QStringList{"old"});
        QCOMPARE(r.errors.size(), 3);
        QVERIFY(QFileInfo(dir.path() + "/a/b/c").isDir());
        QVERIFY(!QFileInfo::exists(dir.path() + "/../out"));
        QCOMPARE(read(dir.path() + "/file"), QByteArray("f"));
        QCOMPARE(CreateFolder::defaultName(dir.path()), QString("New Folder"));
        QDir().mkpath(dir.path() + "/New Folder");
        QCOMPARE(CreateFolder::defaultName(dir.path()), QString("New Folder (2)"));
    }

    void createFolderDialogWorks()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.path() + "/taken");
        CreateFolderDialog::Setup setup;
        setup.base = dir.path();
        setup.suggestions = QStringList{"New Folder", "Holiday"};
        CreateFolderDialog dialog(setup);
        QComboBox *name = dialog.findChildren<QComboBox *>().first();
        QCOMPARE(name->count(), 2); // the default isn't offered twice
        QCOMPARE(name->currentText(), QString("New Folder"));
        QPushButton *create = nullptr;
        for (QPushButton *b : dialog.findChildren<QPushButton *>())
            if (b->text().contains("Create"))
                create = b;
        QVERIFY(create && create->isEnabled());
        name->setCurrentText("taken");
        QVERIFY(!create->isEnabled());
        name->setCurrentText("../up");
        QVERIFY(!create->isEnabled());
        dialog.accept(); // Enter does nothing then
        QCOMPARE(dialog.result().created.size(), 0);

        dialog.show();
        QCheckBox *several = dialog.findChild<QCheckBox *>();
        several->setChecked(true);
        auto *names = dialog.findChild<QPlainTextEdit *>();
        QVERIFY(names->isVisible());
        names->setPlainText("taken\nx{1..2}");
        QVERIFY(create->isEnabled());
        names->moveCursor(QTextCursor::End);
        QTest::keyClick(names, Qt::Key_Return); // a new line first
        QVERIFY(dialog.isVisible());
        QTest::keyClick(names, Qt::Key_Return); // then done
        QVERIFY(!dialog.isVisible());
        QCOMPARE(dialog.result().created, QStringList({"x1", "x2"}));
        QCOMPARE(dialog.result().existing, QStringList{"taken"});
    }

    void createFolderFromTheWindow()
    {
        QTemporaryDir dir, config;
        write(dir.path() + "/report.pdf", "r");
        QSettings settings(config.path() + "/c.conf", QSettings::IniFormat);
        MainWindow w(dir.path(), &settings);
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        FolderTab *tab = w.activeTab();
        QTRY_COMPARE(tab->itemCount(), 1);
        tab->focusName("report.pdf");
        tab->fileView()->setFocus();
        QStringList offered;
        auto answer = [&](const QString &type, CreateFolder::Then then) {
            QTimer::singleShot(200, &w, [&, type, then] {
                auto *d = qobject_cast<CreateFolderDialog *>(QApplication::activeModalWidget());
                QVERIFY(d);
                const QList<QComboBox *> boxes = d->findChildren<QComboBox *>();
                for (int i = 0; i < boxes.first()->count(); ++i)
                    offered << boxes.first()->itemText(i);
                boxes.first()->setCurrentText(type);
                boxes.last()->setCurrentIndex(int(then));
                d->accept();
            });
            // The first dialog having closed, the window is active again only
            // if asked for on the offscreen platform.
            w.activateWindow();
            QVERIFY(QTest::qWaitForWindowActive(&w));
            QTest::keyClick(&w, Qt::Key_F7);
        };
        answer("one/two", CreateFolder::Then::Select);
        QVERIFY(offered.contains("report")); // named for the selected file
        QVERIFY(QFileInfo(dir.path() + "/one/two").isDir());
        QTRY_COMPARE(tab->selectedPaths(), QStringList{canonical(dir.path()) + "/one"});

        answer(".dot", CreateFolder::Then::Select);
        QTRY_VERIFY(tab->statusText().contains("hidden here"));

        answer("tabbed", CreateFolder::Then::NewTab);
        QCOMPARE(w.display(0)->tabCount(), 2);
        QCOMPARE(w.activeTab()->currentPath(), canonical(dir.path()) + "/tabbed");
        QCOMPARE(w.tabOptions().createThen, QString("tab"));
        QCOMPARE(w.tabOptions().createNames.first(), QString("tabbed"));

        w.display(0)->setCurrentIndex(0);
        answer("across", CreateFolder::Then::OtherSide);
        QVERIFY(w.isDual());
        QCOMPARE(w.partnerTab()->currentPath(), canonical(dir.path()) + "/across");
        answer("deep/er", CreateFolder::Then::Open);
        QCOMPARE(tab->currentPath(), canonical(dir.path()) + "/deep/er");
    }

    // --- CLIP-01 ----------------------------------------------------------

    void clipboardSpeaksOtherFileManagers()
    {
        FileClipboard *clip = FileClipboard::instance();
        QTemporaryDir dir;
        const QString base = canonical(dir.path());
        write(base + "/a b#1%.txt", "a");
        QDir().mkpath(base + "/real");
        write(base + "/real/x", "x");
        QVERIFY(QFile::link(base + "/real", base + "/alias"));
        QVERIFY(QFile::link(base + "/real/x", base + "/link"));

        clip->setFiles({base + "/a b#1%.txt"}, true);
        const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
        QCOMPARE(mime->data("x-special/gnome-copied-files"),
                 QByteArray("cut\n") + QUrl::fromLocalFile(base + "/a b#1%.txt").toEncoded());
        QCOMPARE(mime->data("application/x-kde-cutselection"), QByteArray("1"));
        QCOMPARE(mime->text(), base + "/a b#1%.txt");
        QCOMPARE(clip->files().paths, QStringList{base + "/a b#1%.txt"});
        QVERIFY(clip->files().cut);
        QVERIFY(clip->isCut(base + "/a b#1%.txt"));

        // From KDE: URLs, with a separate mark for a cut. Paths through a
        // linked folder come out the same; a link itself stays a link.
        auto *kde = new QMimeData;
        kde->setUrls({QUrl::fromLocalFile(base + "/alias/x"), QUrl::fromLocalFile(base + "/link"),
                      QUrl::fromLocalFile(base + "/real/"), QUrl("sftp://host/y")});
        QGuiApplication::clipboard()->setMimeData(kde);
        FileClipboard::Files files = clip->files();
        QCOMPARE(files.paths, QStringList({base + "/real/x", base + "/link", base + "/real"}));
        QVERIFY(!files.cut);
        QCOMPARE(files.remote, 1);
        QVERIFY(!clip->isCut(base + "/a b#1%.txt"));

        // From a GNOME program that only offers its own format.
        auto *gnome = new QMimeData;
        gnome->setData("x-special/gnome-copied-files", "cut\nfile://" + base.toUtf8() + "/real/x\n");
        QGuiApplication::clipboard()->setMimeData(gnome);
        QCOMPARE(clip->files().paths, QStringList{base + "/real/x"});
        QVERIFY(clip->files().cut);

        clip->setText("just text");
        QVERIFY(clip->files().paths.isEmpty());
        QVERIFY(!clip->isCut(base + "/real/x"));
    }

    void copyAndPasteInTheWindow()
    {
        QTemporaryDir dir;
        const QString base = canonical(dir.path());
        QDir().mkpath(base + "/from");
        QDir().mkpath(base + "/to");
        write(base + "/from/one.txt", "1");
        write(base + "/from/two.txt", "2");
        MainWindow w(base + "/from");
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        FolderTab *tab = w.activeTab();
        QTRY_COMPARE(tab->itemCount(), 2);
        auto press = [&](int key) {
            tab->fileView()->setFocus();
            QTest::keyClick(tab->fileView(), Qt::Key(key), Qt::ControlModifier);
        };
        tab->selectNames({"one.txt"});
        press(Qt::Key_C);
        QCOMPARE(FileClipboard::instance()->files().paths, QStringList{base + "/from/one.txt"});

        // Into its own folder: a copy under a free name, which is selected.
        press(Qt::Key_V);
        QTRY_COMPARE(tab->selectedPaths(), QStringList{base + "/from/one (2).txt"});
        QCOMPARE(read(base + "/from/one (2).txt"), QByteArray("1"));

        // Cut: dimmed until it is pasted, then gone from the clipboard.
        tab->selectNames({"two.txt", "one.txt"});
        press(Qt::Key_X);
        const int row = tab->model()->rowOf("two.txt");
        QVERIFY(tab->model()->index(row, 0).data(FileModel::CutRole).toBool());
        QVERIFY(tab->openPath(base + "/to"));
        QTRY_COMPARE(tab->itemCount(), 0);
        press(Qt::Key_V);
        QTRY_COMPARE(tab->selectedCount(), 2);
        QCOMPARE(QDir(base + "/from").entryList(QDir::Files), QStringList{"one (2).txt"});
        QCOMPARE(read(base + "/to/two.txt"), QByteArray("2"));
        QVERIFY(FileClipboard::instance()->files().paths.isEmpty());

        // Nothing to paste says so.
        FileClipboard::instance()->setText("words");
        press(Qt::Key_V);
        QVERIFY(w.statusBar()->currentMessage().contains("no files"));
        QVERIFY(!w.pasteLinks(FileOperation::LinkType::Symbolic));
    }

    void cuttingAndPastingInPlaceDoesNothing()
    {
        QTemporaryDir dir;
        const QString base = canonical(dir.path());
        write(base + "/a.txt", "a");
        MainWindow w(base);
        QTRY_COMPARE(w.activeTab()->itemCount(), 1);
        FileClipboard::instance()->setFiles({base + "/a.txt"}, true);
        OperationRunner *runner = w.pasteFiles();
        QVERIFY(runner);
        QSignalSpy done(runner, &OperationRunner::finished);
        QVERIFY(done.wait());
        QCOMPARE(QDir(base).entryList(QDir::Files), QStringList{"a.txt"});
        // Still cut, ready to be pasted somewhere else.
        QVERIFY(FileClipboard::instance()->isCut(base + "/a.txt"));
        // What couldn't be moved stays; what was moved doesn't.
        const FileClipboard::Files pasted{{base + "/a.txt", base + "/gone.txt"}, true};
        FileClipboard::instance()->setFiles(pasted.paths, true);
        FileClipboard::instance()->movedAway(pasted);
        QCOMPARE(FileClipboard::instance()->files().paths, QStringList{base + "/a.txt"});
        // Something else copied meanwhile is left alone.
        FileClipboard::instance()->setText("other");
        FileClipboard::instance()->movedAway(pasted);
        QCOMPARE(QGuiApplication::clipboard()->text(), QString("other"));
    }

    void clipboardKeysInThePathFieldEditText()
    {
        QTemporaryDir dir;
        write(dir.path() + "/keep.txt", "k");
        MainWindow w(dir.path());
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        FolderTab *tab = w.activeTab();
        QTRY_COMPARE(tab->itemCount(), 1);
        tab->selectAll();
        FileClipboard::instance()->setText("/pasted");
        tab->editPath();
        QLineEdit *edit = tab->breadcrumb()->editor();
        QTRY_VERIFY(edit->hasFocus());
        edit->selectAll();
        QTest::keyClick(edit, Qt::Key_V, Qt::ControlModifier);
        QCOMPARE(edit->text(), QString("/pasted"));
        edit->selectAll();
        QTest::keyClick(edit, Qt::Key_X, Qt::ControlModifier);
        QCOMPARE(QGuiApplication::clipboard()->text(), QString("/pasted"));
        QVERIFY(edit->text().isEmpty());
    }

    void linksAreMade()
    {
        QTemporaryDir dir;
        const QString base = canonical(dir.path());
        QDir().mkpath(base + "/a/sub");
        QDir().mkpath(base + "/b");
        write(base + "/a/f.txt", "f");
        auto link = [&](FileOperation::LinkType type, const QStringList &sources, const QString &to) {
            FileOperation op(FileOperation::Kind::Link, sources, to);
            op.setLinkType(type);
            op.run();
            return std::make_pair(op.created(), op.errors());
        };
        using Type = FileOperation::LinkType;
        auto r = link(Type::Symbolic, {base + "/a/f.txt", base + "/a/sub"}, base + "/b");
        QCOMPARE(r.first, QStringList({base + "/b/f.txt", base + "/b/sub"}));
        char target[4096] = {};
        auto readLink = [&](const QString &path) {
            const ssize_t n = ::readlink(path.toLocal8Bit().constData(), target, sizeof target);
            return QString::fromLocal8Bit(target, n < 0 ? 0 : n);
        };
        QCOMPARE(readLink(base + "/b/f.txt"), base + "/a/f.txt");
        // A taken name gets a number; nothing is replaced.
        r = link(Type::Relative, {base + "/a/f.txt"}, base + "/b");
        QCOMPARE(r.first, QStringList{base + "/b/f (2).txt"});
        QCOMPARE(readLink(base + "/b/f (2).txt"), QString("../a/f.txt"));
        QCOMPARE(read(base + "/b/f (2).txt"), QByteArray("f"));
        // Beside the original it is called a link.
        r = link(Type::Hard, {base + "/a/f.txt"}, base + "/a");
        QCOMPARE(r.first, QStringList{base + "/a/Link to f.txt"});
        struct stat one, two;
        QVERIFY(::stat((base + "/a/f.txt").toLocal8Bit().constData(), &one) == 0);
        QVERIFY(::stat((base + "/a/Link to f.txt").toLocal8Bit().constData(), &two) == 0);
        QCOMPARE(one.st_ino, two.st_ino);
        // Folders can't have hard links; missing items are reported.
        r = link(Type::Hard, {base + "/a/sub", base + "/a/none"}, base + "/b");
        QVERIFY(r.first.isEmpty());
        QCOMPARE(r.second.size(), 2);
        QVERIFY(r.second.first().contains("hard link"));
    }

    // --- CLIP-03 ----------------------------------------------------------

    void namesAreFormatted()
    {
        QTemporaryDir dir;
        const QString base = canonical(dir.path());
        write(base + "/my file.tar.gz", "12345");
        QDir().mkpath(base + "/v1.2");
        const QString file = base + "/my file.tar.gz";
        using Style = NameFormat::Style;
        auto as = [&](Style style, const QString &path) { return NameFormat::apply(NameFormat::format(style), path); };
        QCOMPARE(as(Style::FullPath, file), file);
        QCOMPARE(as(Style::Name, file), QString("my file.tar.gz"));
        QCOMPARE(as(Style::Stem, file), QString("my file.tar"));
        QCOMPARE(as(Style::Stem, base + "/v1.2"), QString("v1.2")); // a folder has no extension
        QCOMPARE(as(Style::Stem, base + "/.bashrc"), QString(".bashrc"));
        QCOMPARE(as(Style::Folder, file), base);
        QCOMPARE(as(Style::Url, file), "file://" + base + "/my%20file.tar.gz");
        QCOMPARE(as(Style::Quoted, file), "'" + file + "'");
        QCOMPARE(NameFormat::quoted("it's"), QString("'it'\\''s'"));
        QCOMPARE(NameFormat::quoted("/usr/bin"), QString("/usr/bin"));
        QCOMPARE(NameFormat::quoted(""), QString("''"));
        QCOMPARE(NameFormat::quoted("a\n"), QString("'a\n'"));
        QCOMPARE(NameFormat::apply("[{NAME}]({ext})\\t{bytes}\\n{nope}{", file),
                 QString("[my file.tar.gz](gz)\t5\n{nope}{"));
        QCOMPARE(NameFormat::apply("<{ext}>", base + "/v1.2"), QString("<>"));
        QCOMPARE(NameFormat::applyAll("{folder}", {file, base + "/v1.2"}, false), base);
        QCOMPARE(NameFormat::applyAll("{name}", {file, base + "/v1.2"}, true), QString("my file.tar.gz v1.2"));
    }

    void namesAreCopiedFromTheWindow()
    {
        QTemporaryDir dir;
        const QString base = canonical(dir.path());
        write(base + "/a.txt", "a");
        write(base + "/b.txt", "b");
        MainWindow w(base);
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        FolderTab *tab = w.activeTab();
        QTRY_COMPARE(tab->itemCount(), 2);
        tab->fileView()->setFocus();
        // Nothing selected: the folder itself.
        tab->selectNone();
        QTest::keyClick(tab->fileView(), Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(QGuiApplication::clipboard()->text(), base);
        tab->selectAll();
        QTest::keyClick(tab->fileView(), Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(QGuiApplication::clipboard()->text(), base + "/a.txt\n" + base + "/b.txt");
        QTest::keyClick(tab->fileView(), Qt::Key_C, Qt::ControlModifier | Qt::AltModifier);
        QCOMPARE(QGuiApplication::clipboard()->text(), QString("a.txt\nb.txt"));
        QVERIFY(FileClipboard::instance()->files().paths.isEmpty()); // text, not files
        QTimer::singleShot(200, &w, [] {
            auto *d = qobject_cast<NameFormatDialog *>(QApplication::activeModalWidget());
            QVERIFY(d);
            d->findChild<QComboBox *>()->setCurrentText("{stem}!");
            d->accept();
        });
        w.copyNamesAs();
        QCOMPARE(QGuiApplication::clipboard()->text(), QString("a!\nb!"));
        QCOMPARE(w.tabOptions().nameFormats, QStringList{"{stem}!"});
    }

    void clickingSelectsInEveryViewMode()
    {
        QTemporaryDir dir;
        for (const char *name : {"a.txt", "b.txt", "c.txt"})
            write(dir.path() + '/' + name, "x");
        FolderTab tab(dir.path());
        tab.resize(700, 500);
        tab.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tab));
        QTRY_COMPARE(tab.itemCount(), 3);
        for (int m = 0; m < int(ViewFormat::Mode::Count); ++m) {
            tab.selectNone();
            tab.setViewMode(ViewFormat::Mode(m));
            QAbstractItemView *view = tab.fileView();
            auto click = [&](int row, Qt::KeyboardModifiers keys) {
                const QRect r = view->visualRect(tab.proxy()->index(row, 0));
                QTest::mouseClick(view->viewport(), Qt::LeftButton, keys, r.center());
            };
            click(0, Qt::NoModifier);
            QCOMPARE(tab.selectedPaths(), QStringList{canonical(dir.path()) + "/a.txt"});
            click(2, Qt::ControlModifier);
            QCOMPARE(tab.selectedCount(), 2);
        }
        // Changing the mode keeps the selection.
        tab.selectNames({"b.txt", "c.txt"});
        for (int m = 0; m < int(ViewFormat::Mode::Count); ++m) {
            tab.setViewMode(ViewFormat::Mode(m));
            QCOMPARE(tab.selectedCount(), 2);
        }
    }

    void iconModesLayOutAnyOrder()
    {
        QTemporaryDir dir;
        write(dir.path() + "/a", "x");
        const QString longer = "a considerably longer name than fits in any one cell of the grid.txt";
        write(dir.path() + '/' + longer, "x");
        for (int i = 0; i < 40; ++i)
            write(dir.path() + QString("/file %1.txt").arg(i), "x");
        FolderTab tab(dir.path());
        tab.resize(700, 400);
        tab.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tab));
        QTRY_COMPARE(tab.itemCount(), 42);
        QListView *icons = tab.iconView();
        for (auto mode : {ViewFormat::Mode::LargeIcons, ViewFormat::Mode::Thumbnails, ViewFormat::Mode::SmallIcons,
                          ViewFormat::Mode::Tiles}) {
            // List runs down in columns; the others run across all the same.
            tab.setViewMode(ViewFormat::Mode::List);
            tab.setViewMode(mode);
            QCoreApplication::processEvents();
            const QRect first = icons->visualRect(tab.proxy()->index(0, 0));
            const QRect second = icons->visualRect(tab.proxy()->index(1, 0));
            QVERIFY2(second.left() > first.left(), qPrintable(ViewFormat::modeTitle(mode)));
            QVERIFY(icons->visualRect(tab.proxy()->index(20, 0)).top() > first.bottom());
            // Names aren't cut to the first one's width.
            if (mode == ViewFormat::Mode::SmallIcons)
                QVERIFY(second.width() > 2 * first.width());
            // Nor do long ones run into the next item.
            QVERIFY(!second.intersects(icons->visualRect(tab.proxy()->index(2, 0))));
        }
    }

    // --- SEL-11 -----------------------------------------------------------

    void accentsAreStripped()
    {
        const QString name = QString::fromUtf8("Crème Brûlée.txt");
        NameMatch::Stripped s = NameMatch::strip(name, true);
        QCOMPARE(s.text, QString("Creme Brulee.txt"));
        QCOMPARE(s.original(6), 6);
        QCOMPARE(NameMatch::strip(name, false).text, name);
        // Accents spelled separately, as names from a Mac have them.
        const QString apart = name.normalized(QString::NormalizationForm_D);
        s = NameMatch::strip(apart, true);
        QCOMPARE(s.text, QString("Creme Brulee.txt"));
        QCOMPARE(s.original(3), 4);
        QCOMPARE(s.original(int(s.text.size())), int(apart.size()));
        const QString emoji = QString::fromUtf8("é😀é");
        QCOMPARE(NameMatch::strip(emoji, true).text, QString::fromUtf8("e😀e"));
        QCOMPARE(NameMatch::lengthWithoutExtension("a.tar.gz", false), 5);
        QCOMPARE(NameMatch::lengthWithoutExtension("v1.2", true), 4);
    }

    void findMatchesNames()
    {
        FindOptions o;
        auto at = [&o](const char *text, const char *name, bool dir = false) {
            return FindMatcher(QString::fromUtf8(text), o).match(QString::fromUtf8(name), dir);
        };
        using Range = QPair<int, int>;
        QCOMPARE(at("REP", "report.pdf"), Range(0, 3));
        QCOMPARE(at("port", "report.pdf"), Range(2, 4));
        QCOMPARE(at("resume", "My Résumé.pdf"), Range(3, 6));
        QCOMPARE(at("résumé", "My resume.pdf"), Range(3, 6));
        QCOMPARE(at("pdf", "report.pdf"), Range(7, 3));
        QCOMPARE(at("x", "report.pdf"), Range(-1, 0));
        QCOMPARE(at("", "report.pdf"), Range(-1, 0));
        o.ignoreExtensions = true;
        QCOMPARE(at("pdf", "report.pdf"), Range(-1, 0));
        QCOMPARE(at("pdf", "my.pdf", true), Range(3, 3));
        o.anywhere = false;
        QCOMPARE(at("port", "report.pdf"), Range(-1, 0));
        o.ignoreAccents = false;
        QCOMPARE(at("resume", "Résumé.pdf"), Range(-1, 0));
    }

    void typingFindsNames()
    {
        QTemporaryDir dir;
        const QString base = canonical(dir.path());
        for (const char *name : {"alpha.txt", "beta.txt", "delta.txt", "dingo.txt", "dog.txt", "my dog.txt", "zeta.txt"})
            write(base + '/' + name, "x");
        QDir().mkpath(base + "/sub");
        MainWindow w(base);
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        FolderTab *tab = w.activeTab();
        QTRY_COMPARE(tab->itemCount(), 8);
        QWidget *list = tab->fileView();
        list->setFocus();
        QLineEdit *field = tab->findBar()->editor();
        auto found = [&] { return tab->selectedPaths().isEmpty() ? QString() : QFileInfo(tab->selectedPaths().first()).fileName(); };

        QTest::keyClick(list, Qt::Key_D);
        QVERIFY(field->hasFocus());
        QCOMPARE(field->text(), QString("d"));
        QCOMPARE(found(), QString("delta.txt"));
        QTest::keyClick(field, Qt::Key_O);
        QCOMPARE(found(), QString("dog.txt")); // a start is preferred to "my dog"
        QTest::keyClick(field, Qt::Key_Down);
        QCOMPARE(found(), QString("my dog.txt"));
        QTest::keyClick(field, Qt::Key_F3);
        QCOMPARE(found(), QString("dog.txt")); // round again
        QTest::keyClick(field, Qt::Key_Up);
        QCOMPARE(found(), QString("my dog.txt"));
        // Backspace edits; it doesn't go up a folder.
        QTest::keyClick(field, Qt::Key_Backspace);
        QCOMPARE(tab->currentPath(), base);
        QCOMPARE(field->text(), QString("d"));
        QTest::keyClicks(field, " x");
        QCOMPARE(found(), QString("delta.txt")); // no match leaves the cursor
        // Escape closes it and keeps what was found.
        QTest::keyClick(field, Qt::Key_Escape);
        QVERIFY(tab->findBar()->isHidden());
        QVERIFY(list->hasFocus());
        QCOMPARE(found(), QString("delta.txt"));

        // One letter repeated steps through the names starting with it.
        tab->focusName("alpha.txt");
        for (const char *name : {"delta.txt", "dingo.txt", "dog.txt", "delta.txt"}) {
            QTest::keyClick(tab->findBar()->isHidden() ? list : field, Qt::Key_D);
            QCOMPARE(found(), QString(name));
        }
        // Clicking in the list closes it.
        QTest::mouseClick(tab->fileView()->viewport(), Qt::LeftButton, Qt::NoModifier,
                          tab->fileView()->visualRect(tab->proxy()->index(0, 0)).center());
        QVERIFY(tab->findBar()->isHidden());
        // F3 repeats the last search.
        QTest::keyClick(list, Qt::Key_F3);
        QVERIFY(found().startsWith('d'));

        // Enter opens a folder; a new folder starts afresh.
        QTest::keyClick(list, Qt::Key_S);
        QCOMPARE(found(), QString("sub"));
        QTest::keyClick(field, Qt::Key_Return);
        QCOMPARE(tab->currentPath(), base + "/sub");
        QVERIFY(tab->findBar()->isHidden());
        // Space and shortcuts don't start it.
        QTest::keyClick(list, Qt::Key_Space);
        QTest::keyClick(list, Qt::Key_I, Qt::ControlModifier);
        QVERIFY(tab->findBar()->isHidden());
        // Enter on text that matches nothing opens nothing.
        QTest::keyClick(list, Qt::Key_Q);
        QTest::keyClick(field, Qt::Key_Return);
        QCOMPARE(tab->currentPath(), base + "/sub");
    }

    void findCopiesTheFoundFile()
    {
        QTemporaryDir dir;
        const QString base = canonical(dir.path());
        write(base + "/one.txt", "1");
        write(base + "/two.txt", "2");
        MainWindow w(base);
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        FolderTab *tab = w.activeTab();
        QTRY_COMPARE(tab->itemCount(), 2);
        tab->fileView()->setFocus();
        QTest::keyClick(tab->fileView(), Qt::Key_T);
        QLineEdit *field = tab->findBar()->editor();
        QTest::keyClick(field, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(FileClipboard::instance()->files().paths, QStringList{base + "/two.txt"});
        // With text selected it is the text.
        field->selectAll();
        QTest::keyClick(field, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QGuiApplication::clipboard()->text(), QString("t"));
        // Delete edits the text and never the files.
        field->end(false);
        QTest::keyClick(field, Qt::Key_Delete);
        QCOMPARE(QDir(base).entryList(QDir::Files).size(), 2);
    }

    void findOptionsApply()
    {
        QTemporaryDir dir;
        for (const char *name : {"report final.txt", "report.txt", "a report.txt", "zoo.txt"})
            write(dir.path() + '/' + name, "x");
        FolderTab tab(dir.path());
        tab.resize(600, 300);
        tab.show();
        QVERIFY(QTest::qWaitForWindowActive(&tab));
        QTRY_COMPARE(tab.itemCount(), 4);
        auto current = [&] { return tab.fileView()->currentIndex().siblingAtColumn(0).data(Qt::EditRole).toString(); };
        auto marks = [&] { return static_cast<MatchScrollBar *>(tab.fileView()->verticalScrollBar())->marks().size(); };
        tab.findBar()->open("rep");
        QCOMPARE(current(), QString("report final.txt"));
        QCOMPARE(marks(), 3);
        FindOptions o;
        o.preferShorter = true;
        tab.setFindOptions(o);
        QCOMPARE(current(), QString("report.txt"));
        o.anywhere = false;
        o.scrollMarks = false;
        tab.setFindOptions(o);
        QCOMPARE(marks(), 0);
        o.scrollMarks = true;
        o.keepHighlights = true;
        tab.setFindOptions(o);
        QCOMPARE(marks(), 2);
        // Kept until Escape in the list; the selection goes with the next one.
        QTest::keyClick(tab.findBar()->editor(), Qt::Key_Escape);
        QCOMPARE(marks(), 2);
        // A new file that matches is marked as well.
        write(dir.path() + "/repeat.txt", "x");
        QTRY_COMPARE(marks(), 3);
        QTest::keyClick(tab.fileView(), Qt::Key_Escape);
        QCOMPARE(marks(), 0);
        QCOMPARE(tab.selectedCount(), 1);
        // In Power mode only the cursor moves, not the gathered selection.
        tab.setViewMode(ViewFormat::Mode::Power);
        tab.selectNames({"zoo.txt"});
        tab.findBar()->open("report f");
        QCOMPARE(current(), QString("report final.txt"));
        QCOMPARE(tab.selectedPaths(), QStringList{canonical(dir.path()) + "/zoo.txt"});
    }

    // --- FILT-01 ----------------------------------------------------------

    void quickFilterMatches()
    {
        QuickFilter f;
        auto shows = [&f](const char *name, bool dir = false) {
            return QuickFilter::Matcher(f).matches(QString::fromUtf8(name), dir);
        };
        QVERIFY(shows("anything"));
        f.text = "port";
        QVERIFY(shows("Report.pdf"));
        QVERIFY(!shows("notes.txt"));
        f.text = "my file; *.JPG";
        QVERIFY(shows("not my file.txt"));
        QVERIFY(shows("a.jpg"));
        QVERIFY(!shows("a.jpg.bak")); // wildcards say where the rest may go
        QVERIFY(!shows("my.file"));
        f.text = ";";
        QVERIFY(shows("a"));
        f.text = "[2026] a?c";
        QVERIFY(shows("[2026] abc"));
        QVERIFY(!shows("2 abc"));
        f.text = "resume";
        QVERIFY(shows("Mon Résumé.pdf"));
        f.mode.ignoreAccents = false;
        QVERIFY(!shows("Mon Résumé.pdf"));
        f.mode.anyWord = true;
        f.text = "cat  dog";
        QVERIFY(shows("hotdog.txt"));
        QVERIFY(shows("cat"));
        QVERIFY(!shows("cow"));
        f.mode.anyWord = false;
        QVERIFY(!shows("hotdog.txt"));
        f.mode.partial = false;
        f.text = "report.pdf";
        QVERIFY(shows("Report.PDF"));
        QVERIFY(!shows("my report.pdf"));
        f.mode.ignoreExtensions = true;
        f.text = "report";
        QVERIFY(shows("report.pdf"));
        QVERIFY(!shows("report.pdf", true)); // a folder has no extension
        f.mode.ignoreExtensions = false;
        f.mode.partial = true;
        f.mode.regex = true;
        f.text = "^img_\\d+";
        QVERIFY(shows("IMG_12.jpg"));
        QVERIFY(!shows("IMG_x.jpg"));
        QVERIFY(f.error().isEmpty());
        f.text = "(";
        QVERIFY(!f.error().isEmpty());
    }

    void filterBarFilters()
    {
        QTemporaryDir dir, config;
        const QString base = canonical(dir.path());
        for (const char *name : {"a.jpg", "b.jpg", "c.png", "notes.txt", ".dot.jpg"})
            write(base + '/' + name, "x");
        QDir().mkpath(base + "/photos");
        QSettings settings(config.path() + "/c.conf", QSettings::IniFormat);
        MainWindow w(base, &settings);
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        FolderTab *tab = w.activeTab();
        QTRY_COMPARE(tab->itemCount(), 5);
        QWidget *list = tab->fileView();
        list->setFocus();
        tab->selectNames({"notes.txt"});
        QTest::keyClick(list, Qt::Key_F, Qt::ControlModifier);
        QLineEdit *field = tab->filterBar()->editor();
        QVERIFY(field->hasFocus());
        QTest::keyClicks(field, "*.jpg");
        QCOMPARE(tab->itemCount(), 2);
        QCOMPARE(tab->filteredOutCount(), 3);
        QCOMPARE(tab->hiddenCount(), 1);
        QVERIFY(tab->statusText().contains("3 filtered out"));
        QVERIFY(tab->statusText().contains("1 hidden"));
        QCOMPARE(tab->selectedPaths(), QStringList{base + "/a.jpg"}); // the first match

        // Enter goes to the list with the filter still on; find works there.
        QTest::keyClick(field, Qt::Key_Return);
        QVERIFY(list->hasFocus());
        QCOMPARE(tab->itemCount(), 2);
        QTest::keyClick(list, Qt::Key_B);
        QCOMPARE(tab->selectedPaths(), QStringList{base + "/b.jpg"});
        QTest::keyClick(tab->findBar()->editor(), Qt::Key_Escape);
        // Files that come and go are filtered too.
        write(base + "/d.jpg", "x");
        write(base + "/e.png", "x");
        QTRY_COMPARE(tab->itemCount(), 3);
        QTRY_COMPARE(tab->filteredOutCount(), 4);
        // A name that stops matching says why it went.
        QVERIFY(tab->renameItem("d.jpg", "d.gif"));
        QVERIFY(tab->statusText().contains("doesn't match the filter"));
        // Select All takes only what shows.
        tab->selectAll();
        QCOMPARE(tab->selectedCount(), 2);
        // Escape in the list: the selection first, then the filter.
        QTest::keyClick(list, Qt::Key_Escape);
        QCOMPARE(tab->itemCount(), 2);
        QTest::keyClick(list, Qt::Key_Escape);
        QCOMPARE(tab->itemCount(), 7);
        QVERIFY(tab->filterBar()->isHidden());
        QVERIFY(list->hasFocus());

        // A half-typed expression keeps the last whole one.
        QTest::keyClick(list, Qt::Key_F, Qt::ControlModifier);
        for (QAction *a : tab->filterBar()->optionsMenu()->actions())
            if (a->text() == "Regular Expression")
                a->trigger();
        QTest::keyClicks(field, "png");
        QCOMPARE(tab->itemCount(), 2);
        QTest::keyClicks(field, "(");
        QCOMPARE(tab->itemCount(), 2);
        // These ways of matching can become the defaults.
        for (QAction *a : tab->filterBar()->optionsMenu()->actions())
            if (a->text() == "Make These the Defaults")
                a->trigger();
        QVERIFY(w.tabOptions().filterMode.regex);
        QVERIFY(TabOptions::load(settings).filterMode.regex);

        // Another folder clears it.
        QVERIFY(tab->openPath(base + "/photos"));
        QVERIFY(tab->filterBar()->isHidden());
        QVERIFY(!tab->proxy()->quickFilter().isActive());
        QVERIFY(tab->fileView()->hasFocus());
        // Unless it's asked to stay.
        TabOptions options = w.tabOptions();
        options.filterClearOnNavigate = false;
        options.filterRealTime = false;
        w.setTabOptions(options);
        QTest::keyClick(list, Qt::Key_F, Qt::ControlModifier);
        QTest::keyClicks(field, "png");
        QVERIFY(!tab->proxy()->quickFilter().isActive()); // waits for Enter
        QTest::keyClick(field, Qt::Key_Return);
        QVERIFY(tab->openPath(base));
        QTRY_COMPARE(tab->itemCount(), 2);
        QVERIFY(!tab->filterBar()->isHidden());
        // Closing it by its button shows everything.
        tab->filterBar()->close();
        QCOMPARE(tab->itemCount(), 7);
    }

    void filteringOneSideLeavesTheKeyboardAlone()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.path() + "/sub");
        MainWindow w(dir.path());
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        w.setDual(true);
        FolderTab *left = w.display(0)->currentTab();
        FolderTab *right = w.display(1)->currentTab();
        QTRY_COMPARE(left->itemCount(), 1);
        left->openFilterBar();
        right->fileView()->setFocus();
        QVERIFY(left->openPath(dir.path() + "/sub"));
        QVERIFY(left->filterBar()->isHidden());
        QVERIFY(right->fileView()->hasFocus());
    }

    // --- CUST-15 ----------------------------------------------------------

    void rightClickMenuFitsWhatWasClicked()
    {
        QTemporaryDir dir;
        const QString base = canonical(dir.path());
        for (const char *name : {"a.txt", "b.txt", "c.txt"})
            write(base + '/' + name, "x");
        QDir().mkpath(base + "/folder");
        MainWindow w(base);
        w.resize(900, 600);
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        FolderTab *tab = w.activeTab();
        QTRY_COMPARE(tab->itemCount(), 4);
        QStringList shown;
        // Right-clicks, notes the menu's entries, and closes it.
        auto rightClick = [&](const QPoint &at) {
            QWidget *viewport = tab->fileView()->viewport();
            QTest::mousePress(viewport, Qt::RightButton, Qt::NoModifier, at);
            QContextMenuEvent event(QContextMenuEvent::Mouse, at, viewport->mapToGlobal(at));
            QApplication::sendEvent(viewport, &event);
            QTest::mouseRelease(viewport, Qt::RightButton, Qt::NoModifier, at);
            shown.clear();
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QVERIFY(menu);
            for (QAction *a : menu->actions())
                if (!a->isSeparator())
                    shown << a->text();
            menu->close();
        };
        auto itemAt = [&](const char *name) {
            const int row = tab->proxy()->mapFromSource(tab->model()->index(tab->model()->rowOf(name), 0)).row();
            return tab->fileView()->visualRect(tab->proxy()->index(row, 0)).center();
        };
        for (int m = 0; m < int(ViewFormat::Mode::Count); ++m) {
            tab->setViewMode(ViewFormat::Mode(m));
            tab->selectNames({"a.txt"});
            // An unselected item: that one alone.
            rightClick(itemAt("b.txt"));
            QCOMPARE(tab->selectedPaths(), QStringList{base + "/b.txt"});
            QVERIFY(shown.contains("Open"));
            QVERIFY(shown.contains("Rename"));
            QVERIFY(shown.contains("Move to Trash"));
            QVERIFY(!shown.contains("Paste Into Folder"));
            QVERIFY(!shown.contains("Copy to Other Side")); // there's no other side
            // One of several selected: all of them.
            tab->selectNames({"a.txt", "c.txt"});
            rightClick(itemAt("c.txt"));
            QCOMPARE(tab->selectedCount(), 2);
            QVERIFY(shown.contains("Open")); // all of them (IDEA-11)
            QVERIFY(!shown.contains("Rename"));
            QVERIFY(shown.contains("Duplicate"));
            rightClick(itemAt("folder"));
            QVERIFY(shown.contains("Open in New Tab"));
            QVERIFY(shown.contains("Paste Into Folder"));
            // Empty space: the folder's own menu.
            rightClick(QPoint(tab->fileView()->viewport()->width() - 4, tab->fileView()->viewport()->height() - 4));
            QVERIFY(shown.contains("Create Folder…"));
            QVERIFY(shown.contains("Paste"));
            QVERIFY(shown.contains("View Mode"));
            QVERIFY(!shown.contains("Move to Trash"));
        }
        w.setDual(true);
        tab->selectNames({"a.txt"});
        rightClick(itemAt("a.txt"));
        QVERIFY(shown.contains("Copy to Other Side"));

        // The menu key is about the selection, with the cursor on it.
        tab->setViewMode(ViewFormat::Mode::Details);
        tab->focusName("a.txt");
        tab->selectNames({"b.txt"});
        tab->fileView()->selectionModel()->setCurrentIndex(tab->proxy()->index(0, 0), QItemSelectionModel::NoUpdate);
        QSignalSpy asked(tab, &FolderTab::contextMenuRequested);
        QContextMenuEvent key(QContextMenuEvent::Keyboard, QPoint(), QPoint());
        QApplication::sendEvent(tab->fileView(), &key);
        QCOMPARE(asked.size(), 1);
        QVERIFY(asked.at(0).at(1).toBool());
        QCOMPARE(tab->fileView()->currentIndex().siblingAtColumn(0).data(Qt::EditRole).toString(), QString("b.txt"));
        if (QWidget *menu = QApplication::activePopupWidget())
            menu->close();
    }

    void rightClickMenuActsOnTheClickedSide()
    {
        QTemporaryDir dir;
        const QString base = canonical(dir.path());
        QDir().mkpath(base + "/left/inner");
        QDir().mkpath(base + "/right");
        write(base + "/left/l.txt", "l");
        write(base + "/right/r.txt", "r");
        MainWindow w(base + "/left");
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        w.setDual(true);
        FolderTab *left = w.display(0)->currentTab();
        FolderTab *right = w.display(1)->currentTab();
        QVERIFY(right->openPath(base + "/right"));
        QTRY_COMPARE(left->itemCount(), 2);
        QTRY_COMPARE(right->itemCount(), 1);
        // Locked: the left is the source even while the right is in use.
        w.activateSide(0);
        w.lockActiveSide(Role::Source);
        w.activateSide(1);
        QCOMPARE(w.sourceTab(), left);
        left->selectNames({"l.txt"});
        right->selectNames({"r.txt"});
        auto trigger = [&](const QString &text) {
            QMenu *menu = w.contextMenu(true);
            for (QAction *a : menu->actions())
                if (a->text() == text) {
                    a->trigger();
                    return true;
                }
            return false;
        };
        QVERIFY(trigger("Copy to Other Side"));
        QTRY_COMPARE(read(base + "/left/r.txt"), QByteArray("r"));
        QVERIFY(!QFileInfo::exists(base + "/right/l.txt"));

        // Paste into a folder without opening it.
        w.activateSide(0);
        left->selectNames({"inner"});
        FileClipboard::instance()->setFiles({base + "/left/l.txt"}, true);
        QVERIFY(trigger("Paste Into Folder"));
        QTRY_COMPARE(read(base + "/left/inner/l.txt"), QByteArray("l"));
        QVERIFY(!QFileInfo::exists(base + "/left/l.txt"));
        QCOMPARE(left->currentPath(), base + "/left");
        left->selectNames({"inner"});
        QVERIFY(trigger("Open"));
        QCOMPARE(left->currentPath(), base + "/left/inner");
    }

    // --- NAV-07 -----------------------------------------------------------

    void aliasesExpand()
    {
        const FakeHome home;
        write(home.path() + "/.config/user-dirs.dirs",
              "# comment\nXDG_DOWNLOAD_DIR=\"$HOME/Incoming\"\nXDG_TEMPLATES_DIR=\"$HOME/\"\n"
              "XDG_PROJECTS_DIR=\"$HOME/My Code\"\nXDG_PICTURES_DIR=\"/srv/pics\"\n");
        using namespace FolderAliases;
        setUser({{"work", "~projects/cargo"}, {"Pictures", "/mine"}, {"a", "~b"}, {"b", "~a"}, {"slash", "/"}});
        QCOMPARE(expand("~"), home.path());
        QCOMPARE(expand(" ~/x "), home.path() + "/x");
        QCOMPARE(expand("~downloads"), home.path() + "/Incoming");
        QCOMPARE(expand("~Downloads/new"), home.path() + "/Incoming/new");
        QCOMPARE(expand("~projects"), home.path() + "/My Code"); // not a standard one
        QCOMPARE(expand("~work/src"), home.path() + "/My Code/cargo/src"); // by way of another
        QCOMPARE(expand("~pictures"), QString("/mine")); // the user's comes first
        QCOMPARE(expand("~slash/etc"), QString("/etc"));
        QCOMPARE(expand("~root"), QString("/root")); // a user's home, as in a shell
        QCOMPARE(expand("~nosuchthing/x"), QString("~nosuchthing/x"));
        QCOMPARE(expand("~a/x"), QString("~a/x")); // round in circles
        // A folder here might start with ~.
        QCOMPARE(expand("~nosuchthing", "/base"), QString("/base/~nosuchthing"));
        // /name works unless there is such a folder.
        QCOMPARE(expand("/downloads/x"), home.path() + "/Incoming/x");
        QCOMPARE(expand("/home"), QString("/home"));
        QCOMPARE(expand("/nosuchthing"), QString("/nosuchthing"));
        // As pasted from elsewhere.
        QCOMPARE(expand("\"/usr/my lib\""), QString("/usr/my lib"));
        QCOMPARE(expand("file:///usr/my%20lib"), QString("/usr/my lib"));
        QCOMPARE(expand("sub/x", "/base"), QString("/base/sub/x"));
        QCOMPARE(expand("sub", "/"), QString("/sub"));
        QCOMPARE(expand("", "/base"), QString());

        QCOMPARE(shorten(home.path() + "/My Code/cargo/src"), QString("~work/src"));
        QCOMPARE(shorten(home.path() + "/My Code"), QString("~projects"));
        QCOMPARE(shorten(home.path() + "/My Codex"), QString("~/My Codex"));
        QCOMPARE(shorten("/nowhere"), QString("/nowhere"));
        QCOMPARE(NameFormat::apply("{alias}", home.path() + "/Incoming/f"), QString("~downloads/f"));

        QStringList names;
        for (const Alias &a : builtIn())
            names << a.name;
        QVERIFY(names.contains("config"));
        QVERIFY(!names.contains("templates")); // turned off
        names.clear();
        for (const Alias &a : all())
            names << a.name.toLower();
        QCOMPARE(names.count("pictures"), 1);

        QVERIFY(nameError("my-work_2.0").isEmpty());
        for (const char *bad : {"", "a b", "a/b", "~x", ".x"})
            QVERIFY2(!nameError(bad).isEmpty(), bad);
        QCOMPARE(suggestName("/x/My Code (old)"), QString("my-code-old"));
        QCOMPARE(suggestName("/"), QString());
        setUser({});
    }

    void onlyTypedPathsUseSlashAliases()
    {
        const FakeHome home;
        QDir().mkpath(home.path() + "/Downloads");
        QDir().mkpath(home.path() + "/sub ");
        QDir().mkpath(home.path() + "/rel/deep");
        FolderTab tab(home.path());
        // Going to a folder that has gone must not land somewhere else.
        QVERIFY(!tab.openPath("/downloads"));
        QCOMPARE(tab.currentPath(), canonical(home.path()));
        QVERIFY(tab.openPath(home.path() + "/sub ")); // a name can end in a space
        QVERIFY(tab.openPath("~downloads"));
        QCOMPARE(tab.currentPath(), canonical(home.path()) + "/Downloads");
        // Typed, /name works, and a relative path starts in this folder.
        auto type = [&](const QString &text) {
            tab.editPath();
            tab.breadcrumb()->editor()->setText(text);
            QTest::keyClick(tab.breadcrumb()->editor(), Qt::Key_Return);
        };
        type("..");
        QCOMPARE(tab.currentPath(), canonical(home.path()));
        type("rel/deep");
        QCOMPARE(tab.currentPath(), canonical(home.path()) + "/rel/deep");
        type("/downloads");
        QCOMPARE(tab.currentPath(), canonical(home.path()) + "/Downloads");
    }

    void aliasesAreEditedAndKept()
    {
        const FakeHome home;
        QDir().mkpath(home.path() + "/Code");
        QTemporaryDir config;
        QSettings settings(config.path() + "/c.conf", QSettings::IniFormat);
        MainWindow w(home.path() + "/Code", &settings);
        QTimer::singleShot(100, &w, [] {
            auto *d = qobject_cast<AliasEditDialog *>(QApplication::activeModalWidget());
            QVERIFY(d);
            QCOMPARE(d->alias().name, QString("code")); // named for the folder
            QLineEdit *name = d->findChildren<QLineEdit *>().first();
            QPushButton *ok = nullptr;
            for (QPushButton *b : d->findChildren<QPushButton *>())
                if (b->text().contains("OK"))
                    ok = b;
            QVERIFY(ok && ok->isEnabled());
            name->setText("my code");
            QVERIFY(!ok->isEnabled());
            name->setText("~mine"); // the ~ is let off
            QVERIFY(ok->isEnabled());
            d->accept();
        });
        w.addAlias();
        QCOMPARE(w.tabOptions().aliases.size(), 1);
        QCOMPARE(w.tabOptions().aliases.first().name, QString("mine"));
        QCOMPARE(TabOptions::load(settings).aliases, w.tabOptions().aliases);
        QCOMPARE(FolderAliases::expand("~mine"), canonical(home.path()) + "/Code");
        // A colour rule can use one.
        QVERIFY((TabOptions::ColorRule{"~mine/*", "red"}).matches(canonical(home.path()) + "/Code/x"));

        // The list: search, built-in ones, broken ones.
        AliasDialog dialog({{"gone", "/no/such/place"}, {"mine", "~/Code"}}, nullptr);
        auto *list = dialog.findChild<QTreeWidget *>();
        const int all = list->topLevelItemCount();
        dialog.findChild<QCheckBox *>()->setChecked(false);
        QCOMPARE(list->topLevelItemCount(), 2);
        QVERIFY(all > 2);
        dialog.findChild<QLineEdit *>()->setText("code");
        QCOMPARE(list->topLevelItemCount(), 1);
        QCOMPARE(AliasDialog::broken(dialog.aliases()).size(), 1);
        list->setCurrentItem(list->topLevelItem(0));
        for (QPushButton *b : dialog.findChildren<QPushButton *>())
            if (b->text() == "&Delete")
                b->click();
        QCOMPARE(dialog.aliases().size(), 1);
        QCOMPARE(dialog.aliases().first().name, QString("gone"));

        // Names already used are refused; taking a built-in one's place isn't.
        AliasEditDialog taken("t", {"GONE", "/usr"}, {"gone"}, nullptr);
        for (QPushButton *b : taken.findChildren<QPushButton *>())
            if (b->text().contains("OK"))
                QVERIFY(!b->isEnabled());
        FolderAliases::setUser({});
    }

    // --- NAV-03 -----------------------------------------------------------

    void pathsComplete()
    {
        const FakeHome home;
        for (const char *name : {"Documents", "Downloads", "dotfiles", "Desk", ".cache", "Élan", "node_modules"})
            QDir().mkpath(home.path() + '/' + name);
        QDir().mkpath(home.path() + "/Downloads/inside");
        write(home.path() + "/Dfile", "x");
        QLineEdit edit;
        edit.resize(400, 30);
        auto *completer = new PathCompleter(&edit);
        GlobalFilter filter;
        filter.hideNames = true;
        filter.folders = "node_modules";
        completer->setGlobalFilter(filter);
        completer->setBase(home.path());
        edit.show();
        edit.setFocus();
        QVERIFY(QTest::qWaitForWindowActive(&edit));
        QAbstractItemView *popup = completer->completer()->popup();
        auto type = [&](const QString &text) {
            edit.clear();
            QTest::keyClicks(&edit, text);
        };

        const QString h = home.path();
        type(h + "/D");
        // Folders only; the case typed first.
        QTRY_COMPARE(completer->completions(),
                     QStringList({h + "/Desk", h + "/Documents", h + "/Downloads", h + "/dotfiles"}));
        QVERIFY(popup->isVisible());
        type("do"); // in the base folder
        QTRY_COMPARE(completer->completions(), QStringList({"dotfiles", "Documents", "Downloads"}));
        type("el");
        QTRY_COMPARE(completer->completions(), QStringList{QString::fromUtf8("Élan")});
        type("no"); // hidden by a filter
        QTRY_VERIFY(!popup->isVisible());
        type("."); // a dot asks for hidden ones
        QTRY_COMPARE(completer->completions(), QStringList({".cache", ".config"}));
        type("~/Dow");
        QTRY_COMPARE(completer->completions(), QStringList{"~/Downloads"});
        type("~doc");
        QTRY_COMPARE(completer->completions(), QStringList{"~documents"});
        QDir().mkpath(home.path() + "/Documents/letters");
        type("~documents/l");
        QTRY_COMPARE(completer->completions(), QStringList{"~documents/letters"});
        type(h + "/Downloads"); // whole already
        QTRY_VERIFY(!popup->isVisible());
        type("/no/such/place/x");
        QTest::qWait(50);
        QVERIFY(!popup->isVisible());

        // Arrows show each choice; Escape brings back what was typed.
        type("~/D");
        QTRY_COMPARE(completer->completions().size(), 4);
        QTest::keyClick(popup, Qt::Key_Down);
        QCOMPARE(edit.text(), QString("~/Desk"));
        QTest::keyClick(popup, Qt::Key_Down);
        QCOMPARE(edit.text(), QString("~/Documents"));
        QTest::keyClick(popup, Qt::Key_Up);
        QTest::keyClick(popup, Qt::Key_Up);
        QCOMPARE(edit.text(), QString("~/D"));
        QTest::keyClick(popup, Qt::Key_Up);
        QCOMPARE(edit.text(), QString("~/dotfiles"));
        QTest::keyClick(popup, Qt::Key_Escape);
        QCOMPARE(edit.text(), QString("~/D"));
        QVERIFY(!popup->isVisible());

        // Tab: as much as they share, then one by one, then inside it.
        type("~/Dow");
        QTRY_VERIFY(popup->isVisible());
        QTest::keyClick(popup, Qt::Key_Tab);
        QTRY_COMPARE(edit.text(), QString("~/Downloads/"));
        QTRY_COMPARE(completer->completions(), QStringList{"~/Downloads/inside"});
        type("~/do");
        QTRY_VERIFY(popup->isVisible());
        QTest::keyClick(popup, Qt::Key_Tab);
        QTRY_COMPARE(edit.text(), QString("~/dotfiles"));
        QTest::keyClick(popup, Qt::Key_Tab);
        QCOMPARE(edit.text(), QString("~/dotfiles/"));
        // Enter hands over to the field.
        QSignalSpy entered(&edit, &QLineEdit::returnPressed);
        type("~/D");
        QTRY_COMPARE(completer->completions().size(), 4);
        QTest::keyClick(popup, Qt::Key_Down);
        QTest::keyClick(popup, Qt::Key_Return);
        QCOMPARE(entered.size(), 1);
        QCOMPARE(edit.text(), QString("~/Desk"));
        QVERIFY(!popup->isVisible());

        // Only on arrows.
        PathCompleter::Options o;
        o.onArrows = true;
        completer->setOptions(o);
        type("~/Do");
        QTest::qWait(50);
        QVERIFY(!popup->isVisible());
        QTest::keyClick(&edit, Qt::Key_Down);
        QTRY_VERIFY(popup->isVisible());
        QCOMPARE(edit.text(), QString("~/Documents"));
        QTest::keyClick(popup, Qt::Key_Escape);
        // No list: the arrows and Tab step in the field.
        o.onArrows = false;
        o.popup = false;
        completer->setOptions(o);
        type("~/Do");
        QTest::keyClick(&edit, Qt::Key_Down);
        QTRY_COMPARE(edit.text(), QString("~/Documents"));
        QTest::keyClick(&edit, Qt::Key_Down);
        QCOMPARE(edit.text(), QString("~/Downloads"));
        QVERIFY(!popup->isVisible());
        type("~/Dow");
        QTest::keyClick(&edit, Qt::Key_Tab);
        QTRY_COMPARE(edit.text(), QString("~/Downloads/"));
        // Off.
        o.enabled = false;
        completer->setOptions(o);
        type("~/Do");
        QTest::keyClick(&edit, Qt::Key_Down);
        QTest::qWait(50);
        QCOMPARE(edit.text(), QString("~/Do"));
    }

    void thePathFieldCompletes()
    {
        const FakeHome home;
        QDir().mkpath(home.path() + "/alpha/one");
        QDir().mkpath(home.path() + "/beta");
        MainWindow w(home.path());
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        FolderTab *tab = w.activeTab();
        QTRY_COMPARE(tab->itemCount(), 2);
        Breadcrumb *bar = tab->breadcrumb();
        QAbstractItemView *popup = bar->completer()->completer()->popup();
        tab->editPath();
        QTRY_VERIFY(bar->editor()->hasFocus());
        QTest::keyClick(bar->editor(), Qt::Key_End);
        QTest::keyClicks(bar->editor(), "/al");
        QTRY_VERIFY(popup->isVisible());
        // Escape: the list, then the field.
        QTest::keyClick(popup, Qt::Key_Escape);
        QVERIFY(!popup->isVisible());
        QVERIFY(bar->isEditing());
        QTest::keyClick(bar->editor(), Qt::Key_Escape);
        QVERIFY(!bar->isEditing());
        // Enter on a choice goes there, and the list goes away.
        tab->editPath();
        QTest::keyClick(bar->editor(), Qt::Key_End);
        QTest::keyClicks(bar->editor(), "/al");
        QTRY_COMPARE(bar->completer()->completions().size(), 1);
        QTRY_VERIFY(popup->isVisible());
        QTest::keyClick(popup, Qt::Key_Down);
        QTest::keyClick(popup, Qt::Key_Return);
        QCOMPARE(tab->currentPath(), canonical(home.path()) + "/alpha");
        QVERIFY(!bar->isEditing());
        QVERIFY(!popup->isVisible());
        QVERIFY(tab->fileView()->hasFocus());
        // A click does as well.
        tab->editPath();
        bar->editor()->clear();
        QTest::keyClicks(bar->editor(), "~/b");
        QTRY_COMPARE(bar->completer()->completions(), QStringList{"~/beta"});
        QVERIFY(popup->isVisible());
        const QModelIndex first = popup->model()->index(0, 0);
        // The completer looks for the real pointer, which a test has none of.
        emit popup->clicked(first);
        QTRY_COMPARE(tab->currentPath(), canonical(home.path()) + "/beta");
        // Leaving by a click elsewhere leaves nothing behind.
        tab->editPath();
        bar->editor()->clear();
        QTest::keyClicks(bar->editor(), "~/");
        tab->fileView()->setFocus();
        QTest::qWait(100);
        QVERIFY(!popup->isVisible());
        QVERIFY(!bar->isEditing());
    }

    void chooseFolderTakesAliases()
    {
        const FakeHome home;
        QDir().mkpath(home.path() + "/Downloads");
        ChooseFolderDialog dialog("Copy To", "~downloads/new", "/base", nullptr);
        QCOMPARE(dialog.folder(), home.path() + "/Downloads/new");
        dialog.findChild<QLineEdit *>()->setText("../x");
        QCOMPARE(dialog.folder(), QString("/x"));
    }

    // --- LST-08, TREE-01, TREE-03, TREE-04 --------------------------------

    void treeListsDrives()
    {
        const QByteArray mounts = "/dev/nvme0n1p2 / btrfs rw 0 0\n"
                                  "/dev/nvme0n1p2 /home btrfs rw 0 0\n"
                                  "/dev/nvme0n1p1 /boot vfat rw 0 0\n"
                                  "proc /proc proc rw 0 0\n"
                                  "tmpfs /run/user/1000 tmpfs rw 0 0\n"
                                  "/dev/sda1 /run/media/u/My\\040Stick exfat rw 0 0\n"
                                  "/dev/sdb1 /data ext4 rw 0 0\n"
                                  "nas:/vol /srv/nas nfs4 rw 0 0\n"
                                  "/dev/loop3 /snap/x squashfs ro 0 0\n"
                                  "/dev/sda1 /run/media/u/My\\040Stick exfat rw 0 0\n";
        QCOMPARE(FolderTreeModel::drivesFrom(mounts, "/home/u"),
                 QStringList({"/data", "/run/media/u/My Stick", "/srv/nas"}));
    }

    void treeReadsFolders()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.path() + "/full/inner");
        QDir().mkpath(dir.path() + "/hiddenOnly/.x");
        QDir().mkpath(dir.path() + "/empty");
        QDir().mkpath(dir.path() + "/listed");
        write(dir.path() + "/file", "f");
        write(dir.path() + "/.hidden", "listed\n");
        QVERIFY(QFile::link(dir.path() + "/full", dir.path() + "/link"));
        QHash<QString, FolderTreeModel::Folder> found;
        for (const auto &f : FolderTreeModel::readFolders(dir.path()))
            found.insert(f.name, f);
        QCOMPARE(found.size(), 5);
        QVERIFY(found["full"].anyVisibleInside);
        QVERIFY(found["link"].anyVisibleInside);
        QVERIFY(found["hiddenOnly"].anyInside && !found["hiddenOnly"].anyVisibleInside);
        QVERIFY(!found["empty"].anyInside);
        QVERIFY(found["listed"].listed);
        QVERIFY(FolderTreeModel::readFolders(dir.path() + "/nothing").isEmpty());
    }

    void treeFollowsTheFileList()
    {
        const FakeHome home;
        const QString h = home.path();
        for (const char *name : {"/a/b/c", "/a/b2", "/z/y", "/.secret/in"})
            QDir().mkpath(h + name);
        MainWindow w(h);
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        FolderTree *tree = w.windowTree();
        QVERIFY(tree->isHidden());
        QTest::keyClick(w.activeTab()->fileView(), Qt::Key_F9);
        QVERIFY(tree->isVisible());
        FolderTreeView *view = tree->view();
        auto selected = [&] { return tree->model()->path(view->currentIndex()); };
        auto isExpanded = [&](const QString &path) {
            for (const QString &key : tree->expanded())
                if (key.section('\t', 1) == path)
                    return true;
            return false;
        };
        QTRY_COMPARE(selected(), h);
        QCOMPARE(tree->model()->data(view->currentIndex()).toString(), QString("Home"));

        // Deep down: every branch on the way opens, but not its own.
        QVERIFY(w.activeTab()->openPath(h + "/a/b/c"));
        QTRY_COMPARE(selected(), h + "/a/b/c");
        QVERIFY(isExpanded(h) && isExpanded(h + "/a") && isExpanded(h + "/a/b"));
        w.activeTab()->goUp();
        QTRY_COMPARE(selected(), h + "/a/b");

        // A hidden folder shows while it is open, and goes away after.
        auto showsSecret = [&] {
            const QModelIndex root = tree->model()->rootFor(h);
            return tree->model()->childTowards(root, h + "/.secret").isValid();
        };
        QVERIFY(!showsSecret());
        QVERIFY(w.activeTab()->openPath(h + "/.secret/in"));
        QTRY_COMPARE(selected(), h + "/.secret/in");
        QVERIFY(showsSecret());
        QVERIFY(w.activeTab()->openPath(h + "/z"));
        QTRY_COMPARE(selected(), h + "/z");
        QVERIFY(!showsSecret());
        // With hidden files showing, it is there all the time.
        TabOptions options = w.tabOptions();
        options.filter.showHidden = true;
        w.setTabOptions(options);
        QVERIFY(showsSecret());
        options.tree.hidden = false;
        w.setTabOptions(options);
        QVERIFY(!showsSecret());
        options.tree.hidden = true;
        options.filter.showHidden = false;
        options.filter.hideNames = true;
        options.filter.folders = "y";
        w.setTabOptions(options);
        QTRY_VERIFY(!tree->model()->childTowards(view->currentIndex(), h + "/z/y").isValid());

        // Outside home, it is in the file system.
        QVERIFY(w.activeTab()->openPath("/usr"));
        QTRY_COMPARE(selected(), QString("/usr"));
        QCOMPARE(tree->model()->path(tree->model()->rootOf(view->currentIndex())), QString("/"));

        // A new folder turns up in a branch that is open.
        QVERIFY(w.activeTab()->openPath(h + "/a/b"));
        QTRY_COMPARE(selected(), h + "/a/b");
        QDir().mkpath(h + "/a/fresh");
        QTRY_VERIFY(tree->model()->childTowards(view->currentIndex().parent(), h + "/a/fresh").isValid());
        QVERIFY(QDir(h + "/a/fresh").removeRecursively());
        QTRY_VERIFY(!tree->model()->childTowards(view->currentIndex().parent(), h + "/a/fresh").isValid());
        QCOMPARE(selected(), h + "/a/b");
        QTest::keyClick(w.activeTab()->fileView(), Qt::Key_F9);
        QVERIFY(tree->isHidden());
    }

    void treeOpensFolders()
    {
        const FakeHome home;
        const QString h = home.path();
        QDir().mkpath(h + "/a/b");
        QDir().mkpath(h + "/c");
        write(h + "/a/keep.txt", "k");
        MainWindow w(h + "/a");
        w.resize(900, 600);
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        w.focusTree();
        FolderTree *tree = w.windowTree();
        FolderTreeView *view = tree->view();
        QVERIFY(view->hasFocus());
        FolderTab *tab = w.activeTab();
        QTRY_COMPARE(tree->currentPath(), h + "/a");
        QTRY_COMPARE(tab->itemCount(), 2);
        auto place = [&](const QString &path) {
            const QModelIndex root = tree->model()->rootFor(h);
            QModelIndex at = root;
            while (at.isValid() && tree->model()->path(at) != path)
                at = tree->model()->childTowards(at, path);
            return at;
        };
        auto click = [&](const QString &path, Qt::MouseButton button = Qt::LeftButton) {
            QCoreApplication::processEvents();
            const QModelIndex at = place(path);
            QVERIFY(at.isValid());
            QTest::mouseClick(view->viewport(), button, Qt::NoModifier, view->visualRect(at).center());
        };
        click(h + "/c");
        QCOMPARE(tab->currentPath(), h + "/c");
        QCOMPARE(tree->currentPath(), h + "/c");
        click(h + "/a", Qt::MiddleButton);
        QCOMPARE(w.display(0)->tabCount(), 2);
        QTRY_COMPARE(tree->currentPath(), h + "/a");
        w.display(0)->setCurrentIndex(0);
        QTRY_COMPARE(tree->currentPath(), h + "/c");
        // The right button goes nowhere.
        click(h + "/a", Qt::RightButton);
        QCOMPARE(tab->currentPath(), h + "/c");
        QCOMPARE(tree->model()->path(view->currentIndex()), h + "/c");

        // Keys: the folder opens once the cursor rests, or on Enter.
        view->setFocus();
        QTest::keyClick(view, Qt::Key_Up);
        QCOMPARE(tab->currentPath(), h + "/c");
        QTRY_COMPARE(tab->currentPath(), h + "/a");
        QTest::keyClick(view, Qt::Key_Down);
        QTest::keyClick(view, Qt::Key_Return);
        QCOMPARE(tab->currentPath(), h + "/c");
        QTest::keyClick(view, Qt::Key_Up);
        QTRY_COMPARE(tab->currentPath(), h + "/a");

        // File keys never act on the list's selection from here.
        QTRY_COMPARE(tab->itemCount(), 2);
        tab->selectAll();
        view->setFocus();
        for (auto key : {Qt::Key_Delete, Qt::Key_F2})
            QTest::keyClick(view, key);
        QTest::keyClick(view, Qt::Key_Delete, Qt::ShiftModifier);
        QTest::keyClick(view, Qt::Key_D, Qt::ControlModifier);
        QTest::qWait(100);
        QCOMPARE(QDir(h + "/a").entryList(QDir::AllEntries | QDir::NoDotAndDotDot), QStringList({"b", "keep.txt"}));
        QVERIFY(!QApplication::activeModalWidget());
        // The clipboard is for the tree's folder.
        QTest::keyClick(view, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(FileClipboard::instance()->files().paths, QStringList{h + "/a"});
        QVERIFY(QFile::copy(h + "/a/keep.txt", h + "/c/other.txt"));
        FileClipboard::instance()->setFiles({h + "/c/other.txt"}, false);
        QTest::keyClick(view, Qt::Key_V, Qt::ControlModifier);
        QTRY_VERIFY(QFileInfo::exists(h + "/a/other.txt"));
        QTest::keyClick(view, Qt::Key_Tab);
        QVERIFY(tab->fileView()->hasFocus());

        // A locked tab stays where it is, and so does the tree.
        tab->setLock(FolderTab::Lock::Locked);
        const int tabs = w.display(0)->tabCount();
        click(h + "/c");
        QCOMPARE(tab->currentPath(), h + "/a");
        QCOMPARE(w.display(0)->tabCount(), tabs + 1);
        QTRY_COMPARE(tree->currentPath(), w.activeTab()->currentPath());
    }

    void treeStaysInItsBranch()
    {
        const FakeHome home;
        const QString h = home.path();
        QDir().mkpath(h + "/a/b");
        FolderTree tree;
        tree.setOptions({}, {});
        auto root = [&] { return tree.model()->path(tree.model()->rootOf(tree.view()->currentIndex())); };
        // Walking down from / into home stays in the file system…
        tree.follow("/");
        QTRY_COMPARE(tree.currentPath(), QString("/"));
        QString at;
        for (const QString &part : (h + "/a").split('/', Qt::SkipEmptyParts)) {
            at += '/' + part;
            tree.follow(at);
            QTRY_COMPARE(tree.currentPath(), at);
        }
        QCOMPARE(root(), QString("/"));
        // …and a jump goes to the nearest top-level item.
        tree.follow("/usr/share");
        QTRY_COMPARE(tree.currentPath(), QString("/usr/share"));
        tree.follow(h + "/a/b");
        QTRY_COMPARE(tree.currentPath(), h + "/a/b");
        QCOMPARE(root(), h);
        // Up past the top of Home changes over.
        tree.follow(QFileInfo(h).path());
        QTRY_COMPARE(tree.currentPath(), QFileInfo(h).path());
        QCOMPARE(root(), QString("/"));
        // A folder that has gone gets as near as it can.
        tree.follow(h + "/a/gone/deeper");
        QTRY_COMPARE(tree.model()->path(tree.view()->currentIndex()), h + "/a");
    }

    void treeCollapsesOthers()
    {
        const FakeHome home;
        const QString h = home.path();
        for (const char *name : {"/a/b/c", "/m/n/o", "/x/y/z"})
            QDir().mkpath(h + name);
        FolderTree tree;
        TreeOptions o;
        o.collapseOthers = true;
        tree.setOptions(o, {});
        QStringList open;
        tree.setOpenFolders([&] { return open; });
        auto branches = [&] {
            QStringList list;
            for (const QString &key : tree.expanded())
                list << key.section('\t', 1).mid(h.size());
            list.sort();
            return list;
        };
        tree.follow(h + "/a/b/c");
        QTRY_COMPARE(tree.currentPath(), h + "/a/b/c");
        QCOMPARE(branches(), QStringList({"", "/a", "/a/b"}));
        open << h + "/m/n/o" << h + "/x/y/z";
        tree.follow(h + "/m/n/o");
        QTRY_COMPARE(tree.currentPath(), h + "/m/n/o");
        QCOMPARE(branches(), QStringList({"", "/m", "/m/n"}));
        tree.follow(h + "/x/y/z");
        QTRY_COMPARE(tree.currentPath(), h + "/x/y/z");
        QCOMPARE(branches(), QStringList({"", "/m", "/m/n", "/x", "/x/y"})); // m is open in a tab
        o.collapseUnlessOpen = false;
        o.expandSelected = true;
        tree.setOptions(o, {});
        tree.follow(h + "/a/b");
        QTRY_COMPARE(tree.currentPath(), h + "/a/b");
        QCOMPARE(branches(), QStringList({"", "/a", "/a/b"}));
        // Not expanding: as far as is open already.
        o = {};
        o.autoExpand = false;
        tree.setOptions(o, {});
        tree.collapseAll();
        tree.follow(h + "/x/y");
        QTRY_COMPARE(tree.model()->path(tree.view()->currentIndex()), h);
        QCOMPARE(branches(), QStringList());
    }

    void treeIsRemembered()
    {
        const FakeHome home;
        const QString h = home.path();
        for (const char *name : {"/a/b/c", "/m/n/o"})
            QDir().mkpath(h + name);
        ListerState state;
        {
            MainWindow w(h + "/a/b");
            w.resize(900, 500);
            w.show();
            w.setTreeShown(true);
            QTRY_COMPARE(w.windowTree()->currentPath(), h + "/a/b");
            QVERIFY(w.activeTab()->openPath(h + "/m/n"));
            QTRY_COMPARE(w.windowTree()->currentPath(), h + "/m/n");
            state = w.state();
            QVERIFY(state.treeShown);
            QVERIFY(state.trees[2].width > 100);
            QCOMPARE(state.trees[2].expanded.size(), 3); // home, a, m
        }
        QTemporaryDir config;
        QSettings settings(config.path() + "/c.conf", QSettings::IniFormat);
        state.trees[2].width = 310;
        state.save(settings, "lister");
        MainWindow w(ListerState::load(settings, "lister"));
        w.resize(1000, 500);
        w.show();
        QVERIFY(w.isTreeShown());
        QTRY_COMPARE(w.windowTree()->currentPath(), h + "/m/n");
        QTRY_COMPARE(w.windowTree()->expanded().size(), 3);
        QTRY_COMPARE(w.windowTree()->width(), 310);

        // Each tab its own branches.
        TabOptions options = w.tabOptions();
        options.tree.rememberTabs = true;
        w.setTabOptions(options);
        w.display(0)->addTab(h + "/m");
        QTRY_COMPARE(w.windowTree()->currentPath(), h + "/m");
        w.windowTree()->collapseAll();
        w.display(0)->setCurrentIndex(0);
        QTRY_COMPARE(w.windowTree()->expanded().size(), 3);
        w.display(0)->setCurrentIndex(1);
        QTRY_COMPARE(w.windowTree()->currentPath(), h + "/m");
        QTRY_VERIFY(w.windowTree()->expanded().size() < 3);
        QCOMPARE(w.state().sides[0].tabs.at(0).tree.size(), 3);
        QVERIFY(TreeOptions() == TreeOptions::load(settings));
        options.tree.style = TreeOptions::Style::Lines;
        options.tree.indent = 30;
        options.tree.save(settings);
        QVERIFY(TreeOptions::load(settings) == options.tree);
        TreeOptionsDialog dialog(options.tree);
        QVERIFY(dialog.options() == options.tree);
    }

    void treeForEachSide()
    {
        const FakeHome home;
        const QString h = home.path();
        QDir().mkpath(h + "/left");
        QDir().mkpath(h + "/right");
        MainWindow w(h + "/left");
        w.resize(1200, 500);
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        w.setTreeShown(true);
        w.setDual(true);
        QVERIFY(w.display(1)->currentTab()->openPath(h + "/right"));
        // One for both: it follows the side in use.
        QVERIFY(w.windowTree()->isVisible() && w.sideTree(0)->isHidden() && w.sideTree(1)->isHidden());
        QTRY_COMPARE(w.windowTree()->currentPath(), h + "/left");
        w.activateSide(1);
        QTRY_COMPARE(w.windowTree()->currentPath(), h + "/right");
        TabOptions options = w.tabOptions();
        options.tree.shared = false;
        w.setTabOptions(options);
        QVERIFY(w.windowTree()->isHidden() && w.sideTree(0)->isVisible() && w.sideTree(1)->isVisible());
        QTRY_COMPARE(w.sideTree(0)->currentPath(), h + "/left");
        QTRY_COMPARE(w.sideTree(1)->currentPath(), h + "/right");
        auto x = [&](QWidget *widget) { return widget->mapTo(&w, QPoint(0, 0)).x(); };
        QCoreApplication::processEvents();
        QVERIFY(x(w.sideTree(0)) < x(w.display(0)));
        QVERIFY(x(w.sideTree(1)) > x(w.display(1))); // the second at the far right
        // Using a side's tree makes that side the active one.
        w.sideTree(0)->setFocus();
        QCOMPARE(w.activeSide(), 0);
        w.setLayout(MainWindow::Layout::Stacked);
        QCoreApplication::processEvents();
        QVERIFY(x(w.sideTree(1)) < x(w.display(1)));
        w.setDual(false);
        QVERIFY(w.windowTree()->isVisible());
        QVERIFY(!w.sideTree(0)->isVisible());
        options.tree.right = true;
        w.setTabOptions(options);
        QCoreApplication::processEvents();
        QVERIFY(x(w.windowTree()) > x(w.display(0)));
        // Hidden with the keyboard in it, the list gets it.
        w.focusTree();
        QVERIFY(w.windowTree()->view()->hasFocus());
        w.setTreeShown(false);
        QVERIFY(w.activeTab()->fileView()->hasFocus());
    }

    // --- FAV-01 -----------------------------------------------------------

    void favoritesAreStoredAndSorted()
    {
        FavoriteList list;
        Favorite file;
        file.type = Favorite::Type::File;
        file.path = "/x/notes.txt";
        list.bar = {Favorite::folder("~/Work", "Work & \"play\""), Favorite::separator(), file};
        list.items = {Favorite::folder("/b 10"), Favorite::folder("/b 9"),
                      Favorite::branch("Branch", {Favorite::branch("Deep", {Favorite::folder("/d")})}),
                      Favorite::separator(), Favorite::folder("/z"), Favorite::folder("/y", "a name")};
        QVERIFY(FavoriteList::fromJson(list.toJson()) == list);
        QTemporaryDir dir;
        {
            QSettings settings(dir.path() + "/c.conf", QSettings::IniFormat);
            TabOptions o;
            o.favorites = list;
            o.favoritesBar = true;
            o.save(settings);
        }
        QSettings settings(dir.path() + "/c.conf", QSettings::IniFormat);
        QVERIFY(TabOptions::load(settings).favorites == list);
        QVERIFY(TabOptions::load(settings).favoritesBar);
        // Rubbish in the file is left out.
        QVERIFY(FavoriteList::fromJson("not json").isEmpty());
        QVERIFY(FavoriteList::fromJson(R"({"bar":[{"type":"folder"},{"type":"odd","path":"/"},3]})").isEmpty());

        QCOMPARE(Favorite::folder("/usr/share").title(), QString("share"));
        QCOMPARE(Favorite::folder("/").title(), QString("/"));
        FavoriteList::sort(&list.items);
        QStringList titles;
        for (const Favorite &f : list.items)
            titles << f.title();
        QCOMPARE(titles, QStringList({"Branch", "b 9", "b 10", "", "a name", "z"}));
        FavoriteList::sort(&list.bar);
        QCOMPARE(list.bar.at(1).type, Favorite::Type::Separator);
        QList<Favorite> none;
        FavoriteList::sort(&none);
    }

    void favoriteIsAdded()
    {
        FavoriteList list;
        list.items = {Favorite::branch("Projects", {Favorite::folder("/usr/lib")}), Favorite::folder("/usr/share")};
        FavoriteDialog dialog(list, Favorite::folder("/usr/lib", "lib"), false, nullptr);
        auto *branches = dialog.findChild<QTreeWidget *>();
        // At the top level unless another is chosen.
        QCOMPARE(dialog.list().items.size(), 3);
        QVERIFY(!dialog.isOnBar());
        branches->setCurrentItem(branches->topLevelItem(0)->child(0));
        QCOMPARE(dialog.list().items.size(), 2);
        QCOMPARE(dialog.list().items.first().children.size(), 2);
        QString note;
        for (QLabel *l : dialog.findChildren<QLabel *>())
            note += l->text();
        QVERIFY(note.contains("already"));
        branches->setCurrentItem(branches->topLevelItem(1));
        QCOMPARE(dialog.list().bar.size(), 1);
        QVERIFY(dialog.isOnBar());
        // A new branch, and the favourite inside it.
        QTimer::singleShot(100, &dialog, [] {
            auto *d = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            QVERIFY(d);
            d->setTextValue("Fresh");
            d->accept();
        });
        for (QPushButton *b : dialog.findChildren<QPushButton *>())
            if (b->text().contains("Branch"))
                b->click();
        const FavoriteList made = dialog.list();
        QCOMPARE(made.bar.size(), 1);
        QCOMPARE(made.bar.first().name, QString("Fresh"));
        QCOMPARE(made.bar.first().children.first().path, QString("/usr/lib"));
        // Sorted as it is added.
        FavoriteDialog sorted(list, Favorite::folder("/a", "aaa"), true, nullptr);
        QCOMPARE(sorted.list().items.at(1).name, QString("aaa"));
        // A path has to lead somewhere.
        QLineEdit *path = sorted.findChildren<QLineEdit *>().first();
        path->setText("relative");
        for (QPushButton *b : sorted.findChildren<QPushButton *>())
            if (b->text().contains("OK"))
                QVERIFY(!b->isEnabled());
    }

    void favoritesAreEdited()
    {
        FavoriteList list;
        list.bar = {Favorite::folder("/bar")};
        list.items = {Favorite::folder("/c"), Favorite::branch("B", {Favorite::folder("/in")}), Favorite::folder("/a")};
        FavoritesDialog dialog(list, true, nullptr);
        QVERIFY(dialog.list() == list);
        QVERIFY(dialog.autoSort());
        QTreeWidget *tree = dialog.tree();
        auto titles = [&] {
            QStringList t;
            for (const Favorite &f : dialog.list().items)
                t << f.title();
            return t;
        };
        // The bar's branch stays where it is.
        tree->setCurrentItem(tree->topLevelItem(0));
        dialog.deleteItem();
        dialog.moveItem(1);
        QVERIFY(dialog.list() == list);
        tree->setCurrentItem(tree->topLevelItem(1)); // c
        dialog.moveItem(-1);
        QCOMPARE(titles(), QStringList({"c", "B", "a"}));
        dialog.moveItem(1);
        QCOMPARE(titles(), QStringList({"B", "c", "a"}));
        QCOMPARE(tree->currentItem()->text(0), QString("c"));
        dialog.addSeparator();
        QCOMPARE(dialog.list().items.at(2).type, Favorite::Type::Separator);
        dialog.deleteItem();
        tree->setCurrentItem(tree->topLevelItem(2));
        dialog.sortItems();
        QCOMPARE(titles(), QStringList({"B", "a", "c"}));
        // A separator in a branch goes inside it.
        tree->setCurrentItem(tree->topLevelItem(1));
        dialog.addSeparator();
        QCOMPARE(dialog.list().items.first().children.size(), 2);
        // Deleting a branch with something in it asks.
        tree->setCurrentItem(tree->topLevelItem(1));
        QTimer::singleShot(100, &dialog, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->reject();
        });
        dialog.deleteItem();
        QCOMPARE(titles().size(), 3);
        tree->setCurrentItem(tree->topLevelItem(2));
        dialog.deleteItem();
        QCOMPARE(titles(), QStringList({"B", "c"}));
        QCOMPARE(dialog.list().bar, list.bar);
    }

    void favoritesOpenFromMenuAndBar()
    {
        const FakeHome home;
        const QString h = home.path();
        for (const char *name : {"/one", "/two", "/three"})
            QDir().mkpath(h + name);
        QTemporaryDir config;
        QSettings settings(config.path() + "/c.conf", QSettings::IniFormat);
        MainWindow w(h, &settings);
        w.resize(900, 500);
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        QVERIFY(w.favoritesBar()->isHidden());
        TabOptions options = w.tabOptions();
        options.favorites.items = {Favorite::folder("~/one"), Favorite::separator(),
                                   Favorite::branch("More", {Favorite::folder("~/two"), Favorite::folder("~/three")}),
                                   Favorite::folder("~/gone", "Gone")};
        options.favorites.bar = {Favorite::folder("~/two", "A & B"), Favorite::branch("Empty one")};
        options.favoritesBar = true;
        w.setTabOptions(options);
        QVERIFY(w.favoritesBar()->isVisible());

        auto entry = [](QMenu *menu, const QString &text) -> QAction * {
            for (QAction *a : menu->actions())
                if (a->text() == text)
                    return a;
            return nullptr;
        };
        // Filling it over and over leaves one of each.
        for (int i = 0; i < 3; ++i)
            emit w.favoritesMenu()->aboutToShow();
        int count = 0;
        for (QAction *a : w.favoritesMenu()->actions())
            count += a->text() == "one";
        QCOMPARE(count, 1);
        entry(w.favoritesMenu(), "one")->trigger();
        QCOMPARE(w.activeTab()->currentPath(), h + "/one");
        // A folder that has gone is said to be.
        entry(w.favoritesMenu(), "Gone")->trigger();
        QCOMPARE(w.activeTab()->currentPath(), h + "/one");
        QVERIFY(w.activeTab()->statusText().contains("does not exist"));
        QMenu *more = entry(w.favoritesMenu(), "More")->menu();
        entry(more, "Open All in Tabs")->trigger();
        QCOMPARE(w.display(0)->tabCount(), 3);
        QCOMPARE(w.activeTab()->currentPath(), h + "/three");

        QList<QToolButton *> buttons;
        for (QToolButton *b : w.favoritesBar()->findChildren<QToolButton *>())
            if (b->text().startsWith("A &&") || b->text().startsWith("Empty one"))
                buttons << b;
        QCOMPARE(buttons.size(), 2);
        QTest::mouseClick(buttons.first(), Qt::LeftButton);
        QCOMPARE(w.activeTab()->currentPath(), h + "/two");
        QCOMPARE(w.display(0)->tabCount(), 3);
        QTest::mouseClick(buttons.first(), Qt::MiddleButton);
        QCOMPARE(w.display(0)->tabCount(), 4);

        // Adding this folder to the bar, then taking it off by its menu.
        w.addFavorite({}, true);
        QCOMPARE(w.tabOptions().favorites.bar.size(), 3);
        QCOMPARE(w.tabOptions().favorites.bar.last().realPath(), h + "/two");
        QSignalSpy asked(w.favoritesBar(), &FavoritesBar::menuRequested);
        QToolButton *last = nullptr;
        for (QToolButton *b : w.favoritesBar()->findChildren<QToolButton *>())
            if (b->text() == "two")
                last = b;
        QVERIFY(last);
        QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(3, 3), last->mapToGlobal(QPoint(3, 3)));
        QApplication::sendEvent(last, &event);
        QCOMPARE(asked.size(), 1);
        QCOMPARE(asked.first().first().toInt(), 2);
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        QVERIFY(menu);
        entry(menu, "Remove")->trigger();
        menu->close();
        QCOMPARE(w.tabOptions().favorites.bar.size(), 2);
        QVERIFY(TabOptions::load(settings).favorites == w.tabOptions().favorites);

        // Ctrl+B asks, offering this folder.
        w.activeTab()->fileView()->setFocus();
        QTimer::singleShot(200, &w, [&] {
            auto *d = qobject_cast<FavoriteDialog *>(QApplication::activeModalWidget());
            QVERIFY(d);
            QCOMPARE(d->favorite().path, w.activeTab()->currentPath());
            d->accept();
        });
        w.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        QTest::keyClick(&w, Qt::Key_B, Qt::ControlModifier);
        QCOMPARE(w.tabOptions().favorites.items.size(), 5);
        w.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        QTest::keyClick(&w, Qt::Key_B, Qt::ControlModifier | Qt::ShiftModifier);
        QVERIFY(w.favoritesBar()->isHidden());
        // The right-click menus offer it too.
        QVERIFY(entry(w.contextMenu(false), "Add to Favourites…"));
    }

    void errorsClearThemselves()
    {
        QTemporaryDir dir;
        write(dir.path() + "/a.txt", "a");
        write(dir.path() + "/b.txt", "b");
        MainWindow w(dir.path());
        w.resize(900, 600);
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        FolderTab *tab = w.activeTab();
        QTRY_VERIFY(tab->itemCount() == 2);
        auto shown = [&] {
            QStringList texts;
            for (QLabel *label : w.statusBar()->findChildren<QLabel *>())
                if (label->isVisible())
                    texts << label->text();
            return texts.join('|');
        };
        auto fail = [&] {
            tab->showError("Oops.");
            QCOMPARE(shown(), QString("Oops."));
        };
        QWidget *list = tab->fileView();

        // A key clears it, but not a modifier on its own.
        fail();
        QTest::keyClick(list, Qt::Key_Control, Qt::ControlModifier);
        QTest::keyClick(list, Qt::Key_Shift, Qt::ShiftModifier);
        QVERIFY(tab->errorId());
        QTest::keyClick(list, Qt::Key_Down);
        QVERIFY(!tab->errorId());
        QVERIFY(shown().contains("items"));

        // So does a click, anywhere in the window.
        fail();
        QTest::mouseClick(w.windowHandle(), Qt::LeftButton, {}, w.statusBar()->geometry().center());
        QVERIFY(!tab->errorId());

        // Selecting the items it asked for does, through the list itself.
        fail();
        QTest::mouseClick(w.windowHandle(), Qt::LeftButton, {}, list->mapTo(&w, QPoint(40, 40)));
        QVERIFY(!tab->errorId());

        // Files changing by themselves don't.
        fail();
        write(dir.path() + "/c.txt", "c");
        QTRY_COMPARE(tab->fileView()->model()->rowCount(), 3);
        QCOMPARE(shown(), QString("Oops."));

        // An error from a key stays, though the key is offered to the list's
        // parents after it; the same command again shows it again.
        tab->selectAll();
        tab->clearError();
        for (int i = 0; i < 2; ++i) {
            QTest::keyClick(list, Qt::Key_F5);
            QVERIFY(shown().contains("no other side"));
        }
        QTest::keyClick(list, Qt::Key_L, Qt::ControlModifier);
        auto *path = qobject_cast<QLineEdit *>(QApplication::focusWidget());
        QVERIFY(path);
        path->setText("/definitely/not/here");
        QTest::keyClick(path, Qt::Key_Return);
        QVERIFY(shown().contains("/definitely/not/here"));
        // Keys elsewhere don't count.
        QWidget other;
        other.show();
        QTest::keyClick(&other, Qt::Key_A);
        QVERIFY(tab->errorId());
        other.hide();

        // An error isn't covered by a message from before it.
        w.statusBar()->showMessage("Copied.", 4000);
        fail();
        QVERIFY(w.statusBar()->currentMessage().isEmpty());

        // It goes by itself, and a new one gets its full time.
        w.setErrorTimeout(400);
        fail();
        QTest::qWait(250);
        fail();
        QTest::qWait(250);
        QCOMPARE(shown(), QString("Oops."));
        QTRY_VERIFY(!tab->errorId());
        QVERIFY(shown().contains("items"));
    }

    void errorsThatMatterStay()
    {
        QTemporaryDir dir;
        // Being sent home instead of to the folder asked for is worth reading.
        MainWindow w("/definitely/not/here");
        w.setErrorTimeout(50);
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        FolderTab *tab = w.activeTab();
        QTest::qWait(200);
        QVERIFY(tab->errorLasts());
        QVERIFY(tab->statusText().contains("/definitely/not/here"));
        QTest::keyClick(tab->fileView(), Qt::Key_Down);
        QVERIFY(!tab->errorId());
        // Later errors don't wait.
        tab->showError("Oops.");
        QVERIFY(!tab->errorLasts());
        QTRY_VERIFY(!tab->errorId());

        // One in a tab that isn't showing waits until it is.
        QVERIFY(tab->openPath(dir.path()));
        FolderTab *behind = tab;
        w.display(0)->addTab(dir.path());
        QVERIFY(w.activeTab() != behind);
        behind->showError("Oops.");
        QTest::qWait(200);
        QTest::keyClick(w.activeTab()->fileView(), Qt::Key_Down);
        QVERIFY(behind->errorId());
        w.display(0)->setCurrentIndex(0);
        QCOMPARE(w.activeTab(), behind);
        QVERIFY(behind->errorId());
        QTRY_VERIFY(!behind->errorId());
    }

    void aboutShowsVersionAndLicense()
    {
        const QString before = QCoreApplication::applicationVersion();
        QCoreApplication::setApplicationVersion("9.8.7");
        const auto restore = qScopeGuard([&] { QCoreApplication::setApplicationVersion(before); });
        // The logo is built in.
        QVERIFY(!QImage(":/logo.png").isNull());
        // The copyright line comes from the licence, which is built in too.
        QFile file(QFINDTESTDATA("../LICENSE"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(AboutDialog::license(), QString::fromUtf8(file.readAll()).trimmed());
        QVERIFY(AboutDialog::copyright().startsWith(QStringLiteral("Copyright © 20")));
        QVERIFY(AboutDialog::credits().startsWith(QStringLiteral("Made by ")));
        QVERIFY(AboutDialog::credits().endsWith(QStringLiteral(" and Claude, by Anthropic.")));

        // From the menu: Escape closes it again.
        QTemporaryDir dir;
        MainWindow w(dir.path());
        w.show();
        QAction *about = nullptr;
        for (QAction *a : w.findChildren<QAction *>())
            if (a->text() == "About CargoHatch")
                about = a;
        QVERIFY(about);
        bool seen = false;
        QTimer::singleShot(0, &w, [&] {
            auto *dialog = qobject_cast<AboutDialog *>(QApplication::activeModalWidget());
            seen = dialog;
            if (dialog)
                QTest::keyClick(dialog, Qt::Key_Escape);
        });
        about->trigger();
        QVERIFY(seen);

        AboutDialog dialog;
        dialog.show();
        bool version = false;
        for (QLabel *label : dialog.findChildren<QLabel *>())
            version |= label->text().contains("9.8.7");
        QVERIFY(version);
        auto *text = dialog.findChild<QPlainTextEdit *>();
        QVERIFY(!text->isVisible());
        dialog.setLicenseShown(true);
        QVERIFY(text->isVisible());
        QVERIFY(text->isReadOnly());
        dialog.setLicenseShown(false);
        QVERIFY(!text->isVisible());
    }

    // --- The menu bar (CUST-16, CUST-17) ---------------------------------

    // The entry reached by following `path` through the menu bar, filling
    // each menu on the way as opening it would.
    // --- LST-09, VWR-01 ---------------------------------------------------

    void previewShowsEachKind()
    {
        QTemporaryDir dir;
        const QString d = dir.path();
        QImage picture(40, 20, QImage::Format_ARGB32);
        picture.fill(Qt::red);
        QVERIFY(picture.save(d + "/red.png"));
        QImage wide(5000, 10, QImage::Format_RGB32);
        wide.fill(Qt::blue);
        QVERIFY(wide.save(d + "/wide.png"));
        write(d + "/notes.txt", QString::fromUtf8("Crème brûlée\nline two\n").toUtf8());
        write(d + "/settings", "key=value\n");
        write(d + "/blob.bin", QByteArray("AB\0\1\2", 5));
        write(d + "/empty.txt", "");
        write(d + "/song.mp3", "not really a song");
        QDir(d).mkpath("folder");
        write(d + "/folder/a", "x");
        write(d + "/folder/b", "x");
        write(d + "/folder/.c", "x");

        using Kind = Preview::Kind;
        Preview p = Preview::load(d + "/red.png");
        QCOMPARE(p.kind, Kind::Image);
        QCOMPARE(p.image.size(), QSize(40, 20));
        QCOMPARE(p.mimeName, QString("image/png"));
        // Big images are scaled down as they're read, but say their own size.
        p = Preview::load(d + "/wide.png");
        QCOMPARE(p.kind, Kind::Image);
        QCOMPARE(p.image.width(), 4096);
        QCOMPARE(p.imageSize, QSize(5000, 10));
        p = Preview::load(d + "/notes.txt");
        QCOMPARE(p.kind, Kind::Text);
        QVERIFY(p.text.startsWith(QString::fromUtf8("Crème brûlée")));
        QVERIFY(!p.truncated);
        // No type of its own, but text inside.
        QCOMPARE(Preview::load(d + "/settings").kind, Kind::Text);
        p = Preview::load(d + "/blob.bin");
        QCOMPARE(p.kind, Kind::Hex);
        QCOMPARE(p.data, QByteArray("AB\0\1\2", 5));
        p = Preview::load(d + "/empty.txt");
        QCOMPARE(p.kind, Kind::Info);
        QVERIFY(!p.note.isEmpty());
        p = Preview::load(d + "/song.mp3");
        QCOMPARE(p.kind, Kind::Media);
        QVERIFY(!p.video);
        p = Preview::load(d + "/folder");
        QCOMPARE(p.kind, Kind::Folder);
        QCOMPARE(p.items, 3);
        QCOMPARE(p.hiddenItems, 1);
        Preview::Limits few;
        few.folderItems = 2;
        p = Preview::load(d + "/folder", few);
        QCOMPARE(p.items, 2);
        QVERIFY(p.moreItems);
        p = Preview::load(d + "/gone.txt");
        QCOMPARE(p.kind, Kind::Error);
        QVERIFY(p.error.contains("gone.txt"));
        QCOMPARE(Preview::load(QString()).kind, Kind::None);

        // Long text is cut, and says so.
        Preview::Limits small;
        small.textBytes = 8;
        p = Preview::load(d + "/notes.txt", small);
        QCOMPARE(p.kind, Kind::Text);
        QVERIFY(p.truncated);
    }

    void previewDrawsTheFirstPageOfPdfs()
    {
        QTemporaryDir dir;
        const QString file = dir.path() + "/page.pdf";
        {
            QPdfWriter writer(file);
            QPainter painter(&writer);
            painter.drawText(QPointF(100, 100), "Hello");
        }
        const QString renderer = Preview::pdfRenderer();
        Preview::setPdfRenderer(QString());
        Preview p = Preview::load(file);
        QCOMPARE(p.kind, Preview::Kind::Info);
        QVERIFY(p.note.contains("poppler"));
        Preview::setPdfRenderer(renderer);
        if (renderer.isEmpty())
            QSKIP("pdftoppm isn't installed");
        p = Preview::load(file);
        QCOMPARE(p.kind, Preview::Kind::Pdf);
        QVERIFY(!p.image.isNull());
    }

    void previewDumpsAndDecodesBytes()
    {
        const QString dump = Preview::hexDump(QByteArray("AB\0", 3));
        QCOMPARE(dump, QString("00000000  41 42 00                                          |AB.|\n"));
        QCOMPARE(Preview::hexDump(QByteArray(17, 'x')).count('\n'), 2);
        // Narrow panes get eight to a row, and rows are as wide as they say.
        const QString narrow = Preview::hexDump(QByteArray(16, 'x'), -1, 8);
        QCOMPARE(narrow.count('\n'), 2);
        QCOMPARE(narrow.section('\n', 0, 0).size(), Preview::hexRowWidth(-1, 8));
        QCOMPARE(Preview::hexDump(QByteArray(16, 'x')).section('\n', 0, 0).size(), Preview::hexRowWidth(-1, 16));
        bool isText = false;
        QCOMPARE(Preview::decodeText(QString::fromUtf8("é").toUtf8(), &isText), QString::fromUtf8("é"));
        QVERIFY(isText);
        // A character cut in two at the end is left out, not taken as Latin-1.
        QCOMPARE(Preview::decodeText(QByteArray("ab\xc3", 3), &isText), QString("ab"));
        // Not UTF-8: Latin-1.
        QCOMPARE(Preview::decodeText(QByteArray("caf\xe9 au lait"), &isText), QString::fromLatin1("caf\xe9 au lait"));
        Preview::decodeText(QByteArray("a\0b", 3), &isText);
        QVERIFY(!isText);
    }

    void viewerPaneFollowsTheSelection()
    {
        QTemporaryDir dir;
        write(dir.path() + "/a.txt", "first file");
        write(dir.path() + "/b.txt", "second file");
        write(dir.path() + "/c.bin", QByteArray("\0\0", 2));
        MainWindow w(dir.path());
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        FolderTab *tab = w.activeTab();
        QTRY_COMPARE(tab->itemCount(), 3);
        ViewerPane *pane = w.viewerPane();
        QVERIFY(!pane->isVisible());
        QVERIFY(!w.isViewerShown());

        w.setViewerShown(true);
        QVERIFY(pane->isVisible());
        QTRY_COMPARE(pane->kind(), Preview::Kind::None);
        tab->selectNames({"a.txt"});
        const QString a = canonical(dir.path()) + "/a.txt";
        QTRY_COMPARE(pane->preview().path, a);
        QCOMPARE(pane->kind(), Preview::Kind::Text);
        QCOMPARE(pane->contentText(), QString("first file"));
        QVERIFY(pane->detailsText().contains(FileModel::sizeText(10)));

        // The next button moves the selection, and the pane goes with it.
        emit pane->nextRequested();
        QCOMPARE(tab->selectedPaths(), QStringList({canonical(dir.path()) + "/b.txt"}));
        QTRY_COMPARE(pane->contentText(), QString("second file"));

        // Locked, it keeps its file.
        pane->setLocked(true);
        tab->selectNames({"c.bin"});
        QTest::qWait(150);
        QCOMPARE(pane->contentText(), QString("second file"));
        pane->setLocked(false);
        QTRY_COMPARE(pane->kind(), Preview::Kind::Hex);
        QVERIFY(pane->contentText().startsWith("00000000  00 00"));

        // Nothing selected, nothing shown, unless the last is kept.
        tab->selectNone();
        QTRY_COMPARE(pane->kind(), Preview::Kind::None);
        TabOptions options = w.tabOptions();
        options.viewerKeepLast = true;
        w.setTabOptions(options);
        tab->selectNames({"a.txt"});
        QTRY_COMPARE(pane->preview().path, a);
        tab->selectNone();
        QTest::qWait(150);
        QCOMPARE(pane->preview().path, a);

        // Hiding it stops it loading.
        w.setViewerShown(false);
        QVERIFY(!pane->isVisible());
        tab->selectNames({"b.txt"});
        QTest::qWait(150);
        QCOMPARE(pane->preview().path, a);
    }

    void viewerPaneSitsBesideOrBelow()
    {
        QTemporaryDir dir;
        const QString file = dir.path() + "/cargohatch.conf";
        {
            QSettings settings(file, QSettings::IniFormat);
            MainWindow w(dir.path(), &settings);
            w.show();
            QVERIFY(QTest::qWaitForWindowExposed(&w));
            // F11, from the View menu.
            QAction *show = menuEntry(w, {"View", "Viewer Pane"});
            QVERIFY(show);
            QCOMPARE(show->shortcut(), QKeySequence(Qt::Key_F11));
            show->trigger();
            QVERIFY(w.isViewerShown());
            QVERIFY(show->isChecked());
            ViewerPane *pane = w.viewerPane();
            auto *split = qobject_cast<QSplitter *>(pane->parentWidget());
            QCOMPARE(split->orientation(), Qt::Horizontal);
            QCOMPARE(split->indexOf(pane), 1);
            QVERIFY(pane->width() >= 160 && pane->width() < w.width() / 2);
            menuEntry(w, {"View", "Viewer Pane Layout", "Below the Files"})->trigger();
            QVERIFY(w.isViewerBelow());
            QCOMPARE(split->orientation(), Qt::Vertical);
            QVERIFY(menuEntry(w, {"View", "Viewer Pane Layout", "Below the Files"})->isChecked());
            menuEntry(w, {"View", "Viewer Pane Layout", "Lock Viewer Pane"})->trigger();
            QVERIFY(pane->isLocked());
            QVERIFY(entries(w, {"Settings", "Viewer Pane"}).contains("Wrap Long Lines of Text"));
            w.saveSettings();
        }
        QSettings settings(file, QSettings::IniFormat);
        MainWindow w(dir.path(), &settings);
        QVERIFY(w.isViewerShown());
        QVERIFY(w.isViewerBelow());
        QVERIFY(w.viewerPane()->isLocked());
        QVERIFY(menuEntry(w, {"View", "Viewer Pane"})->isChecked());
        w.setViewerShown(false);
        QVERIFY(!menuEntry(w, {"View", "Viewer Pane"})->isChecked());
    }

    // --- REN-05, REN-10 (Rename Several) ---------------------------------

    void renameSeveralWorksOutNames()
    {
        using C = BatchRename::Case;
        QCOMPARE(BatchRename::changeCase("the BIG sleep", C::Upper), QString("THE BIG SLEEP"));
        QCOMPARE(BatchRename::changeCase("the BIG sleep", C::Lower), QString("the big sleep"));
        QCOMPARE(BatchRename::changeCase("the BIG sleep", C::FirstWord), QString("The big sleep"));
        QCOMPARE(BatchRename::changeCase("01 don't stop-me now", C::EveryWord), QString("01 Don't Stop-Me Now"));
        QCOMPARE(BatchRename::changeCase(QString::fromUtf8("élan vital"), C::EveryWord), QString::fromUtf8("Élan Vital"));

        BatchRename r;
        r.nameCase = C::Upper;
        // The extension is left alone unless asked; it has its own case.
        QCOMPARE(r.apply("holiday photo.jpg", false), QString("HOLIDAY PHOTO.jpg"));
        r.ignoreExtension = false;
        QCOMPARE(r.apply("holiday photo.jpg", false), QString("HOLIDAY PHOTO.JPG"));
        r.extensionCase = BatchRename::ExtensionCase::Lower;
        QCOMPARE(r.apply("holiday photo.JPG", false), QString("HOLIDAY PHOTO.jpg"));
        // Folders and dotfiles have no extension.
        r.ignoreExtension = true;
        QCOMPARE(r.apply("photos.2026", true), QString("PHOTOS.2026"));
        QCOMPARE(r.apply(".bashrc", false), QString(".BASHRC"));

        // Find and replace, in any case unless told otherwise.
        BatchRename f;
        f.find = "img";
        f.replace = "Photo";
        bool matched = false;
        QCOMPARE(f.apply("IMG_0001.img", false, &matched), QString("Photo_0001.img"));
        QVERIFY(matched);
        f.matchCase = true;
        QCOMPARE(f.apply("IMG_0001.img", false, &matched), QString("IMG_0001.img"));
        QVERIFY(!matched);
        // Ignoring the extension keeps Find out of it.
        f.matchCase = false;
        f.find = "jpeg";
        f.replace = "jpg";
        QCOMPARE(f.apply("cat.jpeg", false, &matched), QString("cat.jpeg"));
        QVERIFY(!matched);
        f.ignoreExtension = false;
        QCOMPARE(f.apply("cat.jpeg", false), QString("cat.jpg"));

        // Names Find doesn't match keep their case, unless told otherwise.
        BatchRename a;
        a.find = "draft";
        a.replace = "final";
        a.nameCase = C::EveryWord;
        QCOMPARE(a.apply("my draft.txt", false), QString("My Final.txt"));
        QCOMPARE(a.apply("old notes.txt", false), QString("old notes.txt"));
        a.applyWithoutMatch = true;
        QCOMPARE(a.apply("old notes.txt", false), QString("Old Notes.txt"));
    }

    void renameSeveralFindsProblems()
    {
        BatchRename r;
        r.find = "a";
        r.replace = "";
        const QList<QPair<QString, bool>> items = {{"a.txt", false}, {"b.txt", false}, {"ab.txt", false}, {"ca.txt", false}};
        QList<BatchRename::Item> plan = r.plan(items, {"c.txt", "x"});
        QCOMPARE(plan.size(), 4);
        // "a.txt" would be the hidden ".txt"; "ab.txt" would clash with "b.txt", which
        // stays; "ca.txt" would be "c.txt", which is taken.
        QVERIFY(plan.at(0).problem.contains("hidden"));
        QVERIFY(!plan.at(1).changes());
        QVERIFY(plan.at(2).problem.contains("b.txt"));
        QVERIFY(plan.at(3).problem.contains("already exists"));
        r.replace = "/";
        QVERIFY(r.plan({{"a.txt", false}}, {}).first().problem.contains("/"));
        r.replace = "";
        r.ignoreExtension = false;
        QVERIFY(r.plan({{"a", false}}, {}).first().problem.contains("empty"));
    }

    void renameSeveralRenamesOnDisk()
    {
        QTemporaryDir dir;
        const QString d = dir.path();
        write(d + "/one.txt", "1");
        write(d + "/two.txt", "2");
        write(d + "/photo.JPG", "p");
        // Trading places goes by way of temporary names.
        BatchRename::Item a{"one.txt", false, "two.txt", true, {}};
        BatchRename::Item b{"two.txt", false, "one.txt", true, {}};
        BatchRename::Item c{"photo.JPG", false, "photo.jpg", true, {}};
        QStringList renamed;
        QCOMPARE(BatchRename::perform(d, {a, b, c}, &renamed), QStringList());
        QCOMPARE(renamed.size(), 3);
        QCOMPARE(read(d + "/one.txt"), QByteArray("2"));
        QCOMPARE(read(d + "/two.txt"), QByteArray("1"));
        QVERIFY(QFile::exists(d + "/photo.jpg"));
        QCOMPARE(QDir(d).entryList(QDir::Files | QDir::Hidden).size(), 3);

        // Never over another file: it keeps its name and says why.
        write(d + "/three.txt", "3");
        BatchRename::Item clash{"three.txt", false, "one.txt", true, {}};
        const QStringList errors = BatchRename::perform(d, {clash});
        QCOMPARE(errors.size(), 1);
        QVERIFY(QFile::exists(d + "/three.txt"));
        QCOMPARE(read(d + "/one.txt"), QByteArray("2"));
        // Items with problems are left alone.
        BatchRename::Item flagged{"three.txt", false, "four.txt", true, "no"};
        QVERIFY(BatchRename::perform(d, {flagged}).isEmpty());
        QVERIFY(QFile::exists(d + "/three.txt"));
    }

    void renameSeveralDialogPreviews()
    {
        BatchRename rules;
        RenameDialog dialog(rules, {{"IMG_1.jpg", false}, {"IMG_2.jpg", false}, {"notes.txt", false}}, {"Photo 1.jpg"});
        QVERIFY(!dialog.canRename());
        QCOMPARE(dialog.previewList()->topLevelItemCount(), 3);
        dialog.findField()->setText("img_");
        dialog.replaceField()->setText("Photo ");
        // "Photo 1.jpg" is taken by another file.
        QVERIFY(!dialog.canRename());
        QVERIFY(dialog.summary().contains("Photo 1.jpg"));
        dialog.replaceField()->setText("Holiday ");
        QVERIFY(dialog.canRename());
        QCOMPARE(dialog.previewList()->topLevelItem(0)->text(1), QString("Holiday 1.jpg"));
        QCOMPARE(dialog.previewList()->topLevelItem(2)->text(1), QString("(unchanged)"));
        QCOMPARE(dialog.summary(), QString("2 of 3 items will be renamed."));
        QCOMPARE(dialog.rules().find, QString("img_"));
    }

    void renameSeveralFromTheWindow()
    {
        QTemporaryDir dir;
        write(dir.path() + "/a.TXT", "a");
        write(dir.path() + "/b.TXT", "b");
        MainWindow w(dir.path());
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        FolderTab *tab = w.activeTab();
        QTRY_COMPARE(tab->itemCount(), 2);
        QAction *several = menuEntry(w, {"File", "Rename Several…"});
        QVERIFY(several);
        QCOMPARE(several->shortcut(), QKeySequence(Qt::SHIFT | Qt::Key_F2));
        QVERIFY(!several->isEnabled());
        tab->selectAll();
        QTRY_VERIFY(several->isEnabled());
        const auto items = tab->selectedItems();
        QCOMPARE(items.size(), 2);
        BatchRename r;
        r.extensionCase = BatchRename::ExtensionCase::Lower;
        QVERIFY(tab->renameItems(r.plan(items, {})));
        QVERIFY(QFile::exists(dir.path() + "/a.txt"));
        QTRY_COMPARE(tab->selectedPaths(), QStringList({canonical(dir.path()) + "/a.txt", canonical(dir.path()) + "/b.txt"}));
    }

    static QAction *menuEntry(QMainWindow &w, const QStringList &path)
    {
        auto plain = [](QString text) { return text.section('\t', 0, 0).remove('&'); };
        QList<QAction *> actions = w.menuBar()->actions();
        QAction *found = nullptr;
        for (const QString &step : path) {
            found = nullptr;
            for (QAction *a : actions)
                if (!a->isSeparator() && plain(a->text()) == step)
                    found = a;
            if (!found)
                return nullptr;
            if (found->menu()) {
                emit found->menu()->aboutToShow();
                actions = found->menu()->actions();
            }
        }
        return found;
    }

    static QStringList entries(QMainWindow &w, const QStringList &path)
    {
        QStringList names;
        QMenu *menu = path.isEmpty() ? nullptr : menuEntry(w, path)->menu();
        const QList<QAction *> actions = menu ? menu->actions() : w.menuBar()->actions();
        for (QAction *a : actions)
            if (!a->isSeparator() && a->isVisible())
                names << a->text().section('\t', 0, 0).remove('&');
        return names;
    }

    void menuBarKeepsSettingsApartFromCommands()
    {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        w.show();
        QCOMPARE(entries(w, {}), QStringList({"File", "Edit", "View", "Go", "Tabs", "Favourites", "Window", "Settings", "Help"}));
        // Only Settings has submenus of choices; the command menus keep the
        // view's own toggles and the Copy Names format at most.
        for (const QString &name : {"When Names Are Taken", "When Dropping Files", "Deleting", "Rename Starts With",
                                    "Find as You Type", "Filter Bar", "Path Completion", "Tabs", "Folder Tree Options…",
                                    "Global Filters…"})
            QVERIFY2(entries(w, {"Settings"}).contains(name), qPrintable(name));
        QVERIFY(!entries(w, {"Edit"}).contains("Delete Options"));
        QVERIFY(!entries(w, {"Go"}).contains("Path Completion"));
        QCOMPARE(entries(w, {"File"}).mid(0, 4), QStringList({"Open", "Open in New Tab", "Open on Other Side", "Open Terminal Here"}));
        QVERIFY(entries(w, {"File"}).contains("Move to Trash"));
        QVERIFY(entries(w, {"Favourites"}).contains("Folder Aliases"));

        // A setting still applies at once from its new place.
        QAction *ask = menuEntry(w, {"Settings", "Deleting", "Ask Before Moving to Trash"});
        QVERIFY(ask);
        const bool before = ask->isChecked();
        ask->trigger();
        QCOMPARE(menuEntry(w, {"Settings", "Deleting", "Ask Before Moving to Trash"})->isChecked(), !before);

        // Keys work from inside a submenu that was never opened.
        write(dir.path() + "/a.txt", "a");
        write(dir.path() + "/b.txt", "b");
        FolderTab *tab = w.activeTab();
        QTRY_COMPARE(tab->itemCount(), 2);
        tab->selectNames({"a.txt"});
        tab->fileView()->setFocus();
        QTest::keyClick(tab->fileView(), Qt::Key_I, Qt::ControlModifier);
        QCOMPARE(tab->selectedPaths(), QStringList({dir.path() + "/b.txt"}));
        QVERIFY(menuEntry(w, {"Edit", "Select", "Invert Selection"}));
    }

    void tabsMenuLocksColoursAndReopens()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkdir("x");
        MainWindow w(dir.path());
        w.show();
        FolderTab *tab = w.activeTab();
        // Every kind of lock the tab's own menu has.
        QCOMPARE(entries(w, {"Tabs", "Lock Tab"}),
                 QStringList({"Unlocked", "Locked", "Locked, Changes Allowed", "Locked, Reusing One Tab"}));
        menuEntry(w, {"Tabs", "Lock Tab", "Locked, Changes Allowed"})->trigger();
        QCOMPARE(tab->lock(), FolderTab::Lock::AllowChanges);
        QVERIFY(entries(w, {"Tabs", "Lock Tab"}).contains("Return to Locked Folder"));
        menuEntry(w, {"Tabs", "Lock Tab", "Unlocked"})->trigger();
        QCOMPARE(tab->lock(), FolderTab::Lock::None);

        menuEntry(w, {"Tabs", "Colour", "Green"})->trigger();
        QCOMPARE(tab->colorName(), QString("green"));

        QVERIFY(!menuEntry(w, {"Tabs", "Make This Folder the Tab's Home"})->isEnabled() ||
                tab->homePath() != tab->currentPath());
        tab->openPath(dir.path() + "/x");
        QTRY_COMPARE(tab->currentPath(), dir.path() + "/x");
        menuEntry(w, {"Tabs", "Make This Folder the Tab's Home"})->trigger();
        QCOMPARE(tab->homePath(), dir.path() + "/x");

        // Reopening the last closed tab heads the list of closed tabs.
        menuEntry(w, {"Tabs", "New Tab"})->trigger();
        menuEntry(w, {"Tabs", "Close Tab"})->trigger();
        const QStringList closed = entries(w, {"Tabs", "Recently Closed Tabs"});
        QCOMPARE(closed.first(), QString("Reopen Last Closed Tab"));
        QCOMPARE(closed.size(), 2);
    }

    void shortcutsWindowListsEveryKey()
    {
        QTemporaryDir dir;
        MainWindow w(dir.path());
        w.show();
        const QList<ShortcutsDialog::Row> rows = ShortcutsDialog::rows(w.menuBar());
        auto has = [&](const QString &section, const QString &command, const QString &keys) {
            for (const ShortcutsDialog::Row &r : rows)
                if (r.section == section && r.command == command && r.keys == keys)
                    return true;
            return false;
        };
        QVERIFY(has("Edit", "Select ▸ Invert Selection", "Ctrl+I"));
        QVERIFY(has("Edit", "Select ▸ None", "Esc"));
        QVERIFY(has("Go", "Up", "Alt+Up, Backspace"));
        QVERIFY(has("File", "Open", "Enter"));
        QVERIFY(has("While Dragging Files", "Copy", "Ctrl"));
        for (const ShortcutsDialog::Row &r : rows)
            QVERIFY2(r.section != "Settings", qPrintable(r.command)); // no keys there

        // ? in the file list opens it, beside the window.
        w.activeTab()->fileView()->setFocus();
        QTest::keyClick(w.activeTab()->fileView(), Qt::Key_Question, Qt::ShiftModifier);
        auto *dialog = w.findChild<ShortcutsDialog *>();
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->isVisible());
        QVERIFY(!dialog->isModal());
        const int all = dialog->shownCount();
        QCOMPARE(all, int(rows.size()));
        dialog->setFilter("trash");
        QVERIFY(dialog->shownCount() > 0 && dialog->shownCount() < all);
        dialog->setFilter("Shift+F5");
        QCOMPARE(dialog->shownCount(), 1);
        dialog->close();
    }

    // --- Opening several items (IDEA-11) --------------------------------

    void openingSeveralItemsOpensThemAll()
    {
        QTemporaryDir dir;
        const QString base = canonical(dir.path());
        for (const char *name : {"a", "b", "c"})
            QDir().mkpath(base + "/" + name);
        write(base + "/one.txt", "1");
        write(base + "/two.txt", "2");
        QStringList opened;
        FolderTab::setOpener([&](const QString &path) {
            opened << path;
            return !path.endsWith("two.txt");
        });
        MainWindow w(base);
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        FolderTab *tab = w.activeTab();
        QTRY_COMPARE(tab->itemCount(), 5);
        FileDisplay *display = w.display(0);

        // Enter on several: files in their applications, folders in tabs
        // after this one, in order, showing the first.
        tab->selectNames({"a", "c", "one.txt", "two.txt"});
        tab->fileView()->setFocus();
        QTest::keyClick(tab->fileView(), Qt::Key_Return);
        QCOMPARE(opened, (QStringList{base + "/one.txt", base + "/two.txt"}));
        QCOMPARE(display->tabCount(), 3);
        QCOMPARE(display->tab(1)->currentPath(), base + "/a");
        QCOMPARE(display->tab(2)->currentPath(), base + "/c");
        QCOMPARE(display->currentIndex(), 1);
        QCOMPARE(tab->currentPath(), base);
        // What couldn't be opened is said where the user was.
        QVERIFY(tab->statusText().contains("two.txt"));

        // From the right-click menu too, where Open comes first.
        opened.clear();
        display->setCurrentIndex(0);
        tab->selectNames({"one.txt", "b"});
        QMenu *menu = w.contextMenu(true);
        QCOMPARE(menu->actions().first()->text(), QString("Open"));
        menu->actions().first()->trigger();
        QCOMPARE(opened, QStringList{base + "/one.txt"});
        QCOMPARE(display->tab(1)->currentPath(), base + "/b");
        delete menu;

        // One item opens as before: a folder in the same tab.
        display->setCurrentIndex(0);
        tab->selectNames({"c"});
        tab->fileView()->setFocus();
        tab->fileView()->setCurrentIndex(tab->proxy()->mapFromSource(tab->model()->index(tab->model()->rowOf("c"), 0)));
        QTest::keyClick(tab->fileView(), Qt::Key_Return);
        QCOMPARE(tab->currentPath(), base + "/c");

        // Many at once asks first.
        for (int i = 0; i <= MainWindow::kOpenWithoutAsking; ++i)
            write(base + "/c/" + QString::number(i) + ".txt", "x");
        QTRY_COMPARE(tab->itemCount(), MainWindow::kOpenWithoutAsking + 1);
        opened.clear();
        QString asked;
        auto answer = [&](bool yes) {
            QTimer::singleShot(0, [&, yes] {
                auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                QVERIFY(box);
                asked = box->text();
                if (yes) {
                    for (QAbstractButton *b : box->buttons())
                        if (b->text() == "Open")
                            b->click();
                }
                else
                    box->reject();
            });
        };
        tab->selectAll();
        answer(false);
        w.openSelection(tab);
        QCOMPARE(asked, QString("Open %1 items?").arg(MainWindow::kOpenWithoutAsking + 1));
        QVERIFY(opened.isEmpty());
        answer(true);
        w.openSelection(tab);
        QCOMPARE(opened.size(), MainWindow::kOpenWithoutAsking + 1);
        FolderTab::setOpener({});
    }

    // --- Terminal (IDEA-10) --------------------------------------------

    void terminalOpensInTheFolder()
    {
        QTemporaryDir dir;
        const QString base = canonical(dir.path());
        const QString folder = base + "/a folder";
        QDir().mkpath(folder);
        // Stands in for xdg-terminal-exec: notes the folder it was given and
        // prints a "terminal" that notes where it started.
        write(base + "/fake-terminal-exec",
              "#!/bin/sh\nfor a; do case $a in --dir=*) printf '%s' \"${a#--dir=}\" > \"" + base.toUtf8() +
                  "/asked\";; esac; done\nprintf '/bin/sh\\0-c\\0pwd > \"" + base.toUtf8() + "/started\"'\n");
        QFile::setPermissions(base + "/fake-terminal-exec", QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        Terminal::setProgram(base + "/fake-terminal-exec");
        MainWindow w(folder);
        QTRY_VERIFY(w.activeTab()->itemCount() >= 0);
        QAction *open = nullptr;
        for (QAction *a : w.findChildren<QAction *>())
            if (a->shortcut() == QKeySequence(Qt::SHIFT | Qt::Key_F4))
                open = a;
        QVERIFY(open);
        QCOMPARE(open->text(), QString("Open Terminal Here"));
        open->trigger();
        QTRY_COMPARE(read(base + "/started").trimmed(), folder.toUtf8());
        QCOMPARE(read(base + "/asked"), folder.toUtf8());

        // What's wrong is said.
        write(base + "/fake-terminal-exec", "#!/bin/sh\necho 'No valid terminal entry was found' >&2\nexit 1\n");
        w.openTerminal();
        QTRY_VERIFY(w.activeTab()->statusText().contains("No valid terminal entry was found"));
        const QByteArray terminal = qgetenv("TERMINAL");
        qunsetenv("TERMINAL");
        Terminal::setProgram(base + "/missing");
        w.openTerminal(base);
        QTRY_VERIFY(w.activeTab()->statusText().contains("No terminal was found"));
        if (!terminal.isNull())
            qputenv("TERMINAL", terminal);
        Terminal::setProgram("xdg-terminal-exec");
    }

    // --- Drag and drop (IDEA-18) ----------------------------------------

    void dropsChooseWhatToDo()
    {
        using FileDrag::Action;
        FileDrag::Question q;
        // On the same drive a drag moves; between drives, or from another
        // program, it copies.
        QCOMPARE(FileDrag::choose(q), Action::Move);
        q.sameDevice = false;
        QCOMPARE(FileDrag::choose(q), Action::Copy);
        q.sameDevice = true;
        q.foreign = true;
        QCOMPARE(FileDrag::choose(q), Action::Copy);
        // Keys decide.
        q.modifiers = Qt::ShiftModifier;
        QCOMPARE(FileDrag::choose(q), Action::Move);
        q.modifiers = Qt::ControlModifier;
        QCOMPARE(FileDrag::choose(q), Action::Copy);
        q.modifiers = Qt::ControlModifier | Qt::ShiftModifier;
        QCOMPARE(FileDrag::choose(q), Action::Link);
        q.modifiers = Qt::AltModifier;
        QCOMPARE(FileDrag::choose(q), Action::Ask);
        // What isn't allowed isn't done instead.
        q.modifiers = Qt::ShiftModifier;
        q.possible = Qt::CopyAction;
        QCOMPARE(FileDrag::choose(q), Action::None);
        q.modifiers = {};
        QCOMPARE(FileDrag::choose(q), Action::Copy);
        // Let go where they are, nothing happens unless asked for a copy.
        q = {};
        q.alreadyThere = true;
        QCOMPARE(FileDrag::choose(q), Action::None);
        q.modifiers = Qt::ShiftModifier;
        QCOMPARE(FileDrag::choose(q), Action::None);
        q.modifiers = Qt::ControlModifier;
        QCOMPARE(FileDrag::choose(q), Action::Copy);
        // The options.
        q = {};
        q.behaviour = "copy";
        QCOMPARE(FileDrag::choose(q), Action::Copy);
        q.behaviour = "ask";
        QCOMPARE(FileDrag::choose(q), Action::Ask);
        q.behaviour = "move";
        q.sameDevice = false;
        QCOMPARE(FileDrag::choose(q), Action::Move);
        // Keys held as a drag starts limit what other programs are offered.
        QCOMPARE(FileDrag::offered(Qt::ControlModifier), Qt::DropActions(Qt::CopyAction));
        QCOMPARE(FileDrag::offered(Qt::ShiftModifier), Qt::DropActions(Qt::MoveAction));
        QCOMPARE(FileDrag::offered(Qt::ControlModifier | Qt::ShiftModifier), Qt::DropActions(Qt::LinkAction));
        QCOMPARE(FileDrag::offered(Qt::AltModifier), Qt::CopyAction | Qt::MoveAction | Qt::LinkAction);
        // Ctrl let go of later can't turn the copy into a move.
        q = {};
        q.possible = FileDrag::offered(Qt::ControlModifier);
        QCOMPARE(FileDrag::choose(q), Action::Copy);
        // The trash takes them whatever the keys.
        q = {};
        q.toTrash = true;
        q.modifiers = Qt::ControlModifier;
        QCOMPARE(FileDrag::choose(q), Action::Trash);
    }

    void droppingFilesCopiesAndMovesThem()
    {
        QTemporaryDir dir;
        const QString base = canonical(dir.path());
        QDir().mkpath(base + "/from");
        QDir().mkpath(base + "/to/inner");
        write(base + "/from/a.txt", "a");
        write(base + "/from/b.txt", "b");
        MainWindow w(base + "/to");
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        FolderTab *tab = w.activeTab();
        tab->setViewMode(ViewFormat::Mode::Details);
        QTRY_COMPARE(tab->itemCount(), 1);
        QWidget *list = tab->fileView()->viewport();

        // From another program: copied, and what arrived is selected.
        QVERIFY(drop(list, QPoint(5, list->height() - 5), {base + "/from/a.txt"}));
        QTRY_COMPARE(tab->selectedPaths(), QStringList{base + "/to/a.txt"});
        QVERIFY(QFileInfo::exists(base + "/from/a.txt"));
        QVERIFY(w.statusBar()->currentMessage().isEmpty());

        // Shift moves, onto a folder's name puts them in it.
        const QModelIndex inner = tab->proxy()->mapFromSource(tab->model()->index(tab->model()->rowOf("inner"), 0));
        const QPoint onInner = tab->detailsView()->visualRect(inner).center();
        QVERIFY(drop(list, onInner, {base + "/from/b.txt"}, Qt::ShiftModifier));
        QTRY_VERIFY(QFileInfo::exists(base + "/to/inner/b.txt"));
        QTRY_VERIFY(!QFileInfo::exists(base + "/from/b.txt"));

        // Not where they already are, nor a folder into itself.
        QVERIFY(!drop(list, QPoint(5, list->height() - 5), {base + "/to/a.txt"}));
        QVERIFY(!drop(list, onInner, {base + "/to/inner"}));
        QVERIFY(!drop(list, QPoint(5, list->height() - 5), {base}));
        // Things that aren't files are turned away.
        QMimeData text;
        text.setText("words");
        QDragEnterEvent enter(QPointF(5, 5), Qt::CopyAction, &text, Qt::LeftButton, {});
        QApplication::sendEvent(list, &enter);
        QVERIFY(!enter.isAccepted());

        // Onto a tab, into its folder.
        w.display(0)->addTab(base + "/from");
        FolderTabBar *bar = w.display(0)->tabBar();
        QVERIFY(drop(bar, bar->tabRect(1).center(), {base + "/to/inner/b.txt"}, Qt::ShiftModifier));
        QTRY_VERIFY(QFileInfo::exists(base + "/from/b.txt"));
        QTRY_VERIFY(!QFileInfo::exists(base + "/to/inner/b.txt"));
    }

    void droppingCanAskWhatToDo()
    {
        QTemporaryDir dir;
        const QString base = canonical(dir.path());
        QDir().mkpath(base + "/from");
        QDir().mkpath(base + "/to");
        write(base + "/from/a.txt", "a");
        MainWindow w(base + "/to");
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        QTRY_COMPARE(w.activeTab()->itemCount(), 0);
        FileDrag::Drop d;
        d.paths = {base + "/from/a.txt"};
        d.folder = base + "/to";
        d.canMove = false;
        QMenu *menu = w.dropMenu(d);
        QStringList texts;
        for (QAction *a : menu->actions())
            if (!a->isSeparator())
                texts << a->text() + (a->isEnabled() ? "" : " (off)");
        QCOMPARE(texts, (QStringList{"Copy Here", "Move Here (off)", "Link Here", "Cancel"}));
        menu->actions().at(2)->trigger();
        QTRY_VERIFY(QFileInfo(base + "/to/a.txt").isSymLink());
        delete menu;
    }

    void favoritesShowInTheTree()
    {
        const FakeHome home;
        const QString h = home.path();
        QDir().mkpath(h + "/deep/down/here/inside");
        QDir().mkpath(h + "/other");
        MainWindow w(h);
        w.resize(900, 600);
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        w.setTreeShown(true);
        FolderTree *tree = w.windowTree();
        FolderTreeModel *model = tree->model();
        auto title = [&](const QModelIndex &i) { return i.data().toString(); };
        QTRY_COMPARE(tree->currentPath(), h);
        QCOMPARE(title(model->index(0, 0)), QString("Home")); // none yet
        TabOptions options = w.tabOptions();
        options.favorites.items = {Favorite::folder("~/deep/down/here", "Here"), Favorite::separator(),
                                   Favorite::branch("More", {Favorite::folder("~/other")})};
        w.setTabOptions(options);
        const QModelIndex group = model->index(0, 0);
        QCOMPARE(title(group), QString("Favourites"));
        QCOMPARE(model->rowCount(group), 2); // separators aren't for the tree
        QCOMPARE(tree->currentPath(), h);

        // Clicking a group opens it up; a favourite opens its folder, and
        // the tree stays there.
        auto click = [&](const QModelIndex &i) {
            QCoreApplication::processEvents();
            QTest::mouseClick(tree->view()->viewport(), Qt::LeftButton, Qt::NoModifier, tree->view()->visualRect(i).center());
        };
        click(group);
        QVERIFY(tree->view()->isExpanded(group));
        QCOMPARE(w.activeTab()->currentPath(), h);
        click(model->index(0, 0, group));
        QCOMPARE(w.activeTab()->currentPath(), h + "/deep/down/here");
        QCOMPARE(title(tree->view()->currentIndex()), QString("Here"));
        // Its sub-folders are followed in place, and back up to it.
        QVERIFY(w.activeTab()->openPath(h + "/deep/down/here/inside"));
        QTRY_COMPARE(tree->currentPath(), h + "/deep/down/here/inside");
        QCOMPARE(title(tree->view()->currentIndex().parent()), QString("Here"));
        w.activeTab()->goUp();
        QTRY_COMPARE(title(tree->view()->currentIndex()), QString("Here"));
        // Above it, or elsewhere, is in Home.
        w.activeTab()->goUp();
        QTRY_COMPARE(tree->currentPath(), h + "/deep/down");
        QCOMPARE(title(model->rootOf(tree->view()->currentIndex())), QString("Home"));
        QVERIFY(w.activeTab()->openPath(h + "/other"));
        QTRY_COMPARE(tree->currentPath(), h + "/other");
        QCOMPARE(title(model->rootOf(tree->view()->currentIndex())), QString("Home"));

        // Changing them keeps what was expanded, and the place.
        options = w.tabOptions();
        options.favorites.items << Favorite::folder("~/deep");
        w.setTabOptions(options);
        QCOMPARE(model->rowCount(model->index(0, 0)), 3);
        QTRY_VERIFY(tree->view()->isExpanded(model->index(0, 0)));
        QCOMPARE(tree->currentPath(), h + "/other");
        // Filters on files being shown don't upset them.
        options.filter.showHidden = true;
        w.setTabOptions(options);
        QCOMPARE(model->rowCount(model->index(0, 0)), 3);
        // Removed while the tree is in them: it finds its folder elsewhere.
        click(model->index(1, 0, model->index(0, 0)));
        QVERIFY(tree->view()->isExpanded(model->index(1, 0, model->index(0, 0))));
        click(model->index(2, 0, model->index(0, 0)));
        QCOMPARE(w.activeTab()->currentPath(), h + "/deep");
        options.tree.favorites = false;
        w.setTabOptions(options);
        QCOMPARE(title(model->index(0, 0)), QString("Home"));
        QTRY_COMPARE(model->path(tree->view()->currentIndex()), h + "/deep");
    }

private:
    // A home folder of its own for as long as it lives.
    class FakeHome {
    public:
        FakeHome()
        {
            for (const char *name : kVariables)
                m_before << qgetenv(name);
            qputenv("HOME", path().toUtf8());
            qunsetenv("XDG_CONFIG_HOME");
            qunsetenv("XDG_DATA_HOME");
            qunsetenv("XDG_CACHE_HOME");
            QDir().mkpath(path() + "/.config");
        }
        ~FakeHome()
        {
            for (int i = 0; i < m_before.size(); ++i) {
                if (m_before.at(i).isNull())
                    qunsetenv(kVariables[i]);
                else
                    qputenv(kVariables[i], m_before.at(i));
            }
        }
        QString path() const { return QFileInfo(m_dir.path()).canonicalFilePath(); }

    private:
        static constexpr const char *kVariables[] = {"HOME", "XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_CACHE_HOME"};
        QTemporaryDir m_dir;
        QList<QByteArray> m_before;
    };

    static SelectPattern pattern(const QString &text, SelectPattern::Action action = SelectPattern::Action::Select)
    {
        SelectPattern p;
        p.text = text;
        p.action = action;
        return p;
    }

    static QByteArray read(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<missing>");
    }

    static void setModified(const QString &path, const QDateTime &when)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.setFileTime(when, QFileDevice::FileModificationTime));
    }

    static QString canonical(const QString &path) { return QFileInfo(path).canonicalFilePath(); }

    // Drags `paths` from another program over `area` to `pos` and lets go.
    // True if the drop was taken.
    static bool drop(QWidget *area, const QPoint &pos, const QStringList &paths, Qt::KeyboardModifiers keys = {})
    {
        QMimeData mime;
        QList<QUrl> urls;
        for (const QString &path : paths)
            urls << QUrl::fromLocalFile(path);
        mime.setUrls(urls);
        const auto actions = Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
        QDragEnterEvent enter(pos, actions, &mime, Qt::LeftButton, keys);
        QApplication::sendEvent(area, &enter);
        QDragMoveEvent move(pos, actions, &mime, Qt::LeftButton, keys);
        QApplication::sendEvent(area, &move);
        QDropEvent dropped(pos, actions, &mime, Qt::LeftButton, keys);
        QApplication::sendEvent(area, &dropped);
        return dropped.isAccepted();
    }

    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(CargoHatchTests)
#include "cargohatch_tests.moc"
