#ifndef DEBUGFILESDIALOG_H
#define DEBUGFILESDIALOG_H

#include <QtCore/qstringlist.h>
#include <QtWidgets/qdialog.h>

class QPlainTextEdit;
class QProcess;
class QString;

class DebugFilesDialog : public QDialog
{
    Q_OBJECT

public:
    enum class UploadStatus {
        OutOfDate,
        UpToDate,
    };
    Q_ENUM(UploadStatus)

    explicit DebugFilesDialog(QWidget* parent = nullptr);

    UploadStatus uploadStatus() const;
    void refreshUploadStatus();
    void upload(const QString& dsn);

signals:
    void uploadStatusChanged(DebugFilesDialog::UploadStatus status);

private:
    void appendOutput(const QString& text);
    void finishUpload(int exitCode);
    void setUploadStatus(UploadStatus status);

    QStringList m_currentDebugIds;
    QPlainTextEdit* m_output = nullptr;
    QProcess* m_uploadProcess = nullptr;
    UploadStatus m_uploadStatus = UploadStatus::OutOfDate;
};

#endif // DEBUGFILESDIALOG_H
