#ifndef TRACING_H
#define TRACING_H

#include <sentry.h>
#include <atomic>
#include <mutex>
#include <vector>

class Tracing
{
public:
    static bool enabled();
    static void setEnabled(bool enabled);
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
    static std::atomic_bool s_enabled;
    static std::mutex s_mutex;
    static sentry_transaction_t *s_tx;
    static thread_local std::vector<sentry_span_t *> t_spans;
};

#define TRACE_CONCAT_(a, b) a##b
#define TRACE_CONCAT(a, b) TRACE_CONCAT_(a, b)

#define TRACE_FUNCTION() \
    Tracing::Scope TRACE_CONCAT(_trace_scope_, __LINE__){"function", Q_FUNC_INFO}
#define TRACE_SCOPE(op, description) \
    Tracing::Scope TRACE_CONCAT(_trace_scope_, __LINE__){(op), (description)}

#define TRACE_BEGIN(op, description) \
    Tracing::begin((op), (description))
#define TRACE_END() \
    Tracing::end()

#endif // TRACING_H
