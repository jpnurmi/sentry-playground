#include "sentrywindow.h"
#include "sentryplayground.h"
#include "sentrytrace.h"

#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qstatusbar.h>

SentryWindow::SentryWindow(QWidget *parent)
    : QMainWindow(parent)
{
    TRACE_FUNCTION();
    ui.setupUi(this);
    ui.backendLabel->setText(SentryPlayground::backend());
#ifndef Q_OS_WINDOWS
    ui.fastfailButton->setEnabled(false);
#endif

    SentryPlayground* playground = SentryPlayground::instance();
    QObject::connect(ui.crashButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerCrash);
    QObject::connect(ui.stackOverflowButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerStackOverflow);
    QObject::connect(ui.fastfailButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerFastfail);
    QObject::connect(ui.assertButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerAssertFailure);
    QObject::connect(ui.abortButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerAbort);

    QObject::connect(ui.actionQuit, &QAction::triggered, qApp, &QCoreApplication::quit);
    QObject::connect(ui.actionWindow, &QAction::triggered, playground, &SentryPlayground::showWindow);

    QObject::connect(ui.workerBox, &QAbstractButton::toggled, playground, &SentryPlayground::setWorker);
    QObject::connect(playground, &SentryPlayground::workerChanged, ui.workerBox, &QAbstractButton::setChecked);

    auto* consentButton = new QPushButton(this);
    consentButton->setCheckable(true);
    consentButton->setFlat(true);
    auto* consentIcon = new QLabel(consentButton);
    auto* consentText = new QLabel(consentButton);
    auto* consentLayout = new QHBoxLayout(consentButton);
    consentLayout->setContentsMargins(8, 0, 8, 0);
    consentLayout->addWidget(consentIcon);
    consentLayout->addWidget(consentText);
    consentLayout->addStretch();
    consentIcon->setAttribute(Qt::WA_TransparentForMouseEvents);
    consentText->setAttribute(Qt::WA_TransparentForMouseEvents);
    statusBar()->addPermanentWidget(consentButton, 1);
    statusBar()->setSizeGripEnabled(false);
    QObject::connect(consentButton, &QAbstractButton::clicked, playground, [playground]() {
        switch (playground->consent()) {
        case Qt::PartiallyChecked: playground->setConsent(Qt::Checked); break;
        case Qt::Checked: playground->setConsent(Qt::Unchecked); break;
        case Qt::Unchecked: playground->setConsent(Qt::PartiallyChecked); break;
        }
    });

    auto updateConsentStatus = [this, consentButton, consentIcon, consentText](Qt::CheckState state) {
        static const char* kStyle =
            "QStatusBar { background-color: %1; }"
            "QStatusBar QPushButton { border: none; background: transparent; }"
            "QStatusBar QLabel { color: white; font-weight: bold; background: transparent; }"
            "QStatusBar QLabel#consentIcon { font-size: 18px; }";
        switch (state) {
        case Qt::Checked:
            statusBar()->setStyleSheet(QString(kStyle).arg("#2ecc71"));
            consentIcon->setText("✓");
            consentText->setText("Consent given — events will be captured and sent to Sentry");
            consentButton->setChecked(true);
            break;
        case Qt::Unchecked:
            statusBar()->setStyleSheet(QString(kStyle).arg("#e74c3c"));
            consentIcon->setText("⚠");
            consentText->setText("Consent revoked — events will be discarded and not sent to Sentry");
            consentButton->setChecked(false);
            break;
        case Qt::PartiallyChecked:
            statusBar()->setStyleSheet(QString(kStyle).arg("#f39c12"));
            consentIcon->setText("⚠");
            consentText->setText("Consent unknown — events will be discarded until consent is given");
            consentButton->setChecked(false);
            break;
        }
    };
    consentIcon->setObjectName("consentIcon");
    updateConsentStatus(playground->consent());
    QObject::connect(playground, &SentryPlayground::consentChanged, this, updateConsentStatus);
}
