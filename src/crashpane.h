#ifndef CRASHPANE_H
#define CRASHPANE_H

#include <QWidget>

#include <QtCore/qpluginloader.h>
#include <QtCore/qmutex.h>
#include <QtCore/qthread.h>
#include <QtCore/qwaitcondition.h>

#include <vector>

#include "debugfilesdialog.h"
#include "ui_crashpane.h"

class CrashPane : public QWidget
{
    Q_OBJECT

public:
    explicit CrashPane(QWidget* parent = nullptr);
    ~CrashPane() override;

    DebugFilesDialog::UploadStatus debugFilesUploadStatus() const;

public slots:
    void uploadDebugFiles();

signals:
    void debugFilesUploadStatusChanged(DebugFilesDialog::UploadStatus status);

private:
    void triggerPluginCrash();
    void triggerCrash(std::function<void()> crashFunction);
    void setThreadStressEnabled(bool enabled);

    DebugFilesDialog* m_debugFilesDialog = nullptr;
    QPluginLoader m_crashPlugin;
    QMutex m_threadStressMutex;
    QWaitCondition m_threadStressWaitCondition;
    bool m_threadStressRunning = false;
    std::vector<QThread*> m_threadStressWorkers;

    Ui::CrashPane ui;
};

#endif // CRASHPANE_H
