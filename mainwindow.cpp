#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QTimer>

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
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_breadcrumbButton_clicked()
{
    add_breadcrumb();
}

void MainWindow::on_screenshotButton_clicked()
{
    // QTimer::singleShot(1000, []() {
    //     sentry_capture_screenshot();
    // });
}

void MainWindow::on_quickButton_clicked()
{
    qml.loadFromModule("sentry-playground", "Main");
}

void MainWindow::on_nullDerefButton_clicked()
{
    trigger_null_deref();
}

void MainWindow::on_stackOverflowButton_clicked()
{
    trigger_stack_overflow();
}

void MainWindow::on_assertButton_clicked()
{
    assert(false);
}

void MainWindow::on_abortButton_clicked()
{
    std::abort();
}
