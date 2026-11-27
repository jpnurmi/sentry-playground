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
#include <QtWidgets/qtoolbutton.h>

static constexpr int kPageLeftMargin = 22;
static constexpr int kPageTopMargin = 22;
static constexpr int kPageRightMargin = 22;
static constexpr int kPageBottomMargin = 16;
static constexpr int kPageColumnSpacing = 16;
static QString consentCardStyle(const QPalette& palette, const QColor& color)
{
    const QColor window = palette.color(QPalette::Window);
    const QColor text = palette.color(QPalette::WindowText);
    return QStringLiteral(
        "QPushButton#consentButton { %1 padding: 0; }"
        "QPushButton#consentButton:hover { background: %2; }"
        "QPushButton#consentButton:pressed { background: %3; }"
        "QLabel { color: %4; }"
        "QLabel#consentTitle { color: %5; font-weight: bold; }")
        .arg(Style::cardStyle(palette, color),
             Style::cssRgb(Style::blendedColor(window, color, 52)),
             Style::cssRgb(Style::blendedColor(window, color, 68)),
             Style::cssRgb(Style::blendedColor(window, text, 150)),
             Style::cssRgb(text));
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
    QObject::connect(ui.sentryLogoButton, &QAbstractButton::clicked, this, [this, playground]() {
        if (playground->isInitialized() && ui.pages->currentWidget() == ui.runtimePage)
            playground->setOffline(!playground->offline());
    });
    QObject::connect(ui.debugFilesButton, &QAbstractButton::clicked,
        ui.crashPane, &CrashPane::uploadDebugFiles);
    QObject::connect(ui.crashPane, &CrashPane::debugFilesUploadStatusChanged, this,
        [this](DebugFilesDialog::UploadStatus status) {
            m_debugFilesUploadStatus = status;
            updateDebugFilesStatus();
        });
    m_debugFilesUploadStatus = ui.crashPane->debugFilesUploadStatus();
    updateLogo();

    ui.runtimeOptionsButton->setIconSize(QSize(14, 14));
    ui.runtimeOptionsButton->hide();
    QObject::connect(ui.runtimeOptionsButton, &QAbstractButton::clicked, this, []() {
        Playground::close();
    });
    ui.debugFilesButton->setIconSize(QSize(14, 14));
    ui.debugFilesButton->hide();
    applyLeftPanelStyles();

    QObject::connect(ui.actionQuit, &QAction::triggered, qApp, &QCoreApplication::quit);
    QObject::connect(ui.actionWindow, &QAction::triggered, this, [this] {
        MainWindow* subwindow = new MainWindow(this);
        subwindow->show();
    });
    QObject::connect(ui.actionFeedback, &QAction::triggered, this, [this, playground] {
        FeedbackDialog dialog(this);
        QVariantMap user = playground->user();
        dialog.setName(user.value("name").toString());
        dialog.setEmail(user.value("email").toString());
        if (dialog.exec() == QDialog::Accepted)
            playground->captureFeedback(dialog.message(), dialog.name(), dialog.email());
    });

    auto* consentButton = new QPushButton(this);
    consentButton->setObjectName("consentButton");
    consentButton->setCheckable(true);
    consentButton->setFlat(true);
    consentButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* consentIcon = new QLabel(consentButton);
    auto* consentTitle = new QLabel(consentButton);
    auto* consentText = new QLabel(consentButton);
    auto* consentLayout = new QHBoxLayout(consentButton);
    consentLayout->setContentsMargins(8, 0, 8, 0);
    consentLayout->setSpacing(8);
    consentLayout->addWidget(consentIcon);
    consentLayout->addWidget(consentTitle);
    consentLayout->addWidget(consentText);
    consentLayout->addStretch();
    consentIcon->setAttribute(Qt::WA_TransparentForMouseEvents);
    consentTitle->setAttribute(Qt::WA_TransparentForMouseEvents);
    consentText->setAttribute(Qt::WA_TransparentForMouseEvents);
    // match the event card above it
    ui.eventCard->ensurePolished();
    consentButton->setMinimumHeight(ui.eventCard->sizeHint().height());
    ui.footerLayout->addWidget(consentButton);
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
    consentTitle->setObjectName("consentTitle");
    consentText->setObjectName("consentText");
    updateConsentStatus(playground->consent());
    QObject::connect(playground, &Playground::consentChanged,
        this, &MainWindow::updateConsentStatus);
    QObject::connect(playground, &Playground::optionsChanged, this,
        [this](const Options&) { updateConsentVisibility(); });
    QObject::connect(playground, &Playground::offlineChanged, this, &MainWindow::updateLogo);
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
    ui.debugFilesButton->hide();
    ui.pages->setCurrentWidget(ui.initPage);
    updateLogo();
    updateConsentVisibility();
}

void MainWindow::showRuntimePage()
{
    ui.crashPane->show();
    ui.runtimeOptionsButton->show();
    ui.debugFilesButton->show();
    ui.pages->setCurrentWidget(ui.runtimePage);
    updateLogo();
    updateConsentVisibility();
}

void MainWindow::updateConsentVisibility()
{
    Playground* playground = Playground::instance();
    const bool showConsentFooter = playground->isInitialized()
        && ui.pages->currentWidget() == ui.runtimePage
        && playground->options().requireUserConsent;

    auto* consentButton = ui.centralwidget->findChild<QPushButton*>(
        QStringLiteral("consentButton"), Qt::FindDirectChildrenOnly);
    if (!consentButton)
        return;

    consentButton->setVisible(showConsentFooter);
    if (showConsentFooter)
        updateConsentStatus(playground->consent());
    updateFooterMargins();
}

void MainWindow::updateConsentStatus(Qt::CheckState state)
{
    Playground* playground = Playground::instance();
    if (!playground->options().requireUserConsent) {
        updateConsentVisibility();
        return;
    }

    auto* consentButton = ui.centralwidget->findChild<QPushButton*>(
        QStringLiteral("consentButton"), Qt::FindDirectChildrenOnly);
    if (!consentButton)
        return;
    auto* consentIcon = consentButton->findChild<QLabel*>(QStringLiteral("consentIcon"));
    auto* consentTitle = consentButton->findChild<QLabel*>(QStringLiteral("consentTitle"));
    auto* consentText = consentButton->findChild<QLabel*>(QStringLiteral("consentText"));
    if (!consentIcon || !consentTitle || !consentText)
        return;

    switch (state) {
    case Qt::Checked:
        consentButton->setStyleSheet(consentCardStyle(palette(), QColor("#2ecc71")));
        consentIcon->setPixmap(Style::makeCheckIcon(QColor("#2ecc71"), devicePixelRatioF()).pixmap(QSize(16, 16), devicePixelRatioF()));
        consentTitle->setText("Consent given");
        consentText->setText("Events will be captured and sent to Sentry");
        consentButton->setChecked(true);
        break;
    case Qt::Unchecked:
        consentButton->setStyleSheet(consentCardStyle(palette(), QColor("#e74c3c")));
        consentIcon->setPixmap(Style::makeWarningIcon(QColor("#e74c3c"), devicePixelRatioF()).pixmap(QSize(16, 16), devicePixelRatioF()));
        consentTitle->setText("Consent revoked");
        consentText->setText("Events will be discarded and not sent to Sentry");
        consentButton->setChecked(false);
        break;
    case Qt::PartiallyChecked:
        consentButton->setStyleSheet(consentCardStyle(palette(), QColor("#f39c12")));
        consentIcon->setPixmap(Style::makeWarningIcon(QColor("#f39c12"), devicePixelRatioF()).pixmap(QSize(16, 16), devicePixelRatioF()));
        consentTitle->setText("Consent unknown");
        consentText->setText("Events will be discarded until consent is given");
        consentButton->setChecked(false);
        break;
    }
}

void MainWindow::updateFooterMargins()
{
    // an empty layout keeps its margins, so drop the bottom one without cards
    auto* consentButton = ui.centralwidget->findChild<QPushButton*>(
        QStringLiteral("consentButton"), Qt::FindDirectChildrenOnly);
    const bool empty = ui.eventCard->isHidden() && (!consentButton || consentButton->isHidden());
    ui.footerLayout->setContentsMargins(
        kPageLeftMargin, 0, kPageRightMargin, empty ? 0 : kPageBottomMargin);
}

void MainWindow::applyLeftPanelStyles()
{
    const QColor textColor = palette().color(QPalette::WindowText);
    ui.backendLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-weight: bold; background-color: %2; border: none; border-radius: 5px; padding: 1px 7px;")
            .arg(Style::cssRgba(textColor, 170), Style::cssRgba(textColor, 24)));

    ui.runtimeOptionsButton->setIcon(Style::makeBackIcon(palette(), devicePixelRatioF()));
    ui.runtimeOptionsButton->setStyleSheet(Style::circularButtonStyle(palette()));
    updateDebugFilesStatus();
}

void MainWindow::updateDebugFilesStatus()
{
    // the regular circular button, tinted by upload status
    QPalette tinted = palette();
    const QColor color = m_debugFilesUploadStatus == DebugFilesDialog::UploadStatus::UpToDate
        ? QColor("#34c759")
        : QColor("#ff453a");
    tinted.setColor(QPalette::WindowText, color);
    tinted.setColor(QPalette::ButtonText, color);
    ui.debugFilesButton->setIcon(Style::makeUploadIcon(tinted, devicePixelRatioF()));
    ui.debugFilesButton->setStyleSheet(Style::circularButtonStyle(tinted));
    ui.debugFilesButton->setToolTip(debugFilesUploadToolTip());
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange) {
        ui.initPage->refreshPaletteStyles();
        ui.runtimePage->refreshPaletteStyles();
        ui.eventCard->refreshPaletteStyles();
        applyLeftPanelStyles();
        updateLogo();
        updateConsentVisibility();
    }
    QMainWindow::changeEvent(event);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui.eventCard
        && (event->type() == QEvent::ShowToParent || event->type() == QEvent::HideToParent))
        updateFooterMargins();

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
    Playground* playground = Playground::instance();
    const bool offlineAvailable = playground->isInitialized()
        && ui.pages->currentWidget() == ui.runtimePage;
    const bool offline = offlineAvailable && playground->offline();
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

    if (offlineAvailable) {
        // purple while online, gray while offline
        const QColor tint = offline
            ? Style::blendedColor(palette().color(QPalette::Window), palette().color(QPalette::WindowText), 100)
            : QColor("#7553ff");
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(logoRect, tint);

        if (offline) {
            const QRectF slashRect = QRectF(logoRect).adjusted(16, 13, -16, -13);
            const QLineF slash(slashRect.topLeft(), slashRect.bottomRight());
            // cut a gap around the slash so it stands out from the glyph
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
            painter.setPen(QPen(Qt::black, 12, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(slash);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.setPen(QPen(QColor("#ff453a"), 6, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(slash);
        }
    }
    painter.end();

    ui.sentryLogoButton->setIcon(QIcon(composite));
    ui.sentryLogoButton->setIconSize(QSize(canvasWidth, canvasHeight));
    ui.sentryLogoButton->setCursor(offlineAvailable ? Qt::PointingHandCursor : Qt::ArrowCursor);
    QString toolTip;
    if (offline)
        toolTip = playground->options().httpRetry
            ? "Offline — events will be cached until back online"
            : "Offline — events will be discarded and not sent to Sentry";
    else if (offlineAvailable)
        toolTip = "Online — events will be sent to Sentry";
    ui.sentryLogoButton->setToolTip(toolTip);
    const QColor hoverColor = Style::blendedColor(
        palette().color(QPalette::Window),
        palette().color(QPalette::WindowText),
        offlineAvailable ? 20 : 0);
    const QColor pressedColor = Style::blendedColor(
        palette().color(QPalette::Window),
        palette().color(QPalette::WindowText),
        offlineAvailable ? 32 : 0);
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
        : QStringLiteral("Debug files not uploaded");
}
