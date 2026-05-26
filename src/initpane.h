#ifndef INITPANE_H
#define INITPANE_H

#include <QWidget>

#include "options.h"
#include "ui_initpane.h"

class InitPane : public QWidget
{
    Q_OBJECT

public:
    explicit InitPane(QWidget* parent = nullptr);

    void populate();
    void refreshPaletteStyles();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupSummaryRows();
    void setupFormAlignment();
    void setupControls();
    void populateEnvironmentHistory(const QString& currentEnvironment);
    void rememberEnvironment(const QString& environment);
    Options optionsFromPage() const;
    void updateCrashReporterControls();
    void updateDetailsVisibility();
    void updateSummaries();

    Ui::InitPane ui;
};

#endif // INITPANE_H
