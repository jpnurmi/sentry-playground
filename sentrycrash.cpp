#include "sentrycrash.h"
#include "sentrydebug.h"
#include "sentrytrace.h"

#include <QtCore/qdebug.h>

#include <stdexcept>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

static void *invalid_mem = (void *)1;

void SentryCrash::segfault()
{
    TRACE_FUNCTION();
    sentryDebug() << "segfault";

    memset((char *)invalid_mem, 1, 100);
}

void SentryCrash::stackOverflow()
{
    TRACE_FUNCTION();
    sentryDebug() << "stack overflow";

    alloca(1024);
    stackOverflow();
}

void SentryCrash::fastfail()
{
#ifdef Q_OS_WINDOWS
    TRACE_FUNCTION();
    sentryDebug() << "fastfail";

    __fastfail(77);
#endif
}

void SentryCrash::failAssert()
{
    TRACE_FUNCTION();
    sentryDebug() << "failAssert";

    assert(false);
}

void SentryCrash::doAbort()
{
    TRACE_FUNCTION();
    sentryDebug() << "doAbort";

    std::abort();
}

void SentryCrash::throwException()
{
    TRACE_FUNCTION();
    sentryDebug() << "throwException";

    throw std::runtime_error("uncaught C++ exception");
}
