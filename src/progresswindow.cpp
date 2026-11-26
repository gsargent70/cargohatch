#include "progresswindow.h"

#include <QCheckBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "filemodel.h"
#include "fileops.h"

namespace {

// Samples are taken this often, and the graph keeps this many.
constexpr int kSampleMs = 500;
constexpr int kSamples = 120;
// The speed is averaged over this long, so the time left doesn't jump.
constexpr qint64 kSpeedWindowMs = 5000;

QString shortPath(const QString &path) {
    const QString home = QDir::homePath();
    if (path == home)
        return QStringLiteral("~");
    if (path.startsWith(home + '/'))
        return '~' + path.mid(home.size());
    return path;
}

QString bytes(qint64 n) {
    return FileModel::sizeText(n);
}

} // namespace

SpeedGraph::SpeedGraph(QWidget *parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QSize SpeedGraph::sizeHint() const {
    return {fontMetrics().averageCharWidth() * 50, fontMetrics().height() * 3};
}

void SpeedGraph::addSample(double bytesPerSecond) {
    m_samples << bytesPerSecond;
    while (m_samples.size() > kSamples)
        m_samples.removeFirst();
    update();
}

void SpeedGraph::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(QPen(palette().color(QPalette::Mid), 1));
    p.setBrush(palette().color(QPalette::Base));
    p.drawRect(r);
    if (m_samples.size() < 2)
        return;
    const double top = *std::max_element(m_samples.begin(), m_samples.end());
    if (top <= 0)
        return;
    // The newest sample is at the right edge; older ones scroll left.
    const double step = r.width() / (kSamples - 1);
    QPainterPath line;
    const double x0 = r.right() - step * (m_samples.size() - 1);
    for (int i = 0; i < m_samples.size(); ++i) {
        const QPointF point(x0 + step * i, r.bottom() - (r.height() - 2) * m_samples.at(i) / top);
        i == 0 ? line.moveTo(point) : line.lineTo(point);
    }
    QPainterPath area = line;
    area.lineTo(r.right(), r.bottom());
    area.lineTo(x0, r.bottom());
    area.closeSubpath();
    QColor accent = palette().color(QPalette::Accent);
    QColor fill = accent;
    fill.setAlphaF(0.25);
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawPath(area);
    p.setPen(QPen(accent, 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawPath(line);
}

ProgressWindow::ProgressWindow(FileOperation *operation, QWidget *parent)
    : QDialog(parent),
      m_operation(operation),
      m_bytes(operation->kind() == FileOperation::Kind::Copy || operation->kind() == FileOperation::Kind::Move),
      m_action(new QLabel(this)),
      m_route(new QLabel(this)),
      m_fileBar(new QProgressBar(this)),
      m_totalBar(new QProgressBar(this)),
      m_stats(new QLabel(this)),
      m_graph(new SpeedGraph(this)),
      m_pause(new QPushButton(tr("&Pause"), this)),
      m_skip(new QPushButton(tr("S&kip File"), this)),
      m_unattended(new QCheckBox(tr("&Unattended"), this)),
      m_sampler(new QTimer(this)) {
    const int width = fontMetrics().averageCharWidth() * 56;
    m_action->setFixedWidth(width);
    m_route->setFixedWidth(width);
    m_route->setForegroundRole(QPalette::PlaceholderText);
    if (m_bytes) {
        const QStringList sources = operation->sources();
        const QString from = sources.isEmpty() ? QString() : shortPath(QFileInfo(sources.first()).path());
        const QString to = operation->destination().isEmpty() ? from : shortPath(operation->destination());
        m_route->setText(fontMetrics().elidedText(from == to ? tr("In %1").arg(from) : tr("From %1 to %2").arg(from, to),
                                                  Qt::ElideMiddle, width));
    }
    m_route->setVisible(m_bytes);
    for (QProgressBar *bar : {m_fileBar, m_totalBar}) {
        // The theme's accent, which stands out more than the selection
        // colour Fusion fills bars with.
        QPalette p = bar->palette();
        p.setColor(QPalette::Highlight, p.color(QPalette::Accent));
        p.setColor(QPalette::HighlightedText, p.color(QPalette::Window));
        bar->setPalette(p);
        bar->setRange(0, 1000);
        bar->setValue(0);
    }
    m_fileBar->setTextVisible(false);
    m_fileBar->setMaximumHeight(fontMetrics().height() / 2);
    m_fileBar->setVisible(m_bytes);
    m_graph->setVisible(m_bytes);
    m_skip->setVisible(m_bytes);
    m_skip->setToolTip(tr("Leave the file being copied and go on with the next"));
    m_unattended->setToolTip(tr("Don't stop to ask: skip anything that would need an answer, and list it at the end"));
    m_unattended->setChecked(operation->isUnattended());
    m_pause->setCheckable(true);

    connect(m_pause, &QPushButton::toggled, this, [this](bool paused) {
        m_operation->setPaused(paused);
        m_pause->setText(paused ? tr("&Resume") : tr("&Pause"));
        updateText();
    });
    connect(m_skip, &QPushButton::clicked, this, [this] { m_operation->skipCurrent(); });
    connect(m_unattended, &QCheckBox::toggled, this, [this](bool on) { m_operation->setUnattended(on); });
    auto *abort = new QPushButton(tr("Abort"), this);
    abort->setToolTip(tr("Stop now; what's done so far stays done"));
    connect(abort, &QPushButton::clicked, this, &QDialog::reject);
    // Closing the window, or Escape, aborts as well.
    connect(this, &QDialog::rejected, this, [this] {
        m_operation->setPaused(false);
        m_operation->cancel();
    });

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_unattended);
    buttons->addStretch(1);
    buttons->addWidget(m_pause);
    buttons->addWidget(m_skip);
    buttons->addWidget(abort);

    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize); // see ChooseFolderDialog
    layout->addWidget(m_action);
    layout->addWidget(m_route);
    layout->addWidget(m_fileBar);
    layout->addWidget(m_totalBar);
    layout->addWidget(m_stats);
    layout->addWidget(m_graph);
    layout->addLayout(buttons);

    m_clock.start();
    m_sampler->setInterval(kSampleMs);
    connect(m_sampler, &QTimer::timeout, this, &ProgressWindow::sample);
    m_sampler->start();
    updateText();
}

QString ProgressWindow::verb() const {
    switch (m_operation->kind()) {
    case FileOperation::Kind::Copy: return tr("Copying");
    case FileOperation::Kind::Move: return tr("Moving");
    case FileOperation::Kind::Trash: return tr("Moving to Trash");
    case FileOperation::Kind::Delete: return tr("Deleting");
    case FileOperation::Kind::Link: return tr("Creating links");
    }
    return {};
}

QString ProgressWindow::timeLeft(qint64 seconds) {
    if (seconds < 5)
        return tr("a few seconds left");
    if (seconds < 60)
        return tr("about %1 s left").arg((seconds + 4) / 5 * 5);
    if (seconds < 3600)
        return tr("about %1 min left").arg((seconds + 30) / 60);
    return tr("about %1 h %2 min left").arg(seconds / 3600).arg(seconds % 3600 / 60);
}

void ProgressWindow::setProgress(qint64 done, qint64 total, qint64 fileDone, qint64 fileSize, const QString &name) {
    m_done = done;
    m_total = total;
    m_fileDone = fileDone;
    m_fileSize = fileSize;
    if (!name.isEmpty())
        m_name = name;
    updateText();
}

void ProgressWindow::setFile(int number, int count, const QString &path) {
    m_number = number;
    m_count = count;
    m_name = QFileInfo(path).fileName();
    updateText();
}

void ProgressWindow::sample() {
    const qint64 now = m_clock.elapsed();
    m_history << qMakePair(now, m_done);
    while (m_history.size() > 2 && now - m_history.first().first > kSpeedWindowMs)
        m_history.removeFirst();
    const auto &first = m_history.first();
    m_speed = now > first.first ? double(m_done - first.second) * 1000 / double(now - first.first) : 0;
    if (m_operation->isPaused())
        m_speed = 0;
    if (m_bytes)
        m_graph->addSample(m_speed);
    updateText();
}

void ProgressWindow::updateText() {
    const int percent = m_total > 0 ? int(m_done * 100 / m_total) : 0;
    setWindowTitle(tr("%1% · %2").arg(percent).arg(verb()));
    m_totalBar->setValue(m_total > 0 ? int(m_done * 1000 / m_total) : 0);
    m_fileBar->setValue(m_fileSize > 0 ? int(m_fileDone * 1000 / m_fileSize) : 0);
    m_action->setText(fontMetrics().elidedText(tr("%1 %2").arg(verb(), m_name), Qt::ElideMiddle, m_action->width()));

    QStringList parts;
    if (m_bytes)
        parts << tr("%1 of %2").arg(bytes(m_done), bytes(m_total));
    if (m_count > 0)
        parts << tr("item %1 of %2").arg(qMin(m_number, m_count)).arg(m_count);
    if (m_operation->isPaused()) {
        parts << tr("paused");
    } else if (m_bytes && m_speed > 0) {
        parts << tr("%1/s").arg(bytes(qint64(m_speed)));
        parts << timeLeft(qint64(double(m_total - m_done) / m_speed));
    }
    m_stats->setText(parts.join(QStringLiteral("  ·  ")));
}
