#include "sentrywindow.h"
#include "sentryfeedbackdialog.h"
#include "sentryplayground.h"
#include "sentrytrace.h"

#include <sentry.h>

#include <functional>

#include <QtCore/qfileinfo.h>
#include <QtCore/qlocale.h>
#include <QtCore/qobject.h>
#include <QtCore/qsettings.h>
#include <QtCore/qstandardpaths.h>
#include <QtGui/qaction.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qbuttongroup.h>
#include <QtWidgets/qcheckbox.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qdialog.h>
#include <QtWidgets/qfiledialog.h>
#include <QtWidgets/qgridlayout.h>
#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qspinbox.h>
#include <QtGui/qcolor.h>
#include <QtGui/qpainter.h>
#include <QtGui/qpalette.h>
#include <QtGui/qpixmap.h>
#include <QtWidgets/qstackedwidget.h>
#include <QtWidgets/qstatusbar.h>
#include <QtWidgets/qtoolbutton.h>
#include <QtWidgets/qtreewidget.h>

static constexpr int kPageLeftMargin = 22;
static constexpr int kPageTopMargin = 22;
static constexpr int kPageRightMargin = 22;
static constexpr int kPageBottomMargin = 16;
static constexpr int kPageColumnSpacing = 16;

SentryWindow::SentryWindow(QWidget *parent)
    : QMainWindow(parent)
{
    TRACE_FUNCTION();
    ui.setupUi(this);
    ui.rootLayout->setContentsMargins(kPageLeftMargin, kPageTopMargin, kPageRightMargin, kPageBottomMargin);
    ui.columnsLayout->setSpacing(kPageColumnSpacing);
    setupPages();
    ui.backendLabel->setText(SentryPlayground::backend());
    updateLogo();

#ifndef Q_OS_WINDOWS
    ui.fastfailButton->setEnabled(false);
#endif

    SentryPlayground* playground = SentryPlayground::instance();
    QObject::connect(ui.crashButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerCrash);
    QObject::connect(ui.stackOverflowButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerStackOverflow);
    QObject::connect(ui.fastfailButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerFastfail);
    QObject::connect(ui.assertButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerAssertFailure);
    QObject::connect(ui.abortButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerAbort);
    QObject::connect(ui.throwButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerException);

    ui.messageLevelBox->addItem("Debug", SENTRY_LEVEL_DEBUG);
    ui.messageLevelBox->addItem("Info", SENTRY_LEVEL_INFO);
    ui.messageLevelBox->addItem("Warning", SENTRY_LEVEL_WARNING);
    ui.messageLevelBox->addItem("Error", SENTRY_LEVEL_ERROR);
    ui.messageLevelBox->addItem("Fatal", SENTRY_LEVEL_FATAL);
    ui.messageLevelBox->setCurrentIndex(1);
    for (const char* type : { "default", "debug", "info", "navigation", "http", "query", "transaction", "ui", "user", "error" })
        ui.breadcrumbTypeBox->addItem(type);
    for (const char* type : { "std::exception", "std::runtime_error", "std::logic_error", "std::invalid_argument", "std::out_of_range", "std::bad_alloc", "Error" })
        ui.exceptionTypeBox->addItem(type);
    int typeBoxWidth = qMax(ui.breadcrumbTypeBox->sizeHint().width(), ui.exceptionTypeBox->sizeHint().width());
    ui.breadcrumbTypeBox->setFixedWidth(typeBoxWidth);
    ui.exceptionTypeBox->setFixedWidth(typeBoxWidth);

    const char* kMessageSegmentedBase =
        "QPushButton { color: #888; font-weight: bold; background: transparent;"
        " border: 1px solid #555; padding: 3px 12px; %1 }"
        "QPushButton:checked { background: #444; color: white; }";
    ui.messageButton->setStyleSheet(QString(kMessageSegmentedBase).arg(
        "border-top-left-radius: 4px; border-bottom-left-radius: 4px;"));
    ui.exceptionButton->setStyleSheet(QString(kMessageSegmentedBase).arg("border-left: none;"));
    ui.breadcrumbButton->setStyleSheet(QString(kMessageSegmentedBase).arg(
        "border-left: none; border-top-right-radius: 4px; border-bottom-right-radius: 4px;"));
    auto* messageGroup = new QButtonGroup(this);
    messageGroup->setExclusive(true);
    messageGroup->addButton(ui.messageButton);
    messageGroup->addButton(ui.exceptionButton);
    messageGroup->addButton(ui.breadcrumbButton);

    ui.breadcrumbTypeBox->setVisible(false);
    ui.exceptionTypeBox->setVisible(false);
    QObject::connect(ui.messageButton, &QAbstractButton::clicked, this, [this]() {
        ui.breadcrumbTypeBox->hide();
        ui.exceptionTypeBox->hide();
    });
    QObject::connect(ui.breadcrumbButton, &QAbstractButton::clicked, this, [this]() {
        ui.breadcrumbTypeBox->show();
        ui.exceptionTypeBox->hide();
    });
    QObject::connect(ui.exceptionButton, &QAbstractButton::clicked, this, [this]() {
        ui.breadcrumbTypeBox->hide();
        ui.exceptionTypeBox->show();
    });

#ifdef Q_OS_MACOS
    for (QLineEdit* edit : { ui.messageText, ui.userIdEdit, ui.userNameEdit,
             ui.userEmailEdit, ui.userIpEdit, ui.releaseEdit, ui.environmentEdit,
             ui.dsnEdit, ui.databasePathEdit, ui.initReleaseEdit, ui.initEnvironmentEdit,
             ui.initDistEdit, ui.externalReporterPathEdit }) {
        edit->setFixedHeight(28);
        edit->setContentsMargins(0, 4, 0, 0);
    }
    ui.tracesSampleRateBox->setFixedHeight(28);
    ui.maxBreadcrumbsBox->setFixedHeight(28);
    ui.shutdownTimeoutBox->setFixedHeight(28);
    ui.messageText->setContentsMargins(0, 2, 0, 0);
#endif

    auto makeArrowIcon = [this](qreal dpr) {
        const int size = 16;
        QPixmap pixmap(size * dpr, size * dpr);
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(palette().color(QPalette::Text), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.translate(size / 2.0, size / 2.0);
        p.drawLine(QPointF(-5, 0), QPointF(5, 0));
        p.drawLine(QPointF(5, 0), QPointF(1, -4));
        p.drawLine(QPointF(5, 0), QPointF(1, 4));
        return QIcon(pixmap);
    };
    auto* messageAction = ui.messageText->addAction(
        makeArrowIcon(devicePixelRatioF()), QLineEdit::TrailingPosition);
    auto triggerSend = [this, playground]() {
        int level = ui.messageLevelBox->currentData().toInt();
        if (ui.messageButton->isChecked()) {
            playground->captureMessage(level, ui.messageText->text());
        } else if (ui.breadcrumbButton->isChecked()) {
            playground->addBreadcrumb(ui.breadcrumbTypeBox->currentText(), level, ui.messageText->text());
        } else {
            playground->captureException(level, ui.exceptionTypeBox->currentText(), ui.messageText->text());
        }
    };
    QObject::connect(messageAction, &QAction::triggered, playground, triggerSend);
    QObject::connect(ui.messageText, &QLineEdit::returnPressed, playground, triggerSend);
    messageAction->setEnabled(!ui.messageText->text().isEmpty());
    QObject::connect(ui.messageText, &QLineEdit::textChanged, this,
        [messageAction](const QString& text) { messageAction->setEnabled(!text.isEmpty()); });

    const char* kSegmentedBase =
        "QPushButton { color: #888; font-weight: bold; background: transparent;"
        " border: 1px solid #555; padding: 3px 12px; %1 }"
        "QPushButton:checked { background: #444; color: white; }";
    ui.tagsButton->setStyleSheet(QString(kSegmentedBase).arg(
        "border-top-left-radius: 4px; border-bottom-left-radius: 4px;"));
    ui.contextsButton->setStyleSheet(QString(kSegmentedBase).arg("border-left: none;"));
    ui.attachmentsButton->setStyleSheet(QString(kSegmentedBase).arg(
        "border-left: none; border-top-right-radius: 4px; border-bottom-right-radius: 4px;"));
    auto* categoryGroup = new QButtonGroup(this);
    categoryGroup->setExclusive(true);
    categoryGroup->addButton(ui.tagsButton);
    categoryGroup->addButton(ui.contextsButton);
    categoryGroup->addButton(ui.attachmentsButton);

    const auto segmentedButtons = messageGroup->buttons() + categoryGroup->buttons();
    int segmentedWidth = 0;
    for (auto* b : segmentedButtons)
        segmentedWidth = qMax(segmentedWidth, b->sizeHint().width());
    for (auto* b : segmentedButtons)
        b->setFixedWidth(segmentedWidth);

    ui.tagsButton->setChecked(true);
    ui.categoryStack->setCurrentIndex(0);
    QObject::connect(ui.tagsButton, &QAbstractButton::clicked, this,
        [this]() { ui.categoryStack->setCurrentIndex(0); });
    QObject::connect(ui.contextsButton, &QAbstractButton::clicked, this,
        [this]() { ui.categoryStack->setCurrentIndex(1); });
    QObject::connect(ui.attachmentsButton, &QAbstractButton::clicked, this,
        [this]() { ui.categoryStack->setCurrentIndex(2); });

    auto setupTree = [](QTreeWidget* tree, int narrowColumn, QHeaderView::ResizeMode narrowMode, int narrowWidth = -1) {
        int stretchColumn = 1 - narrowColumn;
        tree->header()->setStretchLastSection(false);
        tree->header()->setSectionResizeMode(narrowColumn, narrowMode);
        tree->header()->setSectionResizeMode(stretchColumn, QHeaderView::Stretch);
        if (narrowWidth > 0)
            tree->header()->resizeSection(narrowColumn, narrowWidth);
        tree->setRootIsDecorated(false);
        tree->setUniformRowHeights(true);
        tree->setAllColumnsShowFocus(true);
    };
    setupTree(ui.tagsTable, 0, QHeaderView::Interactive, 100);
    setupTree(ui.contextsTable, 0, QHeaderView::Interactive, 100);
    setupTree(ui.attachmentTable, 1, QHeaderView::ResizeToContents);

    auto wireKeyValueTree = [this](QTreeWidget* tree,
        std::function<void(const QString&, const QString&)> onSet,
        std::function<void(const QString&)> onRemove) {
        QObject::connect(tree, &QTreeWidget::itemChanged, this,
            [onSet, onRemove](QTreeWidgetItem* item, int) {
                QString key = item->text(0).trimmed();
                QString value = item->text(1);
                QString oldKey = item->data(0, Qt::UserRole).toString();
                if (!oldKey.isEmpty() && oldKey != key)
                    onRemove(oldKey);
                if (!key.isEmpty()) {
                    onSet(key, value);
                    item->setData(0, Qt::UserRole, key);
                }
            });
        QObject::connect(tree, &QWidget::customContextMenuRequested, this,
            [this, tree, onRemove](const QPoint& pos) {
                auto* item = tree->itemAt(pos);
                if (!item)
                    return;
                QMenu menu(this);
                menu.addAction("Remove", [tree, item, onRemove]() {
                    QString oldKey = item->data(0, Qt::UserRole).toString();
                    if (!oldKey.isEmpty())
                        onRemove(oldKey);
                    delete tree->takeTopLevelItem(tree->indexOfTopLevelItem(item));
                });
                menu.exec(tree->viewport()->mapToGlobal(pos));
            });
    };
    wireKeyValueTree(ui.tagsTable,
        [playground](const QString& k, const QString& v) { playground->setTag(k, v); },
        [playground](const QString& k) { playground->removeTag(k); });
    wireKeyValueTree(ui.contextsTable,
        [playground](const QString& k, const QString& v) { playground->setContext(k, v); },
        [playground](const QString& k) { playground->removeContext(k); });

    auto populateKeyValueTree = [](QTreeWidget* tree, const QVariantMap& map) {
        QSignalBlocker blocker(tree);
        tree->clear();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            auto* item = new QTreeWidgetItem(tree);
            item->setFlags(item->flags() | Qt::ItemIsEditable);
            item->setText(0, it.key());
            item->setText(1, it.value().toString());
            item->setData(0, Qt::UserRole, it.key());
        }
    };
    populateKeyValueTree(ui.tagsTable, playground->tags());
    populateKeyValueTree(ui.contextsTable, playground->contexts());

    auto refreshAttachments = [this, playground]() {
        QSignalBlocker blocker(ui.attachmentTable);
        ui.attachmentTable->clear();
        for (const QString& path : playground->attachments()) {
            QFileInfo info(path);
            auto* item = new QTreeWidgetItem(ui.attachmentTable);
            item->setText(0, info.fileName());
            item->setToolTip(0, path);
            item->setData(0, Qt::UserRole, path);
            item->setText(1, QLocale::system().formattedDataSize(info.size()));
            item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        }
    };
    refreshAttachments();
    QObject::connect(playground, &SentryPlayground::attachmentsChanged, this,
        [refreshAttachments]() { refreshAttachments(); });

    auto addAttachmentRow = [this, playground]() {
        QString lastDir = QSettings().value("attachmentDir",
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
        QString path = QFileDialog::getOpenFileName(this, "Select attachment", lastDir);
        if (path.isEmpty())
            return;
        QSettings().setValue("attachmentDir", QFileInfo(path).absolutePath());
        playground->addAttachment(path);
    };

    auto addEmptyRow = [](QTreeWidget* tree) {
        auto* item = new QTreeWidgetItem(tree);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        tree->editItem(item, 0);
    };

    const char* kCircularButton =
        "QToolButton {"
        " border: none; border-radius: 11px;"
        " background: #3a3a3a; padding: 0; %1 }"
        "QToolButton:hover { background: #4a4a4a; }"
        "QToolButton:pressed { background: #555; }";

    auto makeBackIcon = [](qreal dpr) {
        const int size = 16;
        QPixmap pixmap(size * dpr, size * dpr);
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor("#f2f2f2"), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(QPointF(4.5, 8), QPointF(11.5, 8));
        p.drawLine(QPointF(7.5, 4.5), QPointF(4.5, 8));
        p.drawLine(QPointF(4.5, 8), QPointF(7.5, 11.5));
        return QIcon(pixmap);
    };

    auto* optionsButton = new QToolButton(ui.runtimePage);
    optionsButton->setFixedSize(22, 22);
    optionsButton->setIconSize(QSize(14, 14));
    optionsButton->setIcon(makeBackIcon(devicePixelRatioF()));
    optionsButton->setStyleSheet(QString(kCircularButton).arg(""));
    optionsButton->setToolTip("Back");
    optionsButton->move(kPageLeftMargin, kPageTopMargin);
    optionsButton->raise();
    QObject::connect(optionsButton, &QAbstractButton::clicked, this, [this]() {
        SentryPlayground::close();
    });

    ui.addButton->setFixedSize(22, 22);
    ui.addButton->setStyleSheet(QString(kCircularButton).arg(""));

    auto makePlusIcon = [this](qreal dpr) {
        const int size = 12;
        QPixmap pixmap(size * dpr, size * dpr);
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(palette().color(QPalette::Text), 1.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(size / 2.0, 2), QPointF(size / 2.0, size - 2));
        p.drawLine(QPointF(2, size / 2.0), QPointF(size - 2, size / 2.0));
        return QIcon(pixmap);
    };
    ui.addButton->setText("");
    ui.addButton->setIcon(makePlusIcon(devicePixelRatioF()));

    auto updateAddButton = [this]() {
        bool isAttachments = ui.categoryStack->currentIndex() == 2;
        ui.addButton->setToolTip(isAttachments ? "Add attachment…" : "Add row");
    };
    updateAddButton();
    QObject::connect(ui.categoryStack, &QStackedWidget::currentChanged, this,
        [updateAddButton](int) { updateAddButton(); });

    QObject::connect(ui.addButton, &QAbstractButton::clicked, this,
        [this, addAttachmentRow, addEmptyRow]() {
            switch (ui.categoryStack->currentIndex()) {
            case 0: addEmptyRow(ui.tagsTable); break;
            case 1: addEmptyRow(ui.contextsTable); break;
            case 2: addAttachmentRow(); break;
            }
        });

    struct UserField { QLineEdit* edit; const char* key; };
    const QList<UserField> userFields = {
        { ui.userIdEdit, "id" },
        { ui.userNameEdit, "name" },
        { ui.userEmailEdit, "email" },
        { ui.userIpEdit, "ip_address" },
    };
    auto populateUser = [userFields, playground]() {
        QVariantMap user = playground->user();
        for (const UserField& f : userFields) {
            QSignalBlocker blocker(f.edit);
            if (!f.edit->hasFocus())
                f.edit->setText(user.value(f.key).toString());
        }
    };
    populateUser();
    for (const UserField& f : userFields) {
        QObject::connect(f.edit, &QLineEdit::editingFinished, this,
            [f, playground]() { playground->updateUser(f.key, f.edit->text()); });
    }
    QObject::connect(playground, &SentryPlayground::userChanged, this,
        [populateUser](const QVariantMap&) { populateUser(); });

    ui.releaseEdit->setText(playground->release());
    ui.environmentEdit->setText(playground->environment());

    auto isSessionPending = [this, playground]() {
        return ui.releaseEdit->text() != playground->release()
            || ui.environmentEdit->text() != playground->environment();
    };
    ui.sessionButton->setFixedSize(22, 22);
    QPalette editDefaultPalette = ui.releaseEdit->palette();
    QPalette editPendingPalette = editDefaultPalette;
    editPendingPalette.setColor(QPalette::Text, QColor("#ff3b30"));
    auto updateSessionButton = [this, playground, editDefaultPalette, editPendingPalette, kCircularButton]() {
        bool releasePending = ui.releaseEdit->text() != playground->release();
        bool envPending = ui.environmentEdit->text() != playground->environment();
        bool pending = releasePending || envPending;
        bool active = playground->session();
        ui.releaseEdit->setPalette(releasePending ? editPendingPalette : editDefaultPalette);
        ui.environmentEdit->setPalette(envPending ? editPendingPalette : editDefaultPalette);
        if (pending) {
            ui.sessionButton->setText("⟳");
            ui.sessionButton->setToolTip("Apply and restart session");
            ui.sessionButton->setStyleSheet(QString(kCircularButton).arg(
                "font-size: 16px; font-weight: bold; color: #ff3b30;"));
        } else {
            ui.sessionButton->setText(active ? "⏹" : "▶");
            ui.sessionButton->setToolTip(active ? "End session" : "Start session");
            ui.sessionButton->setStyleSheet(QString(kCircularButton).arg("font-size: 16px;"));
        }
    };
    updateSessionButton();
    QObject::connect(ui.releaseEdit, &QLineEdit::textChanged, this,
        [updateSessionButton](const QString&) { updateSessionButton(); });
    QObject::connect(ui.environmentEdit, &QLineEdit::textChanged, this,
        [updateSessionButton](const QString&) { updateSessionButton(); });
    QObject::connect(playground, &SentryPlayground::releaseChanged, this,
        [this](const QString& release) {
            if (!ui.releaseEdit->hasFocus())
                ui.releaseEdit->setText(release);
        });
    QObject::connect(playground, &SentryPlayground::environmentChanged, this,
        [this](const QString& environment) {
            if (!ui.environmentEdit->hasFocus())
                ui.environmentEdit->setText(environment);
        });
    QObject::connect(playground, &SentryPlayground::sessionChanged, this,
        [updateSessionButton](bool) { updateSessionButton(); });
    QObject::connect(ui.sessionButton, &QAbstractButton::clicked, this,
        [this, playground, isSessionPending]() {
            if (isSessionPending()) {
                playground->setRelease(ui.releaseEdit->text());
                playground->setEnvironment(ui.environmentEdit->text());
                if (playground->session())
                    playground->setSession(false);
                playground->setSession(true);
            } else {
                playground->setSession(!playground->session());
            }
        });

    QObject::connect(ui.attachmentTable, &QWidget::customContextMenuRequested, this,
        [this, playground](const QPoint& pos) {
            auto* item = ui.attachmentTable->itemAt(pos);
            if (!item)
                return;
            QMenu menu(this);
            QString path = item->data(0, Qt::UserRole).toString();
            menu.addAction("Remove", [playground, path]() { playground->removeAttachment(path); });
            menu.exec(ui.attachmentTable->viewport()->mapToGlobal(pos));
        });

    QObject::connect(ui.actionQuit, &QAction::triggered, qApp, &QCoreApplication::quit);
    QObject::connect(ui.actionWindow, &QAction::triggered, this, [this] {
        SentryWindow* subwindow = new SentryWindow(this);
        subwindow->show();
    });

    QObject::connect(ui.workerBox, &QAbstractButton::toggled, playground, &SentryPlayground::setWorker);
    QObject::connect(playground, &SentryPlayground::workerChanged, ui.workerBox, &QAbstractButton::setChecked);

    QObject::connect(ui.filterBox, &QAbstractButton::toggled, playground, &SentryPlayground::setFilter);
    QObject::connect(playground, &SentryPlayground::filterChanged, ui.filterBox, &QAbstractButton::setChecked);

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
        SentryFeedbackDialog dialog(this);
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
    QObject::connect(playground, &SentryPlayground::consentChanged, this, updateConsentStatus);
    QObject::connect(playground, &SentryPlayground::initializedChanged, this,
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

void SentryWindow::setupPages()
{
    ui.initLeftPanel->setFixedWidth(ui.leftColumn->sizeHint().width());
    ui.initBackendLabel->setText(SentryPlayground::backend());
    for (int column = 0; column < 3; ++column)
        ui.initFieldsGrid->setColumnStretch(column, 1);
    ui.reporterFieldsLayout->setStretch(0, 1);

    auto makeEllipsisIcon = [](qreal dpr) {
        const int size = 16;
        QPixmap pixmap(size * dpr, size * dpr);
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor("#b0b0b0"));
        p.setPen(Qt::NoPen);
        for (int x : { 5, 8, 11 })
            p.drawEllipse(QPointF(x, size / 2.0), 1.2, 1.2);
        return QIcon(pixmap);
    };
    auto makeClearIcon = [](qreal dpr) {
        const int size = 16;
        QPixmap pixmap(size * dpr, size * dpr);
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor("#b0b0b0"), 1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(5, 5), QPointF(11, 11));
        p.drawLine(QPointF(11, 5), QPointF(5, 11));
        return QIcon(pixmap);
    };

    QAction* databaseBrowseAction = ui.databasePathEdit->addAction(
        makeEllipsisIcon(devicePixelRatioF()), QLineEdit::LeadingPosition);
    databaseBrowseAction->setToolTip("Browse");
    QObject::connect(databaseBrowseAction, &QAction::triggered, this, [this]() {
        QString seed = ui.databasePathEdit->text();
        if (seed.isEmpty())
            seed = QSettings().value("databasePath/lastDir",
                QStandardPaths::writableLocation(QStandardPaths::CacheLocation)).toString();
        QString path = QFileDialog::getExistingDirectory(this, "Select database path", seed);
        if (path.isEmpty())
            return;
        QSettings().setValue("databasePath/lastDir", path);
        ui.databasePathEdit->setText(path);
    });

    QAction* browseAction = ui.externalReporterPathEdit->addAction(
        makeEllipsisIcon(devicePixelRatioF()), QLineEdit::LeadingPosition);
    browseAction->setToolTip("Browse");
    QAction* clearReporterAction = ui.externalReporterPathEdit->addAction(
        makeClearIcon(devicePixelRatioF()), QLineEdit::TrailingPosition);
    clearReporterAction->setObjectName("externalReporterClearAction");
    clearReporterAction->setToolTip("Clear");
    auto updateReporterClearAction = [this, clearReporterAction]() {
        clearReporterAction->setVisible(!ui.externalReporterPathEdit->text().isEmpty());
    };
    QObject::connect(ui.externalReporterPathEdit, &QLineEdit::textChanged, this,
        [updateReporterClearAction](const QString&) { updateReporterClearAction(); });
    QObject::connect(clearReporterAction, &QAction::triggered, ui.externalReporterPathEdit, &QLineEdit::clear);
    updateReporterClearAction();

    auto updateReporterControls = [this, browseAction, clearReporterAction](bool enabled) {
        ui.externalReporterPathEdit->setEnabled(enabled);
        browseAction->setEnabled(enabled);
        clearReporterAction->setEnabled(enabled);
    };
    QObject::connect(ui.externalReporterBox, &QAbstractButton::toggled, this, updateReporterControls);

    QObject::connect(browseAction, &QAction::triggered, this, [this]() {
        QString seed = ui.externalReporterPathEdit->text();
        if (seed.isEmpty())
            seed = QSettings().value("externalCrashReporter/lastDir",
                QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation)).toString();
        QString path = QFileDialog::getOpenFileName(this, "Select external crash reporter", seed);
        if (path.isEmpty())
            return;
        QSettings().setValue("externalCrashReporter/lastDir", QFileInfo(path).absolutePath());
        ui.externalReporterPathEdit->setText(path);
    });

    QObject::connect(ui.initializeButton, &QAbstractButton::clicked, this, [this]() {
        SentryPlayground::open(initOptionsFromPage());
    });

    populateInitPage();
}

void SentryWindow::populateInitPage()
{
    if (!ui.dsnEdit)
        return;

    const SentryPlayground::InitOptions options = SentryPlayground::instance()->initOptions();
    const QList<QWidget*> widgets = {
        ui.dsnEdit,
        ui.databasePathEdit,
        ui.initReleaseEdit,
        ui.initEnvironmentEdit,
        ui.initDistEdit,
        ui.tracesSampleRateBox,
        ui.maxBreadcrumbsBox,
        ui.shutdownTimeoutBox,
        ui.attachScreenshotBox,
        ui.requireUserConsentBox,
        ui.systemCrashReporterBox,
        ui.enableLargeAttachmentsBox,
        ui.httpRetryBox,
        ui.cacheKeepBox,
        ui.debugBox,
        ui.externalReporterBox,
        ui.externalReporterPathEdit,
    };
    QList<QSignalBlocker*> blockers;
    blockers.reserve(widgets.size());
    for (QWidget* widget : widgets)
        blockers.append(new QSignalBlocker(widget));

    ui.dsnEdit->setText(options.dsn);
    ui.dsnEdit->setCursorPosition(0);
    ui.databasePathEdit->setText(options.databasePath);
    ui.databasePathEdit->setCursorPosition(0);
    ui.initReleaseEdit->setText(options.release);
    ui.initEnvironmentEdit->setText(options.environment);
    ui.initDistEdit->setText(options.dist);
    ui.tracesSampleRateBox->setValue(options.tracesSampleRate);
    ui.maxBreadcrumbsBox->setValue(options.maxBreadcrumbs);
    ui.shutdownTimeoutBox->setValue(options.shutdownTimeout);
    ui.attachScreenshotBox->setChecked(options.attachScreenshot);
    ui.requireUserConsentBox->setChecked(options.requireUserConsent);
    ui.systemCrashReporterBox->setChecked(options.systemCrashReporterEnabled);
    ui.enableLargeAttachmentsBox->setChecked(options.enableLargeAttachments);
    ui.httpRetryBox->setChecked(options.httpRetry);
    ui.cacheKeepBox->setChecked(options.cacheKeep);
    ui.debugBox->setChecked(options.debug);
    ui.externalReporterBox->setChecked(options.externalCrashReporterEnabled);
    ui.externalReporterPathEdit->setText(options.externalCrashReporterPath);
    for (QAction* action : ui.externalReporterPathEdit->actions()) {
        action->setEnabled(options.externalCrashReporterEnabled);
        if (action->objectName() == "externalReporterClearAction")
            action->setVisible(!ui.externalReporterPathEdit->text().isEmpty());
    }
    ui.externalReporterPathEdit->setEnabled(options.externalCrashReporterEnabled);
    ui.initializeButton->setText(SentryPlayground::instance()->hasInitialized()
        ? "Re-initialize"
        : "Initialize");

    qDeleteAll(blockers);
}

SentryPlayground::InitOptions SentryWindow::initOptionsFromPage() const
{
    SentryPlayground::InitOptions options = SentryPlayground::instance()->initOptions();
    options.dsn = ui.dsnEdit->text().trimmed();
    options.databasePath = ui.databasePathEdit->text();
    options.release = ui.initReleaseEdit->text().trimmed();
    options.environment = ui.initEnvironmentEdit->text().trimmed();
    options.dist = ui.initDistEdit->text().trimmed();
    options.tracesSampleRate = ui.tracesSampleRateBox->value();
    options.maxBreadcrumbs = ui.maxBreadcrumbsBox->value();
    options.shutdownTimeout = ui.shutdownTimeoutBox->value();
    options.attachScreenshot = ui.attachScreenshotBox->isChecked();
    options.requireUserConsent = ui.requireUserConsentBox->isChecked();
    options.systemCrashReporterEnabled = ui.systemCrashReporterBox->isChecked();
    options.enableLargeAttachments = ui.enableLargeAttachmentsBox->isChecked();
    options.httpRetry = ui.httpRetryBox->isChecked();
    options.cacheKeep = ui.cacheKeepBox->isChecked();
    options.debug = ui.debugBox->isChecked();
    options.externalCrashReporterEnabled = ui.externalReporterBox->isChecked();
    options.externalCrashReporterPath = ui.externalReporterPathEdit->text();
    return options;
}

void SentryWindow::showInitPage()
{
    populateInitPage();
    ui.pages->setCurrentWidget(ui.initPage);
    statusBar()->hide();
}

void SentryWindow::showRuntimePage()
{
    ui.pages->setCurrentWidget(ui.runtimePage);
    statusBar()->show();
}

void SentryWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange)
        updateLogo();
    QMainWindow::changeEvent(event);
}

void SentryWindow::updateLogo()
{
    bool isDark = qApp->palette().window().color().lightness() < 128;
    QPixmap logo(isDark ? ":/assets/sentry-glyph-light.png" : ":/assets/sentry-glyph-dark.png");
    ui.sentryLogo->setPixmap(logo);
    if (ui.initLogo)
        ui.initLogo->setPixmap(logo);
}
