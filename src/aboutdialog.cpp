#include "aboutdialog.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

const char *const kWebsite = "https://github.com/gsargent70/cargohatch";

// Drawn rather than set on a QLabel, so it is scaled once, to the pixels of
// whichever screen the window is on.
class Logo : public QWidget {
public:
    explicit Logo(QWidget *parent) : QWidget(parent), m_icon(QStringLiteral(":/logo.png")) {
        const QSize image = m_icon.availableSizes().value(0, QSize(1, 1));
        setFixedSize(image.scaled(480, image.height(), Qt::KeepAspectRatio));
        setAccessibleName(QStringLiteral("CargoHatch"));
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        m_icon.paint(&painter, rect());
    }

private:
    QIcon m_icon;
};

QLabel *line(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setTextFormat(Qt::PlainText);
    label->setAlignment(Qt::AlignCenter);
    return label;
}

// The licence's lines are broken for 80 columns, so its paragraphs are put
// back together to wrap at the window's width.
QString reflowed(const QString &text) {
    QStringList paragraphs = text.split(QStringLiteral("\n\n"));
    for (QString &paragraph : paragraphs)
        paragraph = paragraph.simplified();
    return paragraphs.join(QStringLiteral("\n\n"));
}

} // namespace

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent), m_license(new QPlainTextEdit(reflowed(license()), this)), m_showLicense(new QPushButton(this)) {
    setWindowTitle(tr("About CargoHatch"));

    // Selectable, to paste into a bug report.
    QLabel *version = line(tr("Version %1").arg(QCoreApplication::applicationVersion()), this);
    version->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont bold = version->font();
    bold.setBold(true);
    version->setFont(bold);

    auto *website = new QLabel(QStringLiteral("<a href=\"%1\">%2</a>")
                                   .arg(QLatin1String(kWebsite), QString(QLatin1String(kWebsite)).mid(8)), this);
    website->setAlignment(Qt::AlignCenter);
    website->setOpenExternalLinks(true);

    QLabel *qt = line(tr("Uses Qt %1, under the GNU LGPL version 3.").arg(QLatin1String(qVersion())), this);
    qt->setForegroundRole(QPalette::PlaceholderText);

    m_license->setReadOnly(true);
    m_license->setTabChangesFocus(true);
    m_license->setMinimumHeight(fontMetrics().lineSpacing() * 12);
    m_license->hide();

    m_showLicense->setCheckable(true);
    m_showLicense->setAutoDefault(false);
    m_showLicense->setText(tr("Show &Licence"));
    connect(m_showLicense, &QPushButton::toggled, this, [this](bool on) {
        m_showLicense->setText(on ? tr("Hide &Licence") : tr("Show &Licence"));
        m_license->setVisible(on);
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->addButton(m_showLicense, QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QPushButton *close = buttons->button(QDialogButtonBox::Close);
    close->setDefault(true);
    close->setFocus();

    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize); // see ChooseFolderDialog
    layout->addWidget(new Logo(this), 0, Qt::AlignCenter);
    layout->addWidget(version);
    layout->addWidget(line(tr("A power-user file manager for Linux."), this));
    layout->addSpacing(8);
    layout->addWidget(line(credits(), this));
    layout->addWidget(line(copyright(), this));
    layout->addWidget(line(tr("Free software under the MIT License."), this));
    layout->addWidget(website);
    layout->addWidget(qt);
    layout->addSpacing(8);
    layout->addWidget(m_license);
    layout->addWidget(buttons);
}

QString AboutDialog::license() {
    QFile file(QStringLiteral(":/LICENSE"));
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()).trimmed() : QString();
}

QString AboutDialog::copyright() {
    const QStringList lines = license().split('\n');
    for (const QString &text : lines) {
        if (text.startsWith(QLatin1String("Copyright (c) ")))
            return QString(text).replace(QLatin1String("(c)"), QStringLiteral("©"));
    }
    return {};
}

QString AboutDialog::credits() {
    // The holder named in the licence, then the AI that wrote it with them.
    QString holder = copyright().section(' ', 3);
    if (holder.isEmpty())
        return tr("Made with Claude, by Anthropic.");
    return tr("Made by %1 and Claude, by Anthropic.").arg(holder);
}

bool AboutDialog::isLicenseShown() const {
    return m_showLicense->isChecked();
}

void AboutDialog::setLicenseShown(bool shown) {
    m_showLicense->setChecked(shown);
}
