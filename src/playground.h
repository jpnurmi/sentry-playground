#ifndef PLAYGROUND_H
#define PLAYGROUND_H

#include <QtCore/qmap.h>
#include <QtCore/qobject.h>
#include <QtCore/qstack.h>
#include <QtCore/qvariant.h>
#include <QtGui/qguiapplication.h>

#include "options.h"

class Playground : public QObject
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
    explicit Playground(QObject *parent = nullptr);

    static void open(const Options& options);
    static void close();
    static void reinit(const Options& options);

    static Playground* instance();
    static QString backend();

    bool isInitialized() const;
    bool wasInitialized() const;
    Options options() const;

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
    void optionsChanged(const Options& options);

public slots:
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
    bool m_wasInitialized = false;
    Options m_options;
    Qt::CheckState m_consent = Qt::PartiallyChecked;
    QMap<QString, void*> m_attachments;
    QVariantMap m_tags;
    QVariantMap m_contexts;
    QVariantMap m_user;
    QString m_release;
    QString m_environment;
    bool m_session = true;
};

#endif // PLAYGROUND_H
