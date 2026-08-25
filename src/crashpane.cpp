#include "crashpane.h"
#include "crashplugininterface.h"
#include "playground.h"
#include "tracing.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#include <QtCore/qdir.h>
#include <QtCore/qthread.h>
#include <QtWidgets/qabstractbutton.h>
#include <QtWidgets/qmessagebox.h>

#include <stdexcept>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

namespace {

constexpr int kThreadStressWorkerCount = 100;
constexpr int kThreadStressStackDepth = 128;

Q_NEVER_INLINE void holdThreadStressStack(int depth, QMutex* mutex,
                                          QWaitCondition* waitCondition, bool* running)
{
    volatile int keepFrame = depth;
    if (depth > 0) {
        holdThreadStressStack(depth - 1, mutex, waitCondition, running);
    } else {
        QMutexLocker locker(mutex);
        while (*running)
            waitCondition->wait(mutex);
    }

    if (keepFrame < 0)
        qFatal("invalid thread stress stack depth");
}

} // namespace

static void triggerSegfault()
{
    TRACE_FUNCTION();
    qDebug() << "segfault";

    void *volatile invalid_mem = (void *)1;
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
    connect(ui.pluginButton, &QAbstractButton::clicked, this, &CrashPane::triggerPluginCrash);

    m_debugFilesDialog = new DebugFilesDialog(this);
    connect(m_debugFilesDialog, &DebugFilesDialog::uploadStatusChanged, this, &CrashPane::debugFilesUploadStatusChanged);
    m_debugFilesDialog->refreshUploadStatus();

    connect(ui.workerBox, &QAbstractButton::toggled, playground, &Playground::setWorker);
    connect(playground, &Playground::workerChanged, ui.workerBox, &QAbstractButton::setChecked);

    connect(ui.filterBox, &QAbstractButton::toggled, playground, &Playground::setFilter);
    connect(playground, &Playground::filterChanged, ui.filterBox, &QAbstractButton::setChecked);

    connect(ui.threadStressBox, &QAbstractButton::toggled,
            this, &CrashPane::setThreadStressEnabled);
}

CrashPane::~CrashPane()
{
    setThreadStressEnabled(false);
}

void CrashPane::triggerPluginCrash()
{
    if (m_crashPlugin.fileName().isEmpty()) {
        m_crashPlugin.setFileName(QDir(QCoreApplication::applicationDirPath()).filePath(CRASH_PLUGIN_FILENAME));
    }

    CrashPluginInterface* plugin = qobject_cast<CrashPluginInterface*>(m_crashPlugin.instance());
    if (!plugin) {
        QMessageBox::critical(this, tr("Plugin error"), m_crashPlugin.errorString());
        return;
    }

    triggerCrash([plugin] { plugin->crash(); });
}

void CrashPane::triggerCrash(std::function<void()> crashFunction)
{
    if (Playground::instance()->worker()) {
        QThread::create(crashFunction)->start();
    } else {
        crashFunction();
    }
}

void CrashPane::setThreadStressEnabled(bool enabled)
{
    if (enabled == m_threadStressRunning)
        return;

    if (enabled) {
        {
            QMutexLocker locker(&m_threadStressMutex);
            m_threadStressRunning = true;
        }
        m_threadStressWorkers.reserve(kThreadStressWorkerCount);
        for (int i = 0; i < kThreadStressWorkerCount; ++i) {
            QThread* worker = QThread::create([this] {
                holdThreadStressStack(kThreadStressStackDepth, &m_threadStressMutex,
                                      &m_threadStressWaitCondition, &m_threadStressRunning);
            });
            worker->setObjectName(QStringLiteral("crash-stress-%1").arg(i + 1));
            m_threadStressWorkers.push_back(worker);
            worker->start();
        }
        return;
    }

    {
        QMutexLocker locker(&m_threadStressMutex);
        m_threadStressRunning = false;
        m_threadStressWaitCondition.wakeAll();
    }
    for (QThread* worker : m_threadStressWorkers) {
        worker->wait();
        delete worker;
    }
    m_threadStressWorkers.clear();
}

void CrashPane::uploadDebugFiles()
{
    m_debugFilesDialog->upload(Playground::instance()->options().dsn);
}

DebugFilesDialog::UploadStatus CrashPane::debugFilesUploadStatus() const
{
    return m_debugFilesDialog->uploadStatus();
}
