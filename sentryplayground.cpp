#include "sentryplayground.h"
#include "sentryapp.h"
#include "sentrytrace.h"
#include "sentrywindow.h"

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
}

QGuiApplication* SentryPlayground::init(int& argc, char* argv[])
{
    TRACE_FUNCTION();

    auto *app = new SentryApp(argc, argv);

    QObject::connect(app, &QCoreApplication::aboutToQuit, SentryPlayground::instance(), &SentryPlayground::uninit);
    return app;
}

void SentryPlayground::uninit()
{
    SentryTrace::flush();
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

void SentryPlayground::showWindow()
{
    TRACE_FUNCTION();
    SentryWindow* window = new SentryWindow();
    window->show();
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
    uninit();
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

void SentryPlayground::captureMessage(int level)
{
    TRACE_FUNCTION();
    debug() << "captureMessage" << level;
    sentry_value_t event = sentry_value_new_message_event(
        static_cast<sentry_level_t>(level),
        "sentry-playground",
        "Hello from Sentry Playground");
    sentry_capture_event(event);
}
