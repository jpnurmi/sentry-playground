#ifndef CRASHPANE_H
#define CRASHPANE_H

#include <QWidget>

#include "ui_crashpane.h"

class Playground;

class CrashPane : public QWidget
{
    Q_OBJECT

public:
    explicit CrashPane(QWidget* parent = nullptr);

private:
    void triggerCrash(void (*crashFunction)());

    Playground* playground() const;

    Ui::CrashPane ui;
};

#endif // CRASHPANE_H
