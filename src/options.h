#ifndef OPTIONS_H
#define OPTIONS_H

#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>

typedef struct sentry_options_s sentry_options_t;

struct Options
{
    Options();

    bool load(const QByteArray &data);
    QByteArray save() const;
    sentry_options_t *toNative() const;

    QString dsn = qEnvironmentVariable("SENTRY_DSN");
    QString databasePath;
    QString release = QString::fromUtf8(SENTRY_RELEASE);
    QString environment = "playground";
    QString dist;
    bool attachScreenshot = false;
    double tracesSampleRate = 0.0;
    int maxBreadcrumbs = 100;
    int maxSpans = 1000;
    int shutdownTimeout = 2000;
    bool requireUserConsent = false;
    bool systemCrashReporterEnabled = false;
    bool enableLargeAttachments = false;
    bool httpRetry = false;
    int cacheKeepMode = 0;
    int cacheMaxItems = 30;
    int cacheMaxSize = 0;
    int cacheMaxAge = 0;
    bool debug = false;
    int loggerLevel = -1;
    bool externalCrashReporterEnabled = false;
    QString externalCrashReporterPath;
    int crashReportingMode;
    int minidumpMode;
};

#endif // OPTIONS_H
