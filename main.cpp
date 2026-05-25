#include "sentryapp.h"
#include "sentrydebug.h"
#include "sentryplayground.h"
#include "sentrystyle.h"
#include "sentrywindow.h"

#include <QtCore/qcoreapplication.h>

int main(int argc, char *argv[])
{
    SentryApp app(argc, argv);
    app.setStyle(new SentryStyle(app.style()));

    sentryDebug().nospace() << "backend=" << SENTRY_BACKEND;
    QCoreApplication::setOrganizationName("Sentry");
    QCoreApplication::setOrganizationDomain("sentry.io");
    QCoreApplication::setApplicationName("Playground");
    auto _ = qScopeGuard(SentryPlayground::close);

    SentryWindow window;
    window.show();
    return app.exec();
}
