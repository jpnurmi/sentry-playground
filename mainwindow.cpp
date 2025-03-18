#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QLabel>

#include <sentry.h>

static void add_breadcrumb()
{
    sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, "Just a debug crumb...");
    sentry_value_set_by_key(crumb, "category", sentry_value_new_string("sample"));
    sentry_value_set_by_key(crumb, "level", sentry_value_new_string("debug"));
    sentry_add_breadcrumb(crumb);
}

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
#ifndef Q_OS_WINDOWS
    ui->fastfailButton->setEnabled(false);
#endif
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_breadcrumbButton_clicked()
{
    add_breadcrumb();
}

void MainWindow::on_subwindowButton_clicked()
{
    QLabel* subwindow = new QLabel("Sub-window");
    subwindow->setAlignment(Qt::AlignCenter);
    subwindow->setWindowFilePath(SENTRY_BACKEND);
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
