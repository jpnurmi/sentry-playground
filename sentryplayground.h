#ifndef SENTRYPLAYGROUND_H
#define SENTRYPLAYGROUND_H

#include <QtCore/qobject.h>
#include <QtGui/qguiapplication.h>

class SentryPlayground : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString backend READ backend CONSTANT)
    Q_PROPERTY(bool haveWidgets READ haveWidgets CONSTANT)
    Q_PROPERTY(bool haveQuick READ haveQuick CONSTANT)
    Q_PROPERTY(bool haveOpenGL READ haveOpenGL CONSTANT)
    Q_PROPERTY(bool haveVulkan READ haveVulkan CONSTANT)
    Q_PROPERTY(bool worker READ worker WRITE setWorker NOTIFY workerChanged)

public:
    explicit SentryPlayground(QObject *parent = nullptr);

    static QGuiApplication* init(int& argc, char* argv[]);
    static SentryPlayground* instance();
    static QString backend();
    static QDebug debug();

    static bool haveWidgets();
    static bool haveQuick();
    static bool haveOpenGL();
    static bool haveVulkan();

    bool worker() const;
    void setWorker(bool worker);

signals:
    void workerChanged(bool worker);

public slots:
    void viewWidgets();
    void viewQuick();
    void viewOpenGL();
    void viewVulkan();
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
