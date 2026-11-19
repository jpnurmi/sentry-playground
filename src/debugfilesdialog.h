#ifndef DEBUGFILESDIALOG_H
#define DEBUGFILESDIALOG_H

#include <QtCore/qstringlist.h>
#include <QtWidgets/qdialog.h>

class QLabel;
class QPlainTextEdit;
class QProcess;
class QPushButton;
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
    void prepareUpload(const QString& dsn);

signals:
    void uploadStatusChanged(DebugFilesDialog::UploadStatus status);

private:
    struct Section
    {
        QLabel* summary = nullptr;
        QPushButton* button = nullptr;
        QPlainTextEdit* output = nullptr;
        QProcess* process = nullptr;
        QString directory;
        QStringList arguments;
        QStringList debugIds;
    };

    void refreshQtUpload();
    void previewUpload(Section& section);
    void startUpload(Section& section);
    void detectOrg(Section& section, const QStringList& variables, const QString& key, const QString& project);
    void runUpload(Section& section, QStringList variables, const QString& org, const QString& project);
    void streamSentry(Section& section, const QStringList& variables, const QStringList& arguments);
    QProcess* startSentry(Section& section, const QStringList& variables, const QStringList& arguments);
    void appendOutput(Section& section, const QString& text);
    void finishUpload(Section& section, int exitCode);
    void setUploadStatus(UploadStatus status);

    QString m_dsn;
    Section m_app;
    Section m_qt;
    UploadStatus m_uploadStatus = UploadStatus::OutOfDate;
};

#endif // DEBUGFILESDIALOG_H
