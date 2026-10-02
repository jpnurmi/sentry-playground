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
    void updateLogAction();
    void updateMetricAction();

    QAction* m_messageAction = nullptr;
    QAction* m_logAction = nullptr;
    QAction* m_metricAction = nullptr;

    Ui::RuntimePane ui;
};

#endif // RUNTIMEPANE_H
