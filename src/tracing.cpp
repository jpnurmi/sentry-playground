#include "tracing.h"

#include <QDebug>

#include <cmath>

std::atomic_bool Tracing::s_enabled = false;
std::mutex Tracing::s_mutex;
sentry_transaction_t *Tracing::s_tx = nullptr;
thread_local std::vector<sentry_span_t *> Tracing::t_spans;

bool Tracing::enabled()
{
    return s_enabled.load(std::memory_order_acquire);
}

void Tracing::setEnabled(bool enabled)
{
    s_enabled.store(enabled, std::memory_order_release);
}

void Tracing::begin(const char *op, const char *description)
{
    if (!enabled())
        return;

    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_enabled.load(std::memory_order_relaxed))
        return;

    if (!s_tx) {
        sentry_transaction_context_t *ctx
            = sentry_transaction_context_new("main", "function");
        s_tx = sentry_transaction_start(ctx, sentry_value_new_null());
        sentry_set_transaction_object(s_tx);
    }

    sentry_span_t *span = nullptr;
    if (!t_spans.empty()) {
        span = sentry_span_start_child(t_spans.back(), op, description);
    } else if (s_tx) {
        span = sentry_transaction_start_child(s_tx, op, description);
    }
    t_spans.push_back(span);
    if (span)
        sentry_set_span(span);
}

void Tracing::end()
{
    if (t_spans.empty())
        return;

    std::lock_guard<std::mutex> lock(s_mutex);
    sentry_span_t *span = t_spans.back();
    t_spans.pop_back();
    if (span)
        sentry_span_finish(span);
    if (!t_spans.empty() && t_spans.back())
        sentry_set_span(t_spans.back());
    else if (s_tx)
        sentry_set_transaction_object(s_tx);
}

void Tracing::flush()
{
    sentry_uuid_t uuid = sentry_uuid_nil();
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        while (!t_spans.empty()) {
            sentry_span_t *span = t_spans.back();
            t_spans.pop_back();
            if (span)
                sentry_span_finish(span);
        }
        sentry_transaction_t *tx = s_tx;
        if (!tx)
            return;
        s_tx = nullptr;
        uuid = sentry_transaction_finish(tx);
    }
    if (!sentry_uuid_is_nil(&uuid))
        sentry_flush(2000);
}

Tracing::Benchmark::Benchmark(const char *name, int &count, double &avg,
    double &m2, double &min, double &max)
    : m_name(name)
    , m_count(count)
    , m_avg(avg)
    , m_m2(m2)
    , m_min(min)
    , m_max(max)
{
    m_timer.start();
}

Tracing::Benchmark::~Benchmark()
{
    const double ms = m_timer.nsecsElapsed() / 1000000.0;

    m_count++;
    if (m_count == 1 || ms < m_min)
        m_min = ms;
    if (m_count == 1 || ms > m_max)
        m_max = ms;

    const double delta = ms - m_avg;
    m_avg += delta / m_count;
    m_m2 += delta * (ms - m_avg);
    const double stddev
        = m_count > 1 ? std::sqrt(m_m2 / (m_count - 1)) : 0.0;

    qDebug("BENCHMARK %s %.3f - %dx: %.3f +/- %.3f [%.3f..%.3f]",
        m_name ? m_name : "", ms, m_count, m_avg, stddev, m_min, m_max);
}

Tracing::Scope::Scope(const char *op, const char *description)
{
    Tracing::begin(op, description);
}

Tracing::Scope::~Scope()
{
    Tracing::end();
}
