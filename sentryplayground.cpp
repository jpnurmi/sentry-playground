#include "sentryplayground.h"
#include "sentrytrace.h"

#include <sentry.h>

#include <QtCore/qdebug.h>
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

SentryPlayground::SentryPlayground(QObject *parent) : QObject{parent}
{
    setTag("backend", SENTRY_BACKEND);
    updateUser("name", "nobody");
    updateUser("email", "nobody@example.com");
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
