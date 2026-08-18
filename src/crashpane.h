#ifndef CRASHPANE_H
#define CRASHPANE_H

#include <QWidget>

#include <QtCore/qpluginloader.h>

#include "debugfilesdialog.h"
#include "ui_crashpane.h"

class CrashPane : public QWidget
{
    Q_OBJECT

public:
    explicit CrashPane(QWidget* parent = nullptr);

    DebugFilesDialog::UploadStatus debugFilesUploadStatus() const;

public slots:
    void uploadDebugFiles();

signals:
    void debugFilesUploadStatusChanged(DebugFilesDialog::UploadStatus status);

private:
    void triggerPluginCrash();
    void triggerCrash(std::function<void()> crashFunction);

    DebugFilesDialog* m_debugFilesDialog = nullptr;
    QPluginLoader m_crashPlugin;

    Ui::CrashPane ui;
};

#endif // CRASHPANE_H
