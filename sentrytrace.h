#ifndef SENTRYTRACE_H
#define SENTRYTRACE_H

#include <sentry.h>
#include <vector>

class SentryTrace
{
public:
    static void begin(const char *op, const char *description);
    static void end();
    static void flush();

    class Scope
    {
    public:
        Scope(const char *op, const char *description);
        ~Scope();
    private:
        Scope(const Scope &) = delete;
        Scope &operator=(const Scope &) = delete;
    };

private:
    static sentry_transaction_t *s_tx;
    static thread_local std::vector<sentry_span_t *> t_spans;
};

#define TRACE_CONCAT_(a, b) a##b
#define TRACE_CONCAT(a, b) TRACE_CONCAT_(a, b)

#define TRACE_FUNCTION() \
    SentryTrace::Scope TRACE_CONCAT(_trace_scope_, __LINE__){"function", Q_FUNC_INFO}
#define TRACE_SCOPE(op, description) \
    SentryTrace::Scope TRACE_CONCAT(_trace_scope_, __LINE__){(op), (description)}

#define TRACE_BEGIN(op, description) \
    SentryTrace::begin((op), (description))
#define TRACE_END() \
    SentryTrace::end()

#endif // SENTRYTRACE_H
