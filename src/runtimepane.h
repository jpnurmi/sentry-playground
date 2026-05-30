#ifndef RUNTIMEPANE_H
#define RUNTIMEPANE_H

#include <QWidget>

#include "ui_runtimepane.h"

class QAction;
class QTreeWidget;

class RuntimePane : public QWidget
{
    Q_OBJECT

public:
    explicit RuntimePane(QWidget* parent = nullptr);

    void refreshPaletteStyles();

private:
    void refreshAttachments();
    void updateSegmentedButtonWidths();
    void updateAddButton();
    void updateMessageAction();
    void updateSessionButton();

    QAction* m_messageAction = nullptr;

    Ui::RuntimePane ui;
};

#endif // RUNTIMEPANE_H
