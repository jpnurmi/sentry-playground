#include "debugfilesdialog.h"

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
static constexpr int kMaxUploadedDebugIdSets = 10;

struct UploadTarget
{
    QString url;
    QString key;
    QString org;
    QString project;
};

static QString sentryProgram()
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
    process.setProgram(sentryProgram());
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

static QJsonArray uploadedDebugIdsHistory()
{
    QSettings settings;
    QJsonArray history = QJsonDocument::fromJson(
        settings.value(kUploadedDebugIdsHistorySettingsKey).toByteArray()).array();

    const QStringList legacyDebugIds = normalizedDebugIds(
        settings.value(kUploadedDebugIdsSettingsKey).toStringList());
    if (!legacyDebugIds.isEmpty()) {
        QJsonArray legacyEntry;
        for (const QString& debugId : legacyDebugIds)
            legacyEntry.append(debugId);
        history.prepend(legacyEntry);
        settings.remove(kUploadedDebugIdsSettingsKey);
    }

    return history;
}

static bool uploadedDebugIdsHistoryContains(const QStringList& debugIds)
{
    if (debugIds.isEmpty())
        return false;

    const QJsonArray history = uploadedDebugIdsHistory();
    for (const QJsonValue& entry : history) {
        QStringList uploadedDebugIds;
        for (const QJsonValue& debugId : entry.toArray())
            uploadedDebugIds.append(debugId.toString());
        if (normalizedDebugIds(uploadedDebugIds) == debugIds)
            return true;
    }
    return false;
}

static void rememberUploadedDebugIds(const QStringList& debugIds)
{
    QJsonArray nextEntry;
    for (const QString& debugId : debugIds)
        nextEntry.append(debugId);

    QJsonArray nextHistory;
    nextHistory.append(nextEntry);
    for (const QJsonValue& entry : uploadedDebugIdsHistory()) {
        QStringList uploadedDebugIds;
        for (const QJsonValue& debugId : entry.toArray())
            uploadedDebugIds.append(debugId.toString());
        if (normalizedDebugIds(uploadedDebugIds) != debugIds)
            nextHistory.append(entry);
        if (nextHistory.size() >= kMaxUploadedDebugIdSets)
            break;
    }

    QSettings().setValue(kUploadedDebugIdsHistorySettingsKey,
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

    auto* tabs = new QTabWidget(this);
    layout->addWidget(tabs);

    auto addSection = [this, tabs](Section& section, const QString& title) {
        auto* page = new QWidget(tabs);
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
        tabs->addTab(page, title);
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

    addSection(m_qt, "Qt");
}

DebugFilesDialog::UploadStatus DebugFilesDialog::uploadStatus() const
{
    return m_uploadStatus;
}

void DebugFilesDialog::refreshUploadStatus()
{
    const QStringList debugIds = checkedDebugIds(QCoreApplication::applicationFilePath());
    setUploadStatus(uploadedDebugIdsHistoryContains(debugIds)
        ? UploadStatus::UpToDate
        : UploadStatus::OutOfDate);
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

void DebugFilesDialog::refreshQtUpload()
{
    // only the Qt libraries and plugins loaded in this process, matched by
    // debug ID so that their separate debug files are found too
    const QString prefix = QLibraryInfo::path(QLibraryInfo::PrefixPath);
    QStringList ids;
    QStringList paths;

    const sentry_value_t modules = sentry_get_modules_list();
    for (size_t i = 0; i < sentry_value_get_length(modules); ++i) {
        const sentry_value_t module = sentry_value_get_by_index(modules, i);
        const QString codeFile = QDir::fromNativeSeparators(QString::fromUtf8(
            sentry_value_as_string(sentry_value_get_by_key(module, "code_file"))));
        if (!codeFile.startsWith(prefix + '/'))
            continue;

        ids += {
            QStringLiteral("--id"),
            QString::fromUtf8(sentry_value_as_string(sentry_value_get_by_key(module, "debug_id"))),
        };
        const QString path = codeFile.mid(prefix.size() + 1).section('/', 0, 0);
        if (!paths.contains(path))
            paths.append(path);
    }
    sentry_value_decref(modules);

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
    // the upload status tracks the app's own debug IDs
    if (&section == &m_app)
        section.debugIds = checkedDebugIds(QCoreApplication::applicationFilePath());

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

        appendOutput(section, QString::fromLocal8Bit(process->readAllStandardError()));
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

            appendOutput(section, QString::fromLocal8Bit(process->readAllStandardError()));
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
        appendOutput(section, QString::fromLocal8Bit(process->readAllStandardOutput()));
    });
    connect(process, &QProcess::finished, this, [this, &section, process](int exitCode, QProcess::ExitStatus) {
        if (process != section.process)
            return;

        appendOutput(section, QString::fromLocal8Bit(process->readAllStandardOutput()));
        appendOutput(section, QString::fromLocal8Bit(process->readAllStandardError()));
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
    process->setProgram(sentryProgram());
    process->setArguments(arguments);
    process->setProcessEnvironment(environment);
    process->setWorkingDirectory(section.directory);
    process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(process, &QProcess::readyReadStandardError, this, [this, &section, process]() {
        appendOutput(section, QString::fromLocal8Bit(process->readAllStandardError()));
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
        rememberUploadedDebugIds(debugIds);
        setUploadStatus(UploadStatus::UpToDate);
    } else {
        refreshUploadStatus();
    }
}

void DebugFilesDialog::setUploadStatus(UploadStatus status)
{
    if (m_uploadStatus == status)
        return;

    m_uploadStatus = status;
    emit uploadStatusChanged(status);
}
