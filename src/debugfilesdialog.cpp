#include "debugfilesdialog.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qprocess.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qsettings.h>
#include <QtCore/qurl.h>
#include <QtGui/qfontdatabase.h>
#include <QtGui/qtextcursor.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qplaintextedit.h>

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
    process.setProgram("sentry");
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
    resize(760, 360);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(0);

    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_output->setMaximumBlockCount(2000);
    m_output->setPlaceholderText("sentry output");
    m_output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_output->setStyleSheet(
        "QPlainTextEdit {"
        " background: #111318;"
        " color: #e6edf3;"
        " border: 1px solid #4b5563;"
        " border-radius: 4px;"
        " padding: 6px;"
        " selection-background-color: #3b82f6;"
        " }");
    layout->addWidget(m_output);
}

DebugFilesDialog::UploadStatus DebugFilesDialog::uploadStatus() const
{
    return m_uploadStatus;
}

void DebugFilesDialog::refreshUploadStatus()
{
    m_currentDebugIds = checkedDebugIds(QCoreApplication::applicationFilePath());
    setUploadStatus(uploadedDebugIdsHistoryContains(m_currentDebugIds)
        ? UploadStatus::UpToDate
        : UploadStatus::OutOfDate);
}

void DebugFilesDialog::upload(const QString& dsn)
{
    show();
    raise();
    activateWindow();

    if (m_uploadProcess)
        return;

    const UploadTarget target = uploadTargetFromDsn(dsn);
    const QString workingDirectory = QCoreApplication::applicationDirPath();
    m_currentDebugIds = checkedDebugIds(QCoreApplication::applicationFilePath());

    m_output->clear();
    appendOutput(QStringLiteral("$ cd %1\n").arg(workingDirectory));

    if (target.project.isEmpty()) {
        appendOutput(
            "error: initialize with a DSN that includes a project ID before uploading debug files\n");
        return;
    }

    QStringList variables;
    if (!target.url.isEmpty())
        variables.append(QStringLiteral("SENTRY_HOST=%1").arg(target.url));

    if (!target.org.isEmpty()) {
        runUpload(variables, target.org, target.project);
        return;
    }

    // self-hosted DSNs don't carry the org, so ask the server which org has the DSN key
    QProcess* process = startSentry(variables, {
        QStringLiteral("api"),
        QStringLiteral("projects/"),
        QStringLiteral("-f"),
        QStringLiteral("query=dsn:%1").arg(target.key),
    });
    connect(process, &QProcess::finished, this,
        [this, process, variables, target](int exitCode, QProcess::ExitStatus) {
            if (process != m_uploadProcess)
                return;

            appendOutput(QString::fromLocal8Bit(process->readAllStandardError()));
            const QJsonArray projects = QJsonDocument::fromJson(process->readAllStandardOutput()).array();
            const QString org = projects.at(0).toObject().value("organization").toObject().value("slug").toString();
            if (org.isEmpty()) {
                if (exitCode == 0)
                    appendOutput("error: no organization found for the DSN\n");
                finishUpload(exitCode != 0 ? exitCode : -1);
                return;
            }

            process->deleteLater();
            m_uploadProcess = nullptr;
            runUpload(variables, org, target.project);
        });
}

void DebugFilesDialog::runUpload(QStringList variables, const QString& org, const QString& project)
{
    variables.append(QStringLiteral("SENTRY_ORG=%1").arg(org));
    variables.append(QStringLiteral("SENTRY_PROJECT=%1").arg(project));

    QProcess* process = startSentry(variables, {
        QStringLiteral("debug-files"),
        QStringLiteral("upload"),
        QStringLiteral("."),
    });
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        appendOutput(QString::fromLocal8Bit(process->readAllStandardOutput()));
    });
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus) {
        if (process != m_uploadProcess)
            return;

        appendOutput(QString::fromLocal8Bit(process->readAllStandardOutput()));
        appendOutput(QString::fromLocal8Bit(process->readAllStandardError()));
        finishUpload(exitCode);
    });
}

QProcess* DebugFilesDialog::startSentry(const QStringList& variables, const QStringList& arguments)
{
    appendOutput(QStringLiteral("$ %1\n\n").arg((variables + QStringList{ "sentry" } + arguments).join(' ')));

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    for (const QString& variable : variables) {
        const qsizetype separator = variable.indexOf('=');
        environment.insert(variable.left(separator), variable.mid(separator + 1));
    }

    auto* process = new QProcess(this);
    m_uploadProcess = process;
    process->setProgram("sentry");
    process->setArguments(arguments);
    process->setProcessEnvironment(environment);
    process->setWorkingDirectory(QCoreApplication::applicationDirPath());
    process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        appendOutput(QString::fromLocal8Bit(process->readAllStandardError()));
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (process != m_uploadProcess)
            return;

        appendOutput(QStringLiteral("error: %1\n").arg(process->errorString()));
        if (error == QProcess::FailedToStart)
            finishUpload(-1);
    });

    process->start();
    return process;
}

void DebugFilesDialog::appendOutput(const QString& text)
{
    if (text.isEmpty())
        return;

    QTextCursor cursor = m_output->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    m_output->setTextCursor(cursor);
    m_output->ensureCursorVisible();
}

void DebugFilesDialog::finishUpload(int exitCode)
{
    if (!m_uploadProcess)
        return;

    appendOutput(QStringLiteral("\nexit code: %1\n").arg(exitCode));
    m_uploadProcess->deleteLater();
    m_uploadProcess = nullptr;

    if (exitCode == 0 && !m_currentDebugIds.isEmpty()) {
        rememberUploadedDebugIds(m_currentDebugIds);
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
