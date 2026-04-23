#ifndef SENTRYWINDOW_H
#define SENTRYWINDOW_H

#include <QMainWindow>

#include "ui_sentrywindow.h"

class SentryWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit SentryWindow(QWidget *parent = nullptr);

private:
    Ui::SentryWindow ui;
};

#endif // SENTRYWINDOW_H
