#include "breadcrumb.h"

#include <QCollator>
#include <QDir>
#include <QEvent>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QStackedLayout>
#include <QToolButton>

#include "filedrag.h"
#include "filemodel.h"
#include "folderaliases.h"

namespace {

// Listing a huge folder into a menu would be unusable anyway; past this many
// entries the menu says how many were left out.
constexpr int kMaxMenuEntries = 500;

QString menuText(const QString &text) {
    // '&' marks a mnemonic in menus and buttons, so a literal one is doubled.
    return QString(text).replace('&', "&&");
}

} // namespace

Breadcrumb::Breadcrumb(QWidget *parent)
    : QWidget(parent),
      m_stack(new QStackedLayout(this)),
      m_crumbArea(new QWidget(this)),
      m_crumbLayout(new QHBoxLayout(m_crumbArea)),
      m_overflow(new QToolButton(m_crumbArea)),
      m_edit(new QLineEdit(this)),
      m_completer(nullptr) {
    m_crumbLayout->setContentsMargins(2, 0, 2, 0);
    m_crumbLayout->setSpacing(0);

    m_overflow->setText(QStringLiteral("«"));
    m_overflow->setAutoRaise(true);
    m_overflow->setFocusPolicy(Qt::NoFocus);
    m_overflow->setPopupMode(QToolButton::InstantPopup);
    m_overflow->setStyleSheet(QStringLiteral("QToolButton::menu-indicator { image: none; width: 0; }"));
    auto *overflowMenu = new QMenu(m_overflow);
    connect(overflowMenu, &QMenu::aboutToShow, this, &Breadcrumb::fillOverflowMenu);
    m_overflow->setMenu(overflowMenu);
    m_overflow->hide();

    // Without an explicit minimum, the layout would demand room for every
    // crumb, and a deep path would stop the window from getting narrower.
    // fitToWidth() folds crumbs away instead.
    m_crumbArea->setMinimumWidth(60);
    m_crumbArea->installEventFilter(this);
    m_crumbArea->setCursor(Qt::IBeamCursor);
    new FileDropTarget(m_crumbArea, [this](const QPoint &pos) -> FileDropTarget::Place {
        for (const CrumbWidgets &w : std::as_const(m_widgets))
            if (w.name->isVisible() && w.name->geometry().contains(pos))
                return {w.crumb.path, w.name->geometry()};
        return {};
    });
    m_edit->installEventFilter(this);
    // After this widget's own filter, so that Escape puts the list away
    // before it leaves the field.
    m_completer = new PathCompleter(m_edit);
    auto go = [this] { emit pathRequested(FolderAliases::expand(m_edit->text(), m_path)); };
    connect(m_edit, &QLineEdit::returnPressed, this, go);
    connect(m_completer, &PathCompleter::chosen, this, go);

    m_stack->addWidget(m_crumbArea);
    m_stack->addWidget(m_edit);
    m_stack->setCurrentWidget(m_crumbArea);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    retranslate();
}

QList<Crumb> Breadcrumb::crumbs(const QString &path, const QString &ghost) {
    QList<Crumb> list;
    if (path.isEmpty())
        return list;

    list.append({QStringLiteral("/"), QStringLiteral("/"), false});
    QString built;
    for (const QString &part : path.split('/', Qt::SkipEmptyParts)) {
        built += '/' + part;
        list.append({part, built, false});
    }

    // Ghost crumbs continue the path, so the ghost must lie below it.
    const QString prefix = path.endsWith('/') ? path : path + '/';
    if (!ghost.isEmpty() && ghost.startsWith(prefix)) {
        for (const QString &part : ghost.mid(prefix.size()).split('/', Qt::SkipEmptyParts)) {
            built += '/' + part;
            list.append({part, built, true});
        }
    }
    return list;
}

QStringList Breadcrumb::subfolders(const QString &path, QStringList *hidden) {
    const QStringList names =
        QDir(path).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::NoSort);
    QStringList visible, dotted;
    for (const QString &name : names)
        (name.startsWith('.') ? dotted : visible).append(name);

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    auto byName = [&](const QString &a, const QString &b) { return collator.compare(a, b) < 0; };
    std::sort(visible.begin(), visible.end(), byName);
    std::sort(dotted.begin(), dotted.end(), byName);

    if (hidden)
        *hidden = dotted;
    else
        visible += dotted;
    return visible;
}

void Breadcrumb::setPath(const QString &path, const QString &ghost) {
    const bool same = path == m_path && ghost == m_ghost;
    m_path = path;
    m_ghost = ghost;
    stopEditing();
    if (!same)
        rebuild();
}

void Breadcrumb::rebuild() {
    for (const auto &w : std::as_const(m_widgets)) {
        delete w.name;
        delete w.arrow;
    }
    m_widgets.clear();
    while (QLayoutItem *item = m_crumbLayout->takeAt(0))
        delete item;

    m_crumbLayout->addWidget(m_overflow);
    const QList<Crumb> list = crumbs(m_path, m_ghost);
    const QColor dim = palette().color(QPalette::PlaceholderText);

    for (int i = 0; i < list.size(); ++i) {
        const Crumb &crumb = list.at(i);
        auto *name = new QToolButton(m_crumbArea);
        name->setText(menuText(crumb.label));
        name->setToolTip(crumb.path);
        name->setAutoRaise(true);
        name->setFocusPolicy(Qt::NoFocus);
        name->setCursor(Qt::ArrowCursor);
        if (crumb.ghost) {
            QPalette p = name->palette();
            p.setColor(QPalette::ButtonText, dim);
            name->setPalette(p);
            QFont f = name->font();
            f.setItalic(true);
            name->setFont(f);
        } else if (crumb.path == m_path) {
            QFont f = name->font();
            f.setBold(true);
            name->setFont(f);
        }
        const QString target = crumb.path;
        connect(name, &QToolButton::clicked, this, [this, target] { emit pathRequested(target); });

        // The arrow after a folder lists its sub-folders, with the next crumb
        // (real or ghost) in bold so it's easy to see where the path goes.
        auto *arrow = new QToolButton(m_crumbArea);
        arrow->setText(QStringLiteral("›"));
        arrow->setAutoRaise(true);
        arrow->setFocusPolicy(Qt::NoFocus);
        arrow->setCursor(Qt::ArrowCursor);
        arrow->setPopupMode(QToolButton::InstantPopup);
        arrow->setStyleSheet(QStringLiteral("QToolButton::menu-indicator { image: none; width: 0; }"));
        auto *menu = new QMenu(arrow);
        const QString next = i + 1 < list.size() ? list.at(i + 1).label : QString();
        connect(menu, &QMenu::aboutToShow, this, [this, menu, target, next] { fillFolderMenu(menu, target, next); });
        arrow->setMenu(menu);

        m_crumbLayout->addWidget(name);
        m_crumbLayout->addWidget(arrow);
        m_widgets.append({crumb, name, arrow});
    }
    m_crumbLayout->addStretch(1);
    fitToWidth();
}

void Breadcrumb::fitToWidth() {
    for (const auto &w : std::as_const(m_widgets)) {
        w.name->setVisible(true);
        w.arrow->setVisible(true);
    }
    m_overflow->hide();
    if (m_widgets.isEmpty())
        return;

    const auto margins = m_crumbLayout->contentsMargins();
    // The crumb area fills the bar, but its own size lags behind while the
    // bar is being resized or is not yet shown, so measure the bar.
    const int available = width() - margins.left() - margins.right();
    auto pairWidth = [](const CrumbWidgets &w) { return w.name->sizeHint().width() + w.arrow->sizeHint().width(); };
    int total = 0;
    for (const auto &w : std::as_const(m_widgets))
        total += pairWidth(w);

    // Ghosts are the least important, so they go first, from the far end;
    // then leading folders fold into the « menu. The current folder always
    // stays.
    for (int i = m_widgets.size() - 1; i >= 0 && total > available && m_widgets.at(i).crumb.ghost; --i) {
        total -= pairWidth(m_widgets.at(i));
        m_widgets.at(i).name->hide();
        m_widgets.at(i).arrow->hide();
    }
    int current = 0;
    while (current + 1 < m_widgets.size() && !m_widgets.at(current + 1).crumb.ghost)
        ++current;
    bool folded = false;
    for (int i = 0; i < current && total > available; ++i) {
        if (!folded) {
            total += m_overflow->sizeHint().width();
            folded = true;
        }
        total -= pairWidth(m_widgets.at(i));
        m_widgets.at(i).name->hide();
        m_widgets.at(i).arrow->hide();
    }
    m_overflow->setVisible(folded);
}

void Breadcrumb::fillFolderMenu(QMenu *menu, const QString &folder, const QString &highlight) {
    menu->clear();
    QStringList hidden;
    QStringList visible = subfolders(folder, &hidden);
    const GlobalFilter::Matcher matcher(m_filter);
    const QSet<QString> listed = FileModel::hiddenNames(folder);
    for (QStringList *names : {&visible, &hidden})
        names->removeIf([&](const QString &name) {
            // The folder the path goes through stays, even if it is hidden.
            return name != highlight && matcher.hides(name, true, listed.contains(name));
        });
    const QIcon icon = QFileIconProvider().icon(QFileIconProvider::Folder);
    int added = 0;
    auto add = [&](const QString &name) {
        if (added++ >= kMaxMenuEntries)
            return;
        const QString target = folder == "/" ? "/" + name : folder + "/" + name;
        QAction *action = menu->addAction(icon, menuText(name));
        if (name == highlight) {
            QFont f = action->font();
            f.setBold(true);
            action->setFont(f);
        }
        connect(action, &QAction::triggered, this, [this, target] { emit pathRequested(target); });
    };
    for (const QString &name : visible)
        add(name);
    if (!visible.isEmpty() && !hidden.isEmpty())
        menu->addSeparator();
    for (const QString &name : hidden)
        add(name);

    const int total = visible.size() + hidden.size();
    if (total == 0)
        menu->addAction(tr("No sub-folders"))->setEnabled(false);
    else if (total > kMaxMenuEntries)
        menu->addAction(tr("%n more not shown", "", total - kMaxMenuEntries))->setEnabled(false);
}

void Breadcrumb::fillOverflowMenu() {
    QMenu *menu = m_overflow->menu();
    menu->clear();
    const QIcon icon = QFileIconProvider().icon(QFileIconProvider::Folder);
    for (const auto &w : std::as_const(m_widgets)) {
        if (w.name->isVisible() || w.crumb.ghost)
            continue;
        const QString target = w.crumb.path;
        connect(menu->addAction(icon, menuText(w.crumb.path)), &QAction::triggered, this,
                [this, target] { emit pathRequested(target); });
    }
}

void Breadcrumb::setGlobalFilter(const GlobalFilter &filter) {
    m_filter = filter;
    m_completer->setGlobalFilter(filter);
}

void Breadcrumb::startEditing() {
    m_edit->setText(m_path);
    m_completer->setBase(m_path);
    m_stack->setCurrentWidget(m_edit);
    m_edit->setFocus(Qt::ShortcutFocusReason);
    m_edit->selectAll();
}

void Breadcrumb::stopEditing() {
    if (!isEditing())
        return;
    m_completer->reset();
    const bool hadFocus = m_edit->hasFocus();
    m_stack->setCurrentWidget(m_crumbArea);
    // Hand focus back to the file list rather than letting it fall to
    // whatever widget Qt picks next.
    if (hadFocus && parentWidget())
        parentWidget()->setFocus(Qt::OtherFocusReason);
}

bool Breadcrumb::isEditing() const {
    return m_stack->currentWidget() == m_edit;
}

int Breadcrumb::visibleCrumbCount() const {
    int count = 0;
    for (const auto &w : m_widgets)
        count += w.name->isVisibleTo(m_crumbArea) ? 1 : 0;
    return count;
}

bool Breadcrumb::isOverflowing() const {
    return m_overflow->isVisibleTo(m_crumbArea);
}

void Breadcrumb::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    fitToWidth();
}

void Breadcrumb::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    else if (event->type() == QEvent::PaletteChange)
        rebuild(); // Ghost crumbs take their dim color from the palette.
    QWidget::changeEvent(event);
}

bool Breadcrumb::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_crumbArea && event->type() == QEvent::MouseButtonPress) {
        if (static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
            startEditing();
            return true;
        }
    } else if (watched == m_edit && event->type() == QEvent::KeyPress) {
        if (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            stopEditing();
            return true;
        }
    } else if (watched == m_edit && event->type() == QEvent::FocusOut) {
        // Switching to another window, or a popup, shouldn't throw away a
        // half-typed path; clicking elsewhere in CargoHatch should.
        const auto reason = static_cast<QFocusEvent *>(event)->reason();
        if (reason != Qt::ActiveWindowFocusReason && reason != Qt::PopupFocusReason)
            stopEditing();
    }
    return QWidget::eventFilter(watched, event);
}

void Breadcrumb::retranslate() {
    m_overflow->setToolTip(tr("More folders"));
    m_edit->setPlaceholderText(tr("Type a folder path"));
    m_crumbArea->setToolTip(tr("Click to type a path"));
}
