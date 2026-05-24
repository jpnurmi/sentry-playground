#include "sentrywindow.h"
#include "sentryfeedbackdialog.h"
#include "sentryplayground.h"
#include "sentrytrace.h"

#include <sentry.h>

#include <functional>
#include <utility>

#include <QtCore/qcoreevent.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qlocale.h>
#include <QtCore/qobject.h>
#include <QtCore/qsettings.h>
#include <QtCore/qstandardpaths.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qurl.h>
#include <QtGui/qaction.h>
#include <QtGui/qevent.h>
#include <QtWidgets/qabstractscrollarea.h>
#include <QtWidgets/qabstractspinbox.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qbuttongroup.h>
#include <QtWidgets/qcheckbox.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qdialog.h>
#include <QtWidgets/qfiledialog.h>
#include <QtWidgets/qframe.h>
#include <QtWidgets/qgridlayout.h>
#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qspinbox.h>
#include <QtWidgets/qstackedlayout.h>
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
static constexpr int kMaxEnvironmentHistoryItems = 12;
static constexpr int kLoggerLevelNone = -1000;

static QString cssRgba(QColor color, int alpha)
{
    color.setAlpha(alpha);
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha());
}

static QString cssRgb(const QColor& color)
{
    return QStringLiteral("rgb(%1, %2, %3)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue());
}

static QColor blendedColor(QColor base, QColor overlay, int overlayAlpha)
{
    const qreal alpha = overlayAlpha / 255.0;
    return QColor(
        qRound(base.red() * (1 - alpha) + overlay.red() * alpha),
        qRound(base.green() * (1 - alpha) + overlay.green() * alpha),
        qRound(base.blue() * (1 - alpha) + overlay.blue() * alpha));
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

static QStringList environmentHistoryWith(QStringList history, const QString& environment)
{
    QStringList normalized;
    auto add = [&normalized](const QString& value) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty() && !normalized.contains(trimmed, Qt::CaseSensitive))
            normalized.append(trimmed);
    };
    add(environment);
    for (const QString& value : std::as_const(history))
        add(value);
    while (normalized.size() > kMaxEnvironmentHistoryItems)
        normalized.removeLast();
    return normalized;
}

SentryWindow::SentryWindow(QWidget *parent)
    : QMainWindow(parent)
{
    TRACE_FUNCTION();
    ui.setupUi(this);
    qApp->installEventFilter(this);
    ui.contentLayout->setContentsMargins(kPageLeftMargin, kPageTopMargin, kPageRightMargin, kPageBottomMargin);
    ui.contentLayout->setSpacing(kPageColumnSpacing);
    ui.initRightColumn->setAlignment(Qt::AlignTop);
    setupPages();
    setupWheelScrolling();
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
        "QPushButton { color: palette(mid); font-weight: bold; background: transparent;"
        " border: 1px solid palette(mid); padding: 3px 12px; %1 }"
        "QPushButton:checked { background: palette(midlight); color: palette(text); }";
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
             ui.dsnEdit, ui.databasePathEdit, ui.initReleaseEdit,
             ui.initDistEdit, ui.externalReporterPathEdit }) {
        edit->setFixedHeight(28);
        edit->setContentsMargins(0, 4, 0, 0);
    }
    ui.initEnvironmentEdit->setFixedHeight(28);
    if (QLineEdit* edit = ui.initEnvironmentEdit->lineEdit())
        edit->setContentsMargins(0, 4, 0, 0);
    ui.tracesSampleRateBox->setFixedHeight(28);
    ui.maxBreadcrumbsBox->setFixedHeight(28);
    ui.maxSpansBox->setFixedHeight(28);
    ui.shutdownTimeoutBox->setFixedHeight(28);
    ui.cacheKeepModeBox->setFixedHeight(28);
    ui.cacheMaxItemsBox->setFixedHeight(28);
    ui.cacheMaxSizeBox->setFixedHeight(28);
    ui.cacheMaxAgeBox->setFixedHeight(28);
    ui.loggerLevelBox->setFixedHeight(28);
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
        "QPushButton { color: palette(mid); font-weight: bold; background: transparent;"
        " border: 1px solid palette(mid); padding: 3px 12px; %1 }"
        "QPushButton:checked { background: palette(midlight); color: palette(text); }";
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
        " background: palette(button); padding: 0; %1 }"
        "QToolButton:hover { background: palette(light); }"
        "QToolButton:pressed { background: palette(midlight); }";

    auto makeBackIcon = [](qreal dpr) {
        const int size = 16;
        const QColor color = QApplication::palette().color(QPalette::ButtonText);
        QPixmap pixmap(size * dpr, size * dpr);
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(QPointF(4.5, 8), QPointF(11.5, 8));
        p.drawLine(QPointF(7.5, 4.5), QPointF(4.5, 8));
        p.drawLine(QPointF(4.5, 8), QPointF(7.5, 11.5));
        return QIcon(pixmap);
    };

    auto* optionsButton = new QToolButton(ui.runtimeLeftPanel);
    optionsButton->setFixedSize(22, 22);
    optionsButton->setIconSize(QSize(14, 14));
    optionsButton->setIcon(makeBackIcon(devicePixelRatioF()));
    optionsButton->setStyleSheet(QString(kCircularButton).arg(""));
    optionsButton->setToolTip("Back");
    optionsButton->move(0, 0);
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
    ui.initScrollBackdrop->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui.initScrollBackdrop->lower();

    const int leftWidth = qMax(ui.initLeftPanel->sizeHint().width(), ui.runtimeLeftPanel->sizeHint().width());
    ui.leftStack->setFixedWidth(leftWidth);
    ui.initLeftPanel->setFixedWidth(leftWidth);
    ui.runtimeLeftPanel->setFixedWidth(leftWidth);
    ui.initBackendLabel->setText(SentryPlayground::backend());

    auto wrapSummaryRow = [this](QHBoxLayout* layout, const char* objectName) {
        int rowIndex = -1;
        for (int i = 0; i < ui.initSummarySheetLayout->count(); ++i) {
            if (ui.initSummarySheetLayout->itemAt(i)->layout() == layout) {
                rowIndex = i;
                break;
            }
        }
        if (rowIndex < 0)
            return static_cast<QWidget*>(nullptr);

        QLayoutItem* item = ui.initSummarySheetLayout->takeAt(rowIndex);
        QWidget* row = new QWidget(ui.initSummarySheet);
        row->setObjectName(QLatin1String(objectName));
        row->setProperty("initSummaryRow", true);
        row->setCursor(Qt::PointingHandCursor);
        row->installEventFilter(this);
        layout->setParent(nullptr);
        row->setLayout(layout);
        ui.initSummarySheetLayout->insertWidget(rowIndex, row);
        if (item != layout)
            delete item;
        return row;
    };
    wrapSummaryRow(ui.sdkSummaryLayout, "sdkSummaryWidget");
    wrapSummaryRow(ui.versionSummaryLayout, "versionSummaryWidget");
    wrapSummaryRow(ui.featuresSummaryLayout, "featuresSummaryWidget");
    wrapSummaryRow(ui.advancedOptionsSummaryLayout, "advancedOptionsSummaryWidget");
    wrapSummaryRow(ui.cacheSummaryLayout, "cacheSummaryWidget");
    wrapSummaryRow(ui.externalReporterSummaryLayout, "externalReporterSummaryWidget");

    QLabel* formLabels[] = {
        ui.dsnLabel,
        ui.initReleaseLabel,
        ui.initEnvironmentLabel,
        ui.initDistLabel,
        ui.featuresFormSpacerLabel,
        ui.shutdownTimeoutLabel,
        ui.maxBreadcrumbsLabel,
        ui.maxSpansLabel,
        ui.tracesSampleRateLabel,
        ui.loggerLevelLabel,
        ui.databasePathLabel,
        ui.cacheModeLabel,
        ui.cacheMaxItemsLabel,
        ui.cacheMaxSizeLabel,
        ui.cacheMaxAgeLabel,
        ui.externalReporterPathLabel,
    };
    QLabel* summaryTitles[] = {
        ui.sdkSummaryTitle,
        ui.versionSummaryTitle,
        ui.featuresSummaryTitle,
        ui.advancedOptionsSummaryTitle,
        ui.cacheSummaryTitle,
        ui.externalReporterSummaryTitle,
    };
    int formLabelWidth = 0;
    for (QLabel* label : formLabels)
        formLabelWidth = qMax(formLabelWidth, label->sizeHint().width());
    for (QLabel* title : summaryTitles)
        formLabelWidth = qMax(formLabelWidth, title->sizeHint().width() + 6);
    for (QLabel* label : formLabels) {
        label->setFixedWidth(formLabelWidth);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    const int formHorizontalSpacing = 12;
    const int summaryHorizontalSpacing = formHorizontalSpacing;
    const int summaryIconWidth = ui.sdkSummaryIcon->minimumWidth();
    const int summaryLabelLeftMargin =
        ui.sdkSummaryLayout->contentsMargins().left() + summaryIconWidth + summaryHorizontalSpacing;
    auto alignDetailsForm = [summaryLabelLeftMargin, formHorizontalSpacing](QFormLayout* layout) {
        const QMargins margins = layout->contentsMargins();
        layout->setContentsMargins(summaryLabelLeftMargin, margins.top(), margins.right(), margins.bottom());
        layout->setHorizontalSpacing(formHorizontalSpacing);
    };
    alignDetailsForm(ui.sdkDetailsFormLayout);
    alignDetailsForm(ui.versionDetailsFormLayout);
    alignDetailsForm(ui.featuresDetailsFormLayout);
    alignDetailsForm(ui.advancedOptionsDetailsFormLayout);
    alignDetailsForm(ui.cacheDetailsFormLayout);
    alignDetailsForm(ui.externalReporterDetailsFormLayout);

    auto alignSummaryValue = [formLabelWidth, summaryHorizontalSpacing](
                                 QHBoxLayout* layout, QLabel* title, QLabel* summary) {
        layout->setSpacing(summaryHorizontalSpacing);
        const QMargins margins = layout->contentsMargins();
        layout->setContentsMargins(margins.left(), 3, margins.right(), 3);
        title->setFixedWidth(formLabelWidth);
        title->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        summary->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    };
    alignSummaryValue(ui.sdkSummaryLayout, ui.sdkSummaryTitle, ui.sdkSummaryLabel);
    alignSummaryValue(ui.versionSummaryLayout, ui.versionSummaryTitle, ui.versionSummaryLabel);
    alignSummaryValue(ui.featuresSummaryLayout, ui.featuresSummaryTitle, ui.featuresSummaryLabel);
    alignSummaryValue(ui.advancedOptionsSummaryLayout, ui.advancedOptionsSummaryTitle,
        ui.advancedOptionsSummaryLabel);
    alignSummaryValue(ui.cacheSummaryLayout, ui.cacheSummaryTitle, ui.cacheSummaryLabel);
    alignSummaryValue(ui.externalReporterSummaryLayout, ui.externalReporterSummaryTitle,
        ui.externalReporterSummaryLabel);

    for (QWidget* widget : {
             ui.sdkSummaryIcon, ui.sdkSummaryTitle, ui.sdkSummaryLabel,
             ui.versionSummaryIcon, ui.versionSummaryTitle, ui.versionSummaryLabel,
             ui.featuresSummaryStatus, ui.featuresSummaryTitle, ui.featuresSummaryLabel,
             ui.advancedOptionsSummaryStatus, ui.advancedOptionsSummaryTitle, ui.advancedOptionsSummaryLabel,
             ui.cacheSummaryStatus, ui.cacheSummaryTitle, ui.cacheSummaryLabel,
             ui.externalReporterSummaryStatus, ui.externalReporterSummaryTitle, ui.externalReporterSummaryLabel,
         }) {
        widget->setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    ui.cacheKeepModeBox->addItem("None", SENTRY_CACHE_KEEP_NONE);
    ui.cacheKeepModeBox->addItem("Offline", SENTRY_CACHE_KEEP_OFFLINE);
    ui.cacheKeepModeBox->addItem("Always", SENTRY_CACHE_KEEP_ALWAYS);
    ui.initEnvironmentEdit->setInsertPolicy(QComboBox::NoInsert);
    if (QLineEdit* edit = ui.initEnvironmentEdit->lineEdit())
        edit->setPlaceholderText("production");
    ui.loggerLevelBox->addItem("None", kLoggerLevelNone);
    ui.loggerLevelBox->addItem("Trace", SENTRY_LEVEL_TRACE);
    ui.loggerLevelBox->addItem("Debug", SENTRY_LEVEL_DEBUG);
    ui.loggerLevelBox->addItem("Info", SENTRY_LEVEL_INFO);
    ui.loggerLevelBox->addItem("Warning", SENTRY_LEVEL_WARNING);
    ui.loggerLevelBox->addItem("Error", SENTRY_LEVEL_ERROR);
    ui.loggerLevelBox->addItem("Fatal", SENTRY_LEVEL_FATAL);

    auto makeEllipsisIcon = [](qreal dpr) {
        const int size = 16;
        QColor color = QApplication::palette().color(QPalette::ButtonText);
        color.setAlpha(180);
        QPixmap pixmap(size * dpr, size * dpr);
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        for (int x : { 5, 8, 11 })
            p.drawEllipse(QPointF(x, size / 2.0), 1.2, 1.2);
        return QIcon(pixmap);
    };
    auto makeClearIcon = [](qreal dpr) {
        const int size = 16;
        QColor color = QApplication::palette().color(QPalette::ButtonText);
        color.setAlpha(180);
        QPixmap pixmap(size * dpr, size * dpr);
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(5, 5), QPointF(11, 11));
        p.drawLine(QPointF(11, 5), QPointF(5, 11));
        return QIcon(pixmap);
    };
    auto makeChevronIcon = [](qreal dpr, bool expanded) {
        const int size = 12;
        QPixmap pixmap(size * dpr, size * dpr);
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        QColor color = QApplication::palette().color(QPalette::ButtonText);
        color.setAlpha(150);
        p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (expanded) {
            p.drawLine(QPointF(3.5, 4.5), QPointF(6, 7));
            p.drawLine(QPointF(6, 7), QPointF(8.5, 4.5));
        } else {
            p.drawLine(QPointF(4.5, 3.5), QPointF(7, 6));
            p.drawLine(QPointF(7, 6), QPointF(4.5, 8.5));
        }
        return QIcon(pixmap);
    };
    const char* kDisclosureButton =
        "QToolButton { background: transparent; border: none; padding: 0; }";
    auto setupDisclosureButton = [this, kDisclosureButton, makeChevronIcon](QToolButton* button, const char* settingKey) {
        button->setFixedSize(14, 20);
        button->setIconSize(QSize(12, 12));
        button->setStyleSheet(kDisclosureButton);
        button->setChecked(QSettings().value(settingKey, false).toBool());
        button->setIcon(makeChevronIcon(devicePixelRatioF(), button->isChecked()));
        button->setToolTip(button->isChecked() ? "Collapse" : "Expand");
        QObject::connect(button, &QAbstractButton::toggled, this,
            [this, button, settingKey, makeChevronIcon](bool expanded) {
                QSettings().setValue(settingKey, expanded);
                button->setIcon(makeChevronIcon(devicePixelRatioF(), expanded));
                button->setToolTip(expanded ? "Collapse" : "Expand");
                updateInitDetailsVisibility();
            });
    };
    setupDisclosureButton(ui.versionEditButton, "init/versionExpanded");
    setupDisclosureButton(ui.sdkEditButton, "init/sdkExpanded");
    setupDisclosureButton(ui.featuresEditButton, "init/featuresExpanded");
    setupDisclosureButton(ui.advancedOptionsEditButton, "init/advancedOptionsExpanded");
    setupDisclosureButton(ui.cacheEditButton, "init/cacheOptionsExpanded");
    setupDisclosureButton(ui.externalReporterEditButton, "init/externalReporterOptionsExpanded");

    QAction* databaseBrowseAction = ui.databasePathEdit->addAction(
        makeEllipsisIcon(devicePixelRatioF()), QLineEdit::TrailingPosition);
    databaseBrowseAction->setToolTip("Browse");
    QAction* clearDatabaseAction = ui.databasePathEdit->addAction(
        makeClearIcon(devicePixelRatioF()), QLineEdit::TrailingPosition);
    clearDatabaseAction->setObjectName("databasePathClearAction");
    clearDatabaseAction->setToolTip("Clear");
    auto updateDatabaseClearAction = [this, clearDatabaseAction]() {
        clearDatabaseAction->setEnabled(!ui.databasePathEdit->text().isEmpty());
    };
    QObject::connect(ui.databasePathEdit, &QLineEdit::textChanged, this,
        [updateDatabaseClearAction](const QString&) { updateDatabaseClearAction(); });
    QObject::connect(clearDatabaseAction, &QAction::triggered, ui.databasePathEdit, &QLineEdit::clear);
    updateDatabaseClearAction();
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
        makeEllipsisIcon(devicePixelRatioF()), QLineEdit::TrailingPosition);
    browseAction->setToolTip("Browse");
    QAction* clearReporterAction = ui.externalReporterPathEdit->addAction(
        makeClearIcon(devicePixelRatioF()), QLineEdit::TrailingPosition);
    clearReporterAction->setObjectName("externalReporterClearAction");
    clearReporterAction->setToolTip("Clear");
    auto updateReporterClearAction = [this, clearReporterAction]() {
        const bool enabled = ui.externalCrashReporterBox->isChecked();
        clearReporterAction->setEnabled(enabled && !ui.externalReporterPathEdit->text().isEmpty());
    };
    QObject::connect(ui.externalReporterPathEdit, &QLineEdit::textChanged, this,
        [updateReporterClearAction](const QString&) { updateReporterClearAction(); });
    QObject::connect(clearReporterAction, &QAction::triggered, ui.externalReporterPathEdit, &QLineEdit::clear);
    updateReporterClearAction();

    auto updateReporterControls = [this, browseAction, clearReporterAction](bool enabled) {
        ui.externalReporterPathEdit->setReadOnly(!enabled);
        browseAction->setEnabled(enabled);
        clearReporterAction->setEnabled(enabled && !ui.externalReporterPathEdit->text().isEmpty());
    };
    QObject::connect(ui.externalCrashReporterBox, &QAbstractButton::toggled, this, updateReporterControls);
    updateReporterControls(false);
    QObject::connect(ui.initReleaseEdit, &QLineEdit::textChanged, this, &SentryWindow::updateInitSummaries);
    QObject::connect(ui.initEnvironmentEdit, &QComboBox::currentTextChanged,
        this, &SentryWindow::updateInitSummaries);
    QObject::connect(ui.initDistEdit, &QLineEdit::textChanged, this, &SentryWindow::updateInitSummaries);
    QObject::connect(ui.databasePathEdit, &QLineEdit::textChanged, this, &SentryWindow::updateInitSummaries);
    QObject::connect(ui.tracesSampleRateBox, qOverload<double>(&QDoubleSpinBox::valueChanged),
        this, &SentryWindow::updateInitSummaries);
    QObject::connect(ui.maxBreadcrumbsBox, qOverload<int>(&QSpinBox::valueChanged),
        this, &SentryWindow::updateInitSummaries);
    QObject::connect(ui.maxSpansBox, qOverload<int>(&QSpinBox::valueChanged),
        this, &SentryWindow::updateInitSummaries);
    QObject::connect(ui.shutdownTimeoutBox, qOverload<int>(&QSpinBox::valueChanged),
        this, &SentryWindow::updateInitSummaries);
    QObject::connect(ui.cacheKeepModeBox, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &SentryWindow::updateInitSummaries);
    QObject::connect(ui.cacheMaxItemsBox, qOverload<int>(&QSpinBox::valueChanged),
        this, &SentryWindow::updateInitSummaries);
    QObject::connect(ui.cacheMaxSizeBox, qOverload<int>(&QSpinBox::valueChanged),
        this, &SentryWindow::updateInitSummaries);
    QObject::connect(ui.cacheMaxAgeBox, qOverload<int>(&QSpinBox::valueChanged),
        this, &SentryWindow::updateInitSummaries);
    QObject::connect(ui.loggerLevelBox, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &SentryWindow::updateInitSummaries);
    QObject::connect(ui.externalReporterPathEdit, &QLineEdit::textChanged,
        this, &SentryWindow::updateInitSummaries);
    for (QCheckBox* box : {
             ui.attachScreenshotBox,
             ui.requireUserConsentBox,
             ui.systemCrashReporterBox,
             ui.externalCrashReporterBox,
             ui.enableLargeAttachmentsBox,
             ui.httpRetryBox,
         }) {
        QObject::connect(box, &QAbstractButton::toggled, this, &SentryWindow::updateInitSummaries);
    }
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
        const SentryPlayground::InitOptions options = initOptionsFromPage();
        rememberInitEnvironment(options.environment);
        SentryPlayground::open(options);
    });

    populateInitPage();
    applyInitPaletteStyles();
    updateInitDetailsVisibility();
}

void SentryWindow::setupWheelScrolling()
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
        ui.maxSpansBox,
        ui.shutdownTimeoutBox,
        ui.attachScreenshotBox,
        ui.requireUserConsentBox,
        ui.systemCrashReporterBox,
        ui.externalCrashReporterBox,
        ui.enableLargeAttachmentsBox,
        ui.httpRetryBox,
        ui.cacheKeepModeBox,
        ui.cacheMaxItemsBox,
        ui.cacheMaxSizeBox,
        ui.cacheMaxAgeBox,
        ui.loggerLevelBox,
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
    populateInitEnvironmentHistory(options.environment);
    ui.initDistEdit->setText(options.dist);
    ui.tracesSampleRateBox->setValue(options.tracesSampleRate);
    ui.maxBreadcrumbsBox->setValue(options.maxBreadcrumbs);
    ui.maxSpansBox->setValue(options.maxSpans);
    ui.shutdownTimeoutBox->setValue(options.shutdownTimeout);
    ui.attachScreenshotBox->setChecked(options.attachScreenshot);
    ui.requireUserConsentBox->setChecked(options.requireUserConsent);
    ui.systemCrashReporterBox->setChecked(options.systemCrashReporterEnabled);
    ui.externalCrashReporterBox->setChecked(options.externalCrashReporterEnabled);
    ui.enableLargeAttachmentsBox->setChecked(options.enableLargeAttachments);
    ui.httpRetryBox->setChecked(options.httpRetry);
    const int cacheKeepModeIndex = ui.cacheKeepModeBox->findData(options.cacheKeepMode);
    ui.cacheKeepModeBox->setCurrentIndex(cacheKeepModeIndex >= 0
        ? cacheKeepModeIndex
        : ui.cacheKeepModeBox->findData(SENTRY_CACHE_KEEP_OFFLINE));
    ui.cacheMaxItemsBox->setValue(options.cacheMaxItems);
    ui.cacheMaxSizeBox->setValue(options.cacheMaxSize);
    ui.cacheMaxAgeBox->setValue(options.cacheMaxAge);
    const int loggerLevelIndex = options.debug
        ? ui.loggerLevelBox->findData(options.loggerLevel)
        : ui.loggerLevelBox->findData(kLoggerLevelNone);
    ui.loggerLevelBox->setCurrentIndex(loggerLevelIndex >= 0
        ? loggerLevelIndex
        : ui.loggerLevelBox->findData(kLoggerLevelNone));
    ui.externalReporterPathEdit->setText(options.externalCrashReporterPath);
    for (QAction* action : ui.externalReporterPathEdit->actions()) {
        action->setEnabled(options.externalCrashReporterEnabled);
        if (action->objectName() == "externalReporterClearAction")
            action->setEnabled(options.externalCrashReporterEnabled
                && !ui.externalReporterPathEdit->text().isEmpty());
    }
    for (QAction* action : ui.databasePathEdit->actions()) {
        if (action->objectName() == "databasePathClearAction")
            action->setEnabled(!ui.databasePathEdit->text().isEmpty());
    }
    ui.externalReporterPathEdit->setReadOnly(!options.externalCrashReporterEnabled);
    ui.initializeButton->setText(SentryPlayground::instance()->hasInitialized()
        ? "Re-initialize"
        : "Initialize");
    updateInitSummaries();
    updateInitDetailsVisibility();

    qDeleteAll(blockers);
}

void SentryWindow::populateInitEnvironmentHistory(const QString& currentEnvironment)
{
    ui.initEnvironmentEdit->clear();
    ui.initEnvironmentEdit->addItems(environmentHistoryWith(
        QSettings().value("init/environmentHistory").toStringList(),
        currentEnvironment));
    ui.initEnvironmentEdit->setEditText(currentEnvironment);
}

void SentryWindow::rememberInitEnvironment(const QString& environment)
{
    QSettings settings;
    const QStringList history = environmentHistoryWith(
        settings.value("init/environmentHistory").toStringList(),
        environment);
    settings.setValue("init/environmentHistory", history);

    QSignalBlocker blocker(ui.initEnvironmentEdit);
    ui.initEnvironmentEdit->clear();
    ui.initEnvironmentEdit->addItems(history);
    ui.initEnvironmentEdit->setEditText(environment);
}

SentryPlayground::InitOptions SentryWindow::initOptionsFromPage() const
{
    SentryPlayground::InitOptions options = SentryPlayground::instance()->initOptions();
    options.dsn = ui.dsnEdit->text().trimmed();
    options.databasePath = ui.databasePathEdit->text();
    options.release = ui.initReleaseEdit->text().trimmed();
    options.environment = ui.initEnvironmentEdit->currentText().trimmed();
    options.dist = ui.initDistEdit->text().trimmed();
    options.tracesSampleRate = ui.tracesSampleRateBox->value();
    options.maxBreadcrumbs = ui.maxBreadcrumbsBox->value();
    options.maxSpans = ui.maxSpansBox->value();
    options.shutdownTimeout = ui.shutdownTimeoutBox->value();
    options.attachScreenshot = ui.attachScreenshotBox->isChecked();
    options.requireUserConsent = ui.requireUserConsentBox->isChecked();
    options.systemCrashReporterEnabled = ui.systemCrashReporterBox->isChecked();
    options.enableLargeAttachments = ui.enableLargeAttachmentsBox->isChecked();
    options.httpRetry = ui.httpRetryBox->isChecked();
    options.cacheKeepMode = ui.cacheKeepModeBox->currentData().toInt();
    options.cacheMaxItems = ui.cacheMaxItemsBox->value();
    options.cacheMaxSize = ui.cacheMaxSizeBox->value();
    options.cacheMaxAge = ui.cacheMaxAgeBox->value();
    const int loggerLevel = ui.loggerLevelBox->currentData().toInt();
    options.debug = loggerLevel != kLoggerLevelNone;
    options.loggerLevel = options.debug ? loggerLevel : SENTRY_LEVEL_DEBUG;
    options.externalCrashReporterEnabled = ui.externalCrashReporterBox->isChecked();
    options.externalCrashReporterPath = ui.externalReporterPathEdit->text();
    return options;
}

void SentryWindow::updateInitDetailsVisibility()
{
    const bool dsnVisible = ui.sdkEditButton->isChecked();
    const bool versionVisible = ui.versionEditButton->isChecked();
    const bool featuresVisible = ui.featuresEditButton->isChecked();
    const bool advancedVisible = ui.advancedOptionsEditButton->isChecked();
    const bool cacheVisible = ui.cacheEditButton->isChecked();
    const bool reporterVisible = ui.externalReporterEditButton->isChecked();

    auto setFormVisible = [](QFormLayout* layout, bool visible) {
        QMargins margins = layout->contentsMargins();
        margins.setTop(visible ? 8 : 0);
        margins.setBottom(visible ? 10 : 0);
        layout->setContentsMargins(margins);
        layout->setVerticalSpacing(visible ? 10 : 0);
        for (int row = 0; row < layout->rowCount(); ++row)
            layout->setRowVisible(row, visible);
        layout->invalidate();
    };
    setFormVisible(ui.sdkDetailsFormLayout, dsnVisible);
    setFormVisible(ui.versionDetailsFormLayout, versionVisible);
    setFormVisible(ui.featuresDetailsFormLayout, featuresVisible);
    setFormVisible(ui.advancedOptionsDetailsFormLayout, advancedVisible);
    setFormVisible(ui.cacheDetailsFormLayout, cacheVisible);
    setFormVisible(ui.externalReporterDetailsFormLayout, reporterVisible);

    ui.initRightColumn->invalidate();
    ui.initScrollContents->updateGeometry();
}

void SentryWindow::applyInitPaletteStyles()
{
    const QColor textColor = palette().color(QPalette::WindowText);
    const QColor paneColor = blendedColor(palette().color(QPalette::Window), textColor, 10);

    ui.initBackendLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-weight: bold; background-color: %2; border: none; border-radius: 5px; padding: 1px 7px;")
            .arg(cssRgba(textColor, 170), cssRgba(textColor, 24)));
    ui.backendLabel->setStyleSheet(ui.initBackendLabel->styleSheet());

    ui.initScrollBackdrop->setStyleSheet(QStringLiteral(
        "QFrame#initScrollBackdrop {"
        " background-color: %1;"
        " border: 1px solid %2;"
        " border-radius: 6px;"
        "}")
            .arg(cssRgb(paneColor), cssRgba(textColor, 38)));
    const QString dividerStyle = QStringLiteral(
        "QFrame { background-color: %1; border: none; }").arg(cssRgba(textColor, 26));
    QWidget* const sectionDividers[] = {
        ui.sdkSectionDivider,
        ui.versionSectionDivider,
        ui.featuresSectionDivider,
        ui.parametersSectionDivider,
        ui.databaseSectionDivider,
    };
    for (QWidget* divider : sectionDividers)
        divider->setStyleSheet(dividerStyle);
    ui.initScrollArea->setFrameShape(QFrame::NoFrame);
    ui.initScrollArea->setFrameShadow(QFrame::Plain);

    QPalette panePalette = palette();
    panePalette.setColor(QPalette::Window, paneColor);
    panePalette.setColor(QPalette::Base, paneColor);
    QWidget* const transparentPaneWidgets[] = {
             ui.initScrollArea,
             ui.initScrollArea->viewport(),
             ui.initScrollContents,
             ui.initSummarySheet,
    };
    for (QWidget* widget : transparentPaneWidgets) {
        widget->setPalette(panePalette);
        widget->setAutoFillBackground(false);
    }
    updateInitSummaries();
}

void SentryWindow::updateInitSummaries()
{
    const QColor textColor = palette().color(QPalette::WindowText);
    const QString inactiveStatusStyle = QStringLiteral("color: %1; font-weight: bold;")
        .arg(cssRgba(textColor, 115));
    auto setStatus = [&inactiveStatusStyle](QLabel* label, bool enabled) {
        label->setText(enabled ? QStringLiteral("✓") : QStringLiteral("-"));
        label->setStyleSheet(enabled
            ? QStringLiteral("color: #21c26a; font-weight: bold;")
            : inactiveStatusStyle);
    };

    QString dsnSummary;
    const QUrl dsnUrl(ui.dsnEdit->text().trimmed());
    dsnSummary = dsnUrl.host();
    if (!dsnSummary.isEmpty() && dsnUrl.port() >= 0)
        dsnSummary += QStringLiteral(":%1").arg(dsnUrl.port());
    ui.sdkSummaryLabel->setText(dsnSummary.isEmpty()
                                    ? QStringLiteral("N/A")
                                    : dsnSummary);
    setStatus(ui.sdkSummaryIcon, !dsnSummary.isEmpty());

    QStringList versionParts;
    const QString release = ui.initReleaseEdit->text().trimmed();
    const QString environment = ui.initEnvironmentEdit->currentText().trimmed();
    const QString dist = ui.initDistEdit->text().trimmed();
    if (!release.isEmpty())
        versionParts.append(release);
    if (!environment.isEmpty())
        versionParts.append(environment);
    if (!dist.isEmpty())
        versionParts.append(dist);
    ui.versionSummaryLabel->setText(versionParts.isEmpty()
            ? QStringLiteral("N/A")
            : versionParts.join(QStringLiteral(", ")));
    setStatus(ui.versionSummaryIcon, !versionParts.isEmpty());

    QStringList features;
    if (ui.requireUserConsentBox->isChecked())
        features.append(QStringLiteral("consent"));
    if (ui.httpRetryBox->isChecked())
        features.append(QStringLiteral("retry"));
    if (ui.attachScreenshotBox->isChecked())
        features.append(QStringLiteral("screenshot"));
    if (ui.enableLargeAttachmentsBox->isChecked())
        features.append(QStringLiteral("large attachments"));
    ui.featuresSummaryLabel->setText(features.isEmpty()
            ? QStringLiteral("N/A")
            : features.join(QStringLiteral(", ")));
    setStatus(ui.featuresSummaryStatus, !features.isEmpty());

    ui.advancedOptionsSummaryLabel->setText(
        QStringLiteral("sample %1x, %2 crumbs, %3 spans, %4")
            .arg(QLocale::c().toString(ui.tracesSampleRateBox->value(), 'f', 2))
            .arg(ui.maxBreadcrumbsBox->value())
            .arg(ui.maxSpansBox->value())
            .arg(ui.loggerLevelBox->currentData().toInt() != kLoggerLevelNone
                     ? ui.loggerLevelBox->currentText().toLower()
                     : QString()));
    setStatus(ui.advancedOptionsSummaryStatus, true);

    QStringList databaseParts;
    const QString databasePath = ui.databasePathEdit->text().trimmed();
    if (!databasePath.isEmpty()) {
        databaseParts += QFileInfo(databasePath).fileName();
    }
    if (ui.cacheKeepModeBox->currentIndex() > 0) {
        databaseParts += QString("cache %1").arg(ui.cacheKeepModeBox->currentText().toLower());
        if (ui.cacheMaxItemsBox->value() > 0) {
            databaseParts += QString("%1 items").arg(ui.cacheMaxItemsBox->value());
        }
        if (ui.cacheMaxSizeBox->value() > 0) {
            databaseParts += QLocale().formattedDataSize(ui.cacheMaxSizeBox->value());
        }
        if (ui.cacheMaxAgeBox->value() > 0) {
            databaseParts += QStringLiteral("%1 s").arg(ui.cacheMaxAgeBox->value());
        }
    }
    ui.cacheSummaryLabel->setText(databaseParts.join(", "));
    setStatus(ui.cacheSummaryStatus, true);

    QStringList reporterParts;
    if (ui.systemCrashReporterBox->isChecked())
        reporterParts.append(QStringLiteral("system"));

    const bool externalReporterEnabled = ui.externalCrashReporterBox->isChecked();
    if (externalReporterEnabled) {
        const QString path = ui.externalReporterPathEdit->text().trimmed();
        reporterParts.append(path.isEmpty()
            ? QStringLiteral("external")
            : QFileInfo(path).fileName());
    }

    QString reporterSummary = QStringLiteral("N/A");
    if (!reporterParts.isEmpty()) {
        reporterSummary = reporterParts.join(QStringLiteral(", "));
    }
    ui.externalReporterSummaryLabel->setText(reporterSummary);
    setStatus(ui.externalReporterSummaryStatus, !reporterParts.isEmpty());
}

void SentryWindow::showInitPage()
{
    populateInitPage();
    ui.leftStack->setCurrentWidget(ui.initLeftPanel);
    ui.pages->setCurrentWidget(ui.initPage);
    statusBar()->hide();
}

void SentryWindow::showRuntimePage()
{
    ui.leftStack->setCurrentWidget(ui.runtimeLeftPanel);
    ui.pages->setCurrentWidget(ui.runtimePage);
    statusBar()->show();
}

void SentryWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange) {
        applyInitPaletteStyles();
        updateLogo();
    }
    QMainWindow::changeEvent(event);
}

bool SentryWindow::eventFilter(QObject *watched, QEvent *event)
{
    auto* summaryRow = qobject_cast<QWidget*>(watched);
    if (summaryRow && summaryRow->property("initSummaryRow").toBool()) {
        if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
            const bool hovered = event->type() == QEvent::Enter;
            QPalette rowPalette = summaryRow->palette();
            rowPalette.setColor(QPalette::Window, blendedColor(
                palette().color(QPalette::Window), palette().color(QPalette::WindowText), 18));
            summaryRow->setPalette(rowPalette);
            summaryRow->setAutoFillBackground(hovered);
        } else if (event->type() == QEvent::MouseButtonRelease) {
            const QString name = summaryRow->objectName();
            if (name == QLatin1String("versionSummaryWidget")) {
                ui.versionEditButton->toggle();
                return true;
            }
            if (name == QLatin1String("sdkSummaryWidget")) {
                ui.sdkEditButton->toggle();
                return true;
            }
            if (name == QLatin1String("featuresSummaryWidget")) {
                ui.featuresEditButton->toggle();
                return true;
            }
            if (name == QLatin1String("advancedOptionsSummaryWidget")) {
                ui.advancedOptionsEditButton->toggle();
                return true;
            }
            if (name == QLatin1String("cacheSummaryWidget")) {
                ui.cacheEditButton->toggle();
                return true;
            }
            if (name == QLatin1String("externalReporterSummaryWidget")) {
                ui.externalReporterEditButton->toggle();
                return true;
            }
        }
    }

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

void SentryWindow::updateLogo()
{
    bool isDark = qApp->palette().window().color().lightness() < 128;
    QPixmap logo(isDark ? ":/assets/sentry-glyph-light.png" : ":/assets/sentry-glyph-dark.png");
    ui.sentryLogo->setPixmap(logo);
    if (ui.initLogo)
        ui.initLogo->setPixmap(logo);
}
