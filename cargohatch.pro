QT += core gui widgets concurrent dbus multimedia multimediawidgets

CONFIG += c++17 release
TARGET = cargohatch
TEMPLATE = app

HEADERS += \
    src/breadcrumb.h \
    src/terminal.h \
    src/filedrag.h \
    src/filedisplay.h \
    src/filemodel.h \
    src/fileops.h \
    src/favoritesdialog.h \
    src/favoritesbar.h \
    src/favorites.h \
    src/treeoptions.h \
    src/foldertreemodel.h \
    src/foldertree.h \
    src/pathcompleter.h \
    src/preview.h \
    src/batchrename.h \
    src/renamedialog.h \
    src/viewerpane.h \
    src/folderaliases.h \
    src/aliasdialog.h \
    src/aboutdialog.h \
    src/shortcutsdialog.h \
    src/quickfilter.h \
    src/namematch.h \
    src/findbar.h \
    src/nameformat.h \
    src/globalfilter.h \
    src/fileclipboard.h \
    src/createfolder.h \
    src/folderpicker.h \
    src/operationrunner.h \
    src/progresswindow.h \
    src/renamedelegate.h \
    src/replacedialog.h \
    src/selectpattern.h \
    src/filesortproxy.h \
    src/tiledelegate.h \
    src/viewformat.h \
    src/foldertab.h \
    src/history.h \
    src/icons.h \
    src/listermanager.h \
    src/listerstate.h \
    src/mainwindow.h \
    src/savedsets.h \
    src/tabbar.h \
    src/taboptions.h \
    src/tabswitcher.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/breadcrumb.cpp \
    src/terminal.cpp \
    src/filedrag.cpp \
    src/filedisplay.cpp \
    src/filemodel.cpp \
    src/fileops.cpp \
    src/favoritesdialog.cpp \
    src/favoritesbar.cpp \
    src/favorites.cpp \
    src/treeoptions.cpp \
    src/foldertreemodel.cpp \
    src/foldertree.cpp \
    src/pathcompleter.cpp \
    src/preview.cpp \
    src/batchrename.cpp \
    src/renamedialog.cpp \
    src/viewerpane.cpp \
    src/folderaliases.cpp \
    src/aliasdialog.cpp \
    src/aboutdialog.cpp \
    src/shortcutsdialog.cpp \
    src/quickfilter.cpp \
    src/namematch.cpp \
    src/findbar.cpp \
    src/nameformat.cpp \
    src/globalfilter.cpp \
    src/fileclipboard.cpp \
    src/createfolder.cpp \
    src/folderpicker.cpp \
    src/operationrunner.cpp \
    src/progresswindow.cpp \
    src/renamedelegate.cpp \
    src/replacedialog.cpp \
    src/selectpattern.cpp \
    src/filesortproxy.cpp \
    src/tiledelegate.cpp \
    src/viewformat.cpp \
    src/foldertab.cpp \
    src/history.cpp \
    src/icons.cpp \
    src/listermanager.cpp \
    src/listerstate.cpp \
    src/mainwindow.cpp \
    src/savedsets.cpp \
    src/tabbar.cpp \
    src/taboptions.cpp \
    src/tabswitcher.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
