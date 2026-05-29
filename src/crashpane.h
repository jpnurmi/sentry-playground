#ifndef CRASHPANE_H
#define CRASHPANE_H

#include <QWidget>

#include "debugfilesdialog.h"
#include "ui_crashpane.h"

class Playground;

class CrashPane : public QWidget
{
    Q_OBJECT

public:
    explicit CrashPane(QWidget* parent = nullptr);
    void uploadDebugFiles();
    DebugFilesDialog::UploadStatus debugFilesUploadStatus() const;

signals:
    void debugFilesUploadStatusChanged(DebugFilesDialog::UploadStatus status);

private:
    void triggerCrash(void (*crashFunction)());

    Playground* playground() const;

    DebugFilesDialog* m_debugFilesDialog = nullptr;

    Ui::CrashPane ui;
};

#endif // CRASHPANE_H
