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

private:
    void triggerCrash(void (*crashFunction)());
    void uploadDebugFiles();
    void updateDebugFilesUploadStatus(DebugFilesDialog::UploadStatus status);

    Playground* playground() const;

    DebugFilesDialog* m_debugFilesDialog = nullptr;

    Ui::CrashPane ui;
};

#endif // CRASHPANE_H
