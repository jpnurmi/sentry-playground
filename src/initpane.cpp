#include "initpane.h"
#include "playground.h"
#include "style.h"

#include <sentry.h>

#include <memory>
#include <vector>

#include <QtCore/qfileinfo.h>
#include <QtCore/qlocale.h>
#include <QtCore/qobject.h>
#include <QtCore/qsettings.h>
#include <QtCore/qstandardpaths.h>
#include <QtGui/qaction.h>
#include <QtGui/qevent.h>
#include <QtWidgets/qabstractbutton.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qcheckbox.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qfiledialog.h>
#include <QtWidgets/qformlayout.h>
#include <QtWidgets/qframe.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qspinbox.h>
#include <QtWidgets/qtoolbutton.h>

static constexpr int kMaxHistoryItems = 12;
static constexpr int kLoggerLevelNone = -1000;

bool supportsNativeOptions()
{
    return Playground::backend() == QStringLiteral("native");
}

bool crashReportingModeUsesMinidump(int mode)
{
    return mode != SENTRY_CRASH_REPORTING_MODE_NATIVE;
}

bool crashReportingModeUsesStackwalk(int mode)
{
    return mode != SENTRY_CRASH_REPORTING_MODE_MINIDUMP;
}

QString crashReportingModeSummaryText(int mode)
{
    switch (mode) {
    case SENTRY_CRASH_REPORTING_MODE_NATIVE:
        return QStringLiteral("native");
    case SENTRY_CRASH_REPORTING_MODE_MINIDUMP:
        return QStringLiteral("minidump");
    case SENTRY_CRASH_REPORTING_MODE_NATIVE_WITH_MINIDUMP:
    default:
        return QStringLiteral("native with minidump");
    }
}

QString minidumpModeSummaryText(int mode)
{
    switch (mode) {
    case SENTRY_MINIDUMP_MODE_STACK_ONLY:
        return QStringLiteral("stack only");
    case SENTRY_MINIDUMP_MODE_FULL:
        return QStringLiteral("full");
    case SENTRY_MINIDUMP_MODE_SMART:
    default:
        return QStringLiteral("smart");
    }
}

static QStringList historyWith(QStringList history, const QString& current)
{
    QStringList normalized;
    auto add = [&normalized](const QString& value) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty() && !normalized.contains(trimmed, Qt::CaseSensitive))
            normalized.append(trimmed);
    };
    add(current);
    for (const QString& value : std::as_const(history))
        add(value);
    while (normalized.size() > kMaxHistoryItems)
        normalized.removeLast();
    return normalized;
}

static void populateHistory(QComboBox* box, const QString& key, const QString& current)
{
    QStringList items;
    for (int i = 0; i < box->count(); ++i)
        items.append(box->itemText(i));
    items.append(QSettings().value(key).toStringList());

    box->clear();
    box->addItems(historyWith(items, current));
    box->setEditText(current);
}

static void rememberHistory(QComboBox* box, const QString& key, const QString& current)
{
    QStringList items;
    for (int i = 0; i < box->count(); ++i)
        items.append(box->itemText(i));
    const QStringList history = historyWith(items, current);
    QSettings().setValue(key, history);

    QSignalBlocker blocker(box);
    box->clear();
    box->addItems(history);
    box->setEditText(current);
}

InitPane::InitPane(QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    ui.rightColumn->setAlignment(Qt::AlignTop);
    ui.scrollBackdrop->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui.scrollBackdrop->lower();

    setupSummaryRows();
    setupFormAlignment();
    setupControls();
    populate();
    refreshPaletteStyles();
    updateDetailsVisibility();
}

void InitPane::setupSummaryRows()
{
    auto wrapSummaryRow = [this](QHBoxLayout* layout, const char* objectName) {
        int rowIndex = -1;
        for (int i = 0; i < ui.summarySheetLayout->count(); ++i) {
            if (ui.summarySheetLayout->itemAt(i)->layout() == layout) {
                rowIndex = i;
                break;
            }
        }
        if (rowIndex < 0)
            return static_cast<QWidget*>(nullptr);

        QLayoutItem* item = ui.summarySheetLayout->takeAt(rowIndex);
        QWidget* row = new QWidget(ui.summarySheet);
        row->setObjectName(QLatin1String(objectName));
        row->setProperty("summaryRow", true);
        row->setCursor(Qt::PointingHandCursor);
        row->installEventFilter(this);
        layout->setParent(nullptr);
        row->setLayout(layout);
        ui.summarySheetLayout->insertWidget(rowIndex, row);
        if (item != layout)
            delete item;
        return row;
    };
    wrapSummaryRow(ui.releaseSummaryLayout, "releaseSummaryWidget");
    wrapSummaryRow(ui.healthSummaryLayout, "healthSummaryWidget");
    wrapSummaryRow(ui.loggingSummaryLayout, "loggingSummaryWidget");
    wrapSummaryRow(ui.transportSummaryLayout, "transportSummaryWidget");
    wrapSummaryRow(ui.tracingSummaryLayout, "tracingSummaryWidget");
    wrapSummaryRow(ui.eventsSummaryLayout, "eventsSummaryWidget");
    wrapSummaryRow(ui.databaseSummaryLayout, "databaseSummaryWidget");
    wrapSummaryRow(ui.appHangSummaryLayout, "appHangSummaryWidget");
    wrapSummaryRow(ui.crashHandlingSummaryLayout, "crashHandlingSummaryWidget");
    wrapSummaryRow(ui.nativeSummaryLayout, "nativeSummaryWidget");
}

void InitPane::setupFormAlignment()
{
    QLabel* formLabels[] = {
        ui.dsnLabel,
        ui.releaseLabel,
        ui.environmentLabel,
        ui.distLabel,
        ui.healthFormSpacerLabel,
        ui.shutdownTimeoutLabel,
        ui.transferTimeoutLabel,
        ui.proxyLabel,
        ui.caCertsLabel,
        ui.orgIdLabel,
        ui.maxBreadcrumbsLabel,
        ui.maxSpansLabel,
        ui.sampleRateLabel,
        ui.tracesSampleRateLabel,
        ui.loggerLevelLabel,
        ui.databasePathLabel,
        ui.cacheModeLabel,
        ui.cacheMaxItemsLabel,
        ui.cacheMaxSizeLabel,
        ui.cacheMaxAgeLabel,
        ui.appHangTimeoutLabel,
        ui.appHangThreadStackwalkModeLabel,
        ui.crashHandlingSpacerLabel,
        ui.crashReporterPathLabel,
        ui.crashMemoryReserveLabel,
        ui.crashReportingModeLabel,
        ui.minidumpModeLabel,
        ui.threadStackwalkModeLabel,
        ui.crashUploadModeLabel,
        ui.maxStackCaptureSizeLabel,
    };
    QLabel* summaryTitles[] = {
        ui.releaseSummaryTitle,
        ui.healthSummaryTitle,
        ui.loggingSummaryTitle,
        ui.transportSummaryTitle,
        ui.tracingSummaryTitle,
        ui.eventsSummaryTitle,
        ui.databaseSummaryTitle,
        ui.appHangSummaryTitle,
        ui.crashHandlingSummaryTitle,
        ui.nativeSummaryTitle,
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
    const int summaryIconWidth = ui.releaseSummaryStatus->minimumWidth();
    const int summaryLabelLeftMargin =
        ui.releaseSummaryLayout->contentsMargins().left() + summaryIconWidth + summaryHorizontalSpacing;
    auto alignDetailsForm = [summaryLabelLeftMargin, formHorizontalSpacing](QFormLayout* layout) {
        const QMargins margins = layout->contentsMargins();
        layout->setContentsMargins(summaryLabelLeftMargin, margins.top(), margins.right(), margins.bottom());
        layout->setHorizontalSpacing(formHorizontalSpacing);
        layout->setProperty("visibleVerticalSpacing", layout->verticalSpacing());
    };
    alignDetailsForm(ui.releaseDetailsFormLayout);
    alignDetailsForm(ui.healthDetailsFormLayout);
    alignDetailsForm(ui.loggingDetailsFormLayout);
    alignDetailsForm(ui.transportDetailsFormLayout);
    alignDetailsForm(ui.tracingDetailsFormLayout);
    alignDetailsForm(ui.eventsDetailsFormLayout);
    alignDetailsForm(ui.databaseDetailsFormLayout);
    alignDetailsForm(ui.appHangDetailsFormLayout);
    alignDetailsForm(ui.crashHandlingDetailsFormLayout);
    alignDetailsForm(ui.nativeDetailsFormLayout);

    auto alignSummaryValue = [formLabelWidth, summaryHorizontalSpacing](
                                 QHBoxLayout* layout, QLabel* title, QLabel* summary) {
        layout->setSpacing(summaryHorizontalSpacing);
        const QMargins margins = layout->contentsMargins();
        layout->setContentsMargins(margins.left(), 3, margins.right(), 3);
        title->setFixedWidth(formLabelWidth);
        title->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        summary->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    };
    alignSummaryValue(ui.releaseSummaryLayout, ui.releaseSummaryTitle, ui.releaseSummaryLabel);
    alignSummaryValue(ui.healthSummaryLayout, ui.healthSummaryTitle, ui.healthSummaryLabel);
    alignSummaryValue(ui.loggingSummaryLayout, ui.loggingSummaryTitle, ui.loggingSummaryLabel);
    alignSummaryValue(ui.transportSummaryLayout, ui.transportSummaryTitle, ui.transportSummaryLabel);
    alignSummaryValue(ui.tracingSummaryLayout, ui.tracingSummaryTitle, ui.tracingSummaryLabel);
    alignSummaryValue(ui.eventsSummaryLayout, ui.eventsSummaryTitle, ui.eventsSummaryLabel);
    alignSummaryValue(ui.databaseSummaryLayout, ui.databaseSummaryTitle, ui.databaseSummaryLabel);
    alignSummaryValue(ui.appHangSummaryLayout, ui.appHangSummaryTitle, ui.appHangSummaryLabel);
    alignSummaryValue(ui.crashHandlingSummaryLayout, ui.crashHandlingSummaryTitle, ui.crashHandlingSummaryLabel);
    alignSummaryValue(ui.nativeSummaryLayout, ui.nativeSummaryTitle, ui.nativeSummaryLabel);

    for (QWidget* widget : {
             ui.releaseSummaryStatus, ui.releaseSummaryTitle, ui.releaseSummaryLabel,
             ui.healthSummaryStatus, ui.healthSummaryTitle, ui.healthSummaryLabel,
             ui.loggingSummaryStatus, ui.loggingSummaryTitle, ui.loggingSummaryLabel,
             ui.transportSummaryStatus, ui.transportSummaryTitle, ui.transportSummaryLabel,
             ui.tracingSummaryStatus, ui.tracingSummaryTitle, ui.tracingSummaryLabel,
             ui.eventsSummaryStatus, ui.eventsSummaryTitle, ui.eventsSummaryLabel,
             ui.databaseSummaryStatus, ui.databaseSummaryTitle, ui.databaseSummaryLabel,
             ui.appHangSummaryStatus, ui.appHangSummaryTitle, ui.appHangSummaryLabel,
             ui.crashHandlingSummaryStatus, ui.crashHandlingSummaryTitle, ui.crashHandlingSummaryLabel,
             ui.nativeSummaryStatus, ui.nativeSummaryTitle, ui.nativeSummaryLabel,
         }) {
        widget->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
}

void InitPane::setupControls()
{
    ui.cacheKeepModeBox->addItem("None", SENTRY_CACHE_KEEP_NONE);
    ui.cacheKeepModeBox->addItem("Offline", SENTRY_CACHE_KEEP_OFFLINE);
    ui.cacheKeepModeBox->addItem("Always", SENTRY_CACHE_KEEP_ALWAYS);
    ui.environmentEdit->setInsertPolicy(QComboBox::NoInsert);
    if (QLineEdit* edit = ui.environmentEdit->lineEdit())
        edit->setPlaceholderText("production");
    ui.dsnEdit->setInsertPolicy(QComboBox::NoInsert);
    if (QLineEdit* edit = ui.dsnEdit->lineEdit())
        edit->setPlaceholderText("https://public@example.ingest.sentry.io/project-id");
    ui.loggerLevelBox->addItem("None", kLoggerLevelNone);
    ui.loggerLevelBox->addItem("Trace", SENTRY_LEVEL_TRACE);
    ui.loggerLevelBox->addItem("Debug", SENTRY_LEVEL_DEBUG);
    ui.loggerLevelBox->addItem("Info", SENTRY_LEVEL_INFO);
    ui.loggerLevelBox->addItem("Warning", SENTRY_LEVEL_WARNING);
    ui.loggerLevelBox->addItem("Error", SENTRY_LEVEL_ERROR);
    ui.loggerLevelBox->addItem("Fatal", SENTRY_LEVEL_FATAL);
    ui.crashReportingModeBox->addItem("Native", SENTRY_CRASH_REPORTING_MODE_NATIVE);
    ui.crashReportingModeBox->addItem("Minidump", SENTRY_CRASH_REPORTING_MODE_MINIDUMP);
    ui.crashReportingModeBox->addItem("Native with minidump", SENTRY_CRASH_REPORTING_MODE_NATIVE_WITH_MINIDUMP);
    ui.minidumpModeBox->addItem("Stack only", SENTRY_MINIDUMP_MODE_STACK_ONLY);
    ui.minidumpModeBox->addItem("Smart", SENTRY_MINIDUMP_MODE_SMART);
    ui.minidumpModeBox->addItem("Full", SENTRY_MINIDUMP_MODE_FULL);
    ui.appHangThreadStackwalkModeBox->addItem("Hung thread", SENTRY_THREAD_STACKWALK_MODE_HUNG_ONLY);
    ui.appHangThreadStackwalkModeBox->addItem("All threads", SENTRY_THREAD_STACKWALK_MODE_ALL);
    ui.threadStackwalkModeBox->addItem("Crashed thread", SENTRY_THREAD_STACKWALK_MODE_CRASHED_ONLY);
    ui.threadStackwalkModeBox->addItem("All threads", SENTRY_THREAD_STACKWALK_MODE_ALL);
    ui.crashUploadModeBox->addItem("Sync", SENTRY_CRASH_UPLOAD_MODE_SYNC);
    ui.crashUploadModeBox->addItem("Async", SENTRY_CRASH_UPLOAD_MODE_ASYNC);

    const char* kDisclosureButton =
        "QToolButton { background: transparent; border: none; padding: 0; }";
    auto setupDisclosureButton = [this, kDisclosureButton](QToolButton* button, const char* settingKey) {
        button->setFixedSize(14, 20);
        button->setIconSize(QSize(12, 12));
        button->setStyleSheet(kDisclosureButton);
        button->setChecked(QSettings().value(settingKey, false).toBool());
        button->setIcon(Style::makeChevronIcon(palette(), devicePixelRatioF(), button->isChecked()));
        button->setToolTip(button->isChecked() ? "Collapse" : "Expand");
        connect(button, &QAbstractButton::toggled, this,
            [this, button, settingKey](bool expanded) {
                QSettings().setValue(settingKey, expanded);
                button->setIcon(Style::makeChevronIcon(palette(), devicePixelRatioF(), expanded));
                button->setToolTip(expanded ? "Collapse" : "Expand");
                updateDetailsVisibility();
            });
    };
    setupDisclosureButton(ui.releaseEditButton, "init/releaseExpanded");
    setupDisclosureButton(ui.healthEditButton, "init/healthExpanded");
    setupDisclosureButton(ui.loggingEditButton, "init/loggingExpanded");
    setupDisclosureButton(ui.transportEditButton, "init/transportExpanded");
    setupDisclosureButton(ui.tracingEditButton, "init/tracingExpanded");
    setupDisclosureButton(ui.eventsEditButton, "init/eventsExpanded");
    setupDisclosureButton(ui.databaseEditButton, "init/databaseExpanded");
    setupDisclosureButton(ui.appHangEditButton, "init/appHangExpanded");
    setupDisclosureButton(ui.crashHandlingEditButton, "init/crashHandlingExpanded");
    setupDisclosureButton(ui.nativeEditButton, "init/nativeExpanded");

    QAction* databaseBrowseAction = ui.databasePathEdit->addAction(
        Style::makeEllipsisIcon(palette(), devicePixelRatioF()), QLineEdit::TrailingPosition);
    databaseBrowseAction->setToolTip("Browse");
    QAction* clearDatabaseAction = ui.databasePathEdit->addAction(
        Style::makeClearIcon(palette(), devicePixelRatioF()), QLineEdit::TrailingPosition);
    clearDatabaseAction->setObjectName("databasePathClearAction");
    clearDatabaseAction->setToolTip("Clear");
    auto updateDatabaseClearAction = [this, clearDatabaseAction]() {
        clearDatabaseAction->setEnabled(!ui.databasePathEdit->text().isEmpty());
    };
    connect(ui.databasePathEdit, &QLineEdit::textChanged, this,
        [updateDatabaseClearAction](const QString&) { updateDatabaseClearAction(); });
    connect(clearDatabaseAction, &QAction::triggered, ui.databasePathEdit, &QLineEdit::clear);
    updateDatabaseClearAction();
    connect(databaseBrowseAction, &QAction::triggered, this, [this]() {
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

    QAction* browseCrashReporterAction = ui.crashReporterPathEdit->addAction(
        Style::makeEllipsisIcon(palette(), devicePixelRatioF()), QLineEdit::TrailingPosition);
    browseCrashReporterAction->setToolTip("Browse");
    QAction* clearCrashReporterAction = ui.crashReporterPathEdit->addAction(
        Style::makeClearIcon(palette(), devicePixelRatioF()), QLineEdit::TrailingPosition);
    clearCrashReporterAction->setObjectName("crashReporterClearAction");
    clearCrashReporterAction->setToolTip("Clear");
    connect(ui.crashReporterPathEdit, &QLineEdit::textChanged, this,
        [this](const QString&) { updateCrashReporterControls(); });
    connect(clearCrashReporterAction, &QAction::triggered, ui.crashReporterPathEdit, &QLineEdit::clear);
    connect(ui.externalCrashReporterBox, &QAbstractButton::toggled,
        this, &InitPane::updateCrashReporterControls);
    connect(ui.crashReportingModeBox, qOverload<int>(&QComboBox::currentIndexChanged), this,
        [this](int) { updateNativeControls(); });
    updateCrashReporterControls();
    updateNativeControls();

    connect(ui.dsnEdit, &QComboBox::currentTextChanged, this, &InitPane::updateSummaries);
    connect(ui.releaseEdit, &QLineEdit::textChanged, this, &InitPane::updateSummaries);
    connect(ui.environmentEdit, &QComboBox::currentTextChanged, this, &InitPane::updateSummaries);
    connect(ui.distEdit, &QLineEdit::textChanged, this, &InitPane::updateSummaries);
    connect(ui.databasePathEdit, &QLineEdit::textChanged, this, &InitPane::updateSummaries);
    connect(ui.sampleRateBox, qOverload<double>(&QDoubleSpinBox::valueChanged),
        this, &InitPane::updateSummaries);
    connect(ui.tracesSampleRateBox, qOverload<double>(&QDoubleSpinBox::valueChanged),
        this, &InitPane::updateSummaries);
    connect(ui.maxBreadcrumbsBox, qOverload<int>(&QSpinBox::valueChanged),
        this, &InitPane::updateSummaries);
    connect(ui.maxSpansBox, qOverload<int>(&QSpinBox::valueChanged),
        this, &InitPane::updateSummaries);
    connect(ui.shutdownTimeoutBox, qOverload<int>(&QSpinBox::valueChanged),
        this, &InitPane::updateSummaries);
    connect(ui.cacheKeepModeBox, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &InitPane::updateSummaries);
    connect(ui.cacheMaxItemsBox, qOverload<int>(&QSpinBox::valueChanged),
        this, &InitPane::updateSummaries);
    connect(ui.cacheMaxSizeBox, qOverload<int>(&QSpinBox::valueChanged),
        this, &InitPane::updateSummaries);
    connect(ui.cacheMaxAgeBox, qOverload<int>(&QSpinBox::valueChanged),
        this, &InitPane::updateSummaries);
    connect(ui.appHangTimeoutBox, qOverload<int>(&QSpinBox::valueChanged),
        this, &InitPane::updateSummaries);
    connect(ui.appHangThreadStackwalkModeBox, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &InitPane::updateSummaries);
    connect(ui.loggerLevelBox, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &InitPane::updateSummaries);
    connect(ui.crashReportingModeBox, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &InitPane::updateSummaries);
    connect(ui.minidumpModeBox, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &InitPane::updateSummaries);
    connect(ui.threadStackwalkModeBox, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &InitPane::updateSummaries);
    connect(ui.crashUploadModeBox, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &InitPane::updateSummaries);
    connect(ui.maxStackCaptureSizeBox, qOverload<int>(&QSpinBox::valueChanged),
        this, &InitPane::updateSummaries);
    connect(ui.crashReporterPathEdit, &QLineEdit::textChanged, this, &InitPane::updateSummaries);
    connect(ui.proxyEdit, &QLineEdit::textChanged, this, &InitPane::updateSummaries);
    connect(ui.caCertsEdit, &QLineEdit::textChanged, this, &InitPane::updateSummaries);
    connect(ui.orgIdEdit, &QLineEdit::textChanged, this, &InitPane::updateSummaries);
    connect(ui.transferTimeoutBox, qOverload<int>(&QSpinBox::valueChanged),
        this, &InitPane::updateSummaries);
    connect(ui.crashMemoryReserveBox, qOverload<int>(&QSpinBox::valueChanged),
        this, &InitPane::updateSummaries);
    for (QCheckBox* box : {
             ui.attachScreenshotBox,
             ui.requireUserConsentBox,
             ui.systemCrashReporterBox,
             ui.externalCrashReporterBox,
             ui.enableLargeAttachmentsBox,
             ui.httpRetryBox,
             ui.enableAppHangTrackingBox,
             ui.attachStacktraceBox,
             ui.autoSessionTrackingBox,
             ui.sendClientReportsBox,
             ui.symbolizeStacktracesBox,
             ui.propagateTraceparentBox,
             ui.strictTraceContinuationBox,
             ui.loggerEnabledWhenCrashedBox,
         }) {
        connect(box, &QAbstractButton::toggled, this, &InitPane::updateSummaries);
    }
    connect(browseCrashReporterAction, &QAction::triggered, this, [this]() {
        QString seed = ui.crashReporterPathEdit->text();
        if (seed.isEmpty())
            seed = QSettings().value("externalCrashReporter/lastDir",
                QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation)).toString();
        QString path = QFileDialog::getOpenFileName(this, "Select external crash reporter", seed);
        if (path.isEmpty())
            return;
        QSettings().setValue("externalCrashReporter/lastDir", QFileInfo(path).absolutePath());
        ui.crashReporterPathEdit->setText(path);
    });

    connect(ui.initializeButton, &QAbstractButton::clicked, this, [this]() {
        const Options options = optionsFromPage();
        rememberHistory(ui.dsnEdit, "init/dsnHistory", options.dsn);
        rememberHistory(ui.environmentEdit, "init/environmentHistory", options.environment);
        Playground::open(options);
    });
    connect(ui.resetButton, &QAbstractButton::clicked, this, [this]() { populate(Options()); });
}

void InitPane::populate()
{
    populate(Playground::instance()->options());
}

void InitPane::populate(const Options& options)
{
    const QList<QWidget*> widgets = {
        ui.dsnEdit,
        ui.databasePathEdit,
        ui.releaseEdit,
        ui.environmentEdit,
        ui.distEdit,
        ui.sampleRateBox,
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
        ui.crashReporterPathEdit,
        ui.crashReportingModeBox,
        ui.minidumpModeBox,
        ui.enableAppHangTrackingBox,
        ui.appHangTimeoutBox,
        ui.appHangThreadStackwalkModeBox,
        ui.threadStackwalkModeBox,
        ui.crashUploadModeBox,
        ui.maxStackCaptureSizeBox,
        ui.attachStacktraceBox,
        ui.autoSessionTrackingBox,
        ui.sendClientReportsBox,
        ui.symbolizeStacktracesBox,
        ui.propagateTraceparentBox,
        ui.strictTraceContinuationBox,
        ui.loggerEnabledWhenCrashedBox,
        ui.proxyEdit,
        ui.caCertsEdit,
        ui.orgIdEdit,
        ui.transferTimeoutBox,
        ui.crashMemoryReserveBox,
    };
    std::vector<std::unique_ptr<QSignalBlocker>> blockers;
    blockers.reserve(widgets.size());
    for (QWidget* widget : widgets)
        blockers.push_back(std::make_unique<QSignalBlocker>(widget));

    populateHistory(ui.dsnEdit, "init/dsnHistory", options.dsn);
    ui.dsnEdit->lineEdit()->setCursorPosition(0);
    ui.databasePathEdit->setText(options.databasePath);
    ui.databasePathEdit->setCursorPosition(0);
    ui.releaseEdit->setText(options.release);
    populateHistory(ui.environmentEdit, "init/environmentHistory", options.environment);
    ui.distEdit->setText(options.dist);
    ui.sampleRateBox->setValue(options.sampleRate);
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
    ui.crashReporterPathEdit->setText(options.externalCrashReporterPath);
    const int crashReportingModeIndex = ui.crashReportingModeBox->findData(options.crashReportingMode);
    ui.crashReportingModeBox->setCurrentIndex(crashReportingModeIndex >= 0
        ? crashReportingModeIndex
        : ui.crashReportingModeBox->findData(SENTRY_CRASH_REPORTING_MODE_NATIVE_WITH_MINIDUMP));
    const int minidumpModeIndex = ui.minidumpModeBox->findData(options.minidumpMode);
    ui.minidumpModeBox->setCurrentIndex(minidumpModeIndex >= 0
        ? minidumpModeIndex
        : ui.minidumpModeBox->findData(SENTRY_MINIDUMP_MODE_SMART));
    ui.enableAppHangTrackingBox->setChecked(options.enableAppHangTracking);
    ui.appHangTimeoutBox->setValue(options.appHangTimeout);
    const int appHangThreadStackwalkModeIndex =
        ui.appHangThreadStackwalkModeBox->findData(options.appHangThreadStackwalkMode);
    ui.appHangThreadStackwalkModeBox->setCurrentIndex(appHangThreadStackwalkModeIndex >= 0
        ? appHangThreadStackwalkModeIndex
        : ui.appHangThreadStackwalkModeBox->findData(SENTRY_THREAD_STACKWALK_MODE_HUNG_ONLY));
    const int threadStackwalkModeIndex = ui.threadStackwalkModeBox->findData(options.threadStackwalkMode);
    ui.threadStackwalkModeBox->setCurrentIndex(threadStackwalkModeIndex >= 0
        ? threadStackwalkModeIndex
        : ui.threadStackwalkModeBox->findData(SENTRY_THREAD_STACKWALK_MODE_ALL));
    const int crashUploadModeIndex = ui.crashUploadModeBox->findData(options.crashUploadMode);
    ui.crashUploadModeBox->setCurrentIndex(crashUploadModeIndex >= 0
        ? crashUploadModeIndex
        : ui.crashUploadModeBox->findData(SENTRY_CRASH_UPLOAD_MODE_SYNC));
    ui.maxStackCaptureSizeBox->setValue(options.maxStackCaptureSize);
    ui.attachStacktraceBox->setChecked(options.attachStacktrace);
    ui.autoSessionTrackingBox->setChecked(options.autoSessionTracking);
    ui.sendClientReportsBox->setChecked(options.sendClientReports);
    ui.symbolizeStacktracesBox->setChecked(options.symbolizeStacktraces);
    ui.propagateTraceparentBox->setChecked(options.propagateTraceparent);
    ui.strictTraceContinuationBox->setChecked(options.strictTraceContinuation);
    ui.loggerEnabledWhenCrashedBox->setChecked(options.loggerEnabledWhenCrashed);
    ui.proxyEdit->setText(options.proxy);
    ui.caCertsEdit->setText(options.caCerts);
    ui.caCertsEdit->setCursorPosition(0);
    ui.orgIdEdit->setText(options.orgId);
    ui.transferTimeoutBox->setValue(options.transferTimeout);
    ui.crashMemoryReserveBox->setValue(options.crashMemoryReserve);
    for (QAction* action : ui.databasePathEdit->actions()) {
        if (action->objectName() == "databasePathClearAction")
            action->setEnabled(!ui.databasePathEdit->text().isEmpty());
    }
    updateCrashReporterControls();
    updateNativeControls();
    ui.initializeButton->setText(Playground::instance()->wasInitialized()
        ? "Re-initialize"
        : "Initialize");
    updateSummaries();
    updateDetailsVisibility();
}

Options InitPane::optionsFromPage() const
{
    Options options = Playground::instance()->options();
    options.dsn = ui.dsnEdit->currentText().trimmed();
    options.databasePath = ui.databasePathEdit->text();
    options.release = ui.releaseEdit->text().trimmed();
    options.environment = ui.environmentEdit->currentText().trimmed();
    options.dist = ui.distEdit->text().trimmed();
    options.sampleRate = ui.sampleRateBox->value();
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
    options.externalCrashReporterPath = ui.crashReporterPathEdit->text();
    options.crashReportingMode = ui.crashReportingModeBox->currentData().toInt();
    options.minidumpMode = ui.minidumpModeBox->currentData().toInt();
    options.enableAppHangTracking = ui.enableAppHangTrackingBox->isChecked();
    options.appHangTimeout = ui.appHangTimeoutBox->value();
    options.appHangThreadStackwalkMode = ui.appHangThreadStackwalkModeBox->currentData().toInt();
    options.threadStackwalkMode = ui.threadStackwalkModeBox->currentData().toInt();
    options.crashUploadMode = ui.crashUploadModeBox->currentData().toInt();
    options.maxStackCaptureSize = ui.maxStackCaptureSizeBox->value();
    options.attachStacktrace = ui.attachStacktraceBox->isChecked();
    options.autoSessionTracking = ui.autoSessionTrackingBox->isChecked();
    options.sendClientReports = ui.sendClientReportsBox->isChecked();
    options.symbolizeStacktraces = ui.symbolizeStacktracesBox->isChecked();
    options.propagateTraceparent = ui.propagateTraceparentBox->isChecked();
    options.strictTraceContinuation = ui.strictTraceContinuationBox->isChecked();
    options.loggerEnabledWhenCrashed = ui.loggerEnabledWhenCrashedBox->isChecked();
    options.proxy = ui.proxyEdit->text().trimmed();
    options.caCerts = ui.caCertsEdit->text();
    options.orgId = ui.orgIdEdit->text().trimmed();
    options.transferTimeout = ui.transferTimeoutBox->value();
    options.crashMemoryReserve = ui.crashMemoryReserveBox->value();
    return options;
}

void InitPane::updateCrashReporterControls()
{
    const bool externalCrashReporterEnabled = ui.externalCrashReporterBox->isChecked();
    const bool crashHandlingDetailsVisible = ui.crashHandlingEditButton->isChecked();
    ui.crashHandlingDetailsFormLayout->setRowVisible(
        ui.crashReporterPathEdit, crashHandlingDetailsVisible && externalCrashReporterEnabled);
    ui.crashReporterPathEdit->setReadOnly(!externalCrashReporterEnabled);

    for (QAction* action : ui.crashReporterPathEdit->actions()) {
        action->setEnabled(externalCrashReporterEnabled);
        if (action->objectName() == "crashReporterClearAction")
            action->setEnabled(externalCrashReporterEnabled
                && !ui.crashReporterPathEdit->text().isEmpty());
    }

    ui.rightColumn->invalidate();
    ui.scrollContents->updateGeometry();
}

void InitPane::updateNativeControls()
{
    const bool nativeSectionVisible = supportsNativeOptions();
    const bool nativeDetailsVisible = nativeSectionVisible && ui.nativeEditButton->isChecked();
    const bool usesMinidump = crashReportingModeUsesMinidump(
        ui.crashReportingModeBox->currentData().toInt());
    const bool usesStackwalk = crashReportingModeUsesStackwalk(
        ui.crashReportingModeBox->currentData().toInt());

    if (QWidget* nativeSummaryWidget = findChild<QWidget*>(QStringLiteral("nativeSummaryWidget")))
        nativeSummaryWidget->setVisible(nativeSectionVisible);
    ui.nativeSectionDivider->setVisible(nativeSectionVisible);
    ui.nativeEditButton->setEnabled(nativeSectionVisible);

    ui.nativeDetailsFormLayout->setRowVisible(
        ui.minidumpModeLabel, nativeDetailsVisible && usesMinidump);
    ui.minidumpModeBox->setEnabled(usesMinidump);
    ui.threadStackwalkModeBox->setEnabled(usesStackwalk);

    ui.rightColumn->invalidate();
    ui.scrollContents->updateGeometry();
}

void InitPane::updateDetailsVisibility()
{
    const bool releaseVisible = ui.releaseEditButton->isChecked();
    const bool healthVisible = ui.healthEditButton->isChecked();
    const bool loggingVisible = ui.loggingEditButton->isChecked();
    const bool transportVisible = ui.transportEditButton->isChecked();
    const bool tracingVisible = ui.tracingEditButton->isChecked();
    const bool eventsVisible = ui.eventsEditButton->isChecked();
    const bool databaseVisible = ui.databaseEditButton->isChecked();
    const bool appHangVisible = ui.appHangEditButton->isChecked();
    const bool crashHandlingVisible = ui.crashHandlingEditButton->isChecked();
    const bool nativeVisible = supportsNativeOptions() && ui.nativeEditButton->isChecked();

    auto setFormVisible = [](QFormLayout* layout, bool visible) {
        QMargins margins = layout->contentsMargins();
        margins.setTop(visible ? 8 : 0);
        margins.setBottom(visible ? 10 : 0);
        layout->setContentsMargins(margins);
        layout->setVerticalSpacing(visible ? layout->property("visibleVerticalSpacing").toInt() : 0);
        for (int row = 0; row < layout->rowCount(); ++row)
            layout->setRowVisible(row, visible);
        layout->invalidate();
    };
    setFormVisible(ui.releaseDetailsFormLayout, releaseVisible);
    setFormVisible(ui.healthDetailsFormLayout, healthVisible);
    setFormVisible(ui.loggingDetailsFormLayout, loggingVisible);
    setFormVisible(ui.transportDetailsFormLayout, transportVisible);
    setFormVisible(ui.tracingDetailsFormLayout, tracingVisible);
    setFormVisible(ui.eventsDetailsFormLayout, eventsVisible);
    setFormVisible(ui.databaseDetailsFormLayout, databaseVisible);
    setFormVisible(ui.appHangDetailsFormLayout, appHangVisible);
    setFormVisible(ui.crashHandlingDetailsFormLayout, crashHandlingVisible);
    setFormVisible(ui.nativeDetailsFormLayout, nativeVisible);
    updateCrashReporterControls();
    updateNativeControls();

    ui.rightColumn->invalidate();
    ui.scrollContents->updateGeometry();
}

void InitPane::refreshPaletteStyles()
{
    const QColor textColor = palette().color(QPalette::WindowText);
    const QColor paneColor = Style::blendedColor(palette().color(QPalette::Window), textColor, 10);

    ui.scrollBackdrop->setStyleSheet(QStringLiteral(
        "QFrame#scrollBackdrop {"
        " background-color: %1;"
        " border: 1px solid %2;"
        " border-radius: 6px;"
        "}")
            .arg(Style::cssRgb(paneColor), Style::cssRgba(textColor, 38)));
    const QString dividerStyle = QStringLiteral(
        "QFrame { background-color: %1; border: none; }").arg(Style::cssRgba(textColor, 26));
    QWidget* const sectionDividers[] = {
        ui.dsnSectionDivider,
        ui.releaseSectionDivider,
        ui.healthSectionDivider,
        ui.loggingSectionDivider,
        ui.transportSectionDivider,
        ui.tracingSectionDivider,
        ui.eventsSectionDivider,
        ui.databaseSectionDivider,
        ui.appHangSectionDivider,
        ui.crashHandlingSectionDivider,
        ui.nativeSectionDivider,
    };
    for (QWidget* divider : sectionDividers)
        divider->setStyleSheet(dividerStyle);
    ui.scrollArea->setFrameShape(QFrame::NoFrame);
    ui.scrollArea->setFrameShadow(QFrame::Plain);

    QPalette panePalette = palette();
    panePalette.setColor(QPalette::Window, paneColor);
    panePalette.setColor(QPalette::Base, paneColor);
    QWidget* const transparentPaneWidgets[] = {
             ui.scrollArea,
             ui.scrollArea->viewport(),
             ui.scrollContents,
             ui.summarySheet,
    };
    for (QWidget* widget : transparentPaneWidgets) {
        widget->setPalette(panePalette);
        widget->setAutoFillBackground(false);
    }
    updateSummaries();
}

void InitPane::updateSummaries()
{
    const QColor textColor = palette().color(QPalette::WindowText);
    const QString inactiveStatusStyle = QStringLiteral("color: %1; font-weight: bold;")
        .arg(Style::cssRgba(textColor, 115));
    auto setStatus = [&inactiveStatusStyle](QLabel* label, bool enabled) {
        label->setText(enabled ? QStringLiteral("✓") : QStringLiteral("-"));
        label->setStyleSheet(enabled
            ? QStringLiteral("color: #21c26a; font-weight: bold;")
            : inactiveStatusStyle);
    };

    const bool hasDsn = !ui.dsnEdit->currentText().trimmed().isEmpty();
    ui.dsnStatus->setText(hasDsn ? QStringLiteral("✓") : QStringLiteral("✗"));
    ui.dsnStatus->setStyleSheet(hasDsn
        ? QStringLiteral("color: #21c26a; font-weight: bold;")
        : QStringLiteral("color: #ff453a; font-weight: bold;"));

    QStringList releaseParts;
    const QString release = ui.releaseEdit->text().trimmed();
    const QString environment = ui.environmentEdit->currentText().trimmed();
    const QString dist = ui.distEdit->text().trimmed();
    if (!release.isEmpty())
        releaseParts.append(release);
    if (!environment.isEmpty())
        releaseParts.append(environment);
    if (!dist.isEmpty())
        releaseParts.append(dist);
    ui.releaseSummaryLabel->setText(releaseParts.isEmpty()
            ? QStringLiteral("N/A")
            : releaseParts.join(QStringLiteral(", ")));
    setStatus(ui.releaseSummaryStatus, !releaseParts.isEmpty());

    QStringList healthParts;
    if (ui.autoSessionTrackingBox->isChecked())
        healthParts.append(QStringLiteral("auto-sessions"));
    if (ui.sendClientReportsBox->isChecked())
        healthParts.append(QStringLiteral("client reports"));
    ui.healthSummaryLabel->setText(healthParts.isEmpty()
            ? QStringLiteral("N/A")
            : healthParts.join(QStringLiteral(", ")));
    setStatus(ui.healthSummaryStatus, !healthParts.isEmpty());

    const bool loggingEnabled = ui.loggerLevelBox->currentData().toInt() != kLoggerLevelNone;
    ui.loggingSummaryLabel->setText(loggingEnabled
            ? ui.loggerLevelBox->currentText().toLower()
            : QStringLiteral("N/A"));
    setStatus(ui.loggingSummaryStatus, loggingEnabled);

    QStringList transportParts = {
        QStringLiteral("%1 ms").arg(ui.shutdownTimeoutBox->value()),
    };
    if (!ui.proxyEdit->text().trimmed().isEmpty())
        transportParts.append(QStringLiteral("proxy"));
    if (ui.httpRetryBox->isChecked())
        transportParts.append(QStringLiteral("retry"));
    if (ui.requireUserConsentBox->isChecked())
        transportParts.append(QStringLiteral("consent"));
    if (ui.enableLargeAttachmentsBox->isChecked())
        transportParts.append(QStringLiteral("large attachments"));
    ui.transportSummaryLabel->setText(transportParts.join(QStringLiteral(", ")));
    setStatus(ui.transportSummaryStatus, true);

    QStringList tracingParts = {
        QStringLiteral("sample %1x").arg(QLocale::c().toString(ui.tracesSampleRateBox->value(), 'f', 2)),
        QStringLiteral("%1 spans").arg(ui.maxSpansBox->value()),
    };
    if (ui.propagateTraceparentBox->isChecked())
        tracingParts.append(QStringLiteral("traceparent"));
    if (ui.strictTraceContinuationBox->isChecked())
        tracingParts.append(QStringLiteral("strict traces"));
    ui.tracingSummaryLabel->setText(tracingParts.join(QStringLiteral(", ")));
    setStatus(ui.tracingSummaryStatus, ui.tracesSampleRateBox->value() > 0);

    QStringList eventParts = {
        QStringLiteral("sample %1x").arg(QLocale::c().toString(ui.sampleRateBox->value(), 'f', 2)),
        QStringLiteral("%1 crumbs").arg(ui.maxBreadcrumbsBox->value()),
    };
    if (ui.attachStacktraceBox->isChecked())
        eventParts.append(QStringLiteral("stacktrace"));
    if (ui.symbolizeStacktracesBox->isChecked())
        eventParts.append(QStringLiteral("symbolize"));
    ui.eventsSummaryLabel->setText(eventParts.join(QStringLiteral(", ")));
    setStatus(ui.eventsSummaryStatus, true);

    QStringList databaseParts;
    const QString databasePath = ui.databasePathEdit->text().trimmed();
    if (!databasePath.isEmpty()) {
        databaseParts += QFileInfo(databasePath).fileName();
    }
    switch (ui.cacheKeepModeBox->currentIndex()) {
    case 0:
        databaseParts += QStringLiteral("no caching");
        break;
    case 1:
        databaseParts += QStringLiteral("offline caching");
        break;
    case 2:
        databaseParts += QStringLiteral("cache always");
        break;
    default:
        break;
    }
    if (ui.cacheKeepModeBox->currentIndex() > 0) {
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
    ui.databaseSummaryLabel->setText(databaseParts.join(", "));
    setStatus(ui.databaseSummaryStatus, true);

    const bool appHangEnabled = ui.enableAppHangTrackingBox->isChecked();
    ui.appHangSummaryLabel->setText(appHangEnabled
            ? QStringLiteral("%1 ms, %2")
                  .arg(ui.appHangTimeoutBox->value())
                  .arg(ui.appHangThreadStackwalkModeBox->currentText().toLower())
            : QStringLiteral("N/A"));
    setStatus(ui.appHangSummaryStatus, appHangEnabled);

    QStringList crashHandlingParts = {
        Playground::backend(),
    };
    if (ui.systemCrashReporterBox->isChecked())
        crashHandlingParts.append(QStringLiteral("system"));
    if (ui.externalCrashReporterBox->isChecked()) {
        const QString path = ui.crashReporterPathEdit->text().trimmed();
        crashHandlingParts.append(path.isEmpty()
            ? QStringLiteral("external")
            : QFileInfo(path).fileName());
    }
    if (ui.attachScreenshotBox->isChecked())
        crashHandlingParts.append(QStringLiteral("screenshot"));
    ui.crashHandlingSummaryLabel->setText(crashHandlingParts.join(QStringLiteral(", ")));
    setStatus(ui.crashHandlingSummaryStatus, true);

    if (supportsNativeOptions()) {
        const int crashReportingMode = ui.crashReportingModeBox->currentData().toInt();
        QStringList nativeParts = { crashReportingModeSummaryText(crashReportingMode) };
        if (crashReportingModeUsesMinidump(crashReportingMode))
            nativeParts.append(minidumpModeSummaryText(ui.minidumpModeBox->currentData().toInt()));
        if (crashReportingModeUsesStackwalk(crashReportingMode))
            nativeParts.append(ui.threadStackwalkModeBox->currentText().toLower());
        nativeParts.append(ui.crashUploadModeBox->currentText().toLower());
        ui.nativeSummaryLabel->setText(nativeParts.join(QStringLiteral(", ")));
        setStatus(ui.nativeSummaryStatus, true);
    } else {
        ui.nativeSummaryLabel->setText(QStringLiteral("N/A"));
        setStatus(ui.nativeSummaryStatus, false);
    }

    const Options options = optionsFromPage();
    ui.resetButton->setEnabled(options.toMap() != Options().toMap());
}

bool InitPane::eventFilter(QObject* watched, QEvent* event)
{
    auto* summaryRow = qobject_cast<QWidget*>(watched);
    if (summaryRow && summaryRow->property("summaryRow").toBool()) {
        if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
            const bool hovered = event->type() == QEvent::Enter;
            QPalette rowPalette = summaryRow->palette();
            rowPalette.setColor(QPalette::Window, Style::blendedColor(
                palette().color(QPalette::Window), palette().color(QPalette::WindowText), 18));
            summaryRow->setPalette(rowPalette);
            summaryRow->setAutoFillBackground(hovered);
        } else if (event->type() == QEvent::MouseButtonRelease) {
            const QString name = summaryRow->objectName();
            if (name == QLatin1String("releaseSummaryWidget")) {
                ui.releaseEditButton->toggle();
                return true;
            }
            if (name == QLatin1String("healthSummaryWidget")) {
                ui.healthEditButton->toggle();
                return true;
            }
            if (name == QLatin1String("loggingSummaryWidget")) {
                ui.loggingEditButton->toggle();
                return true;
            }
            if (name == QLatin1String("transportSummaryWidget")) {
                ui.transportEditButton->toggle();
                return true;
            }
            if (name == QLatin1String("tracingSummaryWidget")) {
                ui.tracingEditButton->toggle();
                return true;
            }
            if (name == QLatin1String("eventsSummaryWidget")) {
                ui.eventsEditButton->toggle();
                return true;
            }
            if (name == QLatin1String("databaseSummaryWidget")) {
                ui.databaseEditButton->toggle();
                return true;
            }
            if (name == QLatin1String("appHangSummaryWidget")) {
                ui.appHangEditButton->toggle();
                return true;
            }
            if (name == QLatin1String("crashHandlingSummaryWidget")) {
                ui.crashHandlingEditButton->toggle();
                return true;
            }
            if (name == QLatin1String("nativeSummaryWidget")) {
                ui.nativeEditButton->toggle();
                return true;
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}
