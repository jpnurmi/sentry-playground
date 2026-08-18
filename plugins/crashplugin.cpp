#include "crashplugin.h"

#include <QtCore/qdebug.h>

void CrashPlugin::crash()
{
    qDebug() << "plugin crash";

    volatile int* invalid = nullptr;
    *invalid = 1;
}
