#include "mainwindow.h"
#include "feedbackdialog.h"
#include "playground.h"
#include "tracing.h"
#include "style.h"

#include <QtCore/qcoreevent.h>
#include <QtGui/qaction.h>
#include <QtGui/qevent.h>
#include <QtGui/qpalette.h>
#include <QtGui/qpainter.h>
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
#include <QtWidgets/qsizepolicy.h>
#include <QtWidgets/qstatusbar.h>
#include <QtWidgets/qtoolbutton.h>

static constexpr int kPageLeftMargin = 22;
static constexpr int kPageTopMargin = 22;
static constexpr int kPageRightMargin = 22;
static constexpr int kPageBottomMargin = 16;
static constexpr int kPageColumnSpacing = 16;
static constexpr int kStatusBarRightMargin = 8;
static constexpr int kStatusBarVerticalPadding = 4;
static QString statusBarStyle(const QString& backgroundColor)
{
    return QStringLiteral(
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
        " background: rgba(255, 255, 255, 0.35); }")
        .arg(backgroundColor);
}

static QString mutedStatusBarBackground(const QPalette& palette)
{
    return Style::cssRgb(Style::blendedColor(
        palette.color(QPalette::Window),
        palette.color(QPalette::WindowText),
        4));
}

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

    Playground* playground = Playground::instance();
    QObject::connect(ui.sentryLogoButton, &QAbstractButton::clicked, this, [this]() {
        if (Playground::instance()->isInitialized() && ui.pages->currentWidget() == ui.runtimePage)
            ui.crashPane->uploadDebugFiles();
    });
    QObject::connect(ui.crashPane, &CrashPane::debugFilesUploadStatusChanged, this,
        [this](DebugFilesDialog::UploadStatus status) {
            m_debugFilesUploadStatus = status;
            updateLogo();
        });
    m_debugFilesUploadStatus = ui.crashPane->debugFilesUploadStatus();
    updateLogo();

    ui.runtimeOptionsButton->setIconSize(QSize(14, 14));
    ui.runtimeOptionsButton->hide();
    QObject::connect(ui.runtimeOptionsButton, &QAbstractButton::clicked, this, []() {
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
    consentButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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
    statusBar()->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* feedbackButton = new QPushButton("Feedback", this);
    feedbackButton->setObjectName("feedbackButton");
    feedbackButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    statusBar()->addPermanentWidget(feedbackButton);
    if (QLayout* statusLayout = statusBar()->layout())
        statusLayout->setContentsMargins(
            0, kStatusBarVerticalPadding, kStatusBarRightMargin, kStatusBarVerticalPadding);
    statusBar()->setFixedHeight(feedbackButton->sizeHint().height());
    QObject::connect(feedbackButton, &QAbstractButton::clicked, this, [this, playground]() {
        FeedbackDialog dialog(this);
        QVariantMap user = playground->user();
        dialog.setName(user.value("name").toString());
        dialog.setEmail(user.value("email").toString());
        if (dialog.exec() == QDialog::Accepted)
            playground->captureFeedback(dialog.message(), dialog.name(), dialog.email());
    });
    QObject::connect(consentButton, &QAbstractButton::clicked, playground, [playground]() {
        if (!playground->options().requireUserConsent)
            return;

        switch (playground->consent()) {
        case Qt::PartiallyChecked: playground->setConsent(Qt::Checked); break;
        case Qt::Checked: playground->setConsent(Qt::Unchecked); break;
        case Qt::Unchecked: playground->setConsent(Qt::PartiallyChecked); break;
        }
    });

    consentIcon->setObjectName("consentIcon");
    consentText->setObjectName("consentText");
    updateConsentStatus(playground->consent());
    QObject::connect(playground, &Playground::consentChanged,
        this, &MainWindow::updateConsentStatus);
    QObject::connect(playground, &Playground::optionsChanged, this,
        [this](const Options&) { updateStatusBarVisibility(); });
    QObject::connect(playground, &Playground::initializedChanged, this,
        [this](bool initialized) {
            if (initialized)
                showRuntimePage();
            else
                showInitPage();
        });

    if (playground->isInitialized())
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
    ui.runtimeOptionsButton->hide();
    ui.pages->setCurrentWidget(ui.initPage);
    updateLogo();
    updateStatusBarVisibility();
}

void MainWindow::showRuntimePage()
{
    ui.crashPane->show();
    ui.runtimeOptionsButton->show();
    ui.pages->setCurrentWidget(ui.runtimePage);
    updateLogo();
    updateStatusBarVisibility();
}

void MainWindow::updateStatusBarVisibility()
{
    Playground* playground = Playground::instance();
    const bool showRuntimeFooter = playground->isInitialized()
        && ui.pages->currentWidget() == ui.runtimePage;
    const bool showConsentFooter = playground->isInitialized()
        && ui.pages->currentWidget() == ui.runtimePage
        && playground->options().requireUserConsent;
    statusBar()->setVisible(showRuntimeFooter);

    auto* consentButton = statusBar()->findChild<QPushButton*>(QStringLiteral("consentButton"));
    auto* consentIcon = statusBar()->findChild<QLabel*>(QStringLiteral("consentIcon"));
    auto* consentText = statusBar()->findChild<QLabel*>(QStringLiteral("consentText"));
    if (showConsentFooter) {
        if (consentButton) {
            consentButton->setEnabled(true);
            consentButton->show();
        }
        updateConsentStatus(playground->consent());
    } else {
        statusBar()->setStyleSheet(statusBarStyle(mutedStatusBarBackground(palette())));
        if (consentButton) {
            consentButton->setEnabled(true);
            consentButton->setChecked(false);
            consentButton->hide();
        }
        if (consentIcon)
            consentIcon->clear();
        if (consentText)
            consentText->clear();
    }
}

void MainWindow::updateConsentStatus(Qt::CheckState state)
{
    Playground* playground = Playground::instance();
    if (!playground->options().requireUserConsent) {
        updateStatusBarVisibility();
        return;
    }

    auto* consentButton = statusBar()->findChild<QPushButton*>(QStringLiteral("consentButton"));
    auto* consentIcon = statusBar()->findChild<QLabel*>(QStringLiteral("consentIcon"));
    auto* consentText = statusBar()->findChild<QLabel*>(QStringLiteral("consentText"));
    if (!consentButton || !consentIcon || !consentText)
        return;

    switch (state) {
    case Qt::Checked:
        statusBar()->setStyleSheet(statusBarStyle(QStringLiteral("#2ecc71")));
        consentIcon->setText("✓");
        consentText->setText("Consent given — events will be captured and sent to Sentry");
        consentButton->setChecked(true);
        break;
    case Qt::Unchecked:
        statusBar()->setStyleSheet(statusBarStyle(QStringLiteral("#e74c3c")));
        consentIcon->setText("⚠");
        consentText->setText("Consent revoked — events will be discarded and not sent to Sentry");
        consentButton->setChecked(false);
        break;
    case Qt::PartiallyChecked:
        statusBar()->setStyleSheet(statusBarStyle(QStringLiteral("#f39c12")));
        consentIcon->setText("⚠");
        consentText->setText("Consent unknown — events will be discarded until consent is given");
        consentButton->setChecked(false);
        break;
    }
}

void MainWindow::applyLeftPanelStyles()
{
    const QColor textColor = palette().color(QPalette::WindowText);
    ui.backendLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-weight: bold; background-color: %2; border: none; border-radius: 5px; padding: 1px 7px;")
            .arg(Style::cssRgba(textColor, 170), Style::cssRgba(textColor, 24)));

    ui.runtimeOptionsButton->setIcon(Style::makeBackIcon(palette(), devicePixelRatioF()));
    ui.runtimeOptionsButton->setStyleSheet(Style::circularButtonStyle(palette()));
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange) {
        ui.initPage->refreshPaletteStyles();
        ui.runtimePage->refreshPaletteStyles();
        applyLeftPanelStyles();
        updateLogo();
        updateStatusBarVisibility();
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
    const qreal dpr = logo.devicePixelRatio();
    const int width = qRound(logo.width() / dpr);
    const int height = qRound(logo.height() / dpr);
    const bool uploadAvailable = Playground::instance()->isInitialized()
        && ui.pages->currentWidget() == ui.runtimePage;
    const int padding = 4;
    const int canvasWidth = width + padding * 2;
    const int canvasHeight = height + padding * 2;
    const QRect logoRect(padding, padding, width, height);

    QPixmap composite(qRound(canvasWidth * dpr), qRound(canvasHeight * dpr));
    composite.setDevicePixelRatio(dpr);
    composite.fill(Qt::transparent);

    QPainter painter(&composite);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawPixmap(logoRect, logo);

    if (uploadAvailable) {
        const int dotSize = 10;
        const QRect dotRect(
            logoRect.right() - dotSize / 2 + 1,
            logoRect.bottom() - dotSize / 2 + 1,
            dotSize,
            dotSize);
        const QColor dotColor = m_debugFilesUploadStatus == DebugFilesDialog::UploadStatus::UpToDate
            ? QColor("#34c759")
            : QColor("#ff453a");
        QColor dotOutline = palette().color(QPalette::Window);
        dotOutline.setAlpha(235);
        painter.setBrush(dotColor);
        painter.setPen(QPen(dotOutline, 2));
        painter.drawEllipse(dotRect);
    }
    painter.end();

    ui.sentryLogoButton->setIcon(QIcon(composite));
    ui.sentryLogoButton->setIconSize(QSize(canvasWidth, canvasHeight));
    ui.sentryLogoButton->setCursor(uploadAvailable ? Qt::PointingHandCursor : Qt::ArrowCursor);
    ui.sentryLogoButton->setToolTip(uploadAvailable ? debugFilesUploadToolTip() : QString());
    const QColor hoverColor = Style::blendedColor(
        palette().color(QPalette::Window),
        palette().color(QPalette::WindowText),
        uploadAvailable ? 20 : 0);
    const QColor pressedColor = Style::blendedColor(
        palette().color(QPalette::Window),
        palette().color(QPalette::WindowText),
        uploadAvailable ? 32 : 0);
    ui.sentryLogoButton->setStyleSheet(QStringLiteral(
        "QToolButton { border: none; border-radius: 10px; background: transparent; padding: 0; }"
        "QToolButton:hover { background: %1; }"
        "QToolButton:pressed { background: %2; }")
            .arg(Style::cssRgb(hoverColor), Style::cssRgb(pressedColor)));
}

QString MainWindow::debugFilesUploadToolTip() const
{
    return m_debugFilesUploadStatus == DebugFilesDialog::UploadStatus::UpToDate
        ? QStringLiteral("Debug files uploaded")
        : QStringLiteral("Upload debug files");
}
