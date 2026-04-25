#include "sentryfeedbackdialog.h"

#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qdialogbuttonbox.h>
#include <QtWidgets/qformlayout.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qplaintextedit.h>
#include <QtWidgets/qpushbutton.h>

SentryFeedbackDialog::SentryFeedbackDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Feedback");
    resize(420, 320);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("Name");
    m_emailEdit = new QLineEdit(this);
    m_emailEdit->setPlaceholderText("Email");
    m_messageEdit = new QPlainTextEdit(this);
    m_messageEdit->setPlaceholderText("What happened?");

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->addRow("Name", m_nameEdit);
    form->addRow("Email", m_emailEdit);
    form->addRow("Message", m_messageEdit);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
    m_sendButton = buttons->button(QDialogButtonBox::Ok);
    m_sendButton->setText("Send");
    m_sendButton->setEnabled(false);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    connect(m_messageEdit, &QPlainTextEdit::textChanged, this, [this]() {
        m_sendButton->setEnabled(!m_messageEdit->toPlainText().trimmed().isEmpty());
    });
}

QString SentryFeedbackDialog::message() const
{
    return m_messageEdit->toPlainText();
}

QString SentryFeedbackDialog::name() const
{
    return m_nameEdit->text();
}

QString SentryFeedbackDialog::email() const
{
    return m_emailEdit->text();
}

void SentryFeedbackDialog::setName(const QString &name)
{
    m_nameEdit->setText(name);
}

void SentryFeedbackDialog::setEmail(const QString &email)
{
    m_emailEdit->setText(email);
}
