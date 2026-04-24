#include <QtCore/qscopeguard.h>
#include <QtGui/qguiapplication.h>
#include <sentry.h>
#include "sentryapp.h"
#include "sentryplayground.h"
#include "sentrytrace.h"
#include "sentrywindow.h"

static sentry_value_t ensure_fingerprint(sentry_value_t event)
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

static sentry_value_t before_send(sentry_value_t event, void *hint, void *userdata)
{
    return ensure_fingerprint(event);
}

static sentry_value_t on_crash(const sentry_ucontext_t *uctx, sentry_value_t event, void *userdata)
{
    return ensure_fingerprint(event);
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
    sentry_options_set_on_crash(options, on_crash, NULL);
    sentry_options_set_traces_sample_rate(options, 1.0);
    sentry_options_set_require_user_consent(options, 1);
    sentry_options_set_debug(options, 1);
    sentry_init(options);
    auto _ = qScopeGuard([] {
        SentryTrace::flush();
        sentry_close();
    });

    SentryApp app(argc, argv);
    SentryWindow window;
    window.show();
    return app.exec();
}
