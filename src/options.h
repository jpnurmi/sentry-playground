#ifndef OPTIONS_H
#define OPTIONS_H

#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>

typedef struct sentry_options_s sentry_options_t;

struct Options
{
    Options();

    bool load(const QByteArray &data);
    QByteArray save() const;
    QVariantMap toMap() const;
    sentry_options_t *toNative() const;

    QString dsn = qEnvironmentVariable("SENTRY_DSN");
    QString databasePath;
    QString release = QString::fromUtf8(SENTRY_RELEASE);
    QString environment = "playground";
    QString dist;
    bool attachScreenshot = false;
    double sampleRate = 1.0;
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
    bool enableAppHangTracking = false;
    int appHangTimeout = 5000;
    int appHangThreadStackwalkMode;
    int threadStackwalkMode;
    int crashUploadMode;
    int maxStackCaptureSize = 0;
    bool attachStacktrace = false;
    bool autoSessionTracking = true;
    bool sendClientReports = true;
    bool symbolizeStacktraces;
    bool propagateTraceparent = false;
    bool strictTraceContinuation = false;
    bool loggerEnabledWhenCrashed;
    QString proxy;
    QString caCerts;
    QString orgId;
    int transferTimeout = 0;
    int crashMemoryReserve = 0;
};

#endif // OPTIONS_H
