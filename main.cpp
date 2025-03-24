#include <QtCore/qscopeguard.h>
#include <QtGui/qguiapplication.h>
#include <sentry.h>
#include "sentryplayground.h"

int main(int argc, char *argv[])
{
    SentryPlayground::debug().nospace() << "backend=" << SENTRY_BACKEND;

    sentry_options_t *options = sentry_options_new();
#ifdef SENTRY_PLAYGROUND_HANDLER
    sentry_options_set_handler_path(options, SENTRY_PLAYGROUND_HANDLER);
#endif
    sentry_options_set_dsn(options, SENTRY_PLAYGROUND_DSN);
    sentry_options_set_release(options, SENTRY_PLAYGROUND_RELEASE);
    sentry_options_set_attach_screenshot(options, true);
    sentry_options_set_debug(options, 1);
    sentry_init(options);

    sentry_set_tag("backend", SENTRY_BACKEND);

    auto sentryClose = qScopeGuard([] { sentry_close(); });

    QScopedPointer<QGuiApplication> app(SentryPlayground::init(argc, argv));
    SentryPlayground::instance()->showWindow();
    return app->exec();
}
