#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QQmlApplicationEngine>

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
    void on_breadcrumbButton_clicked();
    void on_screenshotButton_clicked();
    void on_quickButton_clicked();
    void on_nullDerefButton_clicked();
    void on_stackOverflowButton_clicked();
    void on_assertButton_clicked();
    void on_abortButton_clicked();

private:
    Ui::MainWindow *ui;
    QQmlApplicationEngine qml;
};

#endif // MAINWINDOW_H
