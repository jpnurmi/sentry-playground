#include "sentrytrace.h"

sentry_transaction_t *SentryTrace::s_tx = nullptr;
thread_local std::vector<sentry_span_t *> SentryTrace::t_spans;

void SentryTrace::begin(const char *op, const char *description)
{
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

void SentryTrace::end()
{
    if (t_spans.empty())
        return;
    sentry_span_t *span = t_spans.back();
    t_spans.pop_back();
    if (span)
        sentry_span_finish(span);
    if (!t_spans.empty() && t_spans.back())
        sentry_set_span(t_spans.back());
    else if (s_tx)
        sentry_set_transaction_object(s_tx);
}

void SentryTrace::flush()
{
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
    sentry_transaction_finish(tx);
    sentry_flush(2000);
}

SentryTrace::Scope::Scope(const char *op, const char *description)
{
    SentryTrace::begin(op, description);
}

SentryTrace::Scope::~Scope()
{
    SentryTrace::end();
}
