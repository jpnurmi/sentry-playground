#ifndef DEBUGFILESDIALOG_H
#define DEBUGFILESDIALOG_H

#include <QtCore/qstringconverter.h>
#include <QtCore/qstringlist.h>
#include <QtWidgets/qdialog.h>

class QLabel;
class QPlainTextEdit;
class QProcess;
class QPushButton;
class QString;
class QTabWidget;

class DebugFilesDialog : public QDialog
{
    Q_OBJECT

public:
    enum class UploadStatus {
        Unknown,
        OutOfDate,
        UpToDate,
    };
    Q_ENUM(UploadStatus)

    explicit DebugFilesDialog(QWidget* parent = nullptr);

    static QString sentryProgram();

    UploadStatus uploadStatus() const;
    void refreshUploadStatus();
    void prepareUpload(const QString& dsn);

signals:
    void uploadStatusChanged(DebugFilesDialog::UploadStatus status);

private:
    struct Section
    {
        QWidget* page = nullptr;
        QLabel* summary = nullptr;
        QPushButton* button = nullptr;
        QPlainTextEdit* output = nullptr;
        QProcess* process = nullptr;
        QString directory;
        QStringList arguments;
        QString historyKey;
        UploadStatus status = UploadStatus::Unknown;
        QStringList debugIds;
        // the CLI writes UTF-8 regardless of the locale, and streamed reads
        // may split multi-byte characters, so decode with state
        QStringDecoder stdoutDecoder{ QStringDecoder::Utf8 };
        QStringDecoder stderrDecoder{ QStringDecoder::Utf8 };
    };

    QStringList currentDebugIds(const Section& section) const;
    void refreshQtUpload();
    void previewUpload(Section& section);
    void startUpload(Section& section);
    void detectOrg(Section& section, const QStringList& variables, const QString& key, const QString& project);
    void runUpload(Section& section, QStringList variables, const QString& org, const QString& project);
    void streamSentry(Section& section, const QStringList& variables, const QStringList& arguments);
    QProcess* startSentry(Section& section, const QStringList& variables, const QStringList& arguments);
    void appendOutput(Section& section, const QString& text);
    void finishUpload(Section& section, int exitCode);
    void updateUploadStatus();
    void setUploadStatus(UploadStatus status);

    QString m_dsn;
    QTabWidget* m_tabs = nullptr;
    Section m_app;
    Section m_qt;
    QStringList m_qtDebugIds;
    UploadStatus m_uploadStatus = UploadStatus::Unknown;
};

#endif // DEBUGFILESDIALOG_H
