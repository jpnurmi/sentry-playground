#ifndef SENTRYWINDOW_H
#define SENTRYWINDOW_H

#include <QMainWindow>

#include "sentryplayground.h"
#include "ui_sentrywindow.h"

class QAction;

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
    void setupWheelScrolling();
    void populateInitPage();
    void populateInitEnvironmentHistory(const QString& currentEnvironment);
    void rememberInitEnvironment(const QString& environment);
    SentryPlayground::InitOptions initOptionsFromPage() const;
    void showInitPage();
    void showRuntimePage();
    void applyInitPaletteStyles();
    void updateInitDetailsVisibility();
    void updateInitSummaries();
    void updateLogo();

    Ui::SentryWindow ui;
};

#endif // SENTRYWINDOW_H
