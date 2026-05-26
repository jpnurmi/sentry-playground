#include "playground.h"
#include "tracing.h"

#include <sentry.h>

#include <QtCore/qdebug.h>
#include <QtCore/qsettings.h>

namespace {

static constexpr auto kOptionsSettingsKey = "init/options";

} // namespace

Playground::Playground(QObject *parent) : QObject{parent}
{
    m_options.load(QSettings().value(kOptionsSettingsKey).toByteArray());
    m_tags.insert("backend", SENTRY_BACKEND);
    m_user.insert("name", "nobody");
    m_user.insert("email", "nobody@example.com");
    m_release = m_options.release;
    m_environment = m_options.environment;
}

void Playground::open(const Options& options)
{
    if (instance()->m_initialized)
        close();

    Playground* playground = instance();
    playground->m_options = options;
    playground->m_release = options.release;
    playground->m_environment = options.environment;
    QSettings().setValue(kOptionsSettingsKey, options.save());

    sentry_options_t *opt = playground->m_options.toNative();
    sentry_options_set_before_send(opt, [](sentry_value_t event, void *hint, void *userdata) {
        if (Playground::instance()->filter()) {
            sentry_value_decref(event);
            return sentry_value_new_null();
        }
        return event;
    }, NULL);

    sentry_options_set_on_crash(opt, [](const sentry_ucontext_t *uctx, sentry_value_t event, void *userdata) {
        if (Playground::instance()->filter()) {
            sentry_value_decref(event);
            return sentry_value_new_null();
        }
        return event;
    }, NULL);
    sentry_init(opt);

    playground->m_initialized = true;
    playground->m_wasInitialized = true;
    Tracing::setEnabled(true);

    sentry_uuid_t uuid = sentry_uuid_new_v4();
    char buf[37];
    sentry_uuid_as_string(&uuid, buf);
    buf[36] = '\0';
    sentry_set_fingerprint(buf, NULL);

    playground->reapplyScope();
    emit playground->optionsChanged(playground->m_options);
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

void Playground::reinit(const Options& options)
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

bool Playground::isInitialized() const
{
    return m_initialized;
}

bool Playground::wasInitialized() const
{
    return m_wasInitialized;
}

Options Playground::options() const
{
    return m_options;
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
    m_options.release = release;
    if (m_initialized)
        sentry_set_release(release.toUtf8().constData());
    emit releaseChanged(release);
    emit optionsChanged(m_options);
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
    m_options.environment = environment;
    if (m_initialized)
        sentry_set_environment(environment.toUtf8().constData());
    emit environmentChanged(environment);
    emit optionsChanged(m_options);
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
