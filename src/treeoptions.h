#pragma once

#include <QDialog>
#include <QString>

class QCheckBox;
class QComboBox;
class QSettings;
class QSpinBox;

// How the folder tree looks and behaves (LST-08, TREE-01, TREE-03, TREE-04).
struct TreeOptions {
    // One tree for the window, or in a dual display one for each side.
    bool shared = true;
    bool right = false;
    // With a tree for each side, the second goes at the far right.
    bool secondOnRight = true;

    enum class Style { Arrows, Boxes, Lines };
    Style style = Style::Arrows;
    int indent = 18;
    int extraHeight = 2;
    bool fullRow = true;
    bool separateRoots = true;
    bool header = true;

    // Hidden folders, when the global filters let them show.
    bool hidden = true;
    // /proc, /sys and the like.
    bool system = false;
    bool drives = true;
    bool favorites = true;
    bool trash = true;
    // Folders shown only because they were opened go away again after.
    bool removeDynamic = true;

    bool autoExpand = true;
    // Open the current folder's own branch too.
    bool expandSelected = false;
    bool collapseOthers = false;
    bool collapseUnlessOpen = true;
    // Expanded branches come back with the window, in layouts too.
    bool remember = true;
    // Each tab has its own expanded branches.
    bool rememberTabs = false;

    bool operator==(const TreeOptions &o) const;
    static TreeOptions load(QSettings &settings);
    void save(QSettings &settings) const;
};

// View > Folder Tree > Options…
class TreeOptionsDialog : public QDialog {
    Q_OBJECT

public:
    explicit TreeOptionsDialog(const TreeOptions &options, QWidget *parent = nullptr);
    TreeOptions options() const;

private:
    void validate();

    QComboBox *m_count, *m_side, *m_style;
    QCheckBox *m_secondOnRight;
    QSpinBox *m_indent, *m_extraHeight;
    QCheckBox *m_fullRow, *m_separateRoots, *m_header;
    QCheckBox *m_hidden, *m_system, *m_favorites, *m_drives, *m_trash, *m_removeDynamic;
    QCheckBox *m_autoExpand, *m_expandSelected, *m_collapseOthers, *m_collapseUnlessOpen;
    QCheckBox *m_remember, *m_rememberTabs;
};
