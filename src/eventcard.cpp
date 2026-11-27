#include "eventcard.h"
#include "debugfilesdialog.h"
#include "playground.h"
#include "style.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qlibraryinfo.h>
#include <QtCore/qprocess.h>
#include <QtCore/qurl.h>
#include <QtGui/qdesktopservices.h>
#include <QtGui/qfontdatabase.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qtoolbutton.h>

static constexpr int kPollInterval = 3000;
static constexpr int kLookupTimeout = 60000;

namespace {

// elides the text instead of clipping it when the card gets narrow
class ElidedLabel : public QLabel
{
public:
    using QLabel::QLabel;

    QSize minimumSizeHint() const override
    {
        return QSize(0, QLabel::minimumSizeHint().height());
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        const QRect rect = contentsRect();
        painter.drawText(rect, int(alignment()),
            fontMetrics().elidedText(text(), Qt::ElideRight, rect.width()));
    }
};

void setupSentry(QProcess& process, const QStringList& arguments)
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    // the CLI resolves the org and project from the DSN
    environment.insert(QStringLiteral("SENTRY_DSN"), Playground::instance()->options().dsn);
    process.setProcessEnvironment(environment);
    process.setProgram(DebugFilesDialog::sentryProgram());
    process.setArguments(arguments);
}

// the app and Qt directories that the debug files dialog uploads
bool isOwnPath(const QString& path)
{
    const QString file = QDir::fromNativeSeparators(path);
    for (const QString& directory : { QCoreApplication::applicationDirPath(), QLibraryInfo::path(QLibraryInfo::PrefixPath) }) {
        if (file.startsWith(directory + '/'))
            return true;
    }
    return false;
}

QStringList ownDebugIds()
{
    QStringList ids;
    const sentry_value_t modules = sentry_get_modules_list();
    for (size_t i = 0; i < sentry_value_get_length(modules); ++i) {
        const sentry_value_t module = sentry_value_get_by_index(modules, i);
        if (isOwnPath(QString::fromUtf8(sentry_value_as_string(sentry_value_get_by_key(module, "code_file")))))
            ids.append(QString::fromUtf8(sentry_value_as_string(sentry_value_get_by_key(module, "debug_id"))).toLower());
    }
    sentry_value_decref(modules);
    return ids;
}

} // namespace

EventCard::EventCard(QWidget* parent)
    : QPushButton(parent)
{
    setFlat(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_icon = new QLabel(this);
    m_title = new QLabel(this);
    m_detail = new ElidedLabel(this);
    m_id = new QLabel(this);
    m_closeButton = new QToolButton(this);

    m_icon->setFixedSize(16, 16);
    m_icon->setAlignment(Qt::AlignCenter);
    m_title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    m_detail->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_id->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_id->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_closeButton->setToolTip("Dismiss");
    m_closeButton->setFixedSize(22, 22);
    m_closeButton->setIconSize(QSize(14, 14));
    m_closeButton->setAutoRaise(true);
    m_closeButton->setCursor(Qt::PointingHandCursor);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 5, 5, 5);
    layout->setSpacing(8);
    layout->addWidget(m_icon);
    layout->addWidget(m_title);
    layout->addWidget(m_detail);
    layout->addWidget(m_id);
    layout->addWidget(m_closeButton);

    m_pollTimer.setSingleShot(true);
    m_pollTimer.setInterval(kPollInterval);
    connect(&m_pollTimer, &QTimer::timeout, this, &EventCard::lookup);
    connect(this, &QAbstractButton::clicked, this, [this]() {
        if (!canOpen())
            return;
        // event view --web only searches issues by the event ID, so open the event in its issue
        auto* process = new QProcess(this);
        setupSentry(*process, { QStringLiteral("issue"), QStringLiteral("view"), QStringLiteral("--json"),
            QStringLiteral("--fields"), QStringLiteral("permalink"), m_groupId });
        connect(process, &QProcess::finished, this, [process, eventId = m_eventId]() {
            process->deleteLater();
            const QString permalink = QJsonDocument::fromJson(process->readAllStandardOutput())
                .object().value(QStringLiteral("permalink")).toString();
            if (!permalink.isEmpty())
                QDesktopServices::openUrl(QUrl(permalink).resolved(QUrl(QStringLiteral("events/%1/").arg(eventId))));
        });
        process->start();
    });
    connect(m_closeButton, &QAbstractButton::clicked, this, &EventCard::dismiss);

    Playground* playground = Playground::instance();
    connect(playground, &Playground::eventCaptured, this, [this](const QString& eventId) {
        track(eventId);
    });
    connect(playground, &Playground::crashedLastRun, this, [this](const QString& eventId) {
        track(eventId, "Crashed last run");
    });
    connect(playground, &Playground::offlineChanged, this, [this](bool offline) {
        if (!offline && m_state == State::Offline && !isHidden())
            resume();
    });
    connect(playground, &Playground::initializedChanged, this, [this](bool initialized) {
        if (!initialized)
            dismiss();
    });

    refreshPaletteStyles();
    hide();
}

// size by the layout instead of the button text
QSize EventCard::sizeHint() const
{
    return QWidget::sizeHint();
}

QSize EventCard::minimumSizeHint() const
{
    return QWidget::minimumSizeHint();
}

void EventCard::initStyleOption(QStyleOptionButton* option) const
{
    QPushButton::initStyleOption(option);
    // hovering a label counts as hovering the card
    option->state.setFlag(QStyle::State_MouseOver, underMouse());
}

void EventCard::refreshPaletteStyles()
{
    // the card background in the style sheet overrides the card's own palette
    const QPalette windowPalette = window()->palette();
    const qreal dpr = devicePixelRatioF();
    const QColor window = windowPalette.color(QPalette::Window);
    const QColor text = windowPalette.color(QPalette::WindowText);
    const QColor gray = Style::blendedColor(window, text, 120);

    QColor color = gray;
    QIcon icon = Style::makeWarningIcon(gray, dpr);
    switch (m_state) {
    case State::Pending: {
        QPalette tinted = windowPalette;
        tinted.setColor(QPalette::Text, gray);
        icon = Style::makeRestartIcon(tinted, dpr);
        break;
    }
    case State::Received:
        color = QColor("#2ecc71");
        icon = Style::makeCheckIcon(color, dpr);
        break;
    case State::Errors:
        color = QColor("#f39c12");
        icon = Style::makeWarningIcon(color, dpr);
        break;
    case State::NotFound:
        color = QColor("#e74c3c");
        icon = Style::makeWarningIcon(color, dpr);
        break;
    case State::Discarded:
    case State::Offline:
        break;
    }

    QString style = QStringLiteral("EventCard { %1 padding: 0; }").arg(Style::cardStyle(windowPalette, color));
    if (canOpen()) {
        style += QStringLiteral(
            "EventCard:hover { background: %1; }"
            "EventCard:pressed { background: %2; }")
            .arg(Style::cssRgb(Style::blendedColor(window, color, 52)),
                 Style::cssRgb(Style::blendedColor(window, color, 68)));
    }
    setStyleSheet(style);
    m_icon->setPixmap(icon.pixmap(QSize(16, 16), dpr));
    const QString mutedStyle = QStringLiteral("color: %1;")
        .arg(Style::cssRgb(Style::blendedColor(window, text, 150)));
    m_detail->setStyleSheet(mutedStyle);
    m_id->setStyleSheet(mutedStyle);
    m_closeButton->setIcon(Style::makeClearIcon(windowPalette, dpr));
    m_closeButton->setStyleSheet(Style::circularButtonStyle(windowPalette, QStringLiteral("background: transparent;")));
}

bool EventCard::canOpen() const
{
    // only events that reached Sentry have a page there
    return m_state == State::Received || m_state == State::Errors;
}

void EventCard::track(const QString& eventId, const QString& pendingDetail)
{
    dismiss();
    m_eventId = eventId;
    m_pendingDetail = pendingDetail;
    m_id->setText(eventId.left(8));
    m_id->setToolTip(eventId);

    Playground* playground = Playground::instance();
    const Options options = playground->options();
    if (eventId.isEmpty())
        setState(State::Discarded, "Discarded", "Filtered or sampled out before sending");
    else if (options.requireUserConsent && playground->consent() != Qt::Checked)
        setState(State::Discarded, "Discarded", "Consent not given");
    else if (playground->offline() && !options.httpRetry)
        setState(State::Discarded, "Discarded", "Captured offline without HTTP retry");
    else if (playground->offline())
        setState(State::Offline, "Offline", "Cached until back online");
    else
        resume();
    show();
}

void EventCard::resume()
{
    setState(State::Pending, "Waiting for Sentry", m_pendingDetail);
    m_elapsed.start();
    lookup();
}

void EventCard::dismiss()
{
    // a running lookup finishes on its own and gets ignored
    m_process = nullptr;
    m_pollTimer.stop();
    hide();
}

void EventCard::lookup()
{
    auto* process = new QProcess(this);
    setupSentry(*process, { QStringLiteral("event"), QStringLiteral("view"), QStringLiteral("--json"), m_eventId });
    connect(process, &QProcess::finished, this, [this, process](int exitCode) {
        process->deleteLater();
        if (process == m_process)
            finishLookup(process, exitCode);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        process->deleteLater();
        if (process != m_process)
            return;
        m_process = nullptr;
        setState(State::NotFound, "Lookup failed", "Sentry CLI not found");
    });
    m_process = process;
    process->start();
}

void EventCard::finishLookup(QProcess* process, int exitCode)
{
    m_process = nullptr;

    const QJsonObject event = QJsonDocument::fromJson(process->readAllStandardOutput()).object();
    if (exitCode == 0 && event.contains(QStringLiteral("eventID"))) {
        m_groupId = event.value(QStringLiteral("groupID")).toString();

        // only errors in images that the debug files dialog uploads can be fixed
        const QStringList ownIds = ownDebugIds();
        QStringList seen;
        QStringList messages;
        QStringList ignored;
        for (const QJsonValue& error : event.value(QStringLiteral("errors")).toArray()) {
            const QJsonObject object = error.toObject();
            const QJsonObject data = object.value(QStringLiteral("data")).toObject();
            const QString message = object.value(QStringLiteral("message")).toString();
            const QString path = data.value(QStringLiteral("image_path")).toString();
            const QString id = data.value(QStringLiteral("image_uuid")).toString().toLower();
            // an image can be reported by both its path and its soname
            const QString key = object.value(QStringLiteral("type")).toString() + '/' + (id.isEmpty() ? path + message : id);
            if (seen.contains(key))
                continue;
            seen.append(key);

            const QString text = path.isEmpty() ? message : QStringLiteral("%1 (%2)").arg(message, QFileInfo(path).fileName());
            if (path.isEmpty() || isOwnPath(path) || (!id.isEmpty() && ownIds.contains(id)))
                messages.append(text);
            else
                ignored.append(text);
        }

        // native crash titles are just the crashing function, so add the error like Sentry's issue list does
        QString title = event.value(QStringLiteral("title")).toString();
        const QString value = event.value(QStringLiteral("metadata")).toObject().value(QStringLiteral("value")).toString();
        if (!value.isEmpty() && !title.contains(value))
            title = QStringLiteral("%1 · %2").arg(title, value);
        QStringList toolTip = messages.isEmpty() ? QStringList{ title } : messages;
        if (!ignored.isEmpty())
            toolTip << QString() << QStringLiteral("Ignored for system libraries:") << ignored;
        if (messages.isEmpty()) {
            setState(State::Received, "Received", title, toolTip.join('\n'));
            return;
        }
        setState(State::Errors,
            messages.size() == 1 ? QStringLiteral("1 processing error") : QStringLiteral("%1 processing errors").arg(messages.size()),
            messages.first(), toolTip.join('\n'));
        return;
    }

    if (m_elapsed.elapsed() < kLookupTimeout) {
        m_pollTimer.start();
        return;
    }

    // the CLI explains why, such as a missing event or login
    QString reason;
    for (const QString& line : QString::fromUtf8(process->readAllStandardError()).split('\n')) {
        if (line.startsWith(QStringLiteral("Error: "))) {
            reason = line.mid(7).trimmed();
            break;
        }
        if (!line.trimmed().isEmpty())
            reason = line.trimmed();
    }
    setState(State::NotFound, "Not found", reason);
}

void EventCard::setState(State state, const QString& title, const QString& detail, const QString& toolTip)
{
    m_state = state;
    m_title->setText(title);
    m_detail->setText(detail);
    m_detail->setToolTip(toolTip.isEmpty() ? detail : toolTip);
    setToolTip(canOpen() ? QStringLiteral("Open in Sentry") : QString());
    refreshPaletteStyles();
}
