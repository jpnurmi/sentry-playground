#include "sentryapp.h"
#include "sentryplayground.h"
#include "sentrywindow.h"

int main(int argc, char *argv[])
{
    SentryPlayground::init();
    auto _ = qScopeGuard(SentryPlayground::close);

    SentryApp app(argc, argv);
    SentryWindow window;
    window.show();
    return app.exec();
}
