#include "renamedialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

RenameDialog::RenameDialog(const BatchRename &rules, const QList<QPair<QString, bool>> &items,
                           const QSet<QString> &others, QWidget *parent)
    : QDialog(parent),
      m_items(items),
      m_others(others),
      m_find(new QLineEdit(this)),
      m_replace(new QLineEdit(this)),
      m_matchCase(new QCheckBox(tr("Match case"), this)),
      m_ignoreExtension(new QCheckBox(tr("Leave extensions out of it"), this)),
      m_case(new QComboBox(this)),
      m_extensionCase(new QComboBox(this)),
      m_applyWithoutMatch(new QCheckBox(tr("Change capitalisation even where Find doesn't match"), this)),
      m_preview(new QTreeWidget(this)),
      m_summary(new QLabel(this)),
      m_buttons(new QDialogButtonBox(QDialogButtonBox::Cancel, this)) {
    setWindowTitle(tr("Rename Several"));
    m_find->setText(rules.find);
    m_replace->setText(rules.replace);
    m_find->setPlaceholderText(tr("Text in the names"));
    m_replace->setPlaceholderText(tr("Nothing, to remove it"));
    m_matchCase->setChecked(rules.matchCase);
    m_ignoreExtension->setChecked(rules.ignoreExtension);
    m_ignoreExtension->setToolTip(tr("Find, replace and the name's capitalisation leave each file's extension alone"));
    m_applyWithoutMatch->setChecked(rules.applyWithoutMatch);
    using C = BatchRename::Case;
    for (C how : {C::Keep, C::Lower, C::Upper, C::FirstWord, C::EveryWord})
        m_case->addItem(caseTitle(how), int(how));
    m_case->setCurrentIndex(m_case->findData(int(rules.nameCase)));
    using E = BatchRename::ExtensionCase;
    for (E how : {E::Keep, E::Lower, E::Upper})
        m_extensionCase->addItem(extensionCaseTitle(how), int(how));
    m_extensionCase->setCurrentIndex(m_extensionCase->findData(int(rules.extensionCase)));

    m_preview->setColumnCount(2);
    m_preview->setHeaderLabels({tr("Name"), tr("New Name")});
    m_preview->setRootIsDecorated(false);
    m_preview->setSelectionMode(QAbstractItemView::NoSelection);
    m_preview->setFocusPolicy(Qt::NoFocus);
    m_preview->setMinimumSize(560, 240);
    m_preview->header()->setSectionResizeMode(QHeaderView::Stretch);
    m_summary->setWordWrap(true);

    QPushButton *rename = m_buttons->addButton(tr("Rename"), QDialogButtonBox::AcceptRole);
    rename->setDefault(true);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *form = new QFormLayout;
    form->addRow(tr("Find:"), m_find);
    form->addRow(tr("Replace with:"), m_replace);
    auto *checks = new QHBoxLayout;
    checks->addWidget(m_matchCase);
    checks->addWidget(m_ignoreExtension);
    checks->addStretch();
    form->addRow(QString(), checks);
    form->addRow(tr("Capitalisation:"), m_case);
    form->addRow(tr("Extensions:"), m_extensionCase);
    form->addRow(QString(), m_applyWithoutMatch);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_preview, 1);
    layout->addWidget(m_summary);
    layout->addWidget(m_buttons);
    // Omarchy gives new windows a large size unless they fix their own.
    layout->setSizeConstraint(QLayout::SetFixedSize);

    for (QLineEdit *field : {m_find, m_replace})
        connect(field, &QLineEdit::textChanged, this, &RenameDialog::updatePreview);
    for (QCheckBox *box : {m_matchCase, m_ignoreExtension, m_applyWithoutMatch})
        connect(box, &QCheckBox::toggled, this, &RenameDialog::updatePreview);
    for (QComboBox *box : {m_case, m_extensionCase})
        connect(box, &QComboBox::currentIndexChanged, this, &RenameDialog::updatePreview);
    updatePreview();
    m_find->setFocus();
}

BatchRename RenameDialog::rules() const {
    BatchRename r;
    r.find = m_find->text();
    r.replace = m_replace->text();
    r.matchCase = m_matchCase->isChecked();
    r.ignoreExtension = m_ignoreExtension->isChecked();
    r.applyWithoutMatch = m_applyWithoutMatch->isChecked();
    r.nameCase = BatchRename::Case(m_case->currentData().toInt());
    r.extensionCase = BatchRename::ExtensionCase(m_extensionCase->currentData().toInt());
    return r;
}

void RenameDialog::updatePreview() {
    const BatchRename r = rules();
    m_plan = r.plan(m_items, m_others);
    // Only matters when there's something to find and a case to change.
    m_applyWithoutMatch->setEnabled(!r.find.isEmpty() && r.nameCase != BatchRename::Case::Keep);
    m_preview->clear();
    const QColor dim = palette().color(QPalette::PlaceholderText);
    // Red reads on light and dark themes alike.
    const QColor bad(220, 50, 47);
    for (const BatchRename::Item &item : std::as_const(m_plan)) {
        auto *row = new QTreeWidgetItem(m_preview, {item.oldName, item.changes() ? item.newName : QString()});
        if (!item.changes()) {
            row->setForeground(0, dim);
            row->setText(1, tr("(unchanged)"));
            row->setForeground(1, dim);
        } else if (!item.problem.isEmpty()) {
            row->setForeground(1, bad);
            row->setToolTip(1, item.problem);
        }
    }
    for (QAbstractButton *b : m_buttons->buttons())
        if (m_buttons->buttonRole(b) == QDialogButtonBox::AcceptRole)
            b->setEnabled(canRename());
    m_summary->setText(summary());
}

bool RenameDialog::canRename() const {
    bool any = false;
    for (const BatchRename::Item &item : m_plan) {
        if (!item.problem.isEmpty())
            return false;
        any = any || item.changes();
    }
    return any;
}

QString RenameDialog::summary() const {
    int changing = 0;
    for (const BatchRename::Item &item : m_plan) {
        if (!item.problem.isEmpty())
            return item.problem;
        if (item.changes())
            ++changing;
    }
    if (changing == 0)
        return tr("No names change yet.");
    return tr("%1 of %n items will be renamed.", "", int(m_plan.size())).arg(changing);
}

QString RenameDialog::caseTitle(BatchRename::Case how) {
    switch (how) {
    case BatchRename::Case::Keep:
        return tr("Leave as it is");
    case BatchRename::Case::Lower:
        return tr("lower case");
    case BatchRename::Case::Upper:
        return tr("UPPER CASE");
    case BatchRename::Case::FirstWord:
        return tr("First word capitalised");
    case BatchRename::Case::EveryWord:
        return tr("Every Word Capitalised");
    }
    return {};
}

QString RenameDialog::extensionCaseTitle(BatchRename::ExtensionCase how) {
    switch (how) {
    case BatchRename::ExtensionCase::Keep:
        return tr("Leave as they are");
    case BatchRename::ExtensionCase::Lower:
        return tr("lower case");
    case BatchRename::ExtensionCase::Upper:
        return tr("UPPER CASE");
    }
    return {};
}
