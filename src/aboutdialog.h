#pragma once

#include <QDialog>

class QPlainTextEdit;
class QPushButton;

// What the program is, which version, and its licence (CFG-16).
class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);

    // The licence the program was built with, and the line in it that says
    // whose it is.
    static QString license();
    static QString copyright();
    // Who made it: the licence holder, and Claude.
    static QString credits();

    bool isLicenseShown() const;
    void setLicenseShown(bool shown);

private:
    QPlainTextEdit *m_license;
    QPushButton *m_showLicense;
};
