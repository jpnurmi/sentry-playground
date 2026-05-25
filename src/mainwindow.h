#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "playground.h"
#include "ui_mainwindow.h"

class QAction;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupPages();
    void setupWheelScrolling();
    void showInitPage();
    void showRuntimePage();
    void updateStatusBarVisibility();
    void updateConsentStatus(Qt::CheckState state);
    void applyLeftPanelStyles();
    void updateLogo();

    Ui::MainWindow ui;
};

#endif // MAINWINDOW_H
