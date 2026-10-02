#include "runtimepane.h"
#include "playground.h"
#include "style.h"

#include <sentry.h>

#include <functional>

#include <QtCore/qfileinfo.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qlocale.h>
#include <QtCore/qobject.h>
#include <QtCore/qsettings.h>
#include <QtCore/qstandardpaths.h>
#include <QtGui/qaction.h>
#include <QtWidgets/qabstractbutton.h>
#include <QtWidgets/qbuttongroup.h>
#include <QtWidgets/qfiledialog.h>
#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qstackedwidget.h>
#include <QtWidgets/qtreewidget.h>

static void setupTree(QTreeWidget* tree, int narrowColumn, QHeaderView::ResizeMode narrowMode,
    int narrowWidth = -1)
{
    int stretchColumn = 1 - narrowColumn;
    tree->header()->setStretchLastSection(false);
    tree->header()->setSectionResizeMode(narrowColumn, narrowMode);
    tree->header()->setSectionResizeMode(stretchColumn, QHeaderView::Stretch);
    if (narrowWidth > 0)
        tree->header()->resizeSection(narrowColumn, narrowWidth);
    tree->setRootIsDecorated(false);
    tree->setUniformRowHeights(true);
    tree->setAllColumnsShowFocus(true);
}

static void addEmptyRow(QTreeWidget* tree)
{
    auto* item = new QTreeWidgetItem(tree);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    tree->editItem(item, 0);
}

static bool parseAttributes(QLineEdit* edit, QJsonObject* attributes = nullptr)
{
    const QString hint = "Optional JSON object with string, number, or boolean values";
    const QByteArray text = edit->text().trimmed().toUtf8();
    if (text.isEmpty()) {
        edit->setToolTip(hint);
        if (attributes)
            *attributes = {};
        return true;
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(text, &error);
    if (error.error != QJsonParseError::NoError) {
        edit->setToolTip(error.errorString());
        return false;
    }
    if (!document.isObject()) {
        edit->setToolTip(hint);
        return false;
    }
    const QJsonObject object = document.object();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (it.key().trimmed().isEmpty()
            || !(it.value().isString() || it.value().isDouble() || it.value().isBool())) {
            edit->setToolTip("Attributes need non-empty keys and string, number, or boolean values");
            return false;
        }
    }
    edit->setToolTip(hint);
    if (attributes)
        *attributes = object;
    return true;
}

RuntimePane::RuntimePane(QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);

    auto* playground = Playground::instance();

    ui.messageLevelBox->addItem("Debug", SENTRY_LEVEL_DEBUG);
    ui.messageLevelBox->addItem("Info", SENTRY_LEVEL_INFO);
    ui.messageLevelBox->addItem("Warning", SENTRY_LEVEL_WARNING);
    ui.messageLevelBox->addItem("Error", SENTRY_LEVEL_ERROR);
    ui.messageLevelBox->addItem("Fatal", SENTRY_LEVEL_FATAL);
    ui.messageLevelBox->setCurrentIndex(1);

    for (const char* type : { "default", "debug", "info", "navigation", "http", "query",
             "transaction", "ui", "user", "error" })
        ui.breadcrumbTypeBox->addItem(type);
    for (const char* type : { "std::exception", "std::runtime_error", "std::logic_error",
             "std::invalid_argument", "std::out_of_range", "std::bad_alloc", "Error" })
        ui.exceptionTypeBox->addItem(type);

    const int typeBoxWidth = qMax(
        ui.breadcrumbTypeBox->sizeHint().width(), ui.exceptionTypeBox->sizeHint().width());
    ui.breadcrumbTypeBox->setFixedWidth(typeBoxWidth);
    ui.exceptionTypeBox->setFixedWidth(typeBoxWidth);

    auto* messageGroup = new QButtonGroup(this);
    messageGroup->setExclusive(true);
    messageGroup->addButton(ui.messageButton);
    messageGroup->addButton(ui.exceptionButton);
    messageGroup->addButton(ui.breadcrumbButton);

    ui.breadcrumbTypeBox->hide();
    ui.exceptionTypeBox->hide();
    connect(ui.messageButton, &QAbstractButton::clicked, this, [this]() {
        ui.breadcrumbTypeBox->hide();
        ui.exceptionTypeBox->hide();
    });
    connect(ui.breadcrumbButton, &QAbstractButton::clicked, this, [this]() {
        ui.breadcrumbTypeBox->show();
        ui.exceptionTypeBox->hide();
    });
    connect(ui.exceptionButton, &QAbstractButton::clicked, this, [this]() {
        ui.breadcrumbTypeBox->hide();
        ui.exceptionTypeBox->show();
    });

    m_messageAction = ui.messageText->addAction(
        Style::makeArrowIcon(palette(), devicePixelRatioF()), QLineEdit::TrailingPosition);
    auto triggerSend = [this, playground]() {
        const int level = ui.messageLevelBox->currentData().toInt();
        if (ui.messageButton->isChecked()) {
            playground->captureMessage(level, ui.messageText->text());
        } else if (ui.breadcrumbButton->isChecked()) {
            playground->addBreadcrumb(ui.breadcrumbTypeBox->currentText(), level, ui.messageText->text());
        } else {
            playground->captureException(
                level, ui.exceptionTypeBox->currentText(), ui.messageText->text());
        }
    };
    connect(m_messageAction, &QAction::triggered, this, triggerSend);
    connect(ui.messageText, &QLineEdit::returnPressed, this, triggerSend);
    connect(ui.messageText, &QLineEdit::textChanged, this,
        [this](const QString&) { updateMessageAction(); });
    updateMessageAction();

    auto* categoryGroup = new QButtonGroup(this);
    categoryGroup->setExclusive(true);
    categoryGroup->addButton(ui.tagsButton);
    categoryGroup->addButton(ui.contextsButton);
    categoryGroup->addButton(ui.attachmentsButton);

    ui.tagsButton->setChecked(true);
    ui.categoryStack->setCurrentIndex(0);
    connect(ui.tagsButton, &QAbstractButton::clicked, this,
        [this]() { ui.categoryStack->setCurrentIndex(0); });
    connect(ui.contextsButton, &QAbstractButton::clicked, this,
        [this]() { ui.categoryStack->setCurrentIndex(1); });
    connect(ui.attachmentsButton, &QAbstractButton::clicked, this,
        [this]() { ui.categoryStack->setCurrentIndex(2); });

    setupTree(ui.tagsTable, 0, QHeaderView::Interactive, 100);
    setupTree(ui.contextsTable, 0, QHeaderView::Interactive, 100);
    setupTree(ui.attachmentTable, 1, QHeaderView::ResizeToContents);

    auto wireKeyValueTree = [this](QTreeWidget* tree,
        std::function<void(const QString&, const QString&)> onSet,
        std::function<void(const QString&)> onRemove) {
        connect(tree, &QTreeWidget::itemChanged, this,
            [onSet, onRemove](QTreeWidgetItem* item, int) {
                const QString key = item->text(0).trimmed();
                const QString value = item->text(1);
                const QString oldKey = item->data(0, Qt::UserRole).toString();
                if (!oldKey.isEmpty() && oldKey != key)
                    onRemove(oldKey);
                if (!key.isEmpty()) {
                    onSet(key, value);
                    item->setData(0, Qt::UserRole, key);
                }
            });
        connect(tree, &QWidget::customContextMenuRequested, this,
            [this, tree, onRemove](const QPoint& pos) {
                auto* item = tree->itemAt(pos);
                if (!item)
                    return;
                QMenu menu(this);
                menu.addAction("Remove", [tree, item, onRemove]() {
                    const QString oldKey = item->data(0, Qt::UserRole).toString();
                    if (!oldKey.isEmpty())
                        onRemove(oldKey);
                    delete tree->takeTopLevelItem(tree->indexOfTopLevelItem(item));
                });
                menu.exec(tree->viewport()->mapToGlobal(pos));
            });
    };
    wireKeyValueTree(ui.tagsTable,
        [playground](const QString& key, const QString& value) { playground->setTag(key, value); },
        [playground](const QString& key) { playground->removeTag(key); });
    wireKeyValueTree(ui.contextsTable,
        [playground](const QString& key, const QString& value) { playground->setContext(key, value); },
        [playground](const QString& key) { playground->removeContext(key); });

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

    refreshAttachments();
    connect(playground, &Playground::attachmentsChanged, this,
        [this]() { refreshAttachments(); });

    ui.addButton->setFixedSize(22, 22);
    ui.addButton->setText("");
    connect(ui.categoryStack, &QStackedWidget::currentChanged, this,
        [this](int) { updateAddButton(); });
    connect(ui.addButton, &QAbstractButton::clicked, this, [this, playground]() {
        switch (ui.categoryStack->currentIndex()) {
        case 0:
            addEmptyRow(ui.tagsTable);
            break;
        case 1:
            addEmptyRow(ui.contextsTable);
            break;
        case 2: {
            const QString lastDir = QSettings().value("attachmentDir",
                QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
            const QString path = QFileDialog::getOpenFileName(this, "Select attachment", lastDir);
            if (path.isEmpty())
                return;
            QSettings().setValue("attachmentDir", QFileInfo(path).absolutePath());
            playground->addAttachment(path);
            break;
        }
        }
    });
    updateAddButton();

    struct UserField {
        QLineEdit* edit;
        const char* key;
    };
    const QList<UserField> userFields = {
        { ui.userIdEdit, "id" },
        { ui.userNameEdit, "name" },
        { ui.userEmailEdit, "email" },
        { ui.userIpEdit, "ip_address" },
    };
    auto populateUser = [userFields, playground]() {
        const QVariantMap user = playground->user();
        for (const UserField& field : userFields) {
            QSignalBlocker blocker(field.edit);
            if (!field.edit->hasFocus())
                field.edit->setText(user.value(field.key).toString());
        }
    };
    populateUser();
    for (const UserField& field : userFields) {
        connect(field.edit, &QLineEdit::editingFinished, this,
            [field, playground]() { playground->updateUser(field.key, field.edit->text()); });
    }
    connect(playground, &Playground::userChanged, this,
        [populateUser](const QVariantMap&) { populateUser(); });

    ui.releaseEdit->setText(playground->release());
    ui.environmentEdit->setText(playground->environment());
    ui.sessionButton->setFixedSize(22, 22);
    connect(ui.releaseEdit, &QLineEdit::textChanged, this,
        [this](const QString&) { updateSessionButton(); });
    connect(ui.environmentEdit, &QLineEdit::textChanged, this,
        [this](const QString&) { updateSessionButton(); });
    connect(playground, &Playground::releaseChanged, this,
        [this](const QString& release) {
            if (!ui.releaseEdit->hasFocus())
                ui.releaseEdit->setText(release);
            updateSessionButton();
        });
    connect(playground, &Playground::environmentChanged, this,
        [this](const QString& environment) {
            if (!ui.environmentEdit->hasFocus())
                ui.environmentEdit->setText(environment);
            updateSessionButton();
        });
    connect(playground, &Playground::sessionChanged, this,
        [this](bool) { updateSessionButton(); });
    connect(ui.sessionButton, &QAbstractButton::clicked, this, [this, playground]() {
        const bool pending = ui.releaseEdit->text() != playground->release()
            || ui.environmentEdit->text() != playground->environment();
        if (pending) {
            playground->setRelease(ui.releaseEdit->text());
            playground->setEnvironment(ui.environmentEdit->text());
            if (playground->session())
                playground->setSession(false);
            playground->setSession(true);
        } else {
            playground->setSession(!playground->session());
        }
    });
    updateSessionButton();

    connect(ui.attachmentTable, &QWidget::customContextMenuRequested, this,
        [this, playground](const QPoint& pos) {
            auto* item = ui.attachmentTable->itemAt(pos);
            if (!item)
                return;
            QMenu menu(this);
            const QString path = item->data(0, Qt::UserRole).toString();
            menu.addAction("Remove", [playground, path]() { playground->removeAttachment(path); });
            menu.exec(ui.attachmentTable->viewport()->mapToGlobal(pos));
    });

    ui.logLevelBox->addItem("Trace", SENTRY_LEVEL_TRACE);
    ui.logLevelBox->addItem("Debug", SENTRY_LEVEL_DEBUG);
    ui.logLevelBox->addItem("Info", SENTRY_LEVEL_INFO);
    ui.logLevelBox->addItem("Warning", SENTRY_LEVEL_WARNING);
    ui.logLevelBox->addItem("Error", SENTRY_LEVEL_ERROR);
    ui.logLevelBox->addItem("Fatal", SENTRY_LEVEL_FATAL);
    ui.logLevelBox->setCurrentIndex(2);

    m_logAction = ui.logText->addAction(
        Style::makeArrowIcon(palette(), devicePixelRatioF()), QLineEdit::TrailingPosition);
    auto captureLog = [this, playground]() {
        QJsonObject attributes;
        if (m_logAction->isEnabled() && parseAttributes(ui.logAttributesEdit, &attributes))
            playground->captureLog(ui.logLevelBox->currentData().toInt(),
                ui.logText->text(), attributes);
    };
    connect(m_logAction, &QAction::triggered, this, captureLog);
    for (QLineEdit* edit : { ui.logText, ui.logAttributesEdit }) {
        connect(edit, &QLineEdit::returnPressed, this, captureLog);
        connect(edit, &QLineEdit::textChanged, this, &RuntimePane::updateLogAction);
    }
    connect(playground, &Playground::initializedChanged, this, &RuntimePane::updateLogAction);
    updateLogAction();

    ui.metricTypeBox->addItem("Count", SENTRY_METRIC_COUNT);
    ui.metricTypeBox->addItem("Gauge", SENTRY_METRIC_GAUGE);
    ui.metricTypeBox->addItem("Distribution", SENTRY_METRIC_DISTRIBUTION);

    const QList<QComboBox*> headerBoxes = { ui.messageLevelBox, ui.logLevelBox, ui.metricTypeBox };
    int headerWidth = 0;
    for (QComboBox* box : headerBoxes) {
        box->ensurePolished();
        headerWidth = qMax(headerWidth, box->sizeHint().width());
    }
    for (QComboBox* box : headerBoxes)
        box->setFixedWidth(headerWidth);

    ui.metricUnitBox->addItem("", "");
    for (const char* unit : { SENTRY_UNIT_NANOSECOND, SENTRY_UNIT_MICROSECOND,
             SENTRY_UNIT_MILLISECOND, SENTRY_UNIT_SECOND, SENTRY_UNIT_BYTE,
             SENTRY_UNIT_KIBIBYTE, SENTRY_UNIT_MEBIBYTE, SENTRY_UNIT_RATIO, SENTRY_UNIT_PERCENT })
        ui.metricUnitBox->addItem(unit);
    ui.metricUnitBox->lineEdit()->setPlaceholderText("Unit (optional)");
    auto updateMetricType = [this]() {
        const bool count = ui.metricTypeBox->currentData().toInt() == SENTRY_METRIC_COUNT;
        ui.metricValueBox->setDecimals(count ? 0 : 6);
        ui.metricUnitBox->setEnabled(!count);
    };
    connect(ui.metricTypeBox, &QComboBox::currentIndexChanged, this, updateMetricType);
    updateMetricType();

    m_metricAction = ui.metricNameEdit->addAction(
        Style::makeArrowIcon(palette(), devicePixelRatioF()), QLineEdit::TrailingPosition);
    auto captureMetric = [this, playground]() {
        QJsonObject attributes;
        if (m_metricAction->isEnabled() && parseAttributes(ui.metricAttributesEdit, &attributes)) {
            ui.metricValueBox->interpretText();
            playground->captureMetric(ui.metricTypeBox->currentData().toInt(),
                ui.metricNameEdit->text(), ui.metricValueBox->value(),
                ui.metricUnitBox->currentText(), attributes);
        }
    };
    connect(m_metricAction, &QAction::triggered, this, captureMetric);
    for (QLineEdit* edit : { ui.metricNameEdit, ui.metricAttributesEdit }) {
        connect(edit, &QLineEdit::returnPressed, this, captureMetric);
        connect(edit, &QLineEdit::textChanged, this, &RuntimePane::updateMetricAction);
    }
    connect(playground, &Playground::initializedChanged, this, &RuntimePane::updateMetricAction);
    updateMetricAction();

    auto* telemetryGroup = new QButtonGroup(this);
    telemetryGroup->setExclusive(true);
    telemetryGroup->addButton(ui.logsButton);
    telemetryGroup->addButton(ui.metricsButton);
    auto updateTelemetry = [this]() {
        const bool logs = ui.logsButton->isChecked();
        ui.telemetryStack->setCurrentIndex(logs ? 0 : 1);
        ui.logLevelBox->setVisible(logs);
        ui.logAttributesEdit->setVisible(logs);
        ui.metricTypeBox->setVisible(!logs);
        ui.metricAttributesEdit->setVisible(!logs);
    };
    connect(ui.logsButton, &QAbstractButton::toggled, this, updateTelemetry);
    updateTelemetry();

    refreshPaletteStyles();
}

void RuntimePane::refreshPaletteStyles()
{
    ui.messageButton->setStyleSheet(Style::segmentedButtonStyle(
        palette(), QStringLiteral("border-top-left-radius: 4px; border-bottom-left-radius: 4px;")));
    ui.exceptionButton->setStyleSheet(Style::segmentedButtonStyle(
        palette(), QStringLiteral("border-left: none;")));
    ui.breadcrumbButton->setStyleSheet(Style::segmentedButtonStyle(
        palette(), QStringLiteral("border-left: none; border-top-right-radius: 4px; border-bottom-right-radius: 4px;")));

    ui.logsButton->setStyleSheet(Style::segmentedButtonStyle(
        palette(), QStringLiteral("border-top-left-radius: 4px; border-bottom-left-radius: 4px;")));
    ui.metricsButton->setStyleSheet(Style::segmentedButtonStyle(
        palette(), QStringLiteral("border-left: none; border-top-right-radius: 4px; border-bottom-right-radius: 4px;")));

    ui.tagsButton->setStyleSheet(Style::segmentedButtonStyle(
        palette(), QStringLiteral("border-top-left-radius: 4px; border-bottom-left-radius: 4px;")));
    ui.contextsButton->setStyleSheet(Style::segmentedButtonStyle(
        palette(), QStringLiteral("border-left: none;")));
    ui.attachmentsButton->setStyleSheet(Style::segmentedButtonStyle(
        palette(), QStringLiteral("border-left: none; border-top-right-radius: 4px; border-bottom-right-radius: 4px;")));
    updateSegmentedButtonWidths();

    ui.addButton->setStyleSheet(Style::circularButtonStyle(palette()));
    ui.addButton->setIcon(Style::makePlusIcon(palette(), devicePixelRatioF()));
    if (m_messageAction)
        m_messageAction->setIcon(Style::makeArrowIcon(palette(), devicePixelRatioF()));
    if (m_logAction)
        m_logAction->setIcon(Style::makeArrowIcon(palette(), devicePixelRatioF()));
    if (m_metricAction)
        m_metricAction->setIcon(Style::makeArrowIcon(palette(), devicePixelRatioF()));
    updateSessionButton();
}

void RuntimePane::updateSegmentedButtonWidths()
{
    const QList<QPushButton*> buttons = {
        ui.messageButton,
        ui.exceptionButton,
        ui.breadcrumbButton,
        ui.tagsButton,
        ui.contextsButton,
        ui.attachmentsButton,
        ui.logsButton,
        ui.metricsButton,
    };

    int segmentedWidth = 0;
    for (auto* button : buttons) {
        button->setMinimumWidth(0);
        button->setMaximumWidth(QWIDGETSIZE_MAX);
        button->ensurePolished();
        button->updateGeometry();
        segmentedWidth = qMax(segmentedWidth, button->sizeHint().width());
    }

    for (auto* button : buttons)
        button->setFixedWidth(segmentedWidth);
}

void RuntimePane::refreshAttachments()
{
    auto* playground = Playground::instance();
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
}

void RuntimePane::updateAddButton()
{
    const bool isAttachments = ui.categoryStack->currentIndex() == 2;
    ui.addButton->setToolTip(isAttachments ? "Add attachment…" : "Add row");
}

void RuntimePane::updateMessageAction()
{
    if (m_messageAction)
        m_messageAction->setEnabled(!ui.messageText->text().isEmpty());
}

void RuntimePane::updateSessionButton()
{
    auto* playground = Playground::instance();
    const bool releasePending = ui.releaseEdit->text() != playground->release();
    const bool envPending = ui.environmentEdit->text() != playground->environment();
    const bool pending = releasePending || envPending;
    const bool active = playground->session();

    QPalette editDefaultPalette = palette();
    QPalette editPendingPalette = editDefaultPalette;
    editPendingPalette.setColor(QPalette::Text, QColor("#ff3b30"));
    ui.releaseEdit->setPalette(releasePending ? editPendingPalette : editDefaultPalette);
    ui.environmentEdit->setPalette(envPending ? editPendingPalette : editDefaultPalette);

    if (pending) {
        ui.sessionButton->setText("⟳");
        ui.sessionButton->setToolTip("Apply and restart session");
        ui.sessionButton->setStyleSheet(Style::circularButtonStyle(
            palette(), QStringLiteral("font-size: 16px; font-weight: bold; color: #ff3b30;")));
    } else {
        ui.sessionButton->setText(active ? "⏹" : "▶");
        ui.sessionButton->setToolTip(active ? "End session" : "Start session");
        ui.sessionButton->setStyleSheet(Style::circularButtonStyle(
            palette(), QStringLiteral("font-size: 16px;")));
    }
}

void RuntimePane::updateLogAction()
{
    const bool valid = parseAttributes(ui.logAttributesEdit);
    m_logAction->setEnabled(Playground::instance()->isInitialized()
        && !ui.logText->text().trimmed().isEmpty() && valid);
    m_logAction->setToolTip(valid ? "Capture log" : ui.logAttributesEdit->toolTip());
}

void RuntimePane::updateMetricAction()
{
    const bool valid = parseAttributes(ui.metricAttributesEdit);
    m_metricAction->setEnabled(Playground::instance()->isInitialized()
        && !ui.metricNameEdit->text().trimmed().isEmpty() && valid);
    m_metricAction->setToolTip(valid ? "Capture metric" : ui.metricAttributesEdit->toolTip());
}
