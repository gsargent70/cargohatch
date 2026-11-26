#include "shortcutsdialog.h"

#include <QAction>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

QString plain(QString text) {
    text = text.section('\t', 0, 0);
    // "&&" is a real ampersand; a single "&" only marks the access key.
    text.replace("&&", QStringLiteral("\x01"));
    text.remove('&');
    text.replace(QStringLiteral("\x01"), QStringLiteral("&"));
    return text;
}

void collect(const QMenu *menu, const QString &section, const QString &prefix, QList<ShortcutsDialog::Row> *rows) {
    for (QAction *a : menu->actions()) {
        if (a->isSeparator() || !a->isVisible())
            continue;
        if (a->menu()) {
            collect(a->menu(), section, prefix + plain(a->text()) + QStringLiteral(" ▸ "), rows);
            continue;
        }
        // A key shown in the text is one the list handles itself.
        QString keys = a->text().section('\t', 1);
        QStringList all;
        for (const QKeySequence &key : a->shortcuts())
            all << key.toString(QKeySequence::NativeText);
        if (!all.isEmpty())
            keys = all.join(QStringLiteral(", "));
        if (!keys.isEmpty())
            rows->append({section, prefix + plain(a->text()), keys});
    }
}

} // namespace

QList<ShortcutsDialog::Row> ShortcutsDialog::rows(const QMenuBar *bar) {
    QList<Row> rows;
    for (QAction *a : bar->actions())
        if (a->menu() && a->isVisible())
            collect(a->menu(), plain(a->text()), {}, &rows);
    const QString list = tr("Other Keys");
    rows.append({list, tr("Find a name by typing it"), tr("Letters")});
    rows.append({list, tr("Keyboard Shortcuts"), QStringLiteral("?")});
    rows.append({list, tr("Switch Tab, backwards"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab).toString(QKeySequence::NativeText)});
    const QString drag = tr("While Dragging Files");
    rows.append({drag, tr("Copy"), tr("Ctrl")});
    rows.append({drag, tr("Move"), tr("Shift")});
    rows.append({drag, tr("Link"), tr("Ctrl+Shift")});
    rows.append({drag, tr("Ask What to Do"), tr("Alt")});
    return rows;
}

ShortcutsDialog::ShortcutsDialog(const QMenuBar *bar, QWidget *parent)
    : QDialog(parent), m_filter(new QLineEdit(this)), m_list(new QTreeWidget(this)) {
    setWindowTitle(tr("Keyboard Shortcuts"));
    m_filter->setPlaceholderText(tr("Find a command or key"));
    m_filter->setClearButtonEnabled(true);
    m_list->setColumnCount(2);
    m_list->setHeaderLabels({tr("Command"), tr("Keys")});
    m_list->setRootIsDecorated(false);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_list->header()->setStretchLastSection(false);
    QTreeWidgetItem *group = nullptr;
    for (const Row &row : rows(bar)) {
        if (!group || group->text(0) != row.section) {
            group = new QTreeWidgetItem(m_list, {row.section});
            QFont bold = group->font(0);
            bold.setBold(true);
            group->setFont(0, bold);
            group->setFirstColumnSpanned(true);
        }
        new QTreeWidgetItem(group, {row.command, row.keys});
    }
    m_list->expandAll();
    connect(m_filter, &QLineEdit::textChanged, this, &ShortcutsDialog::setFilter);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_filter);
    layout->addWidget(m_list, 1);
    layout->addWidget(buttons);
    resize(640, 640);
}

void ShortcutsDialog::setFilter(const QString &text) {
    for (int g = 0; g < m_list->topLevelItemCount(); ++g) {
        QTreeWidgetItem *group = m_list->topLevelItem(g);
        bool any = false;
        for (int i = 0; i < group->childCount(); ++i) {
            QTreeWidgetItem *item = group->child(i);
            const bool match = text.isEmpty() || item->text(0).contains(text, Qt::CaseInsensitive) ||
                               item->text(1).contains(text, Qt::CaseInsensitive) ||
                               group->text(0).contains(text, Qt::CaseInsensitive);
            item->setHidden(!match);
            any = any || match;
        }
        group->setHidden(!any);
    }
}

int ShortcutsDialog::shownCount() const {
    int n = 0;
    for (int g = 0; g < m_list->topLevelItemCount(); ++g)
        for (int i = 0; i < m_list->topLevelItem(g)->childCount(); ++i)
            n += !m_list->topLevelItem(g)->child(i)->isHidden();
    return n;
}
