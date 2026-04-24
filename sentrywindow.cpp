#include "sentrywindow.h"
#include "sentryplayground.h"
#include "sentrytrace.h"

#include <sentry.h>

#include <functional>

#include <QtCore/qfileinfo.h>
#include <QtCore/qlocale.h>
#include <QtCore/qsettings.h>
#include <QtCore/qstandardpaths.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qfiledialog.h>
#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qstackedwidget.h>
#include <QtWidgets/qstatusbar.h>
#include <QtWidgets/qtoolbutton.h>
#include <QtWidgets/qtreewidget.h>

SentryWindow::SentryWindow(QWidget *parent)
    : QMainWindow(parent)
{
    TRACE_FUNCTION();
    ui.setupUi(this);
    setWindowTitle(windowTitle() + " (" + SentryPlayground::backend() + ")");
#ifndef Q_OS_WINDOWS
    ui.fastfailButton->setEnabled(false);
#endif

    SentryPlayground* playground = SentryPlayground::instance();
    QObject::connect(ui.crashButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerCrash);
    QObject::connect(ui.stackOverflowButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerStackOverflow);
    QObject::connect(ui.fastfailButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerFastfail);
    QObject::connect(ui.assertButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerAssertFailure);
    QObject::connect(ui.abortButton, &QAbstractButton::clicked, playground, &SentryPlayground::triggerAbort);

    ui.messageLevelBox->addItem("Debug", SENTRY_LEVEL_DEBUG);
    ui.messageLevelBox->addItem("Info", SENTRY_LEVEL_INFO);
    ui.messageLevelBox->addItem("Warning", SENTRY_LEVEL_WARNING);
    ui.messageLevelBox->addItem("Error", SENTRY_LEVEL_ERROR);
    ui.messageLevelBox->addItem("Fatal", SENTRY_LEVEL_FATAL);
    ui.messageLevelBox->setCurrentIndex(1);
    ui.messageText->setText(QSettings().value("message", "Hello from Sentry Playground").toString());
    QObject::connect(ui.messageText, &QLineEdit::textEdited, this, [](const QString& text) {
        QSettings().setValue("message", text);
    });
    QObject::connect(ui.captureMessageButton, &QAbstractButton::clicked, playground, [this, playground]() {
        playground->captureMessage(ui.messageLevelBox->currentData().toInt(), ui.messageText->text());
    });

    const char* kSegmentedBase =
        "QPushButton { color: #888; font-weight: bold; background: transparent;"
        " border: 1px solid #555; padding: 2px 10px; %1 }"
        "QPushButton:checked { background: #444; color: white; }";
    ui.tagsButton->setStyleSheet(QString(kSegmentedBase).arg(
        "border-top-left-radius: 4px; border-bottom-left-radius: 4px;"));
    ui.contextsButton->setStyleSheet(QString(kSegmentedBase).arg("border-left: none;"));
    ui.attachmentsButton->setStyleSheet(QString(kSegmentedBase).arg(
        "border-left: none; border-top-right-radius: 4px; border-bottom-right-radius: 4px;"));
    ui.tagsButton->setChecked(true);
    ui.categoryStack->setCurrentIndex(0);
    QObject::connect(ui.tagsButton, &QAbstractButton::clicked, this,
        [this]() { ui.categoryStack->setCurrentIndex(0); });
    QObject::connect(ui.contextsButton, &QAbstractButton::clicked, this,
        [this]() { ui.categoryStack->setCurrentIndex(1); });
    QObject::connect(ui.attachmentsButton, &QAbstractButton::clicked, this,
        [this]() { ui.categoryStack->setCurrentIndex(2); });

    auto setupTree = [](QTreeWidget* tree, int narrowColumn, QHeaderView::ResizeMode narrowMode, int narrowWidth = -1) {
        int stretchColumn = 1 - narrowColumn;
        tree->header()->setStretchLastSection(false);
        tree->header()->setSectionResizeMode(narrowColumn, narrowMode);
        tree->header()->setSectionResizeMode(stretchColumn, QHeaderView::Stretch);
        if (narrowWidth > 0)
            tree->header()->resizeSection(narrowColumn, narrowWidth);
        tree->setRootIsDecorated(false);
        tree->setUniformRowHeights(true);
        tree->setAllColumnsShowFocus(true);
    };
    setupTree(ui.tagsTable, 0, QHeaderView::Interactive, 100);
    setupTree(ui.contextsTable, 0, QHeaderView::Interactive, 100);
    setupTree(ui.attachmentTable, 1, QHeaderView::ResizeToContents);

    auto wireKeyValueTree = [this](QTreeWidget* tree,
        std::function<void(const QString&, const QString&)> onSet,
        std::function<void(const QString&)> onRemove) {
        QObject::connect(tree, &QTreeWidget::itemChanged, this,
            [onSet, onRemove](QTreeWidgetItem* item, int) {
                QString key = item->text(0).trimmed();
                QString value = item->text(1);
                QString oldKey = item->data(0, Qt::UserRole).toString();
                if (!oldKey.isEmpty() && oldKey != key)
                    onRemove(oldKey);
                if (!key.isEmpty()) {
                    onSet(key, value);
                    item->setData(0, Qt::UserRole, key);
                }
            });
        QObject::connect(tree, &QWidget::customContextMenuRequested, this,
            [this, tree, onRemove](const QPoint& pos) {
                auto* item = tree->itemAt(pos);
                if (!item)
                    return;
                QMenu menu(this);
                menu.addAction("Remove", [tree, item, onRemove]() {
                    QString oldKey = item->data(0, Qt::UserRole).toString();
                    if (!oldKey.isEmpty())
                        onRemove(oldKey);
                    delete tree->takeTopLevelItem(tree->indexOfTopLevelItem(item));
                });
                menu.exec(tree->viewport()->mapToGlobal(pos));
            });
    };
    wireKeyValueTree(ui.tagsTable,
        [playground](const QString& k, const QString& v) { playground->setTag(k, v); },
        [playground](const QString& k) { playground->removeTag(k); });
    wireKeyValueTree(ui.contextsTable,
        [playground](const QString& k, const QString& v) { playground->setContext(k, v); },
        [playground](const QString& k) { playground->removeContext(k); });

    auto populateKeyValueTree = [](QTreeWidget* tree, const QVariantMap& map) {
        QSignalBlocker blocker(tree);
        tree->clear();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            auto* item = new QTreeWidgetItem(tree);
            item->setFlags(item->flags() | Qt::ItemIsEditable);
            item->setText(0, it.key());
            item->setText(1, it.value().toString());
            item->setData(0, Qt::UserRole, it.key());
        }
    };
    populateKeyValueTree(ui.tagsTable, playground->tags());
    populateKeyValueTree(ui.contextsTable, playground->contexts());

    auto refreshAttachments = [this, playground]() {
        QSignalBlocker blocker(ui.attachmentTable);
        ui.attachmentTable->clear();
        for (const QString& path : playground->attachments()) {
            QFileInfo info(path);
            auto* item = new QTreeWidgetItem(ui.attachmentTable);
            item->setText(0, info.fileName());
            item->setToolTip(0, path);
            item->setData(0, Qt::UserRole, path);
            item->setText(1, QLocale::system().formattedDataSize(info.size()));
            item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        }
    };
    refreshAttachments();
    QObject::connect(playground, &SentryPlayground::attachmentsChanged, this,
        [refreshAttachments]() { refreshAttachments(); });

    auto addAttachmentRow = [this, playground]() {
        QString lastDir = QSettings().value("attachmentDir",
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
        QString path = QFileDialog::getOpenFileName(this, "Select attachment", lastDir);
        if (path.isEmpty())
            return;
        QSettings().setValue("attachmentDir", QFileInfo(path).absolutePath());
        playground->addAttachment(path);
    };

    auto addEmptyRow = [](QTreeWidget* tree) {
        auto* item = new QTreeWidgetItem(tree);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        tree->editItem(item, 0);
    };

    auto updateAddButton = [this]() {
        bool isAttachments = ui.categoryStack->currentIndex() == 2;
        ui.addButton->setToolTip(isAttachments ? "Add attachment…" : "Add row");
    };
    updateAddButton();
    QObject::connect(ui.categoryStack, &QStackedWidget::currentChanged, this,
        [updateAddButton](int) { updateAddButton(); });

    QObject::connect(ui.addButton, &QAbstractButton::clicked, this,
        [this, addAttachmentRow, addEmptyRow]() {
            switch (ui.categoryStack->currentIndex()) {
            case 0: addEmptyRow(ui.tagsTable); break;
            case 1: addEmptyRow(ui.contextsTable); break;
            case 2: addAttachmentRow(); break;
            }
        });

    QObject::connect(ui.attachmentTable, &QWidget::customContextMenuRequested, this,
        [this, playground](const QPoint& pos) {
            auto* item = ui.attachmentTable->itemAt(pos);
            if (!item)
                return;
            QMenu menu(this);
            QString path = item->data(0, Qt::UserRole).toString();
            menu.addAction("Remove", [playground, path]() { playground->removeAttachment(path); });
            menu.exec(ui.attachmentTable->viewport()->mapToGlobal(pos));
        });

    QObject::connect(ui.actionQuit, &QAction::triggered, qApp, &QCoreApplication::quit);
    QObject::connect(ui.actionWindow, &QAction::triggered, playground, &SentryPlayground::showWindow);

    QObject::connect(ui.workerBox, &QAbstractButton::toggled, playground, &SentryPlayground::setWorker);
    QObject::connect(playground, &SentryPlayground::workerChanged, ui.workerBox, &QAbstractButton::setChecked);

    auto* consentButton = new QPushButton(this);
    consentButton->setCheckable(true);
    consentButton->setFlat(true);
    auto* consentIcon = new QLabel(consentButton);
    auto* consentText = new QLabel(consentButton);
    auto* consentLayout = new QHBoxLayout(consentButton);
    consentLayout->setContentsMargins(8, 0, 8, 0);
    consentLayout->addWidget(consentIcon);
    consentLayout->addWidget(consentText);
    consentLayout->addStretch();
    consentIcon->setAttribute(Qt::WA_TransparentForMouseEvents);
    consentText->setAttribute(Qt::WA_TransparentForMouseEvents);
    statusBar()->addPermanentWidget(consentButton, 1);
    statusBar()->setSizeGripEnabled(false);
    QObject::connect(consentButton, &QAbstractButton::clicked, playground, [playground]() {
        switch (playground->consent()) {
        case Qt::PartiallyChecked: playground->setConsent(Qt::Checked); break;
        case Qt::Checked: playground->setConsent(Qt::Unchecked); break;
        case Qt::Unchecked: playground->setConsent(Qt::PartiallyChecked); break;
        }
    });

    auto updateConsentStatus = [this, consentButton, consentIcon, consentText](Qt::CheckState state) {
        static const char* kStyle =
            "QStatusBar { background-color: %1; }"
            "QStatusBar QPushButton { border: none; background: transparent; }"
            "QStatusBar QLabel { color: white; font-weight: bold; background: transparent; }"
            "QStatusBar QLabel#consentIcon { font-size: 18px; }";
        switch (state) {
        case Qt::Checked:
            statusBar()->setStyleSheet(QString(kStyle).arg("#2ecc71"));
            consentIcon->setText("✓");
            consentText->setText("Consent given — events will be captured and sent to Sentry");
            consentButton->setChecked(true);
            break;
        case Qt::Unchecked:
            statusBar()->setStyleSheet(QString(kStyle).arg("#e74c3c"));
            consentIcon->setText("⚠");
            consentText->setText("Consent revoked — events will be discarded and not sent to Sentry");
            consentButton->setChecked(false);
            break;
        case Qt::PartiallyChecked:
            statusBar()->setStyleSheet(QString(kStyle).arg("#f39c12"));
            consentIcon->setText("⚠");
            consentText->setText("Consent unknown — events will be discarded until consent is given");
            consentButton->setChecked(false);
            break;
        }
    };
    consentIcon->setObjectName("consentIcon");
    updateConsentStatus(playground->consent());
    QObject::connect(playground, &SentryPlayground::consentChanged, this, updateConsentStatus);
}
