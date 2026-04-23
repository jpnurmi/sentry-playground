#ifndef SENTRYPLAYGROUND_H
#define SENTRYPLAYGROUND_H

#include <QtCore/qobject.h>
#include <QtCore/qstack.h>
#include <QtGui/qguiapplication.h>
#include <sentry.h>

class SentryPlayground : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString backend READ backend CONSTANT)
    Q_PROPERTY(bool worker READ worker WRITE setWorker NOTIFY workerChanged)

public:
    explicit SentryPlayground(QObject *parent = nullptr);

    static QGuiApplication* init(int& argc, char* argv[]);
    static void uninit();
    static SentryPlayground* instance();
    static QString backend();
    static QDebug debug();

    bool worker() const;
    void setWorker(bool worker);

    void traceBegin(const char *op, const char *description);
    void traceEnd();

    class TraceScope
    {
    public:
        TraceScope(const char *op, const char *description);
        ~TraceScope();
    private:
        TraceScope(const TraceScope &) = delete;
        TraceScope &operator=(const TraceScope &) = delete;
    };

signals:
    void workerChanged(bool worker);

public slots:
    void viewWidgets();
    void showWindow();

    void triggerCrash();
    void triggerStackOverflow();
    void triggerFastfail();
    void triggerAssertFailure();
    void triggerAbort();

private:
    bool m_worker = false;
    static sentry_transaction_t *s_tx;
    static thread_local QStack<sentry_span_t *> t_spans;
};

#define TRACE_CONCAT_(a, b) a##b
#define TRACE_CONCAT(a, b) TRACE_CONCAT_(a, b)

#define TRACE_FUNCTION() \
    SentryPlayground::TraceScope TRACE_CONCAT(_trace_scope_, __LINE__){"function", Q_FUNC_INFO}
#define TRACE_SCOPE(op, description) \
    SentryPlayground::TraceScope TRACE_CONCAT(_trace_scope_, __LINE__){(op), (description)}

#define TRACE_BEGIN(op, description) \
    SentryPlayground::instance()->traceBegin((op), (description))
#define TRACE_END() \
    SentryPlayground::instance()->traceEnd()

#endif // SENTRYPLAYGROUND_H
