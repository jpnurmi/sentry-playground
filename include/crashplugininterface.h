#ifndef CRASHPLUGININTERFACE_H
#define CRASHPLUGININTERFACE_H

#include <QtCore/qobject.h>

class CrashPluginInterface
{
public:
    virtual ~CrashPluginInterface() = default;

    virtual void crash() = 0;
};

#define CrashPluginInterface_iid "CrashPluginInterface/1.0"
Q_DECLARE_INTERFACE(CrashPluginInterface, CrashPluginInterface_iid)

#endif // CRASHPLUGININTERFACE_H
