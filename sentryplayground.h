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
    Q_PROPERTY(Qt::CheckState consent READ consent WRITE setConsent NOTIFY consentChanged)
    Q_PROPERTY(QStringList attachments READ attachments NOTIFY attachmentsChanged)
    Q_PROPERTY(QVariantMap tags READ tags NOTIFY tagsChanged)
    Q_PROPERTY(QVariantMap contexts READ contexts NOTIFY contextsChanged)

public:
    explicit SentryPlayground(QObject *parent = nullptr);

    static QGuiApplication* init(int& argc, char* argv[]);
    static void uninit();
    static SentryPlayground* instance();
    static QString backend();
    static QDebug debug();

    bool worker() const;
    void setWorker(bool worker);

    Qt::CheckState consent() const;
    void setConsent(Qt::CheckState consent);

    QStringList attachments() const;

    QVariantMap tags() const;
    QVariantMap contexts() const;

signals:
    void workerChanged(bool worker);
    void consentChanged(Qt::CheckState consent);
    void attachmentsChanged(const QStringList& attachments);
    void tagsChanged(const QVariantMap& tags);
    void contextsChanged(const QVariantMap& contexts);

public slots:
    void showWindow();

    void triggerCrash();
    void triggerStackOverflow();
    void triggerFastfail();
    void triggerAssertFailure();
    void triggerAbort();

    void captureMessage(int level, const QString& message);

    void addBreadcrumb(const QString& type, int level, const QString& message);

    void addAttachment(const QString& path);
    void removeAttachment(const QString& path);

    void setTag(const QString& key, const QString& value);
    void removeTag(const QString& key);

    void setContext(const QString& name, const QString& value);
    void removeContext(const QString& name);

private:
    bool m_worker = false;
    QMap<QString, void*> m_attachments;
    QVariantMap m_tags;
    QVariantMap m_contexts;
};

#endif // SENTRYPLAYGROUND_H
