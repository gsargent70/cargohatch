#include "selectpattern.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

QList<QRegularExpression> SelectPattern::expressions() const {
    QList<QRegularExpression> list;
    const auto options = matchCase ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption;
    if (regex) {
        list << QRegularExpression(QRegularExpression::anchoredPattern(text), options);
        return list;
    }
    for (const QString &part : text.split(';', Qt::SkipEmptyParts)) {
        const QString trimmed = part.trimmed();
        if (trimmed.isEmpty())
            continue;
        QRegularExpression re(QRegularExpression::wildcardToRegularExpression(
            trimmed, QRegularExpression::UnanchoredWildcardConversion));
        re.setPattern(QRegularExpression::anchoredPattern(re.pattern()));
        re.setPatternOptions(options);
        list << re;
    }
    return list;
}

QString SelectPattern::error() const {
    if (text.trimmed().isEmpty())
        return QCoreApplication::translate("SelectPattern", "Type a pattern, like *.jpg");
    for (const QRegularExpression &re : expressions())
        if (!re.isValid())
            return QCoreApplication::translate("SelectPattern", "Not a valid regular expression: %1")
                .arg(re.errorString());
    return {};
}

bool SelectPattern::matches(const QString &name) const {
    for (const QRegularExpression &re : expressions())
        if (re.match(name).hasMatch())
            return true;
    return false;
}

SelectPatternDialog::SelectPatternDialog(const QStringList &recent, QWidget *parent)
    : QDialog(parent),
      m_text(new QComboBox(this)),
      m_regex(new QCheckBox(tr("Regular expression"), this)),
      m_matchCase(new QCheckBox(tr("Match case"), this)),
      m_scope(new QComboBox(this)),
      m_select(new QRadioButton(tr("Add matching items to the selection"), this)),
      m_deselect(new QRadioButton(tr("Remove matching items from the selection"), this)),
      m_selectOnly(new QRadioButton(tr("Select only the matching items"), this)),
      m_error(new QLabel(this)) {
    setWindowTitle(tr("Select Matching"));
    m_text->setEditable(true);
    m_text->setInsertPolicy(QComboBox::NoInsert);
    m_text->addItems(recent);
    m_text->setMinimumWidth(fontMetrics().averageCharWidth() * 40);
    m_text->lineEdit()->setPlaceholderText(tr("*.jpg;*.png"));
    m_text->setCurrentText(recent.value(0));
    m_text->lineEdit()->selectAll();
    m_scope->addItems({tr("Files and folders"), tr("Files only"), tr("Folders only")});
    m_select->setChecked(true);
    m_error->setWordWrap(true);
    m_error->setForegroundRole(QPalette::PlaceholderText);

    auto *form = new QFormLayout;
    form->addRow(tr("Names like:"), m_text);
    form->addRow(QString(), m_regex);
    form->addRow(QString(), m_matchCase);
    form->addRow(tr("Look at:"), m_scope);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize); // see ChooseFolderDialog
    layout->addLayout(form);
    layout->addWidget(m_select);
    layout->addWidget(m_deselect);
    layout->addWidget(m_selectOnly);
    layout->addWidget(m_error);
    layout->addWidget(buttons);

    connect(m_text, &QComboBox::currentTextChanged, this, &SelectPatternDialog::validate);
    connect(m_regex, &QCheckBox::toggled, this, &SelectPatternDialog::validate);
    validate();
}

SelectPattern SelectPatternDialog::pattern() const {
    SelectPattern p;
    p.text = m_text->currentText();
    p.regex = m_regex->isChecked();
    p.matchCase = m_matchCase->isChecked();
    p.scope = SelectPattern::Scope(m_scope->currentIndex());
    p.action = m_deselect->isChecked()     ? SelectPattern::Action::Deselect
               : m_selectOnly->isChecked() ? SelectPattern::Action::SelectOnly
                                           : SelectPattern::Action::Select;
    return p;
}

void SelectPatternDialog::validate() {
    const QString error = pattern().error();
    m_error->setText(error.isEmpty() ? tr("Separate several patterns with ;") : error);
    m_ok->setEnabled(error.isEmpty());
}
