#include "globalfilter.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "selectpattern.h"

namespace {

SelectPattern patternFor(const QString &text, bool regex) {
    SelectPattern p;
    p.text = text;
    p.regex = regex;
    return p;
}

// Nothing for an empty or broken pattern, so a mistake in the settings file
// hides nothing rather than everything.
QList<QRegularExpression> compile(const QString &text, bool regex) {
    const SelectPattern p = patternFor(text, regex);
    if (!p.error().isEmpty())
        return {};
    return p.expressions();
}

} // namespace

bool GlobalFilter::operator==(const GlobalFilter &o) const {
    return showHidden == o.showHidden && hideListed == o.hideListed && hideNames == o.hideNames && files == o.files &&
           filesRegex == o.filesRegex && folders == o.folders && foldersRegex == o.foldersRegex;
}

QString GlobalFilter::patternError(const QString &pattern, bool regex) {
    return pattern.trimmed().isEmpty() ? QString() : patternFor(pattern, regex).error();
}

GlobalFilter::Matcher::Matcher(const GlobalFilter &f) : m_showHidden(f.showHidden), m_hideListed(f.hideListed) {
    if (f.hideNames) {
        m_files = compile(f.files, f.filesRegex);
        m_folders = compile(f.folders, f.foldersRegex);
    }
}

bool GlobalFilter::Matcher::hides(const QString &name, bool isDir, bool listed) const {
    if (!m_showHidden && name.startsWith('.'))
        return true;
    // Showing hidden files shows these too; they are hidden files that
    // happen not to start with a dot.
    if (m_hideListed && !m_showHidden && listed)
        return true;
    for (const QRegularExpression &re : isDir ? m_folders : m_files)
        if (re.match(name).hasMatch())
            return true;
    return false;
}

GlobalFilterDialog::GlobalFilterDialog(const GlobalFilter &f, QWidget *parent)
    : QDialog(parent),
      m_hideHidden(new QCheckBox(tr("Hide hidden files (names starting with a dot)"), this)),
      m_hideListed(new QCheckBox(tr("Hide items listed in a folder's .hidden file"), this)),
      m_hideNames(new QCheckBox(tr("Hide these names everywhere:"), this)),
      m_files(new QLineEdit(f.files, this)),
      m_filesRegex(new QCheckBox(tr("Regular expression"), this)),
      m_folders(new QLineEdit(f.folders, this)),
      m_foldersRegex(new QCheckBox(tr("Regular expression"), this)),
      m_note(new QLabel(this)) {
    setWindowTitle(tr("Global Filters"));
    m_hideHidden->setChecked(!f.showHidden);
    m_hideListed->setChecked(f.hideListed);
    m_hideNames->setChecked(f.hideNames);
    m_filesRegex->setChecked(f.filesRegex);
    m_foldersRegex->setChecked(f.foldersRegex);
    m_files->setPlaceholderText(tr("*.bak;*~"));
    m_folders->setPlaceholderText(tr("__pycache__;node_modules"));
    m_files->setMinimumWidth(fontMetrics().averageCharWidth() * 36);
    m_note->setWordWrap(true);
    m_note->setForegroundRole(QPalette::PlaceholderText);
    m_note->setMinimumHeight(fontMetrics().height() * 2);

    auto *info = new QLabel(tr("These apply to every folder in every window."), this);
    auto *grid = new QGridLayout;
    grid->setContentsMargins(24, 0, 0, 0);
    auto *filesLabel = new QLabel(tr("F&iles:"), this);
    auto *foldersLabel = new QLabel(tr("F&olders:"), this);
    filesLabel->setBuddy(m_files);
    foldersLabel->setBuddy(m_folders);
    grid->addWidget(filesLabel, 0, 0);
    grid->addWidget(m_files, 0, 1);
    grid->addWidget(m_filesRegex, 0, 2);
    grid->addWidget(foldersLabel, 1, 0);
    grid->addWidget(m_folders, 1, 1);
    grid->addWidget(m_foldersRegex, 1, 2);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize); // see ChooseFolderDialog
    layout->addWidget(info);
    layout->addSpacing(6);
    layout->addWidget(m_hideHidden);
    layout->addWidget(m_hideListed);
    layout->addWidget(m_hideNames);
    layout->addLayout(grid);
    layout->addWidget(m_note);
    layout->addWidget(buttons);

    for (QLineEdit *edit : {m_files, m_folders})
        connect(edit, &QLineEdit::textChanged, this, &GlobalFilterDialog::validate);
    for (QCheckBox *box : {m_hideHidden, m_hideNames, m_filesRegex, m_foldersRegex})
        connect(box, &QCheckBox::toggled, this, &GlobalFilterDialog::validate);
    validate();
}

GlobalFilter GlobalFilterDialog::filter() const {
    GlobalFilter f;
    f.showHidden = !m_hideHidden->isChecked();
    f.hideListed = m_hideListed->isChecked();
    f.hideNames = m_hideNames->isChecked();
    f.files = m_files->text().trimmed();
    f.filesRegex = m_filesRegex->isChecked();
    f.folders = m_folders->text().trimmed();
    f.foldersRegex = m_foldersRegex->isChecked();
    return f;
}

void GlobalFilterDialog::validate() {
    m_hideListed->setEnabled(m_hideHidden->isChecked());
    for (QWidget *w : {static_cast<QWidget *>(m_files), static_cast<QWidget *>(m_filesRegex),
                       static_cast<QWidget *>(m_folders), static_cast<QWidget *>(m_foldersRegex)})
        w->setEnabled(m_hideNames->isChecked());
    QString error;
    if (m_hideNames->isChecked()) {
        error = GlobalFilter::patternError(m_files->text(), m_filesRegex->isChecked());
        if (error.isEmpty())
            error = GlobalFilter::patternError(m_folders->text(), m_foldersRegex->isChecked());
    }
    m_note->setText(!error.isEmpty() ? error : tr("Separate several patterns with ;"));
    m_ok->setEnabled(error.isEmpty());
}
