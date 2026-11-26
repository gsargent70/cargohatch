#include "filedrag.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QDrag>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QMimeData>
#include <QPainter>
#include <QScrollBar>
#include <QStandardPaths>
#include <QUrl>

#include <sys/stat.h>
#include <unistd.h>

#include "fileclipboard.h"

namespace {

constexpr int kHoverMs = 700;
constexpr int kScrollMs = 60;
constexpr int kScrollEdge = 18;

bool dragging = false;
QList<std::pair<QPointer<QObject>, std::function<void()>>> waiting;

Qt::DropAction toQt(FileDrag::Action action, Qt::DropActions possible) {
    switch (action) {
    case FileDrag::Action::Copy:
        return Qt::CopyAction;
    case FileDrag::Action::Move:
        return Qt::MoveAction;
    case FileDrag::Action::Link:
        return Qt::LinkAction;
    case FileDrag::Action::Trash:
        return possible & Qt::MoveAction ? Qt::MoveAction : Qt::CopyAction;
    case FileDrag::Action::Ask:
        return possible & Qt::CopyAction ? Qt::CopyAction : possible & Qt::MoveAction ? Qt::MoveAction : Qt::LinkAction;
    case FileDrag::Action::None:
        break;
    }
    return Qt::IgnoreAction;
}

bool isUnder(const QString &path, const QString &folder) {
    return path == folder || path.startsWith(folder.endsWith('/') ? folder : folder + '/');
}

// Outlines where files would go, without getting in the way of the pointer.
class DropMarker : public QWidget {
public:
    explicit DropMarker(QWidget *parent) : QWidget(parent) { setAttribute(Qt::WA_TransparentForMouseEvents); }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        QColor color = palette().color(QPalette::Highlight);
        painter.setPen(QPen(color, 2));
        color.setAlpha(40);
        painter.setBrush(color);
        painter.drawRect(rect().adjusted(1, 1, -1, -1));
    }
};

} // namespace

namespace FileDrag {

QStringList behaviours() {
    return {"auto", "copy", "move", "ask"};
}

QString behaviourTitle(const QString &id) {
    if (id == "copy")
        return FileDropTarget::tr("Copy");
    if (id == "move")
        return FileDropTarget::tr("Move");
    if (id == "ask")
        return FileDropTarget::tr("Ask");
    return FileDropTarget::tr("Move on the Same Drive, Copy Between Drives");
}

Action choose(const Question &q) {
    if (q.toTrash)
        return !q.alreadyThere && q.possible & (Qt::CopyAction | Qt::MoveAction) ? Action::Trash : Action::None;
    const bool copy = q.possible & Qt::CopyAction;
    const bool move = q.possible & Qt::MoveAction && !q.alreadyThere;
    const bool link = q.possible & Qt::LinkAction;
    const auto held = q.modifiers & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier);
    if (held == (Qt::ControlModifier | Qt::ShiftModifier))
        return link ? Action::Link : Action::None;
    if (held == Qt::ControlModifier)
        return copy ? Action::Copy : Action::None;
    if (held == Qt::ShiftModifier)
        return move ? Action::Move : Action::None;
    if (held == Qt::AltModifier)
        return copy || move || link ? Action::Ask : Action::None;
    // Letting go of something where it was does nothing, unless a key says
    // otherwise.
    if (q.alreadyThere)
        return Action::None;
    if (q.behaviour == "ask")
        return copy || move || link ? Action::Ask : Action::None;
    // Moving something away from another program without being told to could
    // take a file it is still using.
    const bool rather = q.behaviour == "move" || (q.behaviour != "copy" && q.sameDevice && !q.foreign);
    if (rather && move)
        return Action::Move;
    // A move isn't put in place of a copy that isn't allowed.
    return copy ? Action::Copy : Action::None;
}

QPixmap pixmap(const QIcon &icon, const QString &text, const QWidget *widget) {
    const QFontMetrics metrics(widget->font());
    const QString label = metrics.elidedText(text, Qt::ElideMiddle, 220);
    const QSize size(metrics.horizontalAdvance(label) + 44, qMax(32, metrics.height() + 12));
    const qreal ratio = widget->devicePixelRatioF();
    QPixmap picture(size * ratio);
    picture.setDevicePixelRatio(ratio);
    picture.fill(Qt::transparent);
    QPainter painter(&picture);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(widget->palette().color(QPalette::Highlight));
    painter.setBrush(widget->palette().color(QPalette::Window));
    painter.drawRoundedRect(QRectF(0.5, 0.5, size.width() - 1, size.height() - 1), 4, 4);
    icon.paint(&painter, QRect(6, (size.height() - 22) / 2, 22, 22));
    painter.setFont(widget->font());
    painter.setPen(widget->palette().color(QPalette::WindowText));
    painter.drawText(QRect(34, 0, size.width() - 38, size.height()), Qt::AlignVCenter | Qt::AlignLeft, label);
    return picture;
}

Qt::DropActions offered(Qt::KeyboardModifiers held) {
    held &= Qt::ControlModifier | Qt::ShiftModifier;
    if (held == (Qt::ControlModifier | Qt::ShiftModifier))
        return Qt::LinkAction;
    if (held == Qt::ControlModifier)
        return Qt::CopyAction;
    if (held == Qt::ShiftModifier)
        return Qt::MoveAction;
    return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
}

void start(QWidget *source, const QStringList &paths, const QIcon &icon) {
    if (paths.isEmpty() || dragging)
        return;
    // Only addresses: a terminal given text could run it.
    auto *mime = new QMimeData;
    QList<QUrl> urls;
    for (const QString &path : paths)
        urls << QUrl::fromLocalFile(path);
    mime->setUrls(urls);
    auto *drag = new QDrag(source);
    drag->setMimeData(mime);
    const QString text = paths.size() == 1 ? QFileInfo(paths.first()).fileName()
                                           : FileDropTarget::tr("%n items", "", int(paths.size()));
    drag->setPixmap(pixmap(icon, text, source));
    drag->setHotSpot(QPoint(-12, -8));
    dragging = true;
    // Whoever takes the files moves them; what came of it doesn't matter
    // here.
    const Qt::DropActions actions = offered(QGuiApplication::queryKeyboardModifiers());
    drag->exec(actions, actions & Qt::CopyAction ? Qt::CopyAction : Qt::IgnoreAction);
    dragging = false;
    const auto todo = std::exchange(waiting, {});
    for (const auto &[context, what] : todo)
        if (context)
            QTimer::singleShot(0, context, what);
}

void afterDrag(QObject *context, std::function<void()> what) {
    if (dragging)
        waiting.append({context, std::move(what)});
    else
        QTimer::singleShot(0, context, std::move(what));
}

} // namespace FileDrag

FileDropTarget::FileDropTarget(QWidget *area, Finder placeAt)
    : QObject(area), m_area(area), m_placeAt(std::move(placeAt)) {
    area->setAcceptDrops(true);
    area->installEventFilter(this);
    m_hoverTimer.setSingleShot(true);
    m_hoverTimer.setInterval(kHoverMs);
    connect(&m_hoverTimer, &QTimer::timeout, this, [this] { m_hover(m_pos); });
    m_scrollTimer.setInterval(kScrollMs);
    connect(&m_scrollTimer, &QTimer::timeout, this, [this] { scrollNear(m_pos); });
}

bool FileDropTarget::eventFilter(QObject *watched, QEvent *event) {
    if (watched != m_area)
        return QObject::eventFilter(watched, event);
    switch (event->type()) {
    case QEvent::DragEnter: {
        auto *e = static_cast<QDropEvent *>(event);
        enter(e);
        if (m_paths.isEmpty()) {
            e->ignore();
            return true;
        }
        consider(e);
        // Even where nothing can be dropped, to hear about the rest.
        e->accept();
        return true;
    }
    case QEvent::DragMove: {
        auto *e = static_cast<QDropEvent *>(event);
        const FileDrag::Action action = consider(e);
        if (action == FileDrag::Action::None)
            e->ignore();
        else
            e->accept();
        return true;
    }
    case QEvent::DragLeave:
        leave();
        return true;
    case QEvent::Drop: {
        auto *e = static_cast<QDropEvent *>(event);
        // Not every drop is preceded by the events that would have filled
        // these in.
        if (m_paths.isEmpty())
            enter(e);
        FileDrag::Drop drop;
        drop.action = consider(e);
        auto *receiver = dynamic_cast<FileDrag::Receiver *>(m_area->window());
        if (drop.action == FileDrag::Action::None || !receiver) {
            e->ignore();
            leave();
            return true;
        }
        drop.paths = m_paths;
        drop.folder = m_folder;
        drop.canCopy = m_question.possible & Qt::CopyAction;
        drop.canMove = m_question.possible & Qt::MoveAction && !m_question.alreadyThere;
        drop.canLink = m_question.possible & Qt::LinkAction;
        drop.position = m_area->mapToGlobal(m_pos);
        drop.source = e->source();
        drop.area = m_area;
        // The files are moved here, a moment from now. Another program told
        // it was a move might delete them first.
        if (m_question.foreign && m_question.possible & Qt::CopyAction)
            e->setDropAction(Qt::CopyAction);
        e->accept();
        leave();
        FileDrag::afterDrag(m_area->window(), [receiver, drop] { receiver->receiveDrop(drop); });
        return true;
    }
    default:
        return QObject::eventFilter(watched, event);
    }
}

void FileDropTarget::enter(QDropEvent *event) {
    m_paths = FileClipboard::read(event->mimeData()).paths;
    m_parents.clear();
    m_devices.clear();
    m_place = {};
    m_folder.clear();
    for (const QString &path : std::as_const(m_paths)) {
        m_parents.insert(QFileInfo(path).path());
        struct stat s;
        if (::lstat(QFile::encodeName(path).constData(), &s) == 0)
            m_devices.insert(s.st_dev);
    }
}

void FileDropTarget::leave() {
    m_paths.clear();
    m_place = {};
    m_hoverTimer.stop();
    m_scrollTimer.stop();
    if (m_marker)
        m_marker->hide();
    if (auto *receiver = dynamic_cast<FileDrag::Receiver *>(m_area->window()))
        receiver->describeDrop({});
}

FileDrag::Question FileDropTarget::question(const QString &folder) const {
    FileDrag::Question q;
    struct stat s;
    q.sameDevice = ::stat(QFile::encodeName(folder).constData(), &s) == 0 && m_devices == QSet<quint64>{s.st_dev};
    q.alreadyThere = m_parents == QSet<QString>{folder};
    q.toTrash = folder == QFileInfo(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
                                    "/Trash/files")
                              .canonicalFilePath();
    return q;
}

FileDrag::Action FileDropTarget::consider(QDropEvent *event) {
    m_pos = event->position().toPoint();
    const Place place = m_paths.isEmpty() ? Place{} : m_placeAt(m_pos);
    if (place.folder != m_place.folder) {
        m_folder = QFileInfo(place.folder).canonicalFilePath();
        m_allowed = !place.folder.isEmpty() && !m_folder.isEmpty() && QFileInfo(m_folder).isDir() &&
                    ::access(QFile::encodeName(m_folder).constData(), W_OK | X_OK) == 0;
        // Not on itself, or on anything in it.
        for (const QString &path : std::as_const(m_paths))
            if (isUnder(m_folder, path) || place.folder == path)
                m_allowed = false;
        m_question = question(m_folder);
        // What has been deleted can't be put in folders that have been.
        const QString trash =
            QFileInfo(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Trash").canonicalFilePath();
        if (!m_question.toTrash && !trash.isEmpty() && isUnder(m_folder, trash))
            m_allowed = false;
    }
    if (place.folder != m_place.folder || place.rect != m_place.rect) {
        m_hoverTimer.stop();
        if (m_hover && !place.folder.isEmpty())
            m_hoverTimer.start();
    }
    m_place = place;

    auto *receiver = dynamic_cast<FileDrag::Receiver *>(m_area->window());
    m_question.possible = event->possibleActions();
    m_question.modifiers = event->modifiers();
    m_question.foreign = !event->source();
    if (receiver)
        m_question.behaviour = receiver->dropBehaviour();
    const FileDrag::Action action = m_allowed ? FileDrag::choose(m_question) : FileDrag::Action::None;
    event->setDropAction(toQt(action, m_question.possible));
    mark(action == FileDrag::Action::None ? QRect() : place.rect);
    if (receiver)
        receiver->describeDrop(description(action, m_folder));
    if (!m_paths.isEmpty() && !m_scrollTimer.isActive() && qobject_cast<QAbstractScrollArea *>(m_area->parentWidget()))
        m_scrollTimer.start();
    return action;
}

QString FileDropTarget::description(FileDrag::Action action, const QString &folder) const {
    const int n = int(m_paths.size());
    const QString where = folder == "/" ? folder : QFileInfo(folder).fileName();
    const QString name = n == 1 ? QFileInfo(m_paths.first()).fileName() : tr("%n items", "", n);
    switch (action) {
    case FileDrag::Action::Copy:
        return tr("Copy %1 to %2").arg(name, where);
    case FileDrag::Action::Move:
        return tr("Move %1 to %2").arg(name, where);
    case FileDrag::Action::Link:
        return tr("Make a link to %1 in %2").arg(name, where);
    case FileDrag::Action::Trash:
        return tr("Move %1 to the trash").arg(name);
    case FileDrag::Action::Ask:
        return tr("Ask what to do with %1 in %2").arg(name, where);
    case FileDrag::Action::None:
        break;
    }
    return {};
}

void FileDropTarget::mark(const QRect &rect) {
    if (rect.isEmpty()) {
        if (m_marker)
            m_marker->hide();
        return;
    }
    if (!m_marker)
        m_marker = new DropMarker(m_area);
    m_marker->setGeometry(rect);
    m_marker->raise();
    m_marker->show();
}

void FileDropTarget::scrollNear(const QPoint &pos) {
    auto *scroll = qobject_cast<QAbstractScrollArea *>(m_area->parentWidget());
    if (!scroll || m_paths.isEmpty())
        return;
    auto nudge = [](QScrollBar *bar, int at, int length) {
        const int step = at < kScrollEdge ? -1 : at >= length - kScrollEdge ? 1 : 0;
        const int before = bar->value();
        bar->setValue(before + step * bar->singleStep());
        return bar->value() != before;
    };
    const bool down = nudge(scroll->verticalScrollBar(), pos.y(), m_area->height());
    const bool across = nudge(scroll->horizontalScrollBar(), pos.x(), m_area->width());
    // Marked again with the next movement, where the items are by then.
    if ((down || across) && m_marker)
        m_marker->hide();
}
