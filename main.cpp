#include <QApplication>
#include <sentry.h>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    sentry_options_t *options = sentry_options_new();
#ifdef SENTRY_PLAYGROUND_HANDLER
    sentry_options_set_handler_path(options, SENTRY_PLAYGROUND_HANDLER);
#endif
    sentry_options_set_dsn(options, SENTRY_PLAYGROUND_DSN);
    sentry_options_set_release(options, SENTRY_PLAYGROUND_RELEASE);
    sentry_options_set_debug(options, 1);
    sentry_init(options);

    auto sentryClose = qScopeGuard([] { sentry_close(); });

    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
