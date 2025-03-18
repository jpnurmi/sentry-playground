#include "mainwindow.h"
#include "ui_mainwindow.h"

static void trigger_null_deref()
{
    int* ptr = nullptr;
    *ptr = 42;
}

static void trigger_stack_overflow()
{
    alloca(1024);
    trigger_stack_overflow();
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowFilePath(SENTRY_BACKEND);
#ifndef Q_OS_WINDOWS
    ui->fastfailButton->setEnabled(false);
#endif
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_actionQuit_triggered()
{
    QCoreApplication::quit();
}


void MainWindow::on_actionSubwindow_triggered()
{
    MainWindow* subwindow = new MainWindow();
    subwindow->resize(300, 200);
    subwindow->show();
}

void MainWindow::on_nullDerefButton_clicked()
{
    trigger_null_deref();
}

void MainWindow::on_stackOverflowButton_clicked()
{
    trigger_stack_overflow();
}

void MainWindow::on_fastfailButton_clicked()
{
#ifdef Q_OS_WINDOWS
    __fastfail(77);
#endif
}

void MainWindow::on_assertButton_clicked()
{
    assert(false);
}

void MainWindow::on_abortButton_clicked()
{
    std::abort();
}
