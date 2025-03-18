#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_actionQuit_triggered();
    void on_actionSubwindow_triggered();
    void on_nullDerefButton_clicked();
    void on_stackOverflowButton_clicked();
    void on_fastfailButton_clicked();
    void on_assertButton_clicked();
    void on_abortButton_clicked();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
