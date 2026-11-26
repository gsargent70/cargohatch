#include "aliasdialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "folderpicker.h"

namespace {

bool same(const QString &a, const QString &b) {
    return a.compare(b, Qt::CaseInsensitive) == 0;
}

bool isBroken(const FolderAliases::Alias &a) {
    return !QFileInfo(FolderAliases::expand(a.path)).isDir();
}

} // namespace

AliasEditDialog::AliasEditDialog(const QString &title, const FolderAliases::Alias &alias, const QStringList &taken,
                                 FolderPicker *picker, QWidget *parent)
    : QDialog(parent),
      m_name(new QLineEdit(alias.name, this)),
      m_path(new QLineEdit(alias.path, this)),
      m_note(new QLabel(this)),
      m_taken(taken),
      m_picker(picker) {
    setWindowTitle(title);
    const int width = fontMetrics().averageCharWidth() * 46;
    m_name->setPlaceholderText(tr("work"));
    m_path->setMinimumWidth(width);
    m_note->setWordWrap(true);
    m_note->setTextFormat(Qt::PlainText);
    m_note->setForegroundRole(QPalette::PlaceholderText);
    m_note->setMinimumHeight(fontMetrics().lineSpacing() * 2);
    m_note->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    new PathCompleter(m_path);

    auto *browse = new QPushButton(tr("Browse…"), this);
    browse->setVisible(picker);
    browse->setAutoDefault(false);
    connect(browse, &QPushButton::clicked, this, [this] {
        QString error;
        const QString chosen = m_picker->pickFolder(this, windowTitle(), FolderAliases::expand(m_path->text()), &error);
        if (!chosen.isEmpty())
            m_path->setText(chosen);
        else if (!error.isEmpty())
            m_note->setText(error);
    });
    auto *path = new QHBoxLayout;
    path->addWidget(m_path, 1);
    path->addWidget(browse);
    auto *form = new QFormLayout;
    form->addRow(tr("&Name:"), m_name);
    auto *folderLabel = new QLabel(tr("&Folder:"), this);
    folderLabel->setBuddy(m_path);
    form->addRow(folderLabel, path);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize); // see ChooseFolderDialog
    layout->addLayout(form);
    layout->addWidget(m_note);
    layout->addWidget(buttons);
    connect(m_name, &QLineEdit::textChanged, this, &AliasEditDialog::validate);
    connect(m_path, &QLineEdit::textChanged, this, &AliasEditDialog::validate);
    m_name->selectAll();
    validate();
}

FolderAliases::Alias AliasEditDialog::alias() const {
    QString name = m_name->text().trimmed();
    // The ~ is how it is used, not part of the name, but easily typed.
    if (name.startsWith('~'))
        name.remove(0, 1);
    QString path = m_path->text().trimmed();
    while (path.size() > 1 && path.endsWith('/'))
        path.chop(1);
    return {name, path};
}

void AliasEditDialog::validate() {
    const FolderAliases::Alias a = alias();
    QString problem = FolderAliases::nameError(a.name);
    if (problem.isEmpty() && m_taken.contains(a.name, Qt::CaseInsensitive))
        problem = tr("There's already an alias called ~%1.").arg(a.name);
    const QString folder = FolderAliases::expand(a.path);
    if (problem.isEmpty() && a.path.isEmpty())
        problem = tr("Type the folder it stands for.");
    else if (problem.isEmpty() && !QDir::isAbsolutePath(folder))
        problem = tr("The folder needs a full path, or one starting with ~.");
    else if (problem.isEmpty() && a.path.startsWith('~') && same(a.path.section('/', 0, 0).mid(1), a.name))
        problem = tr("An alias can't stand for itself.");
    QString note = problem;
    if (note.isEmpty()) {
        for (const FolderAliases::Alias &b : FolderAliases::builtIn())
            if (same(b.name, a.name))
                note = tr("This takes the place of the built-in ~%1, which is %2.").arg(b.name, b.path);
    }
    if (note.isEmpty() && !QFileInfo(folder).isDir())
        note = tr("This folder doesn't exist at the moment.");
    if (note.isEmpty())
        note = tr("Type ~%1 wherever a path goes.").arg(a.name);
    m_note->setText(note);
    m_ok->setEnabled(problem.isEmpty());
}

AliasDialog::AliasDialog(const QList<FolderAliases::Alias> &aliases, FolderPicker *picker, QWidget *parent)
    : QDialog(parent),
      m_aliases(aliases),
      m_picker(picker),
      m_search(new QLineEdit(this)),
      m_list(new QTreeWidget(this)),
      m_showBuiltIn(new QCheckBox(tr("Show &built-in aliases"), this)),
      m_edit(new QPushButton(tr("&Edit…"), this)),
      m_duplicate(new QPushButton(tr("D&uplicate…"), this)),
      m_delete(new QPushButton(tr("&Delete"), this)),
      m_deleteBroken(new QPushButton(tr("Delete B&roken"), this)) {
    setWindowTitle(tr("Folder Aliases"));
    m_search->setPlaceholderText(tr("Search"));
    m_search->setClearButtonEnabled(true);
    m_list->setHeaderLabels({tr("Alias"), tr("Folder")});
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setTextElideMode(Qt::ElideMiddle);
    const int em = fontMetrics().averageCharWidth();
    m_list->setFixedSize(em * 64, fontMetrics().lineSpacing() * 17);
    m_list->header()->resizeSection(0, em * 20);
    m_list->header()->setStretchLastSection(true);
    m_showBuiltIn->setChecked(true);

    auto *add = new QPushButton(tr("&Add…"), this);
    auto *side = new QVBoxLayout;
    for (QPushButton *b : {add, m_edit, m_duplicate, m_delete, m_deleteBroken}) {
        b->setAutoDefault(false);
        side->addWidget(b);
    }
    side->addStretch(1);
    auto *middle = new QHBoxLayout;
    middle->addWidget(m_list);
    middle->addLayout(side);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize); // see ChooseFolderDialog
    layout->addWidget(m_search);
    layout->addLayout(middle);
    layout->addWidget(m_showBuiltIn);
    layout->addWidget(buttons);

    connect(m_search, &QLineEdit::textChanged, this, &AliasDialog::fill);
    connect(m_showBuiltIn, &QCheckBox::toggled, this, &AliasDialog::fill);
    connect(m_list, &QTreeWidget::itemSelectionChanged, this, &AliasDialog::updateButtons);
    connect(m_list, &QTreeWidget::itemDoubleClicked, this, [this] {
        if (m_edit->isEnabled())
            m_edit->click();
    });
    connect(add, &QPushButton::clicked, this, [this] { edit(-1, tr("Add Alias"), {}); });
    connect(m_edit, &QPushButton::clicked, this, [this] { edit(selected(), tr("Edit Alias"), m_aliases.at(selected())); });
    connect(m_duplicate, &QPushButton::clicked, this, [this] {
        const QTreeWidgetItem *item = m_list->currentItem();
        edit(-1, tr("Add Alias"), {item->text(0).mid(1) + "-2", item->text(1)});
    });
    connect(m_delete, &QPushButton::clicked, this, [this] {
        m_aliases.removeAt(selected());
        fill();
    });
    connect(m_deleteBroken, &QPushButton::clicked, this, [this] {
        const QList<FolderAliases::Alias> gone = broken(m_aliases);
        QMessageBox box(QMessageBox::Question, tr("Delete Broken Aliases"),
                        tr("Delete %n aliases whose folders aren't there?", "", int(gone.size())),
                        QMessageBox::Cancel, this);
        QStringList lines;
        for (const FolderAliases::Alias &a : gone)
            lines << QStringLiteral("~%1   %2").arg(a.name, a.path);
        // A drive that isn't plugged in looks the same as a deleted folder.
        box.setInformativeText(tr("They may be on a drive that isn't connected.") + "\n\n" +
                               lines.mid(0, 10).join('\n'));
        QPushButton *remove = box.addButton(tr("Delete"), QMessageBox::DestructiveRole);
        box.exec();
        if (box.clickedButton() != remove)
            return;
        m_aliases.removeIf([&gone](const FolderAliases::Alias &a) { return gone.contains(a); });
        fill();
    });
    fill();
}

QList<FolderAliases::Alias> AliasDialog::broken(const QList<FolderAliases::Alias> &aliases) {
    QList<FolderAliases::Alias> list;
    for (const FolderAliases::Alias &a : aliases)
        if (isBroken(a))
            list << a;
    return list;
}

int AliasDialog::selected() const {
    const QTreeWidgetItem *item = m_list->selectedItems().value(0);
    return item ? item->data(0, Qt::UserRole).toInt() : -1;
}

QStringList AliasDialog::namesExcept(int index) const {
    QStringList names;
    for (int i = 0; i < m_aliases.size(); ++i)
        if (i != index)
            names << m_aliases.at(i).name;
    return names;
}

void AliasDialog::edit(int index, const QString &title, FolderAliases::Alias alias) {
    AliasEditDialog dialog(title, alias, namesExcept(index), m_picker, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (index < 0)
        m_aliases << dialog.alias();
    else
        m_aliases[index] = dialog.alias();
    fill();
    for (int i = 0; i < m_list->topLevelItemCount(); ++i)
        if (m_list->topLevelItem(i)->text(0) == '~' + dialog.alias().name)
            m_list->setCurrentItem(m_list->topLevelItem(i));
}

void AliasDialog::fill() {
    m_list->clear();
    const QString wanted = m_search->text().trimmed();
    auto add = [&](const FolderAliases::Alias &a, int index) {
        if (!wanted.isEmpty() && !a.name.contains(wanted, Qt::CaseInsensitive) &&
            !a.path.contains(wanted, Qt::CaseInsensitive))
            return;
        auto *item = new QTreeWidgetItem(m_list, {'~' + a.name, a.path});
        item->setData(0, Qt::UserRole, index);
        if (index < 0) {
            for (int c : {0, 1}) {
                QFont font = item->font(c);
                font.setItalic(true);
                item->setFont(c, font);
            }
            item->setToolTip(0, tr("Built in"));
        }
        if (isBroken(a)) {
            for (int c : {0, 1})
                item->setForeground(c, palette().color(QPalette::PlaceholderText));
            item->setToolTip(1, tr("This folder isn't there at the moment"));
        } else {
            item->setToolTip(1, a.path);
        }
    };
    for (int i = 0; i < m_aliases.size(); ++i)
        add(m_aliases.at(i), i);
    if (m_showBuiltIn->isChecked())
        for (const FolderAliases::Alias &a : FolderAliases::builtIn())
            if (!namesExcept(-1).contains(a.name, Qt::CaseInsensitive))
                add(a, -1);
    m_list->sortItems(0, Qt::AscendingOrder);
    updateButtons();
}

void AliasDialog::updateButtons() {
    const bool any = !m_list->selectedItems().isEmpty();
    m_edit->setEnabled(selected() >= 0);
    m_delete->setEnabled(selected() >= 0);
    m_duplicate->setEnabled(any);
    m_deleteBroken->setEnabled(!broken(m_aliases).isEmpty());
}
