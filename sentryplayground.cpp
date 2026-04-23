#include "sentryplayground.h"
#include "sentrytrace.h"
#include "sentrywindow.h"

#include <QtCore/qdebug.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qthread.h>
#include <QtCore/private/qmetaobject_p.h>
#include <QtCore/private/qobject_p.h>
#include <QtGui/qevent.h>
#include <QtGui/qwindow.h>

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
    SentryTrace::begin("signal", desc.constData());
}

static void onSignalEnd(QObject *caller, int signalIndex)
{
    if (!caller || !qApp || QThread::currentThread() != qApp->thread())
        return;
    QMetaMethod method = QMetaObjectPrivate::signal(caller->metaObject(), signalIndex);
    if (!shouldTraceSignal(caller->metaObject(), method.name().constData()))
        return;
    SentryTrace::end();
}

class PlaygroundApplication : public QApplication
{
public:
    using QApplication::QApplication;

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
                return QApplication::notify(receiver, event);
            break;
        default:
            return QApplication::notify(receiver, event);
        }

        const char *typeName = QMetaEnum::fromType<QEvent::Type>().valueToKey(event->type());
        QByteArray desc = QByteArray(typeName)
            + " -> " + receiver->metaObject()->className()
            + "(" + receiver->objectName().toUtf8() + ")";
        TRACE_SCOPE("event", desc.constData());
        return QApplication::notify(receiver, event);
    }
};

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

    auto *app = new PlaygroundApplication(argc, argv);

    static QSignalSpyCallbackSet spy_callbacks = { &onSignalBegin, nullptr, &onSignalEnd, nullptr };
    qt_register_signal_spy_callbacks(&spy_callbacks);

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
