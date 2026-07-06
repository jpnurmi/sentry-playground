#ifndef TRACING_H
#define TRACING_H

#include <QElapsedTimer>
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

    class Benchmark
    {
    public:
        Benchmark(const char *name, int &count, double &avg, double &m2,
            double &min, double &max);
        ~Benchmark();

    private:
        Benchmark(const Benchmark &) = delete;
        Benchmark &operator=(const Benchmark &) = delete;

        const char *m_name = nullptr;
        int &m_count;
        double &m_avg;
        double &m_m2;
        double &m_min;
        double &m_max;
        QElapsedTimer m_timer;
    };

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
#define TRACE_STRINGIFY_(x) #x
#define TRACE_STRINGIFY(x) TRACE_STRINGIFY_(x)

#define TRACE_FUNCTION() \
    Tracing::Scope TRACE_CONCAT(_trace_scope_, __LINE__){"function", Q_FUNC_INFO}
#define TRACE_SCOPE(op, description) \
    Tracing::Scope TRACE_CONCAT(_trace_scope_, __LINE__){(op), (description)}

#define TRACE_BENCHMARK(name) \
    if (Tracing::Benchmark TRACE_CONCAT(_trace_benchmark_, __LINE__){ \
            TRACE_STRINGIFY(name), \
            []() -> int & { static int count = 0; return count; }(), \
            []() -> double & { static double avg = 0.0; return avg; }(), \
            []() -> double & { static double m2 = 0.0; return m2; }(), \
            []() -> double & { static double min = 0.0; return min; }(), \
            []() -> double & { static double max = 0.0; return max; }()}; \
        true)

#define TRACE_BEGIN(op, description) \
    Tracing::begin((op), (description))
#define TRACE_END() \
    Tracing::end()

#endif // TRACING_H
