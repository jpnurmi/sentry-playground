#include "sentryplayground.h"
#include "sentrytrace.h"

#include <sentry.h>

#include <stdexcept>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#include <QtCore/qsettings.h>
#include <QtCore/qthread.h>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

#ifdef Q_OS_WINDOWS
#include <windows.h>
static int gettid()
{
    return GetCurrentProcessId();
}
#endif

#ifdef Q_OS_MACOS
#include <pthread.h>
static int gettid()
{
    uint64_t tid = 0;
    pthread_threadid_np(pthread_self(), &tid);
    return tid;
}
#endif

static void *invalid_mem = (void *)1;

static void trigger_crash()
{
    TRACE_FUNCTION();
    SentryPlayground::debug() << "trigger_crash";

    memset((char *)invalid_mem, 1, 100);
}

static void trigger_stack_overflow()
{
    TRACE_FUNCTION();
    SentryPlayground::debug() << "trigger_stack_overflow";

    alloca(1024);
    trigger_stack_overflow();
}

static void trigger_fastfail()
{
#ifdef Q_OS_WINDOWS
    TRACE_FUNCTION();
    SentryPlayground::debug() << "trigger_fast_fail";

    __fastfail(77);
#endif
}

static void trigger_assert_failure()
{
    TRACE_FUNCTION();
    SentryPlayground::debug() << "trigger_assert_failure";

    assert(false);
}

static void trigger_abort()
{
    TRACE_FUNCTION();
    SentryPlayground::debug() << "trigger_abort";

    std::abort();
}

static void trigger_exception()
{
    TRACE_FUNCTION();
    SentryPlayground::debug() << "trigger_exception";

    throw std::runtime_error("uncaught C++ exception");
}

static sentry_value_t ensure_fingerprint(sentry_value_t event)
{
    sentry_value_t fp = sentry_value_new_list();
    sentry_uuid_t uuid = sentry_uuid_new_v4();
    char buf[37];
    sentry_uuid_as_string(&uuid, buf);
    buf[36] = '\0';
    sentry_value_append(fp, sentry_value_new_string(buf));
    sentry_value_set_by_key(event, "fingerprint", fp);
    return event;
}

static sentry_value_t before_send(sentry_value_t event, void *hint, void *userdata)
{
    if (SentryPlayground::instance()->filter()) {
        sentry_value_decref(event);
        return sentry_value_new_null();
    }
    return ensure_fingerprint(event);
}

static sentry_value_t on_crash(const sentry_ucontext_t *uctx, sentry_value_t event, void *userdata)
{
    if (SentryPlayground::instance()->filter()) {
        sentry_value_decref(event);
        return sentry_value_new_null();
    }
    return ensure_fingerprint(event);
}

SentryPlayground::SentryPlayground(QObject *parent) : QObject{parent}
{
    setTag("backend", SENTRY_BACKEND);
    updateUser("name", "nobody");
    updateUser("email", "nobody@example.com");
    m_release = SENTRY_RELEASE;
    m_environment = "playground";
}

void SentryPlayground::init()
{
    debug().nospace() << "backend=" << SENTRY_BACKEND;

    QCoreApplication::setOrganizationName("Sentry");
    QCoreApplication::setOrganizationDomain("sentry.io");
    QCoreApplication::setApplicationName("Playground");

    open();
}

void SentryPlayground::open()
{
    QSettings settings;
    bool reporterEnabled = settings.value("externalCrashReporter/enabled", false).toBool();
    QString reporterPath = settings.value("externalCrashReporter/path").toString();

    sentry_options_t *options = sentry_options_new();
    sentry_options_set_dsn(options, SENTRY_DSN);
    sentry_options_set_release(options, SENTRY_RELEASE);
    sentry_options_set_environment(options, "playground");
    if (reporterEnabled && !reporterPath.isEmpty())
        sentry_options_set_external_crash_reporter_path(options, reporterPath.toUtf8().constData());
    sentry_options_set_attach_screenshot(options, true);
    sentry_options_set_before_send(options, before_send, NULL);
    sentry_options_set_on_crash(options, on_crash, NULL);
    sentry_options_set_traces_sample_rate(options, 1.0);
    sentry_options_set_require_user_consent(options, 1);
    sentry_options_set_debug(options, 1);

    sentry_init(options);
}

void SentryPlayground::close()
{
    SentryTrace::flush();
    sentry_close();
}

void SentryPlayground::reinit()
{
    TRACE_FUNCTION();
    close();
    open();
    instance()->reapplyScope();
}

SentryPlayground* SentryPlayground::instance()
{
    static SentryPlayground playground;
    return &playground;
}

QString SentryPlayground::backend()
{
    return SENTRY_BACKEND;
}

QDebug SentryPlayground::debug()
{
    QDebug debug = qDebug();
    debug.nospace() << "[sentry-playground:" << gettid() << "]";
    return debug.space();
}

bool SentryPlayground::worker() const
{
    return m_worker;
}

void SentryPlayground::setWorker(bool worker)
{
    TRACE_FUNCTION();
    if (m_worker == worker)
        return;

    m_worker = worker;
    emit workerChanged(worker);
}

bool SentryPlayground::filter() const
{
    return m_filter;
}

void SentryPlayground::setFilter(bool filter)
{
    TRACE_FUNCTION();
    if (m_filter == filter)
        return;

    m_filter = filter;
    emit filterChanged(filter);
}

Qt::CheckState SentryPlayground::consent() const
{
    switch (sentry_user_consent_get()) {
    case SENTRY_USER_CONSENT_GIVEN: return Qt::Checked;
    case SENTRY_USER_CONSENT_REVOKED: return Qt::Unchecked;
    case SENTRY_USER_CONSENT_UNKNOWN:
    default: return Qt::PartiallyChecked;
    }
}

void SentryPlayground::setConsent(Qt::CheckState consent)
{
    TRACE_FUNCTION();
    if (this->consent() == consent)
        return;

    switch (consent) {
    case Qt::Checked: sentry_user_consent_give(); break;
    case Qt::Unchecked: sentry_user_consent_revoke(); break;
    case Qt::PartiallyChecked: sentry_user_consent_reset(); break;
    }
    emit consentChanged(consent);
}

void SentryPlayground::triggerCrash()
{
    TRACE_FUNCTION();
    if (m_worker) {
        QThread::create([]() { trigger_crash(); })->start();
    } else {
        trigger_crash();
    }
}

void SentryPlayground::triggerStackOverflow()
{
    TRACE_FUNCTION();
    if (m_worker) {
        QThread::create([]() { trigger_stack_overflow(); })->start();
    } else {
        trigger_stack_overflow();
    }
}

void SentryPlayground::triggerFastfail()
{
    TRACE_FUNCTION();
    SentryTrace::flush();
    if (m_worker) {
        QThread::create([]() { trigger_fastfail(); })->start();
    } else {
        trigger_fastfail();
    }
}

void SentryPlayground::triggerAssertFailure()
{
    TRACE_FUNCTION();
    if (m_worker) {
        QThread::create([]() { trigger_assert_failure(); })->start();
    } else {
        trigger_assert_failure();
    }
}

void SentryPlayground::triggerAbort()
{
    TRACE_FUNCTION();
    if (m_worker) {
        QThread::create([]() { trigger_abort(); })->start();
    } else {
        trigger_abort();
    }
}

void SentryPlayground::triggerException()
{
    TRACE_FUNCTION();
    if (m_worker) {
        QThread::create([]() { trigger_exception(); })->start();
    } else {
        trigger_exception();
    }
}

QStringList SentryPlayground::attachments() const
{
    return m_attachments.keys();
}

void SentryPlayground::addAttachment(const QString& path)
{
    TRACE_FUNCTION();
    if (path.isEmpty() || m_attachments.contains(path))
        return;
    sentry_attachment_t *handle = sentry_attach_file(path.toUtf8().constData());
    m_attachments.insert(path, handle);
    emit attachmentsChanged(attachments());
}

void SentryPlayground::removeAttachment(const QString& path)
{
    TRACE_FUNCTION();
    auto it = m_attachments.find(path);
    if (it == m_attachments.end())
        return;
    sentry_remove_attachment(static_cast<sentry_attachment_t *>(it.value()));
    m_attachments.erase(it);
    emit attachmentsChanged(attachments());
}

QVariantMap SentryPlayground::tags() const
{
    return m_tags;
}

void SentryPlayground::setTag(const QString& key, const QString& value)
{
    TRACE_FUNCTION();
    if (m_tags.value(key).toString() == value && m_tags.contains(key))
        return;
    m_tags.insert(key, value);
    sentry_set_tag(key.toUtf8().constData(), value.toUtf8().constData());
    emit tagsChanged(m_tags);
}

void SentryPlayground::removeTag(const QString& key)
{
    TRACE_FUNCTION();
    if (m_tags.remove(key) == 0)
        return;
    sentry_remove_tag(key.toUtf8().constData());
    emit tagsChanged(m_tags);
}

QVariantMap SentryPlayground::contexts() const
{
    return m_contexts;
}

void SentryPlayground::setContext(const QString& name, const QString& value)
{
    TRACE_FUNCTION();
    if (m_contexts.value(name).toString() == value && m_contexts.contains(name))
        return;
    m_contexts.insert(name, value);
    sentry_value_t object = sentry_value_new_object();
    sentry_value_set_by_key(object, "value",
        sentry_value_new_string(value.toUtf8().constData()));
    sentry_set_context(name.toUtf8().constData(), object);
    emit contextsChanged(m_contexts);
}

void SentryPlayground::removeContext(const QString& name)
{
    TRACE_FUNCTION();
    if (m_contexts.remove(name) == 0)
        return;
    sentry_remove_context(name.toUtf8().constData());
    emit contextsChanged(m_contexts);
}

QVariantMap SentryPlayground::user() const
{
    return m_user;
}

void SentryPlayground::updateUser(const QString& field, const QString& value)
{
    TRACE_FUNCTION();
    if (m_user.value(field).toString() == value && m_user.contains(field))
        return;
    m_user.insert(field, value);

    QByteArray id = m_user.value("id").toString().toUtf8();
    QByteArray name = m_user.value("name").toString().toUtf8();
    QByteArray email = m_user.value("email").toString().toUtf8();
    QByteArray ip = m_user.value("ip_address").toString().toUtf8();
    sentry_set_user(sentry_value_new_user(
        id.isEmpty() ? nullptr : id.constData(),
        name.isEmpty() ? nullptr : name.constData(),
        email.isEmpty() ? nullptr : email.constData(),
        ip.isEmpty() ? nullptr : ip.constData()));

    emit userChanged(m_user);
}

QString SentryPlayground::release() const
{
    return m_release;
}

void SentryPlayground::setRelease(const QString& release)
{
    TRACE_FUNCTION();
    if (m_release == release)
        return;
    m_release = release;
    sentry_set_release(release.toUtf8().constData());
    emit releaseChanged(release);
}

QString SentryPlayground::environment() const
{
    return m_environment;
}

void SentryPlayground::setEnvironment(const QString& environment)
{
    TRACE_FUNCTION();
    if (m_environment == environment)
        return;
    m_environment = environment;
    sentry_set_environment(environment.toUtf8().constData());
    emit environmentChanged(environment);
}

bool SentryPlayground::session() const
{
    return m_session;
}

void SentryPlayground::setSession(bool session)
{
    TRACE_FUNCTION();
    if (m_session == session)
        return;
    m_session = session;
    if (session)
        sentry_start_session();
    else
        sentry_end_session();
    emit sessionChanged(session);
}

void SentryPlayground::captureMessage(int level, const QString& message)
{
    TRACE_FUNCTION();
    debug() << "captureMessage" << level << message;
    sentry_value_t event = sentry_value_new_message_event(
        static_cast<sentry_level_t>(level),
        "sentry-playground",
        message.toUtf8().constData());
    sentry_capture_event(event);
}

void SentryPlayground::captureException(int level, const QString& type, const QString& value)
{
    TRACE_FUNCTION();
    debug() << "captureException" << level << type << value;
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

void SentryPlayground::captureFeedback(const QString& message, const QString& name, const QString& email)
{
    TRACE_FUNCTION();
    debug() << "captureFeedback" << name << email << message;
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

void SentryPlayground::reapplyScope()
{
    TRACE_FUNCTION();
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
    QMap<QString, void*> previous = m_attachments;
    m_attachments.clear();
    for (auto it = previous.constBegin(); it != previous.constEnd(); ++it) {
        sentry_attachment_t* handle = sentry_attach_file(it.key().toUtf8().constData());
        m_attachments.insert(it.key(), handle);
    }
    if (!m_session)
        sentry_end_session();
}

void SentryPlayground::addBreadcrumb(const QString& type, int level, const QString& message)
{
    TRACE_FUNCTION();
    debug() << "addBreadcrumb" << type << level << message;
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
