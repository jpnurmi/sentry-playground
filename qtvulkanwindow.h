#ifndef QTVULKANWINDOW_H
#define QTVULKANWINDOW_H

#include <QtGui/qvulkanwindow.h>

class QtVulkanWindow : public QVulkanWindow
{
public:
    QtVulkanWindow(QWindow *parent = nullptr);

    QVulkanWindowRenderer *createRenderer() override;
};

#endif // QTVULKANWINDOW_H
