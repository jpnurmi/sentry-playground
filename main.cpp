#include <QtCore/qscopeguard.h>
#include <QtCore/qtimer.h>
#include <QtGui/qguiapplication.h>
#include <sentry.h>
#include "sentryplayground.h"

static sentry_value_t before_send(sentry_value_t event, void *hint, void *userdata)
{
    sentry_value_t fp = sentry_value_new_list();
    sentry_uuid_t uuid = sentry_uuid_new_v4();
    char buf[37];
    sentry_uuid_as_string(&uuid, buf);
    buf[36] = '\0';
    sentry_value_append(fp, sentry_value_new_string(buf));
    sentry_value_set_by_key(event, "fingerprint", fp);
    return event;
}

int main(int argc, char *argv[])
{
    SentryPlayground::debug().nospace() << "backend=" << SENTRY_BACKEND;

    sentry_options_t *options = sentry_options_new();
    sentry_options_set_dsn(options, SENTRY_DSN);
    sentry_options_set_release(options, SENTRY_RELEASE);
    sentry_options_set_environment(options, "playground");
    sentry_options_set_handler_path(options, SENTRY_HANDLER_PATH);
    sentry_options_set_attach_screenshot(options, true);
    sentry_options_set_before_send(options, before_send, NULL);
    sentry_options_set_traces_sample_rate(options, 1.0);
    sentry_options_set_debug(options, 1);
    sentry_init(options);

    sentry_set_tag("backend", SENTRY_BACKEND);
    sentry_set_user(sentry_value_new_user(NULL, "nobody", "nobody@example.com", NULL));

    sentry_transaction_context_t *ctx
        = sentry_transaction_context_new("main", "function");
    sentry_transaction_t *tx
        = sentry_transaction_start(ctx, sentry_value_new_null());
    auto sentryClose = qScopeGuard([tx] {
        sentry_transaction_finish(tx);
        sentry_close();
    });

    sentry_span_t *startup
        = sentry_transaction_start_child(tx, "app.start", "QGuiApplication init");
    QScopedPointer<QGuiApplication> app(SentryPlayground::init(argc, argv));
    SentryPlayground::instance()->showWindow();
    QTimer::singleShot(0, app.get(), [startup] { sentry_span_finish(startup); });

    return app->exec();
}
