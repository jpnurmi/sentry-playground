#include "app.h"
#include "playground.h"
#include "style.h"
#include "mainwindow.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>

int main(int argc, char *argv[])
{
    App app(argc, argv);
    app.setStyle(new Style(app.style()));

    qSetMessagePattern("[sentry-playground:%{threadid}] %{message}");
    qDebug().nospace() << "backend=" << SENTRY_BACKEND;
    QCoreApplication::setOrganizationName("Sentry");
    QCoreApplication::setOrganizationDomain("sentry.io");
    QCoreApplication::setApplicationName("Playground");
    auto _ = qScopeGuard(Playground::close);

    MainWindow window;
    window.show();
    return app.exec();
}
