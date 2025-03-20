#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QThread>
#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

static void trigger_null_deref()
{
    qDebug().nospace() << "[sentry-playground] trigger_null_deref tid=" << gettid();

    int* ptr = nullptr;
    *ptr = 42;
}

static void trigger_stack_overflow()
{
    qDebug().nospace() << "[sentry-playground] trigger_stack_overflow tid=" << gettid();

    alloca(1024);
    trigger_stack_overflow();
}

static void trigger_fastfail()
{
#ifdef Q_OS_WINDOWS
    qDebug().nospace() << "[sentry-playground] trigger_fastfail tid=" << gettid();

    __fastfail(77);
#endif
}

static void trigger_assert_failure()
{
    qDebug().nospace() << "[sentry-playground] trigger_assert_failure tid=" << gettid();

    assert(false);
}

static void trigger_abort()
{
    qDebug().nospace() << "[sentry-playground] trigger_abort tid=" << gettid();

    std::abort();
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowFilePath(SENTRY_BACKEND);
    ui->backendLabel->setText(SENTRY_BACKEND);
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
    if (ui->workerBox->isChecked()) {
        QThread::create(trigger_null_deref)->start();
    } else {
        trigger_null_deref();
    }
}

void MainWindow::on_stackOverflowButton_clicked()
{
    if (ui->workerBox->isChecked()) {
        QThread::create(trigger_stack_overflow)->start();
    } else {
        trigger_stack_overflow();
    }
}

void MainWindow::on_fastfailButton_clicked()
{
    if (ui->workerBox->isChecked()) {
        QThread::create(trigger_fastfail)->start();
    } else {
        trigger_fastfail();
    }
}

void MainWindow::on_assertButton_clicked()
{
    if (ui->workerBox->isChecked()) {
        QThread::create(trigger_assert_failure)->start();
    } else {
        trigger_assert_failure();
    }
}

void MainWindow::on_abortButton_clicked()
{
    if (ui->workerBox->isChecked()) {
        QThread::create(trigger_abort)->start();
    } else {
        trigger_abort();
    }
}
