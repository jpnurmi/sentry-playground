#include "sentryplayground.h"
#ifdef HAVE_WIDGETS
#include "qtwidgetswindow.h"
#endif
#ifdef HAVE_OPENGL
#include "qtopenglwindow.h"
#endif
#ifdef HAVE_VULKAN
#include "qtvulkanwindow.h"
#endif

#include <QtCore/qdebug.h>
#include <QtCore/qthread.h>
#ifdef HAVE_QUICK
#include <QtQml/qqmlapplicationengine.h>
#endif

sentry_transaction_t *SentryPlayground::s_tx = nullptr;
thread_local QStack<sentry_span_t *> SentryPlayground::t_spans;

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
    sentry_transaction_context_t *ctx
        = sentry_transaction_context_new("main", "function");
    s_tx = sentry_transaction_start(ctx, sentry_value_new_null());
    sentry_set_transaction_object(s_tx);

    TRACE_FUNCTION();

#ifdef HAVE_WIDGETS
    QApplication* app = new QApplication(argc, argv);
#else
    QGuiApplication* app = new QGuiApplication(argc, argv);
#endif
#ifdef HAVE_QUICK
    qmlRegisterSingletonInstance("SentryPlayground", 1, 0, "SentryPlayground", SentryPlayground::instance());
#endif

    QObject::connect(app, &QCoreApplication::aboutToQuit, SentryPlayground::instance(), &SentryPlayground::uninit);
    return app;
}

void SentryPlayground::uninit()
{
    while (!t_spans.isEmpty()) {
        sentry_span_t *span = t_spans.pop();
        if (span)
            sentry_span_finish(span);
    }
    sentry_transaction_t *tx = s_tx;
    if (!tx)
        return;
    s_tx = nullptr;
    sentry_transaction_finish(tx);
    sentry_flush(2000);
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

void SentryPlayground::traceBegin(const char *op, const char *description)
{
    sentry_span_t *span = nullptr;
    if (!t_spans.isEmpty()) {
        span = sentry_span_start_child(t_spans.top(), op, description);
    } else if (s_tx) {
        span = sentry_transaction_start_child(s_tx, op, description);
    }
    t_spans.push(span);
    if (span)
        sentry_set_span(span);
}

void SentryPlayground::traceEnd()
{
    if (t_spans.isEmpty())
        return;
    if (sentry_span_t *span = t_spans.pop())
        sentry_span_finish(span);
    if (!t_spans.isEmpty() && t_spans.top())
        sentry_set_span(t_spans.top());
}

SentryPlayground::TraceScope::TraceScope(const char *op, const char *description)
{
    SentryPlayground::instance()->traceBegin(op, description);
}

SentryPlayground::TraceScope::~TraceScope()
{
    SentryPlayground::instance()->traceEnd();
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

bool SentryPlayground::haveVulkan()
{
#ifdef HAVE_VULKAN
    return true;
#else
    return false;
#endif
}

void SentryPlayground::viewWidgets()
{
    TRACE_FUNCTION();
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

void SentryPlayground::viewVulkan()
{
#ifdef HAVE_VULKAN
    static QVulkanInstance* vulkanInstance = nullptr;
    if (!vulkanInstance) {
        vulkanInstance = new QVulkanInstance;
        vulkanInstance->setLayers(QByteArrayList()
                       << "VK_LAYER_GOOGLE_threading"
                       << "VK_LAYER_LUNARG_parameter_validation"
                       << "VK_LAYER_LUNARG_object_tracker"
                       << "VK_LAYER_LUNARG_core_validation"
                       << "VK_LAYER_LUNARG_image"
                       << "VK_LAYER_LUNARG_swapchain"
                       << "VK_LAYER_GOOGLE_unique_objects");
        vulkanInstance->create();
    }
    QtVulkanWindow* subwindow = new QtVulkanWindow();
    subwindow->setVulkanInstance(vulkanInstance);
    subwindow->show();
#endif
}

void SentryPlayground::showWindow()
{
    TRACE_FUNCTION();
#if defined(HAVE_WIDGETS)
    SentryPlayground::viewWidgets();
#elif defined(HAVE_QUICK)
    SentryPlayground::viewQuick();
#elif defined(HAVE_OPENGL)
    SentryPlayground::viewOpenGL();
#elif defined(HAVE_VULKAN)
    SentryPlayground::viewVulkan();
#else
    #error Either Widgets, Quick, OpenGL, or Vulkan must be enabled.
#endif
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
