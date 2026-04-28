#include "sentrydebug.h"

#if defined(Q_OS_WINDOWS)
#include <windows.h>
static int gettid()
{
    return GetCurrentProcessId();
}
#elif defined(Q_OS_MACOS)
#include <pthread.h>
static int gettid()
{
    uint64_t tid = 0;
    pthread_threadid_np(pthread_self(), &tid);
    return tid;
}
#else
#include <unistd.h>
#endif

QDebug sentryDebug()
{
    QDebug debug = qDebug();
    debug.nospace() << "[sentry-playground:" << gettid() << "]";
    return debug.space();
}
