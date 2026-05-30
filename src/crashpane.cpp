#include "crashpane.h"
#include "playground.h"
#include "tracing.h"

#include <QtCore/qdebug.h>
#include <QtCore/qthread.h>
#include <QtWidgets/qabstractbutton.h>

#include <stdexcept>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

static void *invalid_mem = (void *)1;

static void triggerSegfault()
{
    TRACE_FUNCTION();
    qDebug() << "segfault";

    memset((char *)invalid_mem, 1, 100);
}

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4717)
#endif
static void triggerStackOverflow()
{
    TRACE_FUNCTION();
    qDebug() << "stack overflow";

    alloca(1024);
    triggerStackOverflow();
}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

static void triggerFastfail()
{
#ifdef Q_OS_WINDOWS
    TRACE_FUNCTION();
    qDebug() << "fastfail";

    __fastfail(77);
#endif
}

static void assertFailure()
{
    TRACE_FUNCTION();
    qDebug() << "assert failure";

    assert(false);
}

static void doAbort()
{
    TRACE_FUNCTION();
    qDebug() << "abort";

    std::abort();
}

static void throwException()
{
    TRACE_FUNCTION();
    qDebug() << "throwException";

    throw std::runtime_error("uncaught C++ exception");
}

CrashPane::CrashPane(QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);

#ifndef Q_OS_WINDOWS
    ui.fastfailButton->setEnabled(false);
#endif

    Playground* playground = Playground::instance();
    ui.workerBox->setChecked(playground->worker());
    ui.filterBox->setChecked(playground->filter());

    connect(ui.crashButton, &QAbstractButton::clicked, this, [this] { triggerCrash(&triggerSegfault); });
    connect(ui.stackOverflowButton, &QAbstractButton::clicked, this, [this] { triggerCrash(&triggerStackOverflow); });
    connect(ui.fastfailButton, &QAbstractButton::clicked, this, [this] {
        Tracing::flush();
        triggerCrash(&triggerFastfail);
    });
    connect(ui.assertButton, &QAbstractButton::clicked, this, [this] { triggerCrash(&assertFailure); });
    connect(ui.abortButton, &QAbstractButton::clicked, this, [this] { triggerCrash(&doAbort); });
    connect(ui.throwButton, &QAbstractButton::clicked, this, [this] { triggerCrash(&throwException); });

    m_debugFilesDialog = new DebugFilesDialog(this);
    connect(m_debugFilesDialog, &DebugFilesDialog::uploadStatusChanged, this, &CrashPane::debugFilesUploadStatusChanged);
    m_debugFilesDialog->refreshUploadStatus();

    connect(ui.workerBox, &QAbstractButton::toggled, playground, &Playground::setWorker);
    connect(playground, &Playground::workerChanged, ui.workerBox, &QAbstractButton::setChecked);

    connect(ui.filterBox, &QAbstractButton::toggled, playground, &Playground::setFilter);
    connect(playground, &Playground::filterChanged, ui.filterBox, &QAbstractButton::setChecked);
}

void CrashPane::triggerCrash(std::function<void()> crashFunction)
{
    if (Playground::instance()->worker()) {
        QThread::create(crashFunction)->start();
    } else {
        crashFunction();
    }
}

void CrashPane::uploadDebugFiles()
{
    m_debugFilesDialog->upload(Playground::instance()->options().dsn);
}

DebugFilesDialog::UploadStatus CrashPane::debugFilesUploadStatus() const
{
    return m_debugFilesDialog->uploadStatus();
}
