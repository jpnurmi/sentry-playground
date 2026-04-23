#ifndef SENTRYAPP_H
#define SENTRYAPP_H

#include <QApplication>

class SentryApp : public QApplication
{
public:
    SentryApp(int &argc, char **argv);

    bool notify(QObject *receiver, QEvent *event) override;
};

#endif // SENTRYAPP_H
