#include "mainwindow.h"
#include "feedbackdialog.h"
#include "playground.h"
#include "tracing.h"
#include "style.h"

#include <QtCore/qcoreevent.h>
#include <QtGui/qaction.h>
#include <QtGui/qevent.h>
#include <QtGui/qpalette.h>
#include <QtGui/qpixmap.h>
#include <QtWidgets/qabstractbutton.h>
#include <QtWidgets/qabstractscrollarea.h>
#include <QtWidgets/qabstractspinbox.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qdialog.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qstatusbar.h>
#include <QtWidgets/qtoolbutton.h>

static constexpr int kPageLeftMargin = 22;
static constexpr int kPageTopMargin = 22;
static constexpr int kPageRightMargin = 22;
static constexpr int kPageBottomMargin = 16;
static constexpr int kPageColumnSpacing = 16;

template <typename T>
static T* findParent(QObject* object)
{
    for (QObject* current = object; current; current = current->parent()) {
        if (auto* match = qobject_cast<T*>(current))
            return match;
    }
    return nullptr;
}

static void forwardWheelEventToScrollArea(QObject* control, QWheelEvent* wheelEvent)
{
    if (auto* scrollArea = findParent<QAbstractScrollArea>(control)) {
        QWidget* viewport = scrollArea->viewport();
        QWheelEvent forwardedEvent(
            viewport->mapFromGlobal(wheelEvent->globalPosition().toPoint()),
            wheelEvent->globalPosition(),
            wheelEvent->pixelDelta(),
            wheelEvent->angleDelta(),
            wheelEvent->buttons(),
            wheelEvent->modifiers(),
            wheelEvent->phase(),
            wheelEvent->inverted(),
            wheelEvent->source(),
            wheelEvent->pointingDevice());
        QCoreApplication::sendEvent(viewport, &forwardedEvent);
    }
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    TRACE_FUNCTION();
    ui.setupUi(this);
    qApp->installEventFilter(this);
    ui.contentLayout->setContentsMargins(kPageLeftMargin, kPageTopMargin, kPageRightMargin, kPageBottomMargin);
    ui.contentLayout->setSpacing(kPageColumnSpacing);
    setupPages();
    setupWheelScrolling();
    ui.backendLabel->setText(Playground::backend());
    updateLogo();

    Playground* playground = Playground::instance();

    auto* optionsButton = new QToolButton(ui.leftPanel);
    optionsButton->setObjectName(QStringLiteral("runtimeOptionsButton"));
    optionsButton->setFixedSize(22, 22);
    optionsButton->setIconSize(QSize(14, 14));
    optionsButton->setToolTip("Back");
    optionsButton->move(0, 0);
    optionsButton->hide();
    optionsButton->raise();
    QObject::connect(optionsButton, &QAbstractButton::clicked, this, []() {
        Playground::close();
    });
    applyLeftPanelStyles();

    QObject::connect(ui.actionQuit, &QAction::triggered, qApp, &QCoreApplication::quit);
    QObject::connect(ui.actionWindow, &QAction::triggered, this, [this] {
        MainWindow* subwindow = new MainWindow(this);
        subwindow->show();
    });

    auto* consentButton = new QPushButton(this);
    consentButton->setObjectName("consentButton");
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

    auto* feedbackButton = new QPushButton("Feedback", this);
    feedbackButton->setObjectName("feedbackButton");
    statusBar()->addPermanentWidget(feedbackButton);
    QObject::connect(feedbackButton, &QAbstractButton::clicked, this, [this, playground]() {
        FeedbackDialog dialog(this);
        QVariantMap user = playground->user();
        dialog.setName(user.value("name").toString());
        dialog.setEmail(user.value("email").toString());
        if (dialog.exec() == QDialog::Accepted)
            playground->captureFeedback(dialog.message(), dialog.name(), dialog.email());
    });
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
            "QStatusBar QPushButton#consentButton { border: none; background: transparent; }"
            "QStatusBar QLabel { color: white; font-weight: bold; background: transparent; }"
            "QStatusBar QLabel#consentIcon { font-size: 18px; }"
            "QStatusBar QPushButton#feedbackButton {"
            " color: white; font-weight: bold;"
            " background: rgba(255, 255, 255, 0.15);"
            " border: 1px solid rgba(255, 255, 255, 0.5);"
            " border-radius: 4px; padding: 3px 12px; }"
            "QStatusBar QPushButton#feedbackButton:hover {"
            " background: rgba(255, 255, 255, 0.25); }"
            "QStatusBar QPushButton#feedbackButton:pressed {"
            " background: rgba(255, 255, 255, 0.35); }";
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
    QObject::connect(playground, &Playground::consentChanged, this, updateConsentStatus);
    QObject::connect(playground, &Playground::initializedChanged, this,
        [this](bool initialized) {
            if (initialized)
                showRuntimePage();
            else
                showInitPage();
        });

    if (playground->initialized())
        showRuntimePage();
    else
        showInitPage();
    setFocus();
}

void MainWindow::setupPages()
{
    ui.leftPanel->setFixedWidth(ui.leftPanel->sizeHint().width());
    ui.crashPane->hide();
}

void MainWindow::setupWheelScrolling()
{
    for (QAbstractSpinBox* spinBox : findChildren<QAbstractSpinBox*>()) {
        spinBox->setFocusPolicy(Qt::StrongFocus);
    }

    for (QComboBox* comboBox : findChildren<QComboBox*>()) {
        comboBox->setFocusPolicy(Qt::StrongFocus);
        if (QLineEdit* edit = comboBox->lineEdit())
            edit->setFocusPolicy(Qt::StrongFocus);
    }
}

void MainWindow::showInitPage()
{
    ui.initPage->populate();
    ui.crashPane->hide();
    if (auto* optionsButton = ui.leftPanel->findChild<QToolButton*>(QStringLiteral("runtimeOptionsButton")))
        optionsButton->hide();
    ui.pages->setCurrentWidget(ui.initPage);
    statusBar()->hide();
}

void MainWindow::showRuntimePage()
{
    ui.crashPane->show();
    if (auto* optionsButton = ui.leftPanel->findChild<QToolButton*>(QStringLiteral("runtimeOptionsButton"))) {
        optionsButton->show();
        optionsButton->raise();
    }
    ui.pages->setCurrentWidget(ui.runtimePage);
    statusBar()->show();
}

void MainWindow::applyLeftPanelStyles()
{
    const QColor textColor = palette().color(QPalette::WindowText);
    ui.backendLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-weight: bold; background-color: %2; border: none; border-radius: 5px; padding: 1px 7px;")
            .arg(Style::cssRgba(textColor, 170), Style::cssRgba(textColor, 24)));

    if (auto* optionsButton = ui.leftPanel->findChild<QToolButton*>(QStringLiteral("runtimeOptionsButton"))) {
        optionsButton->setIcon(Style::makeBackIcon(palette(), devicePixelRatioF()));
        optionsButton->setStyleSheet(Style::circularButtonStyle(palette()));
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange) {
        ui.initPage->refreshPaletteStyles();
        ui.runtimePage->refreshPaletteStyles();
        applyLeftPanelStyles();
        updateLogo();
    }
    QMainWindow::changeEvent(event);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Wheel) {
        if (auto* spinBox = findParent<QAbstractSpinBox>(watched)) {
            forwardWheelEventToScrollArea(spinBox, static_cast<QWheelEvent*>(event));
            return true;
        }
        if (auto* comboBox = findParent<QComboBox>(watched)) {
            forwardWheelEventToScrollArea(comboBox, static_cast<QWheelEvent*>(event));
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::updateLogo()
{
    const bool isDark = qApp->palette().window().color().lightness() < 128;
    const QPixmap logo(isDark ? ":/assets/sentry-glyph-light.png" : ":/assets/sentry-glyph-dark.png");
    ui.sentryLogo->setPixmap(logo);
}
