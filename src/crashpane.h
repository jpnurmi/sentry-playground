#ifndef CRASHPANE_H
#define CRASHPANE_H

#include <QWidget>

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
    void triggerCrash(std::function<void()> crashFunction);

    DebugFilesDialog* m_debugFilesDialog = nullptr;

    Ui::CrashPane ui;
};

#endif // CRASHPANE_H
