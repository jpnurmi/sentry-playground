#include "options.h"

#include <sentry.h>

#include <algorithm>
#include <ctime>

#include <QtCore/qbytearray.h>
#include <QtCore/qdatastream.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qtenvironmentvariables.h>
#include <QtCore/qvariant.h>

namespace {

static constexpr int kOptionsSchemaVersion = 2;

bool resolveDebug()
{
    const QByteArray debug = qgetenv("SENTRY_DEBUG");
    if (debug == "1")
        return true;
#if !defined(NDEBUG)
    if (debug != "0")
        return true;
#endif
    return false;
}

int normalizeLoggerLevel(int level)
{
    switch (level) {
    case SENTRY_LEVEL_TRACE:
    case SENTRY_LEVEL_DEBUG:
    case SENTRY_LEVEL_INFO:
    case SENTRY_LEVEL_WARNING:
    case SENTRY_LEVEL_ERROR:
    case SENTRY_LEVEL_FATAL:
        return level;
    default:
        return SENTRY_LEVEL_DEBUG;
    }
}

int normalizeCacheKeepMode(int mode)
{
    switch (mode) {
    case SENTRY_CACHE_KEEP_NONE:
    case SENTRY_CACHE_KEEP_OFFLINE:
    case SENTRY_CACHE_KEEP_ALWAYS:
        return mode;
    default:
        return SENTRY_CACHE_KEEP_OFFLINE;
    }
}

int normalizeCrashReportingMode(int mode)
{
    switch (mode) {
    case SENTRY_CRASH_REPORTING_MODE_MINIDUMP:
    case SENTRY_CRASH_REPORTING_MODE_NATIVE:
    case SENTRY_CRASH_REPORTING_MODE_NATIVE_WITH_MINIDUMP:
        return mode;
    default:
        return SENTRY_CRASH_REPORTING_MODE_NATIVE_WITH_MINIDUMP;
    }
}

int normalizeMinidumpMode(int mode)
{
    switch (mode) {
    case SENTRY_MINIDUMP_MODE_STACK_ONLY:
    case SENTRY_MINIDUMP_MODE_SMART:
    case SENTRY_MINIDUMP_MODE_FULL:
        return mode;
    default:
        return SENTRY_MINIDUMP_MODE_SMART;
    }
}

} // namespace

Options::Options()
    : debug(resolveDebug())
    , crashReportingMode(SENTRY_CRASH_REPORTING_MODE_NATIVE_WITH_MINIDUMP)
    , minidumpMode(SENTRY_MINIDUMP_MODE_SMART)
{
}

bool Options::load(const QByteArray &data)
{
    if (data.isEmpty())
        return false;

    QDataStream stream(data);
    stream.setVersion(QDataStream::Qt_6_0);
    QVariantMap values;
    stream >> values;
    if (stream.status() != QDataStream::Ok)
        return false;

    dsn = values.value("dsn", dsn).toString();
    databasePath = values.value("databasePath", databasePath).toString();
    release = values.value("release", release).toString();
    environment = values.value("environment", environment).toString();
    dist = values.value("dist", dist).toString();
    attachScreenshot = values.value("attachScreenshot", attachScreenshot).toBool();
    tracesSampleRate = values.value("tracesSampleRate", tracesSampleRate).toDouble();
    maxBreadcrumbs = values.value("maxBreadcrumbs", maxBreadcrumbs).toInt();
    maxSpans = values.value("maxSpans", maxSpans).toInt();
    shutdownTimeout = values.value("shutdownTimeout", shutdownTimeout).toInt();
    requireUserConsent = values.value("requireUserConsent", requireUserConsent).toBool();
    systemCrashReporterEnabled = values.value("systemCrashReporterEnabled", systemCrashReporterEnabled).toBool();
    enableLargeAttachments = values.value("enableLargeAttachments", enableLargeAttachments).toBool();
    httpRetry = values.value("httpRetry", httpRetry).toBool();
    cacheKeepMode = values.value("cacheKeepMode", cacheKeepMode).toInt();
    cacheMaxItems = values.value("cacheMaxItems", cacheMaxItems).toInt();
    cacheMaxSize = values.value("cacheMaxSize", cacheMaxSize).toInt();
    cacheMaxAge = values.value("cacheMaxAge", cacheMaxAge).toInt();
    debug = values.value("debug", debug).toBool();
    loggerLevel = values.value("loggerLevel", loggerLevel).toInt();
    externalCrashReporterEnabled = values.value("externalCrashReporterEnabled", externalCrashReporterEnabled).toBool();
    externalCrashReporterPath = values.value("externalCrashReporterPath", externalCrashReporterPath).toString();
    crashReportingMode = values.value("crashReportingMode", crashReportingMode).toInt();
    minidumpMode = values.value("minidumpMode", minidumpMode).toInt();

    maxBreadcrumbs = std::max(0, maxBreadcrumbs);
    maxSpans = std::max(0, maxSpans);
    shutdownTimeout = std::max(0, shutdownTimeout);
    cacheKeepMode = normalizeCacheKeepMode(cacheKeepMode);
    cacheMaxItems = std::max(0, cacheMaxItems);
    cacheMaxSize = std::max(0, cacheMaxSize);
    cacheMaxAge = std::max(0, cacheMaxAge);
    loggerLevel = normalizeLoggerLevel(loggerLevel);
    crashReportingMode = normalizeCrashReportingMode(crashReportingMode);
    minidumpMode = normalizeMinidumpMode(minidumpMode);
    return true;
}

QByteArray Options::save() const
{
    QVariantMap values;
    values.insert("version", kOptionsSchemaVersion);
    values.insert("dsn", dsn);
    values.insert("databasePath", databasePath);
    values.insert("release", release);
    values.insert("environment", environment);
    values.insert("dist", dist);
    values.insert("attachScreenshot", attachScreenshot);
    values.insert("tracesSampleRate", tracesSampleRate);
    values.insert("maxBreadcrumbs", maxBreadcrumbs);
    values.insert("maxSpans", maxSpans);
    values.insert("shutdownTimeout", shutdownTimeout);
    values.insert("requireUserConsent", requireUserConsent);
    values.insert("systemCrashReporterEnabled", systemCrashReporterEnabled);
    values.insert("enableLargeAttachments", enableLargeAttachments);
    values.insert("httpRetry", httpRetry);
    values.insert("cacheKeepMode", normalizeCacheKeepMode(cacheKeepMode));
    values.insert("cacheMaxItems", cacheMaxItems);
    values.insert("cacheMaxSize", cacheMaxSize);
    values.insert("cacheMaxAge", cacheMaxAge);
    values.insert("debug", debug);
    values.insert("loggerLevel", normalizeLoggerLevel(loggerLevel));
    values.insert("externalCrashReporterEnabled", externalCrashReporterEnabled);
    values.insert("externalCrashReporterPath", externalCrashReporterPath);
    values.insert("crashReportingMode", normalizeCrashReportingMode(crashReportingMode));
    values.insert("minidumpMode", normalizeMinidumpMode(minidumpMode));

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << values;
    return data;
}

sentry_options_t *Options::toNative() const
{
    sentry_options_t *opt = sentry_options_new();
    if (!dsn.isEmpty())
        sentry_options_set_dsn(opt, dsn.toUtf8());
    if (!databasePath.isEmpty())
        sentry_options_set_database_path(opt, databasePath.toUtf8());
    if (!release.isEmpty())
        sentry_options_set_release(opt, release.toUtf8());
    if (!environment.isEmpty())
        sentry_options_set_environment(opt, environment.toUtf8());
    if (!dist.isEmpty())
        sentry_options_set_dist(opt, dist.toUtf8());
    if (externalCrashReporterEnabled && !externalCrashReporterPath.isEmpty())
        sentry_options_set_external_crash_reporter_path(opt, externalCrashReporterPath.toUtf8());
    sentry_options_set_attach_screenshot(opt, attachScreenshot);
    sentry_options_set_traces_sample_rate(opt, tracesSampleRate);
    sentry_options_set_max_breadcrumbs(opt, static_cast<size_t>(std::max(0, maxBreadcrumbs)));
    sentry_options_set_max_spans(opt, static_cast<size_t>(std::max(0, maxSpans)));
    sentry_options_set_shutdown_timeout(opt, static_cast<uint64_t>(std::max(0, shutdownTimeout)));
    sentry_options_set_require_user_consent(opt, requireUserConsent);
    sentry_options_set_system_crash_reporter_enabled(opt, systemCrashReporterEnabled);
    sentry_options_set_crashpad_wait_for_upload(opt, true);
    sentry_options_set_enable_large_attachments(opt, enableLargeAttachments);
    sentry_options_set_http_retry(opt, httpRetry);
    sentry_options_set_cache_keep(opt, normalizeCacheKeepMode(cacheKeepMode));
    sentry_options_set_cache_max_items(opt, static_cast<size_t>(std::max(0, cacheMaxItems)));
    sentry_options_set_cache_max_size(opt, static_cast<size_t>(std::max(0, cacheMaxSize)));
    sentry_options_set_cache_max_age(opt, static_cast<time_t>(std::max(0, cacheMaxAge)));
    sentry_options_set_debug(opt, debug);
    sentry_options_set_logger_level(opt, static_cast<sentry_level_t>(normalizeLoggerLevel(loggerLevel)));
    sentry_options_set_crash_reporting_mode(
        opt,
        static_cast<sentry_crash_reporting_mode_t>(normalizeCrashReportingMode(crashReportingMode)));
    sentry_options_set_minidump_mode(
        opt,
        static_cast<sentry_minidump_mode_t>(normalizeMinidumpMode(minidumpMode)));
    return opt;
}
