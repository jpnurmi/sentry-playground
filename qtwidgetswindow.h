#ifndef QTWIDGETSWINDOW_H
#define QTWIDGETSWINDOW_H

#include <QMainWindow>

#include "ui_qtwidgetswindow.h"

class QtWidgetsWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit QtWidgetsWindow(QWidget *parent = nullptr);

private:
    Ui::QtWidgetsWindow ui;
};

#endif // QTWIDGETSWINDOW_H
