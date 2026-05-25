#include "playground.h"
#include "tracing.h"

#include <sentry.h>

#include <algorithm>
#include <ctime>

#include <QtCore/qdebug.h>
#include <QtCore/qsettings.h>
#include <QtCore/qthread.h>

namespace {

int normalizedLoggerLevel(int level)
{
    switch (level) {
    case SENTRY_LEVEL_TRACE:
    case SENTRY_LEVEL_DEBUG:
    case SENTRY_LEVEL_INFO:
    case SENTRY_LEVEL_WARNING:
    case SENTRY_LEVEL_ERROR:
    case SENTRY_LEVEL_FATAL:
        return level;
    default:
        return SENTRY_LEVEL_DEBUG;
    }
}

int normalizedCacheKeepMode(int mode)
{
    switch (mode) {
    case SENTRY_CACHE_KEEP_NONE:
    case SENTRY_CACHE_KEEP_OFFLINE:
    case SENTRY_CACHE_KEEP_ALWAYS:
        return mode;
    default:
        return SENTRY_CACHE_KEEP_OFFLINE;
    }
}

} // namespace

Playground::Playground(QObject *parent) : QObject{parent}
{
    m_initOptions = loadInitOptions();
    m_tags.insert("backend", SENTRY_BACKEND);
    m_user.insert("name", "nobody");
    m_user.insert("email", "nobody@example.com");
    m_release = m_initOptions.release;
    m_environment = m_initOptions.environment;
}

Playground::InitOptions Playground::loadInitOptions()
{
    QSettings settings;

    InitOptions options;
    options.dsn = QString::fromUtf8(SENTRY_DSN);
    options.release = QString::fromUtf8(SENTRY_RELEASE);
    options.environment = "playground";
    options.dsn = settings.value("init/dsn", options.dsn).toString();
    options.databasePath = settings.value("init/databasePath", options.databasePath).toString();
    options.release = settings.value("init/release", options.release).toString();
    options.environment = settings.value("init/environment", options.environment).toString();
    options.dist = settings.value("init/dist", options.dist).toString();
    options.attachScreenshot = settings.value("init/attachScreenshot", options.attachScreenshot).toBool();
    options.tracesSampleRate = settings.value("init/tracesSampleRate", options.tracesSampleRate).toDouble();
    options.maxBreadcrumbs = std::max(0, settings.value("init/maxBreadcrumbs", options.maxBreadcrumbs).toInt());
    options.maxSpans = std::max(0, settings.value("init/maxSpans", options.maxSpans).toInt());
    options.shutdownTimeout = std::max(0, settings.value("init/shutdownTimeout", options.shutdownTimeout).toInt());
    options.requireUserConsent = settings.value("init/requireUserConsent", options.requireUserConsent).toBool();
    options.systemCrashReporterEnabled = settings.value(
        "init/systemCrashReporterEnabled", options.systemCrashReporterEnabled).toBool();
    options.enableLargeAttachments = settings.value(
        "init/enableLargeAttachments", options.enableLargeAttachments).toBool();
    options.httpRetry = settings.value("init/httpRetry", options.httpRetry).toBool();
    options.cacheKeepMode = normalizedCacheKeepMode(
        settings.value("init/cacheKeep", options.cacheKeepMode).toInt());
    options.cacheMaxItems = std::max(0, settings.value("init/cacheMaxItems", options.cacheMaxItems).toInt());
    options.cacheMaxSize = std::max(0, settings.value("init/cacheMaxSize", options.cacheMaxSize).toInt());
    options.cacheMaxAge = std::max(0, settings.value("init/cacheMaxAge", options.cacheMaxAge).toInt());
    options.debug = settings.value("init/debug", options.debug).toBool();
    options.loggerLevel = normalizedLoggerLevel(settings.value("init/loggerLevel", options.loggerLevel).toInt());
    options.externalCrashReporterEnabled = settings.value(
        "init/externalCrashReporter/enabled",
        settings.value("externalCrashReporter/enabled", options.externalCrashReporterEnabled)).toBool();
    options.externalCrashReporterPath = settings.value(
        "init/externalCrashReporter/path",
        settings.value("externalCrashReporter/path", options.externalCrashReporterPath)).toString();
    return options;
}

void Playground::saveInitOptions(const InitOptions& options)
{
    QSettings settings;
    settings.setValue("init/dsn", options.dsn);
    settings.setValue("init/databasePath", options.databasePath);
    settings.setValue("init/release", options.release);
    settings.setValue("init/environment", options.environment);
    settings.setValue("init/dist", options.dist);
    settings.setValue("init/attachScreenshot", options.attachScreenshot);
    settings.setValue("init/tracesSampleRate", options.tracesSampleRate);
    settings.setValue("init/maxBreadcrumbs", options.maxBreadcrumbs);
    settings.setValue("init/maxSpans", options.maxSpans);
    settings.setValue("init/shutdownTimeout", options.shutdownTimeout);
    settings.setValue("init/requireUserConsent", options.requireUserConsent);
    settings.setValue("init/systemCrashReporterEnabled", options.systemCrashReporterEnabled);
    settings.setValue("init/enableLargeAttachments", options.enableLargeAttachments);
    settings.setValue("init/httpRetry", options.httpRetry);
    settings.setValue("init/cacheKeep", normalizedCacheKeepMode(options.cacheKeepMode));
    settings.setValue("init/cacheMaxItems", options.cacheMaxItems);
    settings.setValue("init/cacheMaxSize", options.cacheMaxSize);
    settings.setValue("init/cacheMaxAge", options.cacheMaxAge);
    settings.setValue("init/debug", options.debug);
    settings.setValue("init/loggerLevel", normalizedLoggerLevel(options.loggerLevel));
    settings.setValue("init/externalCrashReporter/enabled", options.externalCrashReporterEnabled);
    settings.setValue("init/externalCrashReporter/path", options.externalCrashReporterPath);
    settings.setValue("externalCrashReporter/enabled", options.externalCrashReporterEnabled);
    settings.setValue("externalCrashReporter/path", options.externalCrashReporterPath);
}

void Playground::open(const InitOptions& initOptions)
{
    if (instance()->m_initialized)
        close();

    Playground* playground = instance();
    playground->m_initOptions = initOptions;
    playground->m_release = initOptions.release;
    playground->m_environment = initOptions.environment;
    saveInitOptions(initOptions);

    sentry_options_t *options = sentry_options_new();
    QByteArray dsn = playground->m_initOptions.dsn.toUtf8();
    QByteArray databasePath = playground->m_initOptions.databasePath.toUtf8();
    QByteArray release = playground->m_initOptions.release.toUtf8();
    QByteArray environment = playground->m_initOptions.environment.toUtf8();
    QByteArray dist = playground->m_initOptions.dist.toUtf8();
    QByteArray reporterPath = playground->m_initOptions.externalCrashReporterPath.toUtf8();
    if (!dsn.isEmpty())
        sentry_options_set_dsn(options, dsn.constData());
    if (!databasePath.isEmpty())
        sentry_options_set_database_path(options, databasePath.constData());
    if (!release.isEmpty())
        sentry_options_set_release(options, release.constData());
    if (!environment.isEmpty())
        sentry_options_set_environment(options, environment.constData());
    if (!dist.isEmpty())
        sentry_options_set_dist(options, dist.constData());
    if (playground->m_initOptions.externalCrashReporterEnabled && !reporterPath.isEmpty())
        sentry_options_set_external_crash_reporter_path(options, reporterPath.constData());
    sentry_options_set_attach_screenshot(options, playground->m_initOptions.attachScreenshot);
    sentry_options_set_traces_sample_rate(options, playground->m_initOptions.tracesSampleRate);
    sentry_options_set_max_breadcrumbs(
        options, static_cast<size_t>(std::max(0, playground->m_initOptions.maxBreadcrumbs)));
    sentry_options_set_max_spans(
        options, static_cast<size_t>(std::max(0, playground->m_initOptions.maxSpans)));
    sentry_options_set_shutdown_timeout(
        options, static_cast<uint64_t>(std::max(0, playground->m_initOptions.shutdownTimeout)));
    sentry_options_set_require_user_consent(options, playground->m_initOptions.requireUserConsent);
    sentry_options_set_system_crash_reporter_enabled(
        options, playground->m_initOptions.systemCrashReporterEnabled);
    sentry_options_set_crashpad_wait_for_upload(options, true);
    sentry_options_set_enable_large_attachments(options, playground->m_initOptions.enableLargeAttachments);
    sentry_options_set_http_retry(options, playground->m_initOptions.httpRetry);
    sentry_options_set_cache_keep(options, normalizedCacheKeepMode(playground->m_initOptions.cacheKeepMode));
    sentry_options_set_cache_max_items(
        options, static_cast<size_t>(std::max(0, playground->m_initOptions.cacheMaxItems)));
    sentry_options_set_cache_max_size(
        options, static_cast<size_t>(std::max(0, playground->m_initOptions.cacheMaxSize)));
    sentry_options_set_cache_max_age(
        options, static_cast<time_t>(std::max(0, playground->m_initOptions.cacheMaxAge)));
    sentry_options_set_debug(options, playground->m_initOptions.debug);
    sentry_options_set_logger_level(
        options, static_cast<sentry_level_t>(normalizedLoggerLevel(playground->m_initOptions.loggerLevel)));

    sentry_options_set_before_send(options, [](sentry_value_t event, void *hint, void *userdata) {
        if (Playground::instance()->filter()) {
            sentry_value_decref(event);
            return sentry_value_new_null();
        }
        return event;
    }, NULL);

    sentry_options_set_on_crash(options, [](const sentry_ucontext_t *uctx, sentry_value_t event, void *userdata) {
        if (Playground::instance()->filter()) {
            sentry_value_decref(event);
            return sentry_value_new_null();
        }
        return event;
    }, NULL);

    sentry_init(options);
    playground->m_initialized = true;
    playground->m_hasInitialized = true;
    Tracing::setEnabled(true);

    sentry_uuid_t uuid = sentry_uuid_new_v4();
    char buf[37];
    sentry_uuid_as_string(&uuid, buf);
    buf[36] = '\0';
    sentry_set_fingerprint(buf, NULL);

    playground->reapplyScope();
    emit playground->initOptionsChanged(playground->m_initOptions);
    emit playground->releaseChanged(playground->m_release);
    emit playground->environmentChanged(playground->m_environment);
    emit playground->initializedChanged(true);
}

void Playground::close()
{
    Playground* playground = instance();
    if (!playground->m_initialized)
        return;

    Tracing::setEnabled(false);
    Tracing::flush();
    sentry_close();
    playground->m_initialized = false;
    emit playground->initializedChanged(false);
}

void Playground::reinit(const InitOptions& options)
{
    TRACE_FUNCTION();

    close();
    open(options);
}

Playground* Playground::instance()
{
    static Playground playground;
    return &playground;
}

QString Playground::backend()
{
    return SENTRY_BACKEND;
}

bool Playground::initialized() const
{
    return m_initialized;
}

bool Playground::hasInitialized() const
{
    return m_hasInitialized;
}

Playground::InitOptions Playground::initOptions() const
{
    return m_initOptions;
}

bool Playground::worker() const
{
    return m_worker;
}

void Playground::setWorker(bool worker)
{
    TRACE_FUNCTION();

    if (m_worker == worker)
        return;

    m_worker = worker;
    emit workerChanged(worker);
}

bool Playground::filter() const
{
    return m_filter;
}

void Playground::setFilter(bool filter)
{
    TRACE_FUNCTION();

    if (m_filter == filter)
        return;

    m_filter = filter;
    emit filterChanged(filter);
}

Qt::CheckState Playground::consent() const
{
    if (!m_initialized)
        return m_consent;

    switch (sentry_user_consent_get()) {
    case SENTRY_USER_CONSENT_GIVEN: return Qt::Checked;
    case SENTRY_USER_CONSENT_REVOKED: return Qt::Unchecked;
    case SENTRY_USER_CONSENT_UNKNOWN:
    default: return Qt::PartiallyChecked;
    }
}

void Playground::setConsent(Qt::CheckState consent)
{
    TRACE_FUNCTION();

    if (this->consent() == consent)
        return;

    m_consent = consent;
    if (!m_initialized) {
        emit consentChanged(consent);
        return;
    }

    applyConsent();
    emit consentChanged(consent);
}

void Playground::applyConsent()
{
    switch (m_consent) {
    case Qt::Checked: sentry_user_consent_give(); break;
    case Qt::Unchecked: sentry_user_consent_revoke(); break;
    case Qt::PartiallyChecked: sentry_user_consent_reset(); break;
    }
}

QStringList Playground::attachments() const
{
    return m_attachments.keys();
}

void Playground::addAttachment(const QString& path)
{
    TRACE_FUNCTION();

    if (path.isEmpty() || m_attachments.contains(path))
        return;
    sentry_attachment_t *handle = nullptr;
    if (m_initialized)
        handle = sentry_attach_file(path.toUtf8().constData());
    m_attachments.insert(path, handle);
    emit attachmentsChanged(attachments());
}

void Playground::removeAttachment(const QString& path)
{
    TRACE_FUNCTION();

    auto it = m_attachments.find(path);
    if (it == m_attachments.end())
        return;
    if (m_initialized && it.value())
        sentry_remove_attachment(static_cast<sentry_attachment_t *>(it.value()));
    m_attachments.erase(it);
    emit attachmentsChanged(attachments());
}

QVariantMap Playground::tags() const
{
    return m_tags;
}

void Playground::setTag(const QString& key, const QString& value)
{
    TRACE_FUNCTION();

    if (m_tags.value(key).toString() == value && m_tags.contains(key))
        return;
    m_tags.insert(key, value);
    if (m_initialized)
        sentry_set_tag(key.toUtf8().constData(), value.toUtf8().constData());
    emit tagsChanged(m_tags);
}

void Playground::removeTag(const QString& key)
{
    TRACE_FUNCTION();

    if (m_tags.remove(key) == 0)
        return;
    if (m_initialized)
        sentry_remove_tag(key.toUtf8().constData());
    emit tagsChanged(m_tags);
}

QVariantMap Playground::contexts() const
{
    return m_contexts;
}

void Playground::setContext(const QString& name, const QString& value)
{
    TRACE_FUNCTION();

    if (m_contexts.value(name).toString() == value && m_contexts.contains(name))
        return;
    m_contexts.insert(name, value);
    if (m_initialized) {
        sentry_value_t object = sentry_value_new_object();
        sentry_value_set_by_key(object, "value",
            sentry_value_new_string(value.toUtf8().constData()));
        sentry_set_context(name.toUtf8().constData(), object);
    }
    emit contextsChanged(m_contexts);
}

void Playground::removeContext(const QString& name)
{
    TRACE_FUNCTION();

    if (m_contexts.remove(name) == 0)
        return;
    if (m_initialized)
        sentry_remove_context(name.toUtf8().constData());
    emit contextsChanged(m_contexts);
}

QVariantMap Playground::user() const
{
    return m_user;
}

void Playground::updateUser(const QString& field, const QString& value)
{
    TRACE_FUNCTION();

    if (m_user.value(field).toString() == value && m_user.contains(field))
        return;
    m_user.insert(field, value);

    if (m_initialized) {
        QByteArray id = m_user.value("id").toString().toUtf8();
        QByteArray name = m_user.value("name").toString().toUtf8();
        QByteArray email = m_user.value("email").toString().toUtf8();
        QByteArray ip = m_user.value("ip_address").toString().toUtf8();
        sentry_set_user(sentry_value_new_user(
            id.isEmpty() ? nullptr : id.constData(),
            name.isEmpty() ? nullptr : name.constData(),
            email.isEmpty() ? nullptr : email.constData(),
            ip.isEmpty() ? nullptr : ip.constData()));
    }

    emit userChanged(m_user);
}

QString Playground::release() const
{
    return m_release;
}

void Playground::setRelease(const QString& release)
{
    TRACE_FUNCTION();

    if (m_release == release)
        return;
    m_release = release;
    m_initOptions.release = release;
    if (m_initialized)
        sentry_set_release(release.toUtf8().constData());
    emit releaseChanged(release);
    emit initOptionsChanged(m_initOptions);
}

QString Playground::environment() const
{
    return m_environment;
}

void Playground::setEnvironment(const QString& environment)
{
    TRACE_FUNCTION();

    if (m_environment == environment)
        return;
    m_environment = environment;
    m_initOptions.environment = environment;
    if (m_initialized)
        sentry_set_environment(environment.toUtf8().constData());
    emit environmentChanged(environment);
    emit initOptionsChanged(m_initOptions);
}

bool Playground::session() const
{
    return m_session;
}

void Playground::setSession(bool session)
{
    TRACE_FUNCTION();

    if (m_session == session)
        return;
    m_session = session;
    if (!m_initialized) {
        emit sessionChanged(session);
        return;
    }
    if (session)
        sentry_start_session();
    else
        sentry_end_session();
    emit sessionChanged(session);
}

void Playground::captureMessage(int level, const QString& message)
{
    TRACE_FUNCTION();
    if (!m_initialized)
        return;
    qDebug() << "captureMessage" << level << message;

    sentry_value_t event = sentry_value_new_message_event(
        static_cast<sentry_level_t>(level),
        "sentry-playground",
        message.toUtf8().constData());
    sentry_capture_event(event);
}

void Playground::captureException(int level, const QString& type, const QString& value)
{
    TRACE_FUNCTION();
    if (!m_initialized)
        return;
    qDebug() << "captureException" << level << type << value;

    const char* levelStr = nullptr;
    switch (static_cast<sentry_level_t>(level)) {
    case SENTRY_LEVEL_TRACE: levelStr = "trace"; break;
    case SENTRY_LEVEL_DEBUG: levelStr = "debug"; break;
    case SENTRY_LEVEL_INFO: levelStr = "info"; break;
    case SENTRY_LEVEL_WARNING: levelStr = "warning"; break;
    case SENTRY_LEVEL_ERROR: levelStr = "error"; break;
    case SENTRY_LEVEL_FATAL: levelStr = "fatal"; break;
    }
    sentry_value_t event = sentry_value_new_event();
    if (levelStr)
        sentry_value_set_by_key(event, "level", sentry_value_new_string(levelStr));
    sentry_value_t exc = sentry_value_new_exception(
        type.toUtf8().constData(),
        value.toUtf8().constData());
    sentry_value_set_stacktrace(exc, nullptr, 0);
    sentry_event_add_exception(event, exc);
    sentry_capture_event(event);
}

void Playground::captureFeedback(const QString& message, const QString& name, const QString& email)
{
    TRACE_FUNCTION();
    if (!m_initialized)
        return;
    qDebug() << "captureFeedback" << name << email << message;

    QByteArray msg = message.toUtf8();
    QByteArray nm = name.toUtf8();
    QByteArray em = email.toUtf8();
    sentry_value_t feedback = sentry_value_new_feedback(
        msg.constData(),
        em.isEmpty() ? nullptr : em.constData(),
        nm.isEmpty() ? nullptr : nm.constData(),
        nullptr);
    sentry_capture_feedback(feedback);
}

void Playground::reapplyScope()
{
    TRACE_FUNCTION();

    if (!m_initialized)
        return;

    for (auto it = m_tags.constBegin(); it != m_tags.constEnd(); ++it)
        sentry_set_tag(it.key().toUtf8().constData(), it.value().toString().toUtf8().constData());
    for (auto it = m_contexts.constBegin(); it != m_contexts.constEnd(); ++it) {
        sentry_value_t object = sentry_value_new_object();
        sentry_value_set_by_key(object, "value",
            sentry_value_new_string(it.value().toString().toUtf8().constData()));
        sentry_set_context(it.key().toUtf8().constData(), object);
    }
    QByteArray id = m_user.value("id").toString().toUtf8();
    QByteArray name = m_user.value("name").toString().toUtf8();
    QByteArray email = m_user.value("email").toString().toUtf8();
    QByteArray ip = m_user.value("ip_address").toString().toUtf8();
    sentry_set_user(sentry_value_new_user(
        id.isEmpty() ? nullptr : id.constData(),
        name.isEmpty() ? nullptr : name.constData(),
        email.isEmpty() ? nullptr : email.constData(),
        ip.isEmpty() ? nullptr : ip.constData()));
    if (!m_release.isEmpty())
        sentry_set_release(m_release.toUtf8().constData());
    if (!m_environment.isEmpty())
        sentry_set_environment(m_environment.toUtf8().constData());
    applyConsent();
    QMap<QString, void*> previous = m_attachments;
    m_attachments.clear();
    for (auto it = previous.constBegin(); it != previous.constEnd(); ++it) {
        sentry_attachment_t* handle = sentry_attach_file(it.key().toUtf8().constData());
        m_attachments.insert(it.key(), handle);
    }
    if (!m_session)
        sentry_end_session();
}

void Playground::addBreadcrumb(const QString& type, int level, const QString& message)
{
    TRACE_FUNCTION();
    if (!m_initialized)
        return;
    qDebug() << "addBreadcrumb" << type << level << message;

    const char* levelStr = nullptr;
    switch (static_cast<sentry_level_t>(level)) {
    case SENTRY_LEVEL_TRACE: levelStr = "trace"; break;
    case SENTRY_LEVEL_DEBUG: levelStr = "debug"; break;
    case SENTRY_LEVEL_INFO: levelStr = "info"; break;
    case SENTRY_LEVEL_WARNING: levelStr = "warning"; break;
    case SENTRY_LEVEL_ERROR: levelStr = "error"; break;
    case SENTRY_LEVEL_FATAL: levelStr = "fatal"; break;
    }
    sentry_value_t crumb = sentry_value_new_breadcrumb(
        type.toUtf8().constData(),
        message.toUtf8().constData());
    if (levelStr)
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string(levelStr));
    sentry_add_breadcrumb(crumb);
}
