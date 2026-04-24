#ifndef SENTRYFEEDBACKDIALOG_H
#define SENTRYFEEDBACKDIALOG_H

#include <QtWidgets/qdialog.h>

class QLineEdit;
class QPlainTextEdit;
class QPushButton;

class SentryFeedbackDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SentryFeedbackDialog(QWidget *parent = nullptr);

    QString message() const;
    QString name() const;
    QString email() const;

    void setName(const QString &name);
    void setEmail(const QString &email);

private:
    QPlainTextEdit *m_messageEdit;
    QLineEdit *m_nameEdit;
    QLineEdit *m_emailEdit;
    QPushButton *m_sendButton;
};

#endif // SENTRYFEEDBACKDIALOG_H
