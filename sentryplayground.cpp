#include "sentryplayground.h"
#ifdef HAVE_WIDGETS
#include "qtwidgetswindow.h"
#endif
#ifdef HAVE_OPENGL
#include "qtopenglwindow.h"
#endif

#include <QtCore/qdebug.h>
#include <QtCore/qthread.h>
#include <QtQml/qqmlapplicationengine.h>

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
    SentryPlayground::debug() << "trigger_crash";

    memset((char *)invalid_mem, 1, 100);
}

static void trigger_stack_overflow()
{
    SentryPlayground::debug() << "trigger_stack_overflow";

    alloca(1024);
    trigger_stack_overflow();
}

static void trigger_fastfail()
{
#ifdef Q_OS_WINDOWS
    SentryPlayground::debug() << "trigger_fast_fail";

    __fastfail(77);
#endif
}

static void trigger_assert_failure()
{
    SentryPlayground::debug() << "trigger_assert_failure";

    assert(false);
}

static void trigger_abort()
{
    SentryPlayground::debug() << "trigger_abort";

    std::abort();
}

SentryPlayground::SentryPlayground(QObject *parent) : QObject{parent}
{
}

QGuiApplication* SentryPlayground::init(int& argc, char* argv[])
{

#ifdef HAVE_WIDGETS
    QApplication* app = new QApplication(argc, argv);
#else
    QGuiApplication* app = new QGuiApplication(argc, argv);
#endif
#ifdef HAVE_QUICK
    qmlRegisterSingletonInstance("SentryPlayground", 1, 0, "SentryPlayground", SentryPlayground::instance());
#endif
    return app;
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
    if (m_worker == worker)
        return;

    m_worker = worker;
    emit workerChanged(worker);
}

bool SentryPlayground::haveWidgets()
{
#ifdef HAVE_WIDGETS
    return true;
#else
    return false;
#endif
}

bool SentryPlayground::haveQuick()
{
#ifdef HAVE_QUICK
    return true;
#else
    return false;
#endif
}

bool SentryPlayground::haveOpenGL()
{
#ifdef HAVE_OPENGL
    return true;
#else
    return false;
#endif
}

void SentryPlayground::viewWidgets()
{
#ifdef HAVE_WIDGETS
    QtWidgetsWindow* subwindow = new QtWidgetsWindow();
    subwindow->show();
#endif
}

void SentryPlayground::viewQuick()
{
#ifdef HAVE_QUICK
    QQmlApplicationEngine* engine = new QQmlApplicationEngine(qApp);
    engine->load(QUrl("qrc:/qtquickwindow.qml"));
#endif
}

void SentryPlayground::viewOpenGL()
{
#ifdef HAVE_OPENGL
    QtOpenGLWindow* subwindow = new QtOpenGLWindow();
    subwindow->show();
#endif
}

void SentryPlayground::showWindow()
{
#if defined(HAVE_WIDGETS)
    SentryPlayground::viewWidgets();
#elif defined(HAVE_QUICK)
    SentryPlayground::viewQuick();
#elif defined(HAVE_OPENGL)
    SentryPlayground::viewOpenGL();
#else
    #error Either QtWidgets, QtQuick, QtOpenGL must be enabled.
#endif
}

void SentryPlayground::triggerCrash()
{
    if (m_worker) {
        QThread::create([]() { trigger_crash(); })->start();
    } else {
        trigger_crash();
    }
}

void SentryPlayground::triggerStackOverflow()
{
    if (m_worker) {
        QThread::create([]() { trigger_stack_overflow(); })->start();
    } else {
        trigger_stack_overflow();
    }
}

void SentryPlayground::triggerFastfail()
{
    if (m_worker) {
        QThread::create([]() { trigger_fastfail(); })->start();
    } else {
        trigger_fastfail();
    }
}

void SentryPlayground::triggerAssertFailure()
{
    if (m_worker) {
        QThread::create([]() { trigger_assert_failure(); })->start();
    } else {
        trigger_assert_failure();
    }
}

void SentryPlayground::triggerAbort()
{
    if (m_worker) {
        QThread::create([]() { trigger_abort(); })->start();
    } else {
        trigger_abort();
    }
}
