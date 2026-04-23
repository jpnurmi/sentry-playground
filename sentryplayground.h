#ifndef SENTRYPLAYGROUND_H
#define SENTRYPLAYGROUND_H

#include <QtCore/qobject.h>
#include <QtCore/qstack.h>
#include <QtGui/qguiapplication.h>

class SentryPlayground : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString backend READ backend CONSTANT)
    Q_PROPERTY(bool worker READ worker WRITE setWorker NOTIFY workerChanged)
    Q_PROPERTY(Qt::CheckState consent READ consent WRITE setConsent NOTIFY consentChanged)

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

signals:
    void workerChanged(bool worker);
    void consentChanged(Qt::CheckState consent);

public slots:
    void showWindow();

    void triggerCrash();
    void triggerStackOverflow();
    void triggerFastfail();
    void triggerAssertFailure();
    void triggerAbort();

private:
    bool m_worker = false;
};

#endif // SENTRYPLAYGROUND_H
