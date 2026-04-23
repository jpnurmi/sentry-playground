#include "sentryapp.h"
#include "sentrytrace.h"

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

SentryApp::SentryApp(int &argc, char **argv)
    : QApplication(argc, argv)
{
    static QSignalSpyCallbackSet spy_callbacks = { &onSignalBegin, nullptr, &onSignalEnd, nullptr };
    qt_register_signal_spy_callbacks(&spy_callbacks);
}

bool SentryApp::notify(QObject *receiver, QEvent *event)
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
