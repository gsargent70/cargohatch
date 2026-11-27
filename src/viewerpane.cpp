#include "viewerpane.h"

#include "filemodel.h"
#include "icons.h"

#include <QAudioOutput>
#include <QBuffer>
#include <QEvent>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QMediaPlayer>
#include <QMouseEvent>
#include <QMovie>
#include <QPainter>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSlider>
#include <QStackedWidget>
#include <QTextDocument>
#include <QThreadPool>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVideoWidget>
#include <QtConcurrent>

namespace {

// Loads may wait a long time on a network mount that has gone away. The pool
// is never destroyed, so quitting never waits for them.
QThreadPool *loadPool() {
    static QThreadPool *pool = [] {
        auto *p = new QThreadPool;
        p->setMaxThreadCount(4);
        return p;
    }();
    return pool;
}

QString duration(qint64 ms) {
    const qint64 s = ms / 1000;
    return s >= 3600 ? QString("%1:%2:%3")
                           .arg(s / 3600)
                           .arg((s / 60) % 60, 2, 10, QChar('0'))
                           .arg(s % 60, 2, 10, QChar('0'))
                     : QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}

} // namespace

// Draws an image fitted to the pane (never enlarged), or at its own size
// inside a scroll area. Clicking switches between the two.
class ImageCanvas : public QWidget {
    Q_OBJECT

public:
    explicit ImageCanvas(QWidget *parent) : QWidget(parent) {}

    void setImage(const QImage &image) {
        m_image = image;
        if (!m_fit)
            resize(m_image.size());
        update();
    }
    void setFit(bool fit) {
        m_fit = fit;
        if (!fit)
            resize(m_image.size());
        update();
    }
    QRect imageRect() const {
        if (!m_fit || m_image.isNull())
            return QRect(QPoint(0, 0), m_image.size());
        QSize size = m_image.size();
        if (size.width() > width() || size.height() > height())
            size.scale(this->size(), Qt::KeepAspectRatio);
        return QRect(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2), size);
    }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *) override {
        if (m_image.isNull())
            return;
        QPainter painter(this);
        const QRect r = imageRect();
        // A checkerboard behind see-through images, so their edges show.
        if (m_image.hasAlphaChannel()) {
            const QColor light = palette().color(QPalette::Base);
            const QColor dark = palette().color(QPalette::AlternateBase).darker(110);
            painter.save();
            painter.setClipRect(r);
            const int step = 8;
            for (int y = r.top(); y < r.bottom(); y += step)
                for (int x = r.left(); x < r.right(); x += step)
                    painter.fillRect(x, y, step, step, (((x - r.left()) / step + (y - r.top()) / step) % 2) ? dark : light);
            painter.restore();
        }
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage(r, m_image);
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton)
            emit clicked();
    }

private:
    QImage m_image;
    bool m_fit = true;
};

ViewerPane::ViewerPane(QWidget *parent)
    : QWidget(parent),
      m_pool(loadPool()),
      m_delay(new QTimer(this)),
      m_title(new QLabel(this)),
      m_previous(new QToolButton(this)),
      m_next(new QToolButton(this)),
      m_fit(new QToolButton(this)),
      m_hexButton(new QToolButton(this)),
      m_play(new QToolButton(this)),
      m_lock(new QToolButton(this)),
      m_pages(new QStackedWidget(this)),
      m_message(new QLabel(m_pages)),
      m_imageScroll(new QScrollArea(m_pages)),
      m_canvas(new ImageCanvas(m_imageScroll)),
      m_text(new QPlainTextEdit(m_pages)),
      m_details(new QLabel(this)) {
    setObjectName("viewerPane");
    // Moving through a list with the arrow keys loads only where it stops.
    m_delay->setSingleShot(true);
    m_delay->setInterval(40);
    connect(m_delay, &QTimer::timeout, this, &ViewerPane::startLoad);

    m_title->setTextFormat(Qt::PlainText);
    m_title->setMinimumWidth(40);
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    for (QToolButton *button : {m_previous, m_next, m_fit, m_hexButton, m_play, m_lock}) {
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
    }
    m_fit->setCheckable(true);
    m_hexButton->setCheckable(true);
    m_lock->setCheckable(true);
    connect(m_previous, &QToolButton::clicked, this, &ViewerPane::previousRequested);
    connect(m_next, &QToolButton::clicked, this, &ViewerPane::nextRequested);
    connect(m_fit, &QToolButton::toggled, this, &ViewerPane::setActualSize);
    connect(m_hexButton, &QToolButton::toggled, this, &ViewerPane::setHex);
    connect(m_lock, &QToolButton::toggled, this, &ViewerPane::setLocked);
    connect(m_play, &QToolButton::clicked, this, [this] {
        if (!m_player)
            return;
        if (m_player->playbackState() == QMediaPlayer::PlayingState)
            m_player->pause();
        else
            m_player->play();
    });

    auto *bar = new QHBoxLayout;
    bar->setContentsMargins(6, 2, 2, 2);
    bar->setSpacing(2);
    bar->addWidget(m_title, 1);
    for (QToolButton *button : {m_play, m_fit, m_hexButton, m_previous, m_next, m_lock})
        bar->addWidget(button);

    m_message->setAlignment(Qt::AlignCenter);
    m_message->setWordWrap(true);
    m_message->setTextFormat(Qt::PlainText);
    m_message->setMargin(12);
    m_message->setForegroundRole(QPalette::PlaceholderText);
    m_imageScroll->setWidget(m_canvas);
    m_imageScroll->setWidgetResizable(true);
    m_imageScroll->setFrameShape(QFrame::NoFrame);
    m_imageScroll->setAlignment(Qt::AlignCenter);
    connect(m_canvas, &ImageCanvas::clicked, this, [this] { m_fit->toggle(); });
    m_text->setReadOnly(true);
    m_text->setFrameShape(QFrame::NoFrame);
    m_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_text->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_pages->addWidget(m_message);
    m_pages->addWidget(m_imageScroll);
    m_pages->addWidget(m_text);

    m_details->setTextFormat(Qt::PlainText);
    m_details->setWordWrap(true);
    m_details->setContentsMargins(6, 2, 6, 4);
    m_details->setForegroundRole(QPalette::PlaceholderText);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(bar);
    layout->addWidget(m_pages, 1);
    layout->addWidget(m_details);
    setMinimumSize(160, 120);

    retranslate();
    present(Preview());
}

ViewerPane::~ViewerPane() {
    // A load still running finds no one to tell.
    ++m_generation;
}

void ViewerPane::showPath(const QString &path, bool force) {
    if (m_locked && !force)
        return;
    if (path == m_path && !force && (m_loading || m_preview.path == path))
        return;
    m_path = path;
    m_title->setText(QFileInfo(path).fileName().isEmpty() ? path : QFileInfo(path).fileName());
    m_title->setToolTip(path);
    m_delay->start();
}

void ViewerPane::startLoad() {
    const quint64 generation = ++m_generation;
    stop();
    if (m_path.isEmpty()) {
        m_loading = false;
        present(Preview());
        return;
    }
    m_loading = true;
    // Say so if it takes a while, but don't flash the message for quick ones.
    QTimer::singleShot(300, this, [this, generation] {
        if (generation == m_generation && m_loading)
            showMessage(tr("Loading…"));
    });
    auto *watcher = new QFutureWatcher<Preview>(this);
    connect(watcher, &QFutureWatcher<Preview>::finished, this, [this, watcher, generation] {
        watcher->deleteLater();
        if (generation != m_generation)
            return;
        m_loading = false;
        present(watcher->result());
    });
    const QString path = m_path;
    watcher->setFuture(QtConcurrent::run(m_pool, [path] { return Preview::load(path); }));
}

void ViewerPane::present(const Preview &preview) {
    m_preview = preview;
    clearMovie();
    using Kind = Preview::Kind;
    switch (preview.kind) {
    case Kind::None:
        m_title->clear();
        showMessage(tr("Select a file to see it here."));
        break;
    case Kind::Error:
        showMessage(preview.error, "dialog-warning");
        break;
    case Kind::Folder: {
        QString text = preview.name + '\n';
        if (preview.moreItems)
            text += tr("More than %L1 items").arg(preview.items);
        else if (preview.items == 0)
            text += tr("Empty folder");
        else
            text += tr("%n items", "", preview.items);
        if (preview.hiddenItems > 0)
            text += '\n' + tr("%n hidden", "", preview.hiddenItems);
        showMessage(text, preview.iconName);
        break;
    }
    case Kind::Image:
    case Kind::Pdf:
        showImage(preview.image);
        break;
    case Kind::Animation:
        m_movieData = new QBuffer(this);
        m_movieData->setData(preview.data);
        m_movieData->open(QIODevice::ReadOnly);
        m_movie = new QMovie(m_movieData, QByteArray(), this);
        connect(m_movie, &QMovie::frameChanged, this, [this] { m_canvas->setImage(m_movie->currentImage()); });
        showImage(QImage());
        m_movie->start();
        break;
    case Kind::Text:
    case Kind::Hex:
        showText();
        break;
    case Kind::Media:
        showMedia();
        break;
    case Kind::Info:
        showMessage(preview.note.isEmpty() ? preview.name : preview.name + "\n\n" + preview.note, preview.iconName);
        break;
    }
    updateButtons();
    updateDetails();
    emit loaded();
}

void ViewerPane::showMessage(const QString &text, const QString &iconName) {
    m_message->setText(text);
    if (!iconName.isEmpty()) {
        const QIcon icon = QIcon::fromTheme(iconName, QIcon::fromTheme("text-x-generic"));
        if (!icon.isNull()) {
            // The icon sits above the words, both in the middle.
            m_message->setPixmap(QPixmap());
            m_message->setTextFormat(Qt::RichText);
            QByteArray png;
            QBuffer buffer(&png);
            icon.pixmap(64, 64).save(&buffer, "PNG");
            m_message->setText(QString("<img src=\"data:image/png;base64,%1\"><br>%2")
                                   .arg(QString::fromLatin1(png.toBase64()), text.toHtmlEscaped().replace('\n', "<br>")));
            m_message->setProperty("plain", text);
            m_pages->setCurrentWidget(m_message);
            return;
        }
    }
    m_message->setTextFormat(Qt::PlainText);
    m_message->setText(text);
    m_message->setProperty("plain", text);
    m_pages->setCurrentWidget(m_message);
}

void ViewerPane::showImage(const QImage &image) {
    m_canvas->setImage(image);
    m_canvas->setFit(!m_actualSize);
    m_imageScroll->setWidgetResizable(!m_actualSize);
    m_pages->setCurrentWidget(m_imageScroll);
}

void ViewerPane::showText() {
    const bool hex = m_preview.kind == Preview::Kind::Hex || m_hex;
    m_shownPerRow = hex ? hexPerRow() : 0;
    QString text = hex ? Preview::hexDump(m_preview.data, m_preview.size, m_shownPerRow) : m_preview.text;
    m_text->setLineWrapMode(m_wrap && !hex ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    m_text->setPlainText(text);
    m_pages->setCurrentWidget(m_text);
}

void ViewerPane::showMedia() {
    if (!m_mediaPage) {
        // Made only when first needed: most people preview few media files,
        // and the player starts the multimedia back end.
        m_mediaPage = new QWidget(m_pages);
        m_video = new QVideoWidget(m_mediaPage);
        m_audioLabel = new QLabel(m_mediaPage);
        m_audioLabel->setAlignment(Qt::AlignCenter);
        m_audioLabel->setWordWrap(true);
        m_position = new QSlider(Qt::Horizontal, m_mediaPage);
        m_position->setFocusPolicy(Qt::NoFocus);
        m_time = new QLabel(m_mediaPage);
        auto *controls = new QHBoxLayout;
        controls->setContentsMargins(6, 2, 6, 2);
        controls->addWidget(m_position, 1);
        controls->addWidget(m_time);
        auto *layout = new QVBoxLayout(m_mediaPage);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_video, 1);
        layout->addWidget(m_audioLabel, 1);
        layout->addLayout(controls);
        m_pages->addWidget(m_mediaPage);

        m_player = new QMediaPlayer(this);
        m_audio = new QAudioOutput(this);
        m_player->setAudioOutput(m_audio);
        m_player->setVideoOutput(m_video);
        auto showTime = [this] {
            m_time->setText(duration(m_player->position()) + " / " + duration(m_player->duration()));
        };
        connect(m_player, &QMediaPlayer::durationChanged, this, [this, showTime](qint64 ms) {
            m_position->setRange(0, int(ms));
            showTime();
            updateDetails();
        });
        connect(m_player, &QMediaPlayer::positionChanged, this, [this, showTime](qint64 ms) {
            if (!m_position->isSliderDown())
                m_position->setValue(int(ms));
            showTime();
        });
        connect(m_position, &QSlider::sliderMoved, m_player, [this](int ms) { m_player->setPosition(ms); });
        connect(m_player, &QMediaPlayer::playbackStateChanged, this, &ViewerPane::updateButtons);
        connect(m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &message) {
            if (m_preview.kind == Preview::Kind::Media)
                showMessage(tr("This file can't be played: %1").arg(message), "dialog-warning");
        });
    }
    m_video->setVisible(m_preview.video);
    m_audioLabel->setVisible(!m_preview.video);
    m_audioLabel->setText(m_preview.name);
    m_position->setValue(0);
    m_time->clear();
    m_pages->setCurrentWidget(m_mediaPage);
    m_player->setSource(QUrl::fromLocalFile(m_preview.path));
}

void ViewerPane::stop() {
    if (m_player) {
        m_player->stop();
        m_player->setSource(QUrl());
    }
    clearMovie();
}

void ViewerPane::clearMovie() {
    if (m_movie) {
        m_movie->stop();
        delete m_movie;
        m_movie = nullptr;
    }
    delete m_movieData;
    m_movieData = nullptr;
}

void ViewerPane::setLocked(bool locked) {
    if (locked == m_locked)
        return;
    m_locked = locked;
    const QSignalBlocker block(m_lock);
    m_lock->setChecked(locked);
    m_lock->setToolTip(locked ? tr("Locked: keeps this file while the selection moves") : tr("Lock Viewer Pane"));
    emit lockChanged(locked);
}

void ViewerPane::setWrapText(bool wrap) {
    if (wrap == m_wrap)
        return;
    m_wrap = wrap;
    if (m_pages->currentWidget() == m_text)
        showText();
}

void ViewerPane::setHex(bool hex) {
    m_hex = hex;
    const QSignalBlocker block(m_hexButton);
    m_hexButton->setChecked(hex);
    if (m_preview.kind == Preview::Kind::Text)
        showText();
    updateButtons();
}

void ViewerPane::setActualSize(bool actual) {
    m_actualSize = actual;
    const QSignalBlocker block(m_fit);
    m_fit->setChecked(actual);
    m_canvas->setFit(!actual);
    m_imageScroll->setWidgetResizable(!actual);
    updateButtons();
}

void ViewerPane::updateButtons() {
    using Kind = Preview::Kind;
    const Kind kind = m_preview.kind;
    m_fit->setVisible(kind == Kind::Image || kind == Kind::Pdf || kind == Kind::Animation);
    m_fit->setText(m_actualSize ? tr("Fit") : tr("Actual Size"));
    m_hexButton->setVisible(kind == Kind::Text);
    m_hexButton->setText(m_hex ? tr("Text") : tr("Hex"));
    m_play->setVisible(kind == Kind::Media && m_pages->currentWidget() == m_mediaPage);
    const bool playing = m_player && m_player->playbackState() == QMediaPlayer::PlayingState;
    m_play->setText(playing ? tr("Pause") : tr("Play"));
}

void ViewerPane::updateDetails() {
    const Preview &p = m_preview;
    QStringList parts;
    if (!p.mimeComment.isEmpty())
        parts << p.mimeComment;
    if (p.imageSize.isValid())
        parts << tr("%1 × %2").arg(p.imageSize.width()).arg(p.imageSize.height());
    if (p.kind == Preview::Kind::Media && m_player && m_player->duration() > 0)
        parts << duration(m_player->duration());
    const QLocale locale;
    if (p.size >= 0)
        parts << FileModel::sizeText(p.size);
    if (p.modified.isValid())
        parts << locale.toString(p.modified, QLocale::ShortFormat);
    if ((p.kind == Preview::Kind::Text || p.kind == Preview::Kind::Hex) && p.truncated)
        parts << tr("first %1 shown").arg(FileModel::sizeText(m_hex || p.kind == Preview::Kind::Hex ? p.data.size()
                                                                                                         : p.text.toUtf8().size()));
    if (p.kind == Preview::Kind::None || p.kind == Preview::Kind::Error)
        parts.clear();
    m_details->setText(parts.join(QStringLiteral(" · ")));
    m_details->setVisible(!parts.isEmpty());
}

QString ViewerPane::messageText() const {
    return m_pages->currentWidget() == m_message ? m_message->property("plain").toString() : QString();
}

QString ViewerPane::contentText() const {
    return m_pages->currentWidget() == m_text ? m_text->toPlainText() : QString();
}

QString ViewerPane::detailsText() const {
    return m_details->isVisibleTo(const_cast<ViewerPane *>(this)) ? m_details->text() : QString();
}

void ViewerPane::retranslate() {
    m_previous->setToolTip(tr("Previous Item"));
    m_next->setToolTip(tr("Next Item"));
    m_lock->setToolTip(m_locked ? tr("Locked: keeps this file while the selection moves") : tr("Lock Viewer Pane"));
    m_fit->setToolTip(tr("Show the image at its own size, or fitted to the pane"));
    m_hexButton->setToolTip(tr("Show the bytes of the file"));
    m_previous->setIcon(Icons::arrow(Qt::LeftArrow, palette()));
    m_next->setIcon(Icons::arrow(Qt::RightArrow, palette()));
    m_lock->setIcon(Icons::padlock(palette()));
    updateButtons();
    updateDetails();
    if (m_preview.kind == Preview::Kind::None)
        showMessage(tr("Select a file to see it here."));
}

void ViewerPane::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange || event->type() == QEvent::PaletteChange)
        retranslate();
}

int ViewerPane::hexPerRow() const {
    const int room = m_text->viewport()->width() - 2 * int(m_text->document()->documentMargin());
    const int wide = m_text->fontMetrics().horizontalAdvance(QString(Preview::hexRowWidth(m_preview.size, 16), 'M'));
    return wide <= room ? 16 : 8;
}

void ViewerPane::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (m_shownPerRow && m_pages->currentWidget() == m_text && hexPerRow() != m_shownPerRow)
        showText();
}

void ViewerPane::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    if (m_player)
        m_player->pause();
}

#include "viewerpane.moc"
