#include "qtwidgetswindow.h"
#include "sentryplayground.h"

QtWidgetsWindow::QtWidgetsWindow(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    ui.backendLabel->setText(SentryPlayground::backend());
#ifndef Q_OS_WINDOWS
    ui.fastfailButton->setEnabled(false);
#endif
#ifndef HAVE_QUICK
    ui.actionQuick->setEnabled(false);
#endif
#ifndef HAVE_OPENGL
    ui.actionOpenGL->setEnabled(false);
#endif
#ifndef HAVE_VULKAN
    ui.actionVulkan->setEnabled(false);
#endif

    SentryPlayground* playground = SentryPlayground::instance();
    QObject::connect(ui.crashButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerCrash);
    QObject::connect(ui.stackOverflowButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerStackOverflow);
    QObject::connect(ui.fastfailButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerFastfail);
    QObject::connect(ui.assertButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerAssertFailure);
    QObject::connect(ui.abortButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerAbort);

    QObject::connect(ui.actionQuit, &QAction::triggered, qApp, &QCoreApplication::quit);
    QObject::connect(ui.actionWidgets, &QAction::triggered, playground, &SentryPlayground::viewWidgets);
    QObject::connect(ui.actionQuick, &QAction::triggered, playground, &SentryPlayground::viewQuick);
    QObject::connect(ui.actionOpenGL, &QAction::triggered, playground, &SentryPlayground::viewOpenGL);
    QObject::connect(ui.actionVulkan, &QAction::triggered, playground, &SentryPlayground::viewVulkan);

    QObject::connect(ui.workerBox, &QAbstractButton::toggled, playground, &SentryPlayground::setWorker);
    QObject::connect(playground, &SentryPlayground::workerChanged, ui.workerBox, &QAbstractButton::setChecked);
}
