#include "treeoptions.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

const char *const kStyles[] = {"arrows", "boxes", "lines"};

// The check boxes, by their names in the settings file.
const std::pair<const char *, bool TreeOptions::*> kFlags[] = {
    {"shared", &TreeOptions::shared},
    {"right", &TreeOptions::right},
    {"secondOnRight", &TreeOptions::secondOnRight},
    {"fullRow", &TreeOptions::fullRow},
    {"separateRoots", &TreeOptions::separateRoots},
    {"header", &TreeOptions::header},
    {"hidden", &TreeOptions::hidden},
    {"system", &TreeOptions::system},
    {"drives", &TreeOptions::drives},
    {"favorites", &TreeOptions::favorites},
    {"trash", &TreeOptions::trash},
    {"removeDynamic", &TreeOptions::removeDynamic},
    {"autoExpand", &TreeOptions::autoExpand},
    {"expandSelected", &TreeOptions::expandSelected},
    {"collapseOthers", &TreeOptions::collapseOthers},
    {"collapseUnlessOpen", &TreeOptions::collapseUnlessOpen},
    {"remember", &TreeOptions::remember},
    {"rememberTabs", &TreeOptions::rememberTabs},
};

} // namespace

bool TreeOptions::operator==(const TreeOptions &o) const {
    for (const auto &[name, flag] : kFlags)
        if (this->*flag != o.*flag)
            return false;
    return style == o.style && indent == o.indent && extraHeight == o.extraHeight;
}

TreeOptions TreeOptions::load(QSettings &settings) {
    TreeOptions o;
    settings.beginGroup("tree");
    for (const auto &[name, flag] : kFlags)
        o.*flag = settings.value(name, o.*flag).toBool();
    const QString style = settings.value("style").toString();
    for (int i = 0; i < 3; ++i)
        if (style == kStyles[i])
            o.style = Style(i);
    o.indent = qBound(8, settings.value("indent", o.indent).toInt(), 60);
    o.extraHeight = qBound(0, settings.value("extraHeight", o.extraHeight).toInt(), 40);
    settings.endGroup();
    return o;
}

void TreeOptions::save(QSettings &settings) const {
    settings.beginGroup("tree");
    for (const auto &[name, flag] : kFlags)
        settings.setValue(name, this->*flag);
    settings.setValue("style", kStyles[int(style)]);
    settings.setValue("indent", indent);
    settings.setValue("extraHeight", extraHeight);
    settings.endGroup();
}

TreeOptionsDialog::TreeOptionsDialog(const TreeOptions &o, QWidget *parent)
    : QDialog(parent),
      m_count(new QComboBox(this)),
      m_side(new QComboBox(this)),
      m_style(new QComboBox(this)),
      m_secondOnRight(new QCheckBox(tr("Put the second tree at the far right"), this)),
      m_indent(new QSpinBox(this)),
      m_extraHeight(new QSpinBox(this)),
      m_fullRow(new QCheckBox(tr("Select the full row"), this)),
      m_separateRoots(new QCheckBox(tr("Separate groups of top-level items"), this)),
      m_header(new QCheckBox(tr("Show the heading"), this)),
      m_hidden(new QCheckBox(tr("Hidden folders, when hidden files show"), this)),
      m_system(new QCheckBox(tr("System folders (/proc, /sys, /dev…)"), this)),
      m_favorites(new QCheckBox(tr("Favourites"), this)),
      m_drives(new QCheckBox(tr("Drives and network mounts"), this)),
      m_trash(new QCheckBox(tr("Trash"), this)),
      m_removeDynamic(new QCheckBox(tr("Remove other folders shown only\nwhile they were open"), this)),
      m_autoExpand(new QCheckBox(tr("Expand to the current folder"), this)),
      m_expandSelected(new QCheckBox(tr("Expand the current folder itself"), this)),
      m_collapseOthers(new QCheckBox(tr("Collapse other branches"), this)),
      m_collapseUnlessOpen(new QCheckBox(tr("Except those open in other tabs"), this)),
      m_remember(new QCheckBox(tr("Remember expanded branches, in layouts too"), this)),
      m_rememberTabs(new QCheckBox(tr("Remember them for each tab"), this)) {
    setWindowTitle(tr("Folder Tree Options"));
    m_count->addItems({tr("One for both sides"), tr("One for each side")});
    m_count->setCurrentIndex(o.shared ? 0 : 1);
    m_side->addItems({tr("Left"), tr("Right")});
    m_side->setCurrentIndex(o.right ? 1 : 0);
    m_style->addItems({tr("Arrows"), tr("Plus and minus boxes"), tr("Boxes and lines")});
    m_style->setCurrentIndex(int(o.style));
    m_indent->setRange(8, 60);
    m_indent->setSuffix(tr(" px"));
    m_indent->setValue(o.indent);
    m_extraHeight->setRange(0, 40);
    m_extraHeight->setSuffix(tr(" px"));
    m_extraHeight->setValue(o.extraHeight);
    m_secondOnRight->setChecked(o.secondOnRight);
    m_fullRow->setChecked(o.fullRow);
    m_separateRoots->setChecked(o.separateRoots);
    m_header->setChecked(o.header);
    m_hidden->setChecked(o.hidden);
    m_system->setChecked(o.system);
    m_favorites->setChecked(o.favorites);
    m_drives->setChecked(o.drives);
    m_trash->setChecked(o.trash);
    m_removeDynamic->setChecked(o.removeDynamic);
    m_autoExpand->setChecked(o.autoExpand);
    m_expandSelected->setChecked(o.expandSelected);
    m_collapseOthers->setChecked(o.collapseOthers);
    m_collapseUnlessOpen->setChecked(o.collapseUnlessOpen);
    m_remember->setChecked(o.remember);
    m_rememberTabs->setChecked(o.rememberTabs);

    auto group = [this](const QString &title, const QList<QWidget *> &widgets, QFormLayout *form = nullptr) {
        auto *box = new QGroupBox(title, this);
        auto *column = new QVBoxLayout(box);
        if (form)
            column->addLayout(form);
        for (QWidget *w : widgets)
            column->addWidget(w);
        column->addStretch(1);
        return box;
    };
    auto *place = new QFormLayout;
    place->addRow(tr("Trees in a dual display:"), m_count);
    place->addRow(tr("Side:"), m_side);
    auto *look = new QFormLayout;
    look->addRow(tr("Style:"), m_style);
    look->addRow(tr("Indent:"), m_indent);
    look->addRow(tr("Extra row height:"), m_extraHeight);

    // What an option depends on sits above it, indented.
    auto indented = [this](QWidget *w) {
        auto *holder = new QWidget(this);
        auto *row = new QHBoxLayout(holder);
        row->setContentsMargins(20, 0, 0, 0);
        row->addWidget(w);
        return holder;
    };
    auto *grid = new QGridLayout;
    grid->addWidget(group(tr("Position"), {m_secondOnRight}, place), 0, 0);
    grid->addWidget(group(tr("Appearance"), {m_fullRow, m_separateRoots, m_header}, look), 1, 0);
    grid->addWidget(group(tr("Contents"), {m_favorites, m_hidden, m_system, m_drives, m_trash, m_removeDynamic}), 0, 1);
    grid->addWidget(group(tr("Following"), {m_autoExpand, m_expandSelected, m_collapseOthers,
                                            indented(m_collapseUnlessOpen), m_remember, m_rememberTabs}), 1, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize); // see ChooseFolderDialog
    layout->addLayout(grid);
    layout->addWidget(buttons);
    connect(m_count, &QComboBox::currentIndexChanged, this, &TreeOptionsDialog::validate);
    for (QCheckBox *box : {m_collapseOthers, m_autoExpand, m_remember})
        connect(box, &QCheckBox::toggled, this, &TreeOptionsDialog::validate);
    validate();
}

void TreeOptionsDialog::validate() {
    m_secondOnRight->setEnabled(m_count->currentIndex() == 1);
    m_expandSelected->setEnabled(m_autoExpand->isChecked());
    m_collapseUnlessOpen->setEnabled(m_collapseOthers->isChecked());
}

TreeOptions TreeOptionsDialog::options() const {
    TreeOptions o;
    o.shared = m_count->currentIndex() == 0;
    o.right = m_side->currentIndex() == 1;
    o.secondOnRight = m_secondOnRight->isChecked();
    o.style = TreeOptions::Style(m_style->currentIndex());
    o.indent = m_indent->value();
    o.extraHeight = m_extraHeight->value();
    o.fullRow = m_fullRow->isChecked();
    o.separateRoots = m_separateRoots->isChecked();
    o.header = m_header->isChecked();
    o.hidden = m_hidden->isChecked();
    o.system = m_system->isChecked();
    o.favorites = m_favorites->isChecked();
    o.drives = m_drives->isChecked();
    o.trash = m_trash->isChecked();
    o.removeDynamic = m_removeDynamic->isChecked();
    o.autoExpand = m_autoExpand->isChecked();
    o.expandSelected = m_expandSelected->isChecked();
    o.collapseOthers = m_collapseOthers->isChecked();
    o.collapseUnlessOpen = m_collapseUnlessOpen->isChecked();
    o.remember = m_remember->isChecked();
    o.rememberTabs = m_rememberTabs->isChecked();
    return o;
}
