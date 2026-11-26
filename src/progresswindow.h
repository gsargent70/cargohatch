#pragma once

#include <QDialog>
#include <QElapsedTimer>
#include <QList>

class FileOperation;
class QCheckBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;

// A small chart of the copy speed over the last minute or so.
class SpeedGraph : public QWidget {
    Q_OBJECT

public:
    explicit SpeedGraph(QWidget *parent = nullptr);
    void addSample(double bytesPerSecond);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QList<double> m_samples;
};

// The progress of one file operation (COPY-08): what is being done and
// where, a bar for the current file and one for the whole job, the speed
// with a graph, the time left, and Pause, Skip, Unattended and Abort.
// The percentage is in the window title, so it shows in the window list.
class ProgressWindow : public QDialog {
    Q_OBJECT

public:
    ProgressWindow(FileOperation *operation, QWidget *parent = nullptr);

    void setProgress(qint64 done, qint64 total, qint64 fileDone, qint64 fileSize, const QString &name);
    void setFile(int number, int count, const QString &path);
    // "Copying" etc., for the title and the first line.
    QString verb() const;

    // Formats a duration like "about 2 min" (for tests too).
    static QString timeLeft(qint64 seconds);

private:
    void sample();
    void updateText();

    FileOperation *m_operation;
    bool m_bytes;
    QLabel *m_action;
    QLabel *m_route;
    QProgressBar *m_fileBar;
    QProgressBar *m_totalBar;
    QLabel *m_stats;
    SpeedGraph *m_graph;
    QPushButton *m_pause;
    QPushButton *m_skip;
    QCheckBox *m_unattended;
    QTimer *m_sampler;
    qint64 m_done = 0;
    qint64 m_total = 0;
    qint64 m_fileDone = 0;
    qint64 m_fileSize = 0;
    int m_number = 0;
    int m_count = 0;
    QString m_name;
    // (time in ms, bytes done) over the last few seconds, for the speed.
    QList<QPair<qint64, qint64>> m_history;
    QElapsedTimer m_clock;
    double m_speed = 0;
};
