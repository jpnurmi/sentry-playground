#ifndef SENTRYWINDOW_H
#define SENTRYWINDOW_H

#include <QMainWindow>

#include "sentryplayground.h"
#include "ui_sentrywindow.h"

class SentryWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit SentryWindow(QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupPages();
    void populateInitPage();
    SentryPlayground::InitOptions initOptionsFromPage() const;
    void showInitPage();
    void showRuntimePage();
    void updateLogo();

    Ui::SentryWindow ui;
};

#endif // SENTRYWINDOW_H
