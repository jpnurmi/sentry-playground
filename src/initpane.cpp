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
#include <QtCore/qurl.h>
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

static constexpr int kMaxEnvironmentHistoryItems = 12;
static constexpr int kLoggerLevelNone = -1000;

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

InitPane::InitPane(QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    ui.initRightColumn->setAlignment(Qt::AlignTop);
    ui.initScrollBackdrop->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui.initScrollBackdrop->lower();

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
    wrapSummaryRow(ui.dsnSummaryLayout, "dsnSummaryWidget");
    wrapSummaryRow(ui.versionSummaryLayout, "versionSummaryWidget");
    wrapSummaryRow(ui.featuresSummaryLayout, "featuresSummaryWidget");
    wrapSummaryRow(ui.parametersSummaryLayout, "parametersSummaryWidget");
    wrapSummaryRow(ui.databaseSummaryLayout, "databaseSummaryWidget");
    wrapSummaryRow(ui.crashReporterSummaryLayout, "crashReporterSummaryWidget");
}

void InitPane::setupFormAlignment()
{
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
        ui.crashReporterSpacerLabel,
        ui.crashReporterPathLabel,
    };
    QLabel* summaryTitles[] = {
        ui.dsnSummaryTitle,
        ui.versionSummaryTitle,
        ui.featuresSummaryTitle,
        ui.parametersSummaryTitle,
        ui.databaseSummaryTitle,
        ui.crashReporterSummaryTitle,
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
    const int summaryIconWidth = ui.dsnSummaryStatus->minimumWidth();
    const int summaryLabelLeftMargin =
        ui.dsnSummaryLayout->contentsMargins().left() + summaryIconWidth + summaryHorizontalSpacing;
    auto alignDetailsForm = [summaryLabelLeftMargin, formHorizontalSpacing](QFormLayout* layout) {
        const QMargins margins = layout->contentsMargins();
        layout->setContentsMargins(summaryLabelLeftMargin, margins.top(), margins.right(), margins.bottom());
        layout->setHorizontalSpacing(formHorizontalSpacing);
    };
    alignDetailsForm(ui.dsnDetailsFormLayout);
    alignDetailsForm(ui.versionDetailsFormLayout);
    alignDetailsForm(ui.featuresDetailsFormLayout);
    alignDetailsForm(ui.parametersDetailsFormLayout);
    alignDetailsForm(ui.databaseDetailsFormLayout);
    alignDetailsForm(ui.crashReporterDetailsFormLayout);

    auto alignSummaryValue = [formLabelWidth, summaryHorizontalSpacing](
                                 QHBoxLayout* layout, QLabel* title, QLabel* summary) {
        layout->setSpacing(summaryHorizontalSpacing);
        const QMargins margins = layout->contentsMargins();
        layout->setContentsMargins(margins.left(), 3, margins.right(), 3);
        title->setFixedWidth(formLabelWidth);
        title->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        summary->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    };
    alignSummaryValue(ui.dsnSummaryLayout, ui.dsnSummaryTitle, ui.dsnSummaryLabel);
    alignSummaryValue(ui.versionSummaryLayout, ui.versionSummaryTitle, ui.versionSummaryLabel);
    alignSummaryValue(ui.featuresSummaryLayout, ui.featuresSummaryTitle, ui.featuresSummaryLabel);
    alignSummaryValue(ui.parametersSummaryLayout, ui.parametersSummaryTitle, ui.parametersSummaryLabel);
    alignSummaryValue(ui.databaseSummaryLayout, ui.databaseSummaryTitle, ui.databaseSummaryLabel);
    alignSummaryValue(ui.crashReporterSummaryLayout, ui.crashReporterSummaryTitle, ui.crashReporterSummaryLabel);

    for (QWidget* widget : {
             ui.dsnSummaryStatus, ui.dsnSummaryTitle, ui.dsnSummaryLabel,
             ui.versionSummaryStatus, ui.versionSummaryTitle, ui.versionSummaryLabel,
             ui.featuresSummaryStatus, ui.featuresSummaryTitle, ui.featuresSummaryLabel,
             ui.parametersSummaryStatus, ui.parametersSummaryTitle, ui.parametersSummaryLabel,
             ui.databaseSummaryStatus, ui.databaseSummaryTitle, ui.databaseSummaryLabel,
             ui.crashReporterSummaryStatus, ui.crashReporterSummaryTitle, ui.crashReporterSummaryLabel,
         }) {
        widget->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
}

void InitPane::setupControls()
{
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
    setupDisclosureButton(ui.versionEditButton, "init/versionExpanded");
    setupDisclosureButton(ui.dsnEditButton, "init/dsnExpanded");
    setupDisclosureButton(ui.featuresEditButton, "init/featuresExpanded");
    setupDisclosureButton(ui.parametersEditButton, "init/parametersExpanded");
    setupDisclosureButton(ui.databaseEditButton, "init/databaseExpanded");
    setupDisclosureButton(ui.crashReporterEditButton, "init/crashReporterExpanded");

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
    updateCrashReporterControls();

    connect(ui.initReleaseEdit, &QLineEdit::textChanged, this, &InitPane::updateSummaries);
    connect(ui.initEnvironmentEdit, &QComboBox::currentTextChanged, this, &InitPane::updateSummaries);
    connect(ui.initDistEdit, &QLineEdit::textChanged, this, &InitPane::updateSummaries);
    connect(ui.databasePathEdit, &QLineEdit::textChanged, this, &InitPane::updateSummaries);
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
    connect(ui.loggerLevelBox, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &InitPane::updateSummaries);
    connect(ui.crashReporterPathEdit, &QLineEdit::textChanged, this, &InitPane::updateSummaries);
    for (QCheckBox* box : {
             ui.attachScreenshotBox,
             ui.requireUserConsentBox,
             ui.systemCrashReporterBox,
             ui.externalCrashReporterBox,
             ui.enableLargeAttachmentsBox,
             ui.httpRetryBox,
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
        rememberEnvironment(options.environment);
        Playground::open(options);
    });
}

void InitPane::populate()
{
    const Options options = Playground::instance()->options();
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
        ui.crashReporterPathEdit,
    };
    std::vector<std::unique_ptr<QSignalBlocker>> blockers;
    blockers.reserve(widgets.size());
    for (QWidget* widget : widgets)
        blockers.push_back(std::make_unique<QSignalBlocker>(widget));

    ui.dsnEdit->setText(options.dsn);
    ui.dsnEdit->setCursorPosition(0);
    ui.databasePathEdit->setText(options.databasePath);
    ui.databasePathEdit->setCursorPosition(0);
    ui.initReleaseEdit->setText(options.release);
    populateEnvironmentHistory(options.environment);
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
    ui.crashReporterPathEdit->setText(options.externalCrashReporterPath);
    for (QAction* action : ui.databasePathEdit->actions()) {
        if (action->objectName() == "databasePathClearAction")
            action->setEnabled(!ui.databasePathEdit->text().isEmpty());
    }
    updateCrashReporterControls();
    ui.initializeButton->setText(Playground::instance()->wasInitialized()
        ? "Re-initialize"
        : "Initialize");
    updateSummaries();
    updateDetailsVisibility();
}

void InitPane::populateEnvironmentHistory(const QString& currentEnvironment)
{
    ui.initEnvironmentEdit->clear();
    ui.initEnvironmentEdit->addItems(environmentHistoryWith(
        QSettings().value("init/environmentHistory").toStringList(),
        currentEnvironment));
    ui.initEnvironmentEdit->setEditText(currentEnvironment);
}

void InitPane::rememberEnvironment(const QString& environment)
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

Options InitPane::optionsFromPage() const
{
    Options options = Playground::instance()->options();
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
    options.externalCrashReporterPath = ui.crashReporterPathEdit->text();
    return options;
}

void InitPane::updateCrashReporterControls()
{
    const bool externalCrashReporterEnabled = ui.externalCrashReporterBox->isChecked();
    const bool crashReporterDetailsVisible = ui.crashReporterEditButton->isChecked();
    ui.crashReporterDetailsFormLayout->setRowVisible(
        ui.crashReporterPathEdit, crashReporterDetailsVisible && externalCrashReporterEnabled);
    ui.crashReporterPathEdit->setReadOnly(!externalCrashReporterEnabled);

    for (QAction* action : ui.crashReporterPathEdit->actions()) {
        action->setEnabled(externalCrashReporterEnabled);
        if (action->objectName() == "crashReporterClearAction")
            action->setEnabled(externalCrashReporterEnabled
                && !ui.crashReporterPathEdit->text().isEmpty());
    }

    ui.initRightColumn->invalidate();
    ui.initScrollContents->updateGeometry();
}

void InitPane::updateDetailsVisibility()
{
    const bool dsnVisible = ui.dsnEditButton->isChecked();
    const bool versionVisible = ui.versionEditButton->isChecked();
    const bool featuresVisible = ui.featuresEditButton->isChecked();
    const bool parametersVisible = ui.parametersEditButton->isChecked();
    const bool databaseVisible = ui.databaseEditButton->isChecked();
    const bool crashReporterVisible = ui.crashReporterEditButton->isChecked();

    auto setFormVisible = [](QFormLayout* layout, bool visible, int visibleVerticalSpacing = 10) {
        QMargins margins = layout->contentsMargins();
        margins.setTop(visible ? 8 : 0);
        margins.setBottom(visible ? 10 : 0);
        layout->setContentsMargins(margins);
        layout->setVerticalSpacing(visible ? visibleVerticalSpacing : 0);
        for (int row = 0; row < layout->rowCount(); ++row)
            layout->setRowVisible(row, visible);
        layout->invalidate();
    };
    setFormVisible(ui.dsnDetailsFormLayout, dsnVisible);
    setFormVisible(ui.versionDetailsFormLayout, versionVisible);
    setFormVisible(ui.featuresDetailsFormLayout, featuresVisible);
    setFormVisible(ui.parametersDetailsFormLayout, parametersVisible);
    setFormVisible(ui.databaseDetailsFormLayout, databaseVisible);
    setFormVisible(ui.crashReporterDetailsFormLayout, crashReporterVisible, 4);
    updateCrashReporterControls();

    ui.initRightColumn->invalidate();
    ui.initScrollContents->updateGeometry();
}

void InitPane::refreshPaletteStyles()
{
    const QColor textColor = palette().color(QPalette::WindowText);
    const QColor paneColor = Style::blendedColor(palette().color(QPalette::Window), textColor, 10);

    ui.initScrollBackdrop->setStyleSheet(QStringLiteral(
        "QFrame#initScrollBackdrop {"
        " background-color: %1;"
        " border: 1px solid %2;"
        " border-radius: 6px;"
        "}")
            .arg(Style::cssRgb(paneColor), Style::cssRgba(textColor, 38)));
    const QString dividerStyle = QStringLiteral(
        "QFrame { background-color: %1; border: none; }").arg(Style::cssRgba(textColor, 26));
    QWidget* const sectionDividers[] = {
        ui.dsnSectionDivider,
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

    QString dsnSummary;
    const QUrl dsnUrl(ui.dsnEdit->text().trimmed());
    dsnSummary = dsnUrl.host();
    if (!dsnSummary.isEmpty() && dsnUrl.port() >= 0)
        dsnSummary += QStringLiteral(":%1").arg(dsnUrl.port());
    ui.dsnSummaryLabel->setText(dsnSummary.isEmpty()
                                    ? QStringLiteral("N/A")
                                    : dsnSummary);
    setStatus(ui.dsnSummaryStatus, !dsnSummary.isEmpty());

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
    setStatus(ui.versionSummaryStatus, !versionParts.isEmpty());

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

    ui.parametersSummaryLabel->setText(
        QStringLiteral("sample %1x, %2 crumbs, %3 spans, %4")
            .arg(QLocale::c().toString(ui.tracesSampleRateBox->value(), 'f', 2))
            .arg(ui.maxBreadcrumbsBox->value())
            .arg(ui.maxSpansBox->value())
            .arg(ui.loggerLevelBox->currentData().toInt() != kLoggerLevelNone
                     ? ui.loggerLevelBox->currentText().toLower()
                     : QString()));
    setStatus(ui.parametersSummaryStatus, true);

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

    QStringList crashReporterParts;
    if (ui.systemCrashReporterBox->isChecked())
        crashReporterParts.append(QStringLiteral("system"));
    if (ui.externalCrashReporterBox->isChecked()) {
        const QString path = ui.crashReporterPathEdit->text().trimmed();
        crashReporterParts.append(path.isEmpty()
            ? QStringLiteral("external")
            : QFileInfo(path).fileName());
    }
    ui.crashReporterSummaryLabel->setText(crashReporterParts.isEmpty()
                                         ? QStringLiteral("N/A")
                                         : crashReporterParts.join(QStringLiteral(", ")));
    setStatus(ui.crashReporterSummaryStatus, !crashReporterParts.isEmpty());
}

bool InitPane::eventFilter(QObject* watched, QEvent* event)
{
    auto* summaryRow = qobject_cast<QWidget*>(watched);
    if (summaryRow && summaryRow->property("initSummaryRow").toBool()) {
        if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
            const bool hovered = event->type() == QEvent::Enter;
            QPalette rowPalette = summaryRow->palette();
            rowPalette.setColor(QPalette::Window, Style::blendedColor(
                palette().color(QPalette::Window), palette().color(QPalette::WindowText), 18));
            summaryRow->setPalette(rowPalette);
            summaryRow->setAutoFillBackground(hovered);
        } else if (event->type() == QEvent::MouseButtonRelease) {
            const QString name = summaryRow->objectName();
            if (name == QLatin1String("versionSummaryWidget")) {
                ui.versionEditButton->toggle();
                return true;
            }
            if (name == QLatin1String("dsnSummaryWidget")) {
                ui.dsnEditButton->toggle();
                return true;
            }
            if (name == QLatin1String("featuresSummaryWidget")) {
                ui.featuresEditButton->toggle();
                return true;
            }
            if (name == QLatin1String("parametersSummaryWidget")) {
                ui.parametersEditButton->toggle();
                return true;
            }
            if (name == QLatin1String("databaseSummaryWidget")) {
                ui.databaseEditButton->toggle();
                return true;
            }
            if (name == QLatin1String("crashReporterSummaryWidget")) {
                ui.crashReporterEditButton->toggle();
                return true;
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}
