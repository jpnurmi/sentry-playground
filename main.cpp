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

    sentry_set_tag("backend", SENTRY_BACKEND);

    auto sentryClose = qScopeGuard([] { sentry_close(); });

    QApplication app(argc, argv);
    app.setApplicationDisplayName("Sentry Playground");
    MainWindow window;
    window.setWindowFilePath(SENTRY_BACKEND);
    window.show();

    return app.exec();
}
