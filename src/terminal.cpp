#include "terminal.h"

#include <QCoreApplication>
#include <QProcess>
#include <QTimer>

namespace {

QString program = QStringLiteral("xdg-terminal-exec");

QString tr(const char *text) {
    return QCoreApplication::translate("Terminal", text);
}

void start(const QString &command, const QStringList &arguments, const QString &folder,
           const std::function<void(const QString &)> &failed) {
    // Detached, so the terminal stays when CargoHatch is closed.
    if (!QProcess::startDetached(command, arguments, folder))
        failed(tr("Couldn't start the terminal %1.").arg(command));
}

} // namespace

namespace Terminal {

void setProgram(const QString &p) {
    program = p;
}

void open(const QString &folder, QObject *context, std::function<void(const QString &)> failed) {
    auto *process = new QProcess(context);
    // Each argument is followed by a NUL, so names with spaces stay whole.
    process->start(program, {"--dir=" + folder, QStringLiteral("--print-cmd=\\0")});
    auto *timer = new QTimer(process);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, process, [process, failed] {
        process->disconnect();
        process->kill();
        process->deleteLater();
        failed(tr("Finding the terminal took too long."));
    });
    timer->start(5000);
    QObject::connect(process, &QProcess::errorOccurred, process, [process, folder, failed](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        process->disconnect();
        process->deleteLater();
        // Without xdg-terminal-exec, the terminal the user has named, which
        // starts in the folder it is started from.
        const QString named = qEnvironmentVariable("TERMINAL");
        if (named.isEmpty() || named == program)
            failed(tr("No terminal was found. Install xdg-terminal-exec, or set TERMINAL to your terminal."));
        else
            start(named, {}, folder, failed);
    });
    QObject::connect(process, &QProcess::finished, process, [process, folder, failed](int code, QProcess::ExitStatus status) {
        process->disconnect();
        process->deleteLater();
        if (status != QProcess::NormalExit || code != 0) {
            const QString why = QString::fromLocal8Bit(process->readAllStandardError()).trimmed().section('\n', 0, 0);
            failed(why.isEmpty() ? tr("No terminal is set up to open.") : tr("Couldn't open a terminal: %1").arg(why));
            return;
        }
        QStringList parts = QString::fromLocal8Bit(process->readAllStandardOutput()).split(QChar(0));
        if (!parts.isEmpty() && parts.last().isEmpty())
            parts.removeLast();
        if (parts.isEmpty() || parts.first().isEmpty()) {
            failed(tr("No terminal is set up to open."));
            return;
        }
        const QString command = parts.takeFirst();
        start(command, parts, folder, failed);
    });
}

} // namespace Terminal
