#pragma once

#include "preview.h"

#include <QPointer>
#include <QWidget>

class QAudioOutput;
class QBuffer;
class QLabel;
class QMediaPlayer;
class QMovie;
class QPlainTextEdit;
class QScrollArea;
class QSlider;
class QStackedWidget;
class QThreadPool;
class QTimer;
class QToolButton;
class QVideoWidget;
class ImageCanvas;

// The window's viewer pane (LST-09, VWR-01): a preview of the file or folder
// under the cursor in the active side's list. Images, text, a hex dump of
// anything else, the first page of a PDF, audio and video, and the details
// of folders. Files are read on worker threads, so a slow mount never holds
// up the window; only the newest request is shown.
class ViewerPane : public QWidget {
    Q_OBJECT

public:
    explicit ViewerPane(QWidget *parent = nullptr);
    ~ViewerPane() override;

    // Shows `path`, or nothing for an empty path. While locked the pane
    // keeps what it has, unless `force`.
    void showPath(const QString &path, bool force = false);
    QString path() const { return m_path; }
    // What's on show now; None while loading.
    Preview::Kind kind() const { return m_preview.kind; }
    const Preview &preview() const { return m_preview; }
    bool isLoading() const { return m_loading; }

    // A locked pane keeps its file while the selection moves.
    void setLocked(bool locked);
    bool isLocked() const { return m_locked; }

    void setWrapText(bool wrap);
    // Text files as a hex dump instead.
    void setHex(bool hex);
    bool isHex() const { return m_hex; }
    // Images at their own size, scrolled, rather than fitted to the pane.
    void setActualSize(bool actual);
    bool isActualSize() const { return m_actualSize; }

    // Stops any playing audio or video.
    void stop();

    // What's shown, for tests: the message, the text, the details line.
    QString messageText() const;
    QString contentText() const;
    QString detailsText() const;

signals:
    void previousRequested();
    void nextRequested();
    void lockChanged(bool locked);
    // A preview has been shown (or a load failed).
    void loaded();

protected:
    void changeEvent(QEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void startLoad();
    void present(const Preview &preview);
    void showMessage(const QString &text, const QString &iconName = {});
    void showImage(const QImage &image);
    void showText();
    // 16 bytes to a row of the hex dump when they fit, otherwise 8.
    int hexPerRow() const;
    void showMedia();
    void updateButtons();
    void updateDetails();
    void retranslate();
    void clearMovie();

    QString m_path;
    Preview m_preview;
    bool m_loading = false;
    bool m_locked = false;
    bool m_hex = false;
    bool m_wrap = false;
    bool m_actualSize = false;
    int m_shownPerRow = 0;
    quint64 m_generation = 0;

    QThreadPool *m_pool;
    QTimer *m_delay;
    QLabel *m_title;
    QToolButton *m_previous;
    QToolButton *m_next;
    QToolButton *m_fit;
    QToolButton *m_hexButton;
    QToolButton *m_play;
    QToolButton *m_lock;
    QStackedWidget *m_pages;
    QLabel *m_message;
    QScrollArea *m_imageScroll;
    ImageCanvas *m_canvas;
    QPlainTextEdit *m_text;
    QWidget *m_mediaPage = nullptr;
    QVideoWidget *m_video = nullptr;
    QLabel *m_audioLabel = nullptr;
    QSlider *m_position = nullptr;
    QLabel *m_time = nullptr;
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audio = nullptr;
    QMovie *m_movie = nullptr;
    QBuffer *m_movieData = nullptr;
    QLabel *m_details;
};
