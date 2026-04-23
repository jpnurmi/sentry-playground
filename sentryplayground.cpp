#include "sentryplayground.h"
#ifdef HAVE_WIDGETS
#include "qtwidgetswindow.h"
#endif
#ifdef HAVE_OPENGL
#include "qtopenglwindow.h"
#endif

#include <QtCore/qdebug.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qthread.h>
#include <QtCore/private/qmetaobject_p.h>
#include <QtCore/private/qobject_p.h>
#include <QtGui/qevent.h>
#include <QtGui/qwindow.h>
#ifdef HAVE_QUICK
#include <QtQml/qqmlapplicationengine.h>
#endif

static bool shouldTraceSignal(const QMetaObject *mo, const char *signalName)
{
    for (; mo; mo = mo->superClass()) {
        const char *className = mo->className();
        if (qstrcmp(className, "QAbstractButton") == 0) {
            return qstrcmp(signalName, "clicked") == 0
                || qstrcmp(signalName, "pressed") == 0
                || qstrcmp(signalName, "released") == 0
                || qstrcmp(signalName, "toggled") == 0;
        }
        if (qstrcmp(className, "QAction") == 0) {
            return qstrcmp(signalName, "triggered") == 0
                || qstrcmp(signalName, "toggled") == 0
                || qstrcmp(signalName, "hovered") == 0;
        }
    }
    return false;
}

static void onSignalBegin(QObject *caller, int signalIndex, void **)
{
    if (!caller || !qApp || QThread::currentThread() != qApp->thread())
        return;
    QMetaMethod method = QMetaObjectPrivate::signal(caller->metaObject(), signalIndex);
    QByteArray desc = QByteArray(caller->metaObject()->className())
        + "::" + method.methodSignature();
    if (!shouldTraceSignal(caller->metaObject(), method.name().constData())) {
        return;
    }
    SentryPlayground::instance()->traceBegin("signal", desc.constData());
}

static void onSignalEnd(QObject *caller, int signalIndex)
{
    if (!caller || !qApp || QThread::currentThread() != qApp->thread())
        return;
    QMetaMethod method = QMetaObjectPrivate::signal(caller->metaObject(), signalIndex);
    if (!shouldTraceSignal(caller->metaObject(), method.name().constData()))
        return;
    SentryPlayground::instance()->traceEnd();
}

#ifdef HAVE_WIDGETS
using PlaygroundApplicationBase = QApplication;
#else
using PlaygroundApplicationBase = QGuiApplication;
#endif

class PlaygroundApplication : public PlaygroundApplicationBase
{
public:
    using PlaygroundApplicationBase::PlaygroundApplicationBase;

    bool notify(QObject *receiver, QEvent *event) override
    {
        switch (event->type()) {
        // window / lifecycle
        case QEvent::Show:
        case QEvent::Hide:
        case QEvent::Close:
        case QEvent::Resize:
        // mouse
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        // keyboard / focus
        case QEvent::FocusIn:
        case QEvent::FocusOut:
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        case QEvent::ShortcutOverride:
        case QEvent::Shortcut:
            if (!qobject_cast<QWindow *>(receiver))
                return PlaygroundApplicationBase::notify(receiver, event);
            break;
        default:
            return PlaygroundApplicationBase::notify(receiver, event);
        }

        const char *typeName = QMetaEnum::fromType<QEvent::Type>().valueToKey(event->type());
        QByteArray desc = QByteArray(typeName)
            + " -> " + receiver->metaObject()->className()
            + "(" + receiver->objectName().toUtf8() + ")";
        TRACE_SCOPE("event", desc.constData());
        return PlaygroundApplicationBase::notify(receiver, event);
    }
};

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

    auto *app = new PlaygroundApplication(argc, argv);

    static QSignalSpyCallbackSet spy_callbacks = { &onSignalBegin, nullptr, &onSignalEnd, nullptr };
    qt_register_signal_spy_callbacks(&spy_callbacks);
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

void SentryPlayground::showWindow()
{
    TRACE_FUNCTION();
#if defined(HAVE_WIDGETS)
    SentryPlayground::viewWidgets();
#elif defined(HAVE_QUICK)
    SentryPlayground::viewQuick();
#elif defined(HAVE_OPENGL)
    SentryPlayground::viewOpenGL();
#else
    #error Either Widgets, Quick, or OpenGL must be enabled.
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
