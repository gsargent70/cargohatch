#pragma once

#include <QObject>
#include <QPointer>
#include <QRect>
#include <QSet>
#include <QStringList>
#include <QTimer>

#include <functional>

class QDropEvent;
class QIcon;
class QMimeData;
class QWidget;

// Files dragged and dropped (IDEA-18).
namespace FileDrag {

enum class Action { None, Copy, Move, Link, Trash, Ask };

// What dropping does when no key is held: "auto" moves on the same drive and
// copies between drives.
QStringList behaviours();
QString behaviourTitle(const QString &id);

// Everything that decides what a drop does.
struct Question {
    // What whoever is dragging allows.
    Qt::DropActions possible = Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
    Qt::KeyboardModifiers modifiers;
    QString behaviour = QStringLiteral("auto");
    // Dragged from another program.
    bool foreign = false;
    bool sameDevice = true;
    // Every item is in the folder already.
    bool alreadyThere = false;
    bool toTrash = false;
};
Action choose(const Question &question);

// Files that have been dropped on a folder.
struct Drop {
    QStringList paths;
    QString folder;
    Action action = Action::None;
    // What Ask can offer.
    bool canCopy = true;
    bool canMove = true;
    bool canLink = true;
    // Where, on the screen.
    QPoint position;
    // The widget they were dragged from, if it is one of ours, and the one
    // they were dropped on.
    QPointer<QObject> source;
    QPointer<QWidget> area;
};

// The window, which does what a drop asks for.
class Receiver {
public:
    virtual ~Receiver() = default;
    virtual QString dropBehaviour() const = 0;
    // What would happen on dropping here; empty for nothing.
    virtual void describeDrop(const QString &text) = 0;
    virtual void receiveDrop(const Drop &drop) = 0;
};

// What a drag offers, from the keys held as it starts. Other programs on
// Wayland never hear about keys held here, and what is offered can't change
// once the drag has begun, so a copy has to be the only thing offered.
Qt::DropActions offered(Qt::KeyboardModifiers held);
// Lets the user drag `paths` away from `source`, until they are dropped.
void start(QWidget *source, const QStringList &paths, const QIcon &icon);
// The picture that follows the pointer.
QPixmap pixmap(const QIcon &icon, const QString &text, const QWidget *widget);
// Runs `what` once no drag is going on: whoever is dragging waits until the
// drop has been dealt with.
void afterDrag(QObject *context, std::function<void()> what);

} // namespace FileDrag

// A list view that hands the start of a drag to whoever made it.
template <class View>
class FileDragView : public View {
public:
    FileDragView(std::function<void()> start, QWidget *parent) : View(parent), m_start(std::move(start)) {}

protected:
    void startDrag(Qt::DropActions) override { m_start(); }

private:
    std::function<void()> m_start;
};

// Takes files dropped on parts of `area`, marking the folder they would go
// in. When `area` is the inside of a scroll area, it scrolls while files are
// held near its edges.
class FileDropTarget : public QObject {
    Q_OBJECT

public:
    struct Place {
        // Empty where files can't be dropped.
        QString folder;
        // What to mark, within the area.
        QRect rect;
    };
    using Finder = std::function<Place(const QPoint &)>;

    FileDropTarget(QWidget *area, Finder placeAt);
    // Called when files have been held over one place for a moment.
    void setHover(std::function<void(const QPoint &)> hover) { m_hover = std::move(hover); }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void enter(QDropEvent *event);
    void leave();
    // What dropping where `event` is would do; shows it as well.
    FileDrag::Action consider(QDropEvent *event);
    FileDrag::Question question(const QString &folder) const;
    QString description(FileDrag::Action action, const QString &folder) const;
    void scrollNear(const QPoint &pos);
    void mark(const QRect &rect);

    QWidget *m_area;
    Finder m_placeAt;
    std::function<void(const QPoint &)> m_hover;
    QPointer<QWidget> m_marker;
    QTimer m_hoverTimer;
    QTimer m_scrollTimer;
    QPoint m_pos;
    QStringList m_paths;
    QSet<QString> m_parents;
    QSet<quint64> m_devices;
    Place m_place;
    // What was found out about the place's folder.
    bool m_allowed = false;
    FileDrag::Question m_question;
    // The folder by its one real path.
    QString m_folder;
};
