#ifndef EVENTCARD_H
#define EVENTCARD_H

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qtimer.h>
#include <QtWidgets/qpushbutton.h>

class QLabel;
class QProcess;
class QToolButton;

class EventCard : public QPushButton
{
    Q_OBJECT

public:
    explicit EventCard(QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void refreshPaletteStyles();

protected:
    void initStyleOption(QStyleOptionButton* option) const override;

private:
    enum class State {
        Pending,
        Received,
        Errors,
        NotFound,
        Discarded,
        Offline,
    };

    bool canOpen() const;
    void track(const QString& eventId, const QString& pendingDetail = {});
    void resume();
    void dismiss();
    void lookup();
    void finishLookup(QProcess* process, int exitCode);
    void setState(State state, const QString& title, const QString& detail = {}, const QString& toolTip = {});

    QLabel* m_icon = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_detail = nullptr;
    QLabel* m_id = nullptr;
    QToolButton* m_closeButton = nullptr;
    QProcess* m_process = nullptr;
    QTimer m_pollTimer;
    QElapsedTimer m_elapsed;
    QString m_eventId;
    QString m_groupId;
    QString m_pendingDetail;
    State m_state = State::Pending;
};

#endif // EVENTCARD_H
