#include "sentrywindow.h"
#include "sentryplayground.h"
#include "sentrytrace.h"

#include <sentry.h>

#include <QtCore/qfileinfo.h>
#include <QtCore/qlocale.h>
#include <QtCore/qsettings.h>
#include <QtCore/qstandardpaths.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qfiledialog.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qlistwidget.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qstatusbar.h>
#include <QtWidgets/qtoolbutton.h>

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

    ui.messageLevelBox->addItem("Debug", SENTRY_LEVEL_DEBUG);
    ui.messageLevelBox->addItem("Info", SENTRY_LEVEL_INFO);
    ui.messageLevelBox->addItem("Warning", SENTRY_LEVEL_WARNING);
    ui.messageLevelBox->addItem("Error", SENTRY_LEVEL_ERROR);
    ui.messageLevelBox->addItem("Fatal", SENTRY_LEVEL_FATAL);
    ui.messageLevelBox->setCurrentIndex(1);
    ui.messageText->setText(QSettings().value("message", "Hello from Sentry Playground").toString());
    QObject::connect(ui.messageText, &QLineEdit::textEdited, this, [](const QString& text) {
        QSettings().setValue("message", text);
    });
    QObject::connect(ui.captureMessageButton, &QAbstractButton::clicked, playground, [this, playground]() {
        playground->captureMessage(ui.messageLevelBox->currentData().toInt(), ui.messageText->text());
    });

    auto refreshAttachments = [this, playground]() {
        ui.attachmentList->clear();
        for (const QString& path : playground->attachments()) {
            QFileInfo info(path);
            QString label = QString("%1  (%2)").arg(info.fileName(),
                QLocale::system().formattedDataSize(info.size()));
            auto *item = new QListWidgetItem(label);
            item->setToolTip(path);
            item->setData(Qt::UserRole, path);
            ui.attachmentList->addItem(item);
        }
    };
    refreshAttachments();
    QObject::connect(playground, &SentryPlayground::attachmentsChanged, this,
        [refreshAttachments]() { refreshAttachments(); });
    QObject::connect(ui.addButton, &QAbstractButton::clicked, this, [this, playground]() {
        QString lastDir = QSettings().value("attachmentDir",
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
        QString path = QFileDialog::getOpenFileName(this, "Select attachment", lastDir);
        if (path.isEmpty())
            return;
        QSettings().setValue("attachmentDir", QFileInfo(path).absolutePath());
        playground->addAttachment(path);
    });
    QObject::connect(ui.attachmentList, &QListWidget::customContextMenuRequested, this,
        [this, playground](const QPoint& pos) {
            auto *item = ui.attachmentList->itemAt(pos);
            if (!item)
                return;
            QMenu menu(this);
            QString path = item->data(Qt::UserRole).toString();
            menu.addAction("Remove", [playground, path]() { playground->removeAttachment(path); });
            menu.exec(ui.attachmentList->mapToGlobal(pos));
        });

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
