#include "debugfilesdialog.h"
#include "style.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdir.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qlibraryinfo.h>
#include <QtCore/qprocess.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qsettings.h>
#include <QtCore/qstandardpaths.h>
#include <QtCore/qurl.h>
#include <QtGui/qfontdatabase.h>
#include <QtGui/qtextcursor.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qplaintextedit.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qtabwidget.h>
#include <sentry.h>

static constexpr auto kUploadedDebugIdsSettingsKey = "debugFiles/uploadedDebugIds";
static constexpr auto kUploadedDebugIdsHistorySettingsKey = "debugFiles/uploadedDebugIdsHistory";
static constexpr auto kUploadedQtDebugIdsHistorySettingsKey = "debugFiles/uploadedQtDebugIdsHistory";
static constexpr int kMaxUploadedDebugIdSets = 10;

struct UploadTarget
{
    QString url;
    QString key;
    QString org;
    QString project;
};

QString DebugFilesDialog::sentryProgram()
{
    // QProcess doesn't apply PATHEXT on Windows, so a bare "sentry" would
    // only match sentry.exe and miss the npm-installed sentry.cmd shim
    const QString program = QStandardPaths::findExecutable(QStringLiteral("sentry"));
    return program.isEmpty() ? QStringLiteral("sentry") : program;
}

static QStringList normalizedDebugIds(QStringList debugIds)
{
    debugIds.removeDuplicates();
    debugIds.sort();
    return debugIds;
}

static UploadTarget uploadTargetFromDsn(const QString& dsn)
{
    const QUrl parsed(dsn);
    const QString scheme = parsed.scheme();

    if (!parsed.isValid()
        || (scheme != QLatin1String("http") && scheme != QLatin1String("https"))
        || parsed.host().isEmpty()
        || parsed.path().isEmpty()
        || parsed.path() == QLatin1String("/")) {
        return {};
    }

    QStringList pathSegments = parsed.path().split('/', Qt::SkipEmptyParts);
    if (pathSegments.isEmpty())
        return {};

    UploadTarget target;
    target.key = parsed.userName();
    target.project = pathSegments.takeLast();

    const QString host = parsed.host();
    if (host == QLatin1String("sentry.io") || host.endsWith(QLatin1String(".sentry.io"))) {
        // sentry.io ingest hosts carry the org ID, e.g. o123.ingest.us.sentry.io
        static const QRegularExpression orgIdPattern(
            QStringLiteral("^o(\\d+)\\.ingest(?:\\.[a-z]+)?\\.sentry\\.io$"));
        target.org = orgIdPattern.match(host).captured(1);
    } else {
        // self-hosted API lives under the DSN host and path prefix
        QUrl url = parsed.adjusted(QUrl::RemoveUserInfo | QUrl::RemoveQuery | QUrl::RemoveFragment);
        url.setPath(pathSegments.isEmpty() ? QString() : '/' + pathSegments.join('/'));
        target.url = url.toString();
    }
    return target;
}

static QStringList checkedDebugIds(const QString& path)
{
    QProcess process;
    process.setProgram(DebugFilesDialog::sentryProgram());
    process.setArguments({
        QStringLiteral("debug-files"),
        QStringLiteral("check"),
        QStringLiteral("--json"),
        path,
    });
    process.start();
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished();
        return {};
    }
    if (process.exitCode() != 0)
        return {};

    const QJsonDocument document = QJsonDocument::fromJson(process.readAllStandardOutput());
    const QJsonArray objects = document.object().value("objects").toArray();
    QStringList debugIds;
    for (const QJsonValue& object : objects) {
        const QString debugId = object.toObject().value("debugId").toString();
        if (!debugId.isEmpty())
            debugIds.append(debugId);
    }
    return normalizedDebugIds(debugIds);
}

static QJsonArray uploadedDebugIdsHistory(const QString& key)
{
    QSettings settings;
    QJsonArray history = QJsonDocument::fromJson(
        settings.value(key).toByteArray()).array();

    // only the app's debug IDs were stored before the history
    const QStringList legacyDebugIds = key == QLatin1String(kUploadedDebugIdsHistorySettingsKey)
        ? normalizedDebugIds(settings.value(kUploadedDebugIdsSettingsKey).toStringList())
        : QStringList();
    if (!legacyDebugIds.isEmpty()) {
        QJsonArray legacyEntry;
        for (const QString& debugId : legacyDebugIds)
            legacyEntry.append(debugId);
        history.prepend(legacyEntry);
        settings.remove(kUploadedDebugIdsSettingsKey);
    }

    return history;
}

static bool uploadedDebugIdsHistoryContains(const QString& key, const QStringList& debugIds)
{
    if (debugIds.isEmpty())
        return false;

    // Qt plugins loaded later add debug IDs, so look for each in any upload
    QStringList uploadedDebugIds;
    for (const QJsonValue& entry : uploadedDebugIdsHistory(key)) {
        for (const QJsonValue& debugId : entry.toArray())
            uploadedDebugIds.append(debugId.toString());
    }
    for (const QString& debugId : debugIds) {
        if (!uploadedDebugIds.contains(debugId))
            return false;
    }
    return true;
}

static void rememberUploadedDebugIds(const QString& key, const QStringList& debugIds)
{
    QJsonArray nextEntry;
    for (const QString& debugId : debugIds)
        nextEntry.append(debugId);

    QJsonArray nextHistory;
    nextHistory.append(nextEntry);
    for (const QJsonValue& entry : uploadedDebugIdsHistory(key)) {
        QStringList uploadedDebugIds;
        for (const QJsonValue& debugId : entry.toArray())
            uploadedDebugIds.append(debugId.toString());
        if (normalizedDebugIds(uploadedDebugIds) != debugIds)
            nextHistory.append(entry);
        if (nextHistory.size() >= kMaxUploadedDebugIdSets)
            break;
    }

    QSettings().setValue(key,
        QJsonDocument(nextHistory).toJson(QJsonDocument::Compact));
}

DebugFilesDialog::DebugFilesDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Upload Debug Files");
    resize(760, 480);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(0);

    m_tabs = new QTabWidget(this);
    layout->addWidget(m_tabs);

    auto addSection = [this](Section& section, const QString& title) {
        auto* page = new QWidget(m_tabs);
        section.page = page;
        auto* pageLayout = new QVBoxLayout(page);
        auto* headerLayout = new QHBoxLayout;
        section.summary = new QLabel(page);
        section.summary->setWordWrap(true);
        headerLayout->addWidget(section.summary, 1);
        section.button = new QPushButton("Upload", page);
        headerLayout->addWidget(section.button);
        pageLayout->addLayout(headerLayout);
        connect(section.button, &QPushButton::clicked, this, [this, &section]() { startUpload(section); });

        section.output = new QPlainTextEdit(page);
        section.output->setReadOnly(true);
        section.output->setLineWrapMode(QPlainTextEdit::NoWrap);
        section.output->setMaximumBlockCount(2000);
        section.output->setPlaceholderText("sentry output");
        section.output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        section.output->setStyleSheet(
            "QPlainTextEdit {"
            " background: #111318;"
            " color: #e6edf3;"
            " border: 1px solid #4b5563;"
            " border-radius: 4px;"
            " padding: 6px;"
            " selection-background-color: #3b82f6;"
            " }");
        pageLayout->addWidget(section.output);
        m_tabs->addTab(page, title);
    };

    addSection(m_app, "App");
    m_app.arguments = {
        QStringLiteral("debug-files"),
        QStringLiteral("upload"),
        QStringLiteral("--include-sources"),
        QStringLiteral("."),
    };
    m_app.directory = QCoreApplication::applicationDirPath();
    m_app.summary->setText(QDir::toNativeSeparators(m_app.directory));
    m_app.historyKey = kUploadedDebugIdsHistorySettingsKey;

    addSection(m_qt, "Qt");
    m_qt.historyKey = kUploadedQtDebugIdsHistorySettingsKey;
}

DebugFilesDialog::UploadStatus DebugFilesDialog::uploadStatus() const
{
    return m_uploadStatus;
}

void DebugFilesDialog::refreshUploadStatus()
{
    if (!m_qt.process)
        refreshQtUpload();
    for (Section* section : { &m_app, &m_qt }) {
        // without debug IDs, nothing is known to be missing
        const QStringList debugIds = currentDebugIds(*section);
        if (debugIds.isEmpty())
            section->status = UploadStatus::Unknown;
        else if (uploadedDebugIdsHistoryContains(section->historyKey, debugIds))
            section->status = UploadStatus::UpToDate;
        else
            section->status = UploadStatus::OutOfDate;
    }
    updateUploadStatus();
}

void DebugFilesDialog::prepareUpload(const QString& dsn)
{
    m_dsn = dsn;
    if (!m_qt.process)
        refreshQtUpload();
    for (Section* section : { &m_app, &m_qt }) {
        if (!section->process)
            previewUpload(*section);
    }

    show();
    raise();
    activateWindow();
}

QStringList DebugFilesDialog::currentDebugIds(const Section& section) const
{
    // the app's own debug IDs, and those of the loaded Qt libraries and plugins
    if (&section == &m_app)
        return checkedDebugIds(QCoreApplication::applicationFilePath());
    return m_qtDebugIds;
}

void DebugFilesDialog::refreshQtUpload()
{
    // only the Qt libraries and plugins loaded in this process, matched by
    // debug ID so that their separate debug files are found too
    const QString prefix = QLibraryInfo::path(QLibraryInfo::PrefixPath);
    QStringList ids;
    QStringList debugIds;
    QStringList paths;

    const sentry_value_t modules = sentry_get_modules_list();
    for (size_t i = 0; i < sentry_value_get_length(modules); ++i) {
        const sentry_value_t module = sentry_value_get_by_index(modules, i);
        const QString codeFile = QDir::fromNativeSeparators(QString::fromUtf8(
            sentry_value_as_string(sentry_value_get_by_key(module, "code_file"))));
        if (!codeFile.startsWith(prefix + '/'))
            continue;

        const QString debugId = QString::fromUtf8(sentry_value_as_string(sentry_value_get_by_key(module, "debug_id")));
        ids += { QStringLiteral("--id"), debugId };
        debugIds.append(debugId);
        const QString path = codeFile.mid(prefix.size() + 1).section('/', 0, 0);
        if (!paths.contains(path))
            paths.append(path);
    }
    sentry_value_decref(modules);
    // the SDK caches the list for events, so don't leave it one that misses
    // libraries loaded later
    sentry_clear_modulecache();

    m_qtDebugIds = normalizedDebugIds(debugIds);
    m_qt.directory = prefix;
    m_qt.summary->setText(QDir::toNativeSeparators(prefix));
    if (ids.isEmpty())
        m_qt.arguments.clear();
    else
        m_qt.arguments = QStringList{ QStringLiteral("debug-files"), QStringLiteral("upload") } + ids + paths;
    m_qt.button->setEnabled(!m_qt.arguments.isEmpty());
}

void DebugFilesDialog::previewUpload(Section& section)
{
    section.output->clear();
    if (section.arguments.isEmpty())
        return;

    section.button->setEnabled(false);
    appendOutput(section, QStringLiteral("$ cd %1\n").arg(section.directory));
    streamSentry(section, {}, section.arguments + QStringList{ QStringLiteral("--no-upload") });
}

void DebugFilesDialog::startUpload(Section& section)
{
    if (section.process)
        return;

    const UploadTarget target = uploadTargetFromDsn(m_dsn);
    section.debugIds = currentDebugIds(section);

    section.output->clear();
    appendOutput(section, QStringLiteral("$ cd %1\n").arg(section.directory));

    if (target.project.isEmpty()) {
        appendOutput(section,
            "error: initialize with a DSN that includes a project ID before uploading debug files\n");
        return;
    }

    section.button->setEnabled(false);

    if (!target.org.isEmpty()) {
        runUpload(section, {}, target.org, target.project);
        return;
    }
    if (target.url.isEmpty()) {
        detectOrg(section, {}, target.key, target.project);
        return;
    }

    // DSNs may point to an ingest host such as Relay, so prefer the server
    // of a stored login with the same host name
    QProcess* process = startSentry(section, {}, {
        QStringLiteral("auth"),
        QStringLiteral("list"),
        QStringLiteral("--json"),
    });
    connect(process, &QProcess::finished, this, [this, &section, process, target](int, QProcess::ExitStatus) {
        if (process != section.process)
            return;

        appendOutput(section, section.stderrDecoder.decode(process->readAllStandardError()));
        QString url = target.url;
        const QJsonArray accounts = QJsonDocument::fromJson(process->readAllStandardOutput()).array();
        for (const QJsonValue& account : accounts) {
            const QString host = account.toObject().value("host").toString();
            if (QUrl(host).host() == QUrl(target.url).host()) {
                url = host;
                break;
            }
        }

        process->deleteLater();
        section.process = nullptr;
        detectOrg(section, { QStringLiteral("SENTRY_HOST=%1").arg(url) }, target.key, target.project);
    });
}

void DebugFilesDialog::detectOrg(Section& section, const QStringList& variables, const QString& key, const QString& project)
{
    // self-hosted DSNs don't carry the org, so ask the server which org has the DSN key
    QProcess* process = startSentry(section, variables, {
        QStringLiteral("api"),
        QStringLiteral("projects/"),
        QStringLiteral("-f"),
        QStringLiteral("query=dsn:%1").arg(key),
    });
    connect(process, &QProcess::finished, this,
        [this, &section, process, variables, project](int exitCode, QProcess::ExitStatus) {
            if (process != section.process)
                return;

            appendOutput(section, section.stderrDecoder.decode(process->readAllStandardError()));
            const QJsonArray projects = QJsonDocument::fromJson(process->readAllStandardOutput()).array();
            const QString org = projects.at(0).toObject().value("organization").toObject().value("slug").toString();
            if (org.isEmpty()) {
                if (exitCode == 0)
                    appendOutput(section, "error: no organization found for the DSN\n");
                finishUpload(section, exitCode != 0 ? exitCode : -1);
                return;
            }

            process->deleteLater();
            section.process = nullptr;
            runUpload(section, variables, org, project);
        });
}

void DebugFilesDialog::runUpload(Section& section, QStringList variables, const QString& org, const QString& project)
{
    variables.append(QStringLiteral("SENTRY_ORG=%1").arg(org));
    variables.append(QStringLiteral("SENTRY_PROJECT=%1").arg(project));
    streamSentry(section, variables, section.arguments);
}

void DebugFilesDialog::streamSentry(Section& section, const QStringList& variables, const QStringList& arguments)
{
    QProcess* process = startSentry(section, variables, arguments);
    connect(process, &QProcess::readyReadStandardOutput, this, [this, &section, process]() {
        appendOutput(section, section.stdoutDecoder.decode(process->readAllStandardOutput()));
    });
    connect(process, &QProcess::finished, this, [this, &section, process](int exitCode, QProcess::ExitStatus) {
        if (process != section.process)
            return;

        appendOutput(section, section.stdoutDecoder.decode(process->readAllStandardOutput()));
        appendOutput(section, section.stderrDecoder.decode(process->readAllStandardError()));
        finishUpload(section, exitCode);
    });
}

QProcess* DebugFilesDialog::startSentry(Section& section, const QStringList& variables, const QStringList& arguments)
{
    appendOutput(section, QStringLiteral("$ %1\n\n").arg((variables + QStringList{ "sentry" } + arguments).join(' ')));

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    for (const QString& variable : variables) {
        const qsizetype separator = variable.indexOf('=');
        environment.insert(variable.left(separator), variable.mid(separator + 1));
    }

    auto* process = new QProcess(this);
    section.process = process;
    section.stdoutDecoder.resetState();
    section.stderrDecoder.resetState();
    process->setProgram(sentryProgram());
    process->setArguments(arguments);
    process->setProcessEnvironment(environment);
    process->setWorkingDirectory(section.directory);
    process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(process, &QProcess::readyReadStandardError, this, [this, &section, process]() {
        appendOutput(section, section.stderrDecoder.decode(process->readAllStandardError()));
    });
    connect(process, &QProcess::errorOccurred, this, [this, &section, process](QProcess::ProcessError error) {
        if (process != section.process)
            return;

        appendOutput(section, QStringLiteral("error: %1\n").arg(process->errorString()));
        if (error == QProcess::FailedToStart)
            finishUpload(section, -1);
    });

    process->start();
    return process;
}

void DebugFilesDialog::appendOutput(Section& section, const QString& text)
{
    if (text.isEmpty())
        return;

    QTextCursor cursor = section.output->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    section.output->setTextCursor(cursor);
    section.output->ensureCursorVisible();
}

void DebugFilesDialog::finishUpload(Section& section, int exitCode)
{
    if (!section.process)
        return;

    appendOutput(section, QStringLiteral("\nexit code: %1\n").arg(exitCode));
    section.process->deleteLater();
    section.process = nullptr;
    section.button->setEnabled(!section.arguments.isEmpty());

    const QStringList debugIds = section.debugIds;
    section.debugIds.clear();
    if (debugIds.isEmpty())
        return;

    if (exitCode == 0) {
        rememberUploadedDebugIds(section.historyKey, debugIds);
        section.status = UploadStatus::UpToDate;
        updateUploadStatus();
    } else {
        refreshUploadStatus();
    }
}

void DebugFilesDialog::updateUploadStatus()
{
    QPalette red = palette();
    red.setColor(QPalette::ButtonText, QColor("#ff453a"));
    for (const Section* section : { &m_app, &m_qt }) {
        QIcon icon;
        if (section->status == UploadStatus::UpToDate)
            icon = Style::makeCheckIcon(QColor("#34c759"), devicePixelRatioF());
        else if (section->status == UploadStatus::OutOfDate)
            icon = Style::makeClearIcon(red, devicePixelRatioF());
        m_tabs->setTabIcon(m_tabs->indexOf(section->page), icon);
    }

    if (m_app.status == UploadStatus::OutOfDate || m_qt.status == UploadStatus::OutOfDate)
        setUploadStatus(UploadStatus::OutOfDate);
    else if (m_app.status == UploadStatus::UpToDate || m_qt.status == UploadStatus::UpToDate)
        setUploadStatus(UploadStatus::UpToDate);
    else
        setUploadStatus(UploadStatus::Unknown);
}

void DebugFilesDialog::setUploadStatus(UploadStatus status)
{
    if (m_uploadStatus == status)
        return;

    m_uploadStatus = status;
    emit uploadStatusChanged(status);
}
