#ifndef CRASHPLUGIN_H
#define CRASHPLUGIN_H

#include "crashplugininterface.h"

#include <QtCore/qobject.h>

class CrashPlugin : public QObject, public CrashPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID CrashPluginInterface_iid)
    Q_INTERFACES(CrashPluginInterface)

public:
    void crash() override;
};

#endif // CRASHPLUGIN_H
