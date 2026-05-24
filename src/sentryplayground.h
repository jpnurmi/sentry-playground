#ifndef SENTRYPLAYGROUND_H
#define SENTRYPLAYGROUND_H

#include <QtCore/qmap.h>
#include <QtCore/qobject.h>
#include <QtCore/qstack.h>
#include <QtCore/qvariant.h>
#include <QtGui/qguiapplication.h>

class SentryPlayground : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString backend READ backend CONSTANT)
    Q_PROPERTY(bool worker READ worker WRITE setWorker NOTIFY workerChanged)
    Q_PROPERTY(bool filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(Qt::CheckState consent READ consent WRITE setConsent NOTIFY consentChanged)
    Q_PROPERTY(QStringList attachments READ attachments NOTIFY attachmentsChanged)
    Q_PROPERTY(QVariantMap tags READ tags NOTIFY tagsChanged)
    Q_PROPERTY(QVariantMap contexts READ contexts NOTIFY contextsChanged)
    Q_PROPERTY(QVariantMap user READ user NOTIFY userChanged)
    Q_PROPERTY(QString release READ release WRITE setRelease NOTIFY releaseChanged)
    Q_PROPERTY(QString environment READ environment WRITE setEnvironment NOTIFY environmentChanged)
    Q_PROPERTY(bool session READ session WRITE setSession NOTIFY sessionChanged)

public:
    struct InitOptions
    {
        QString dsn;
        QString databasePath;
        QString release;
        QString environment;
        QString dist;
        bool attachScreenshot = true;
        double tracesSampleRate = 1.0;
        int maxBreadcrumbs = 100;
        int maxSpans = 1000;
        int shutdownTimeout = 2000;
        bool requireUserConsent = true;
        bool systemCrashReporterEnabled = false;
        bool enableLargeAttachments = true;
        bool httpRetry = true;
        int cacheKeepMode = 1;
        int cacheMaxItems = 30;
        int cacheMaxSize = 0;
        int cacheMaxAge = 0;
        bool debug = true;
        int loggerLevel = -1;
        bool externalCrashReporterEnabled = false;
        QString externalCrashReporterPath;
    };

    explicit SentryPlayground(QObject *parent = nullptr);

    static void open(const InitOptions& options);
    static void close();
    static void reinit(const InitOptions& options);

    static SentryPlayground* instance();
    static QString backend();

    static InitOptions loadInitOptions();
    static void saveInitOptions(const InitOptions& options);

    bool initialized() const;
    bool hasInitialized() const;
    InitOptions initOptions() const;

    bool worker() const;
    void setWorker(bool worker);

    bool filter() const;
    void setFilter(bool filter);

    Qt::CheckState consent() const;
    void setConsent(Qt::CheckState consent);

    QStringList attachments() const;

    QVariantMap tags() const;
    QVariantMap contexts() const;
    QVariantMap user() const;

    QString release() const;
    void setRelease(const QString& release);

    QString environment() const;
    void setEnvironment(const QString& environment);

    bool session() const;
    void setSession(bool session);

signals:
    void workerChanged(bool worker);
    void filterChanged(bool filter);
    void consentChanged(Qt::CheckState consent);
    void attachmentsChanged(const QStringList& attachments);
    void tagsChanged(const QVariantMap& tags);
    void contextsChanged(const QVariantMap& contexts);
    void userChanged(const QVariantMap& user);
    void releaseChanged(const QString& release);
    void environmentChanged(const QString& environment);
    void sessionChanged(bool session);
    void initializedChanged(bool initialized);
    void initOptionsChanged(const SentryPlayground::InitOptions& options);

public slots:
    void triggerCrash();
    void triggerStackOverflow();
    void triggerFastfail();
    void triggerAssertFailure();
    void triggerAbort();
    void triggerException();

    void captureMessage(int level, const QString& message);
    void captureException(int level, const QString& type, const QString& value);
    void captureFeedback(const QString& message, const QString& name, const QString& email);

    void addBreadcrumb(const QString& type, int level, const QString& message);

    void addAttachment(const QString& path);
    void removeAttachment(const QString& path);

    void setTag(const QString& key, const QString& value);
    void removeTag(const QString& key);

    void setContext(const QString& name, const QString& value);
    void removeContext(const QString& name);

    void updateUser(const QString& field, const QString& value);

private:
    void reapplyScope();
    void applyConsent();
    bool m_worker = false;
    bool m_filter = false;
    bool m_initialized = false;
    bool m_hasInitialized = false;
    InitOptions m_initOptions;
    Qt::CheckState m_consent = Qt::PartiallyChecked;
    QMap<QString, void*> m_attachments;
    QVariantMap m_tags;
    QVariantMap m_contexts;
    QVariantMap m_user;
    QString m_release;
    QString m_environment;
    bool m_session = true;
};

#endif // SENTRYPLAYGROUND_H
