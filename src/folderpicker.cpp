#include "folderpicker.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusObjectPath>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QUrl>
#include <QVBoxLayout>

#include "folderaliases.h"

namespace {

// Receives the portal's answer, which arrives as a signal on a request
// object rather than as the method's reply.
class PortalResponse : public QObject {
    Q_OBJECT

public:
    QEventLoop loop;
    uint code = 2;
    QVariantMap results;

public slots:
    void response(uint response, const QVariantMap &values) {
        code = response;
        results = values;
        loop.quit();
    }
};

} // namespace

QString PortalFolderPicker::pickFolder(QWidget *, const QString &title, const QString &start, QString *error) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        if (error)
            *error = QObject::tr("The desktop's file chooser isn't available (no D-Bus session).");
        return {};
    }
    const QString token = QStringLiteral("cargohatch%1").arg(QRandomGenerator::global()->generate());
    const QString sender = bus.baseService().mid(1).replace('.', '_');
    const QString handle = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);

    // Listen before asking, so a quick answer isn't missed.
    PortalResponse response;
    bus.connect("org.freedesktop.portal.Desktop", handle, "org.freedesktop.portal.Request", "Response", &response,
                SLOT(response(uint, QVariantMap)));

    QVariantMap options;
    options["handle_token"] = token;
    options["directory"] = true;
    options["modal"] = true;
    if (!start.isEmpty())
        options["current_folder"] = QByteArray(QFile::encodeName(start) + '\0');
    QDBusMessage call = QDBusMessage::createMethodCall("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
                                                       "org.freedesktop.portal.FileChooser", "OpenFile");
    call << QString() << title << options;
    const QDBusReply<QDBusObjectPath> reply = bus.call(call);
    if (!reply.isValid()) {
        if (error)
            *error = QObject::tr("The desktop's file chooser isn't available: %1").arg(reply.error().message());
        return {};
    }
    if (reply.value().path() != handle) {
        // Older portals pick their own request path.
        bus.disconnect("org.freedesktop.portal.Desktop", handle, "org.freedesktop.portal.Request", "Response",
                       &response, SLOT(response(uint, QVariantMap)));
        bus.connect("org.freedesktop.portal.Desktop", reply.value().path(), "org.freedesktop.portal.Request",
                    "Response", &response, SLOT(response(uint, QVariantMap)));
    }
    response.loop.exec();
    if (response.code != 0)
        return {};
    const QStringList uris = response.results.value("uris").toStringList();
    return uris.isEmpty() ? QString() : QUrl(uris.first()).toLocalFile();
}

ChooseFolderDialog::ChooseFolderDialog(const QString &title, const QString &start, const QString &base,
                                       FolderPicker *picker, QWidget *parent)
    : QDialog(parent), m_path(new QLineEdit(this)), m_completer(new PathCompleter(m_path)), m_note(new QLabel(this)), m_picker(picker), m_base(base) {
    setWindowTitle(title);
    m_path->setText(start);
    m_path->setMinimumWidth(fontMetrics().averageCharWidth() * 50);
    m_completer->setBase(base);
    m_note->setWordWrap(true);
    m_note->setForegroundRole(QPalette::PlaceholderText);

    auto *browse = new QPushButton(tr("Browse…"), this);
    browse->setVisible(picker);
    connect(browse, &QPushButton::clicked, this, [this] {
        QString error;
        const QString chosen = m_picker->pickFolder(this, windowTitle(), folder(), &error);
        if (!chosen.isEmpty())
            m_path->setText(chosen);
        else if (!error.isEmpty())
            m_note->setText(error);
    });
    auto *row = new QHBoxLayout;
    row->addWidget(m_path, 1);
    row->addWidget(browse);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    // A fixed size, so a window manager rule that sizes new windows (as
    // Omarchy's floating workspaces do) can't stretch the dialog.
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->addWidget(new QLabel(tr("Folder:"), this));
    layout->addLayout(row);
    layout->addWidget(m_note);
    layout->addWidget(buttons);
    connect(m_path, &QLineEdit::textChanged, this, &ChooseFolderDialog::validate);
    validate();
}

QString ChooseFolderDialog::folder() const {
    const QString path = FolderAliases::expand(m_path->text(), m_base);
    return path.isEmpty() ? QString() : QDir::cleanPath(path);
}

void ChooseFolderDialog::validate() {
    const QString path = folder();
    const QFileInfo info(path);
    QString note;
    bool ok = !path.isEmpty();
    if (!ok)
        note = tr("Type a folder or choose one with Browse.");
    else if (info.exists() && !info.isDir()) {
        note = tr("%1 is a file, not a folder.").arg(path);
        ok = false;
    } else if (!info.exists())
        note = tr("This folder doesn't exist yet and will be created.");
    m_note->setText(note);
    m_ok->setEnabled(ok);
}

#include "folderpicker.moc"
