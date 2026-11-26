#include "replacedialog.h"

#include "filemodel.h"

#include <QCheckBox>
#include <QDir>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QImageReader>
#include <QInputDialog>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMimeDatabase>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kPreviewSize = 96;

QString shortPath(const QString &path) {
    const QString home = QDir::homePath();
    if (path == home)
        return QStringLiteral("~");
    if (path.startsWith(home + '/'))
        return '~' + path.mid(home.size());
    return path;
}

QToolButton *menuButton(const QString &text, QWidget *parent) {
    auto *b = new QToolButton(parent);
    b->setText(text);
    b->setToolButtonStyle(Qt::ToolButtonTextOnly);
    b->setPopupMode(QToolButton::MenuButtonPopup);
    b->setMenu(new QMenu(b));
    b->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    return b;
}

} // namespace

QPixmap ReplaceDialog::preview(const QString &path, bool isDir, int size) {
    if (!isDir) {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        if (reader.canRead()) {
            const QSize full = reader.size();
            if (full.isValid())
                reader.setScaledSize(full.scaled(size, size, Qt::KeepAspectRatio));
            const QImage image = reader.read();
            if (!image.isNull())
                return QPixmap::fromImage(image);
        }
    }
    QIcon icon;
    if (isDir) {
        icon = QIcon::fromTheme("folder", QFileIconProvider().icon(QFileIconProvider::Folder));
    } else {
        const QMimeType mime = QMimeDatabase().mimeTypeForFile(path);
        icon = QIcon::fromTheme(mime.iconName(), QIcon::fromTheme(mime.genericIconName()));
        if (icon.isNull())
            icon = QFileIconProvider().icon(QFileInfo(path));
    }
    return icon.pixmap(size, size);
}

ReplaceDialog::ReplaceDialog(const FileOperation::Conflict &c, QWidget *parent)
    : QDialog(parent), m_conflict(c), m_forAll(new QCheckBox(tr("Do this for the rest as well"), this)) {
    const bool folders = c.sourceIsDir && c.targetIsDir;
    setWindowTitle(folders ? tr("Folder Already Exists") : tr("Replace File?"));
    // Closing the window cancels the whole operation.
    m_resolution.cancel = true;

    const QFileInfo target(c.target);
    auto *message = new QLabel(
        (folders ? tr("A folder called <b>%1</b> is already in %2.") : tr("<b>%1</b> is already in %2."))
            .arg(target.fileName().toHtmlEscaped(), shortPath(target.path()).toHtmlEscaped()),
        this);
    message->setWordWrap(true);

    // The two items side by side, with the differences marked.
    const QLocale locale;
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(4);
    struct Side {
        QString title, path;
        bool isDir;
        qint64 size;
        QDateTime modified;
        bool larger, newer;
    };
    const Side sides[] = {
        {tr("Existing"), c.target, c.targetIsDir, c.targetSize, c.targetModified, c.targetSize > c.sourceSize,
         c.targetModified > c.sourceModified},
        {tr("Incoming"), c.source, c.sourceIsDir, c.sourceSize, c.sourceModified, c.sourceSize > c.targetSize,
         c.sourceModified > c.targetModified},
    };
    for (int col = 0; col < 2; ++col) {
        const Side &side = sides[col];
        auto *title = new QLabel(QStringLiteral("<b>%1</b>").arg(side.title), this);
        auto *picture = new QLabel(this);
        picture->setFixedSize(kPreviewSize, kPreviewSize);
        picture->setAlignment(Qt::AlignCenter);
        picture->setPixmap(preview(side.path, side.isDir, kPreviewSize));
        auto *where = new QLabel((col == 0 ? tr("in %1") : tr("from %1")).arg(shortPath(QFileInfo(side.path).path())), this);
        where->setForegroundRole(QPalette::PlaceholderText);
        where->setMaximumWidth(fontMetrics().averageCharWidth() * 36);
        where->setText(fontMetrics().elidedText(where->text(), Qt::ElideMiddle, where->maximumWidth()));
        QString size = side.isDir ? tr("Folder")
                                  : FileModel::sizeText(side.size);
        if (!side.isDir && side.larger)
            size += QStringLiteral("  ·  ") + tr("larger");
        QString date = locale.toString(side.modified, QLocale::ShortFormat);
        if (side.newer)
            date += QStringLiteral("  ·  ") + tr("newer");
        grid->addWidget(title, 0, col);
        grid->addWidget(picture, 1, col);
        grid->addWidget(new QLabel(QFileInfo(side.path).fileName(), this), 2, col);
        grid->addWidget(where, 3, col);
        grid->addWidget(new QLabel(size, this), 4, col);
        grid->addWidget(new QLabel(date, this), 5, col);
    }

    grid->setColumnStretch(2, 1);

    auto *warning = new QLabel(tr("The existing file is read-only."), this);
    warning->setForegroundRole(QPalette::Link);
    warning->setVisible(c.targetReadOnly);

    // Buttons: the main choice of each kind, with its variants on a menu.
    auto *buttons = new QHBoxLayout;
    if (folders) {
        auto *merge = new QPushButton(tr("&Merge"), this);
        merge->setToolTip(tr("Put the incoming folder's contents into the existing one"));
        connect(merge, &QPushButton::clicked, this, [this] { choose(WhenExists::Replace); });
        buttons->addWidget(merge);
    } else {
        QToolButton *replace = menuButton(tr("&Replace"), this);
        connect(replace, &QToolButton::clicked, this, [this] { choose(WhenExists::Replace); });
        connect(replace->menu()->addAction(tr("Replace if Newer")), &QAction::triggered, this,
                [this] { choose(WhenExists::KeepNewer); });
        connect(replace->menu()->addAction(tr("Move the Existing File to Trash, Then Copy")), &QAction::triggered,
                this, [this] { choose(WhenExists::TrashOld); });
        buttons->addWidget(replace);
    }
    QToolButton *rename = menuButton(tr("Re&name the Copy"), this);
    rename->setToolTip(tr("Copy it as %1").arg(FileOperation::freeName(target.path(), target.fileName())));
    connect(rename, &QToolButton::clicked, this, [this] { choose(WhenExists::RenameNew); });
    connect(rename->menu()->addAction(tr("Choose a Name…")), &QAction::triggered, this, &ReplaceDialog::chooseName);
    connect(rename->menu()->addAction(folders ? tr("Rename the Existing Folder") : tr("Rename the Existing File")),
            &QAction::triggered, this, [this] { choose(WhenExists::RenameOld); });
    if (folders)
        connect(rename->menu()->addAction(tr("Move the Existing Folder to Trash, Then Copy")), &QAction::triggered,
                this, [this] { choose(WhenExists::TrashOld); });
    buttons->addWidget(rename);
    if (!folders && !c.sourceIsDir && !c.targetIsDir && c.targetSize < c.sourceSize && c.targetSize > 0) {
        auto *resume = new QPushButton(tr("Res&ume"), this);
        resume->setToolTip(tr("Carry on from where an earlier copy stopped"));
        connect(resume, &QPushButton::clicked, this, [this] { choose(WhenExists::Resume); });
        buttons->addWidget(resume);
    }
    buttons->addStretch(1);
    auto *skip = new QPushButton(tr("&Skip"), this);
    skip->setDefault(true);
    connect(skip, &QPushButton::clicked, this, [this] { choose(WhenExists::Skip); });
    buttons->addWidget(skip);
    auto *cancel = new QPushButton(tr("Cancel"), this);
    cancel->setToolTip(tr("Stop, leaving this and the remaining items"));
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(cancel);

    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize); // see ChooseFolderDialog
    layout->addWidget(message);
    layout->addSpacing(6);
    layout->addLayout(grid);
    layout->addWidget(warning);
    layout->addSpacing(6);
    layout->addWidget(m_forAll);
    layout->addLayout(buttons);
}

void ReplaceDialog::choose(WhenExists action, const QString &newName) {
    m_resolution.action = action;
    m_resolution.newName = newName;
    m_resolution.cancel = false;
    m_resolution.forAll = newName.isEmpty() && m_forAll->isChecked();
    accept();
}

void ReplaceDialog::chooseName() {
    const QFileInfo target(m_conflict.target);
    QString name = FileOperation::freeName(target.path(), target.fileName());
    for (;;) {
        bool ok = false;
        name = QInputDialog::getText(this, tr("Choose a Name"), tr("Copy it as:"), QLineEdit::Normal, name, &ok).trimmed();
        if (!ok || name.isEmpty())
            return;
        if (name.contains('/') || name == "." || name == "..") {
            continue;
        }
        if (!QFileInfo::exists(target.path() + '/' + name) && !QFileInfo(target.path() + '/' + name).isSymLink())
            break;
    }
    choose(WhenExists::RenameNew, name);
}

FileOperation::Resolution ReplaceDialog::ask(const FileOperation::Conflict &conflict, QWidget *parent) {
    ReplaceDialog dialog(conflict, parent);
    dialog.exec();
    return dialog.resolution();
}
