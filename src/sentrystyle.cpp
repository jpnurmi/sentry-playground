#include "sentrystyle.h"

#include <QtWidgets/qabstractspinbox.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qstyleoption.h>

#ifdef Q_OS_MACOS
static bool isComboBoxEditor(const QWidget* widget)
{
    for (QObject* parent = widget->parent(); parent; parent = parent->parent()) {
        if (qobject_cast<QComboBox*>(parent))
            return true;
        if (qobject_cast<QAbstractSpinBox*>(parent))
            return false;
    }
    return false;
}

static bool isEmbeddedEditor(const QWidget* widget)
{
    for (QObject* parent = widget->parent(); parent; parent = parent->parent()) {
        if (qobject_cast<QComboBox*>(parent) || qobject_cast<QAbstractSpinBox*>(parent))
            return true;
    }
    return false;
}

static bool isInputWidget(const QWidget* widget)
{
    if (qobject_cast<const QAbstractSpinBox*>(widget) || qobject_cast<const QComboBox*>(widget))
        return true;
    if (qobject_cast<const QLineEdit*>(widget))
        return !isEmbeddedEditor(widget);
    return false;
}
#endif

SentryStyle::SentryStyle(QStyle* style)
    : QProxyStyle(style)
{
}

#ifdef Q_OS_MACOS
int SentryStyle::inputHeight(const QWidget* widget) const
{
    if (qobject_cast<const QAbstractSpinBox*>(widget))
        return nativeSpinBoxHeight(nullptr, widget);
    return nativeComboBoxHeight(nullptr, widget);
}

int SentryStyle::nativeComboBoxHeight(const QStyleOption* option, const QWidget* widget) const
{
    QStyleOptionComboBox comboOption;
    if (widget) {
        comboOption.initFrom(widget);
    } else if (option) {
        comboOption.direction = option->direction;
        comboOption.fontMetrics = option->fontMetrics;
        comboOption.palette = option->palette;
        comboOption.rect = option->rect;
        comboOption.state = option->state;
    }

    if (const auto* comboBox = qobject_cast<const QComboBox*>(widget))
        comboOption.editable = comboBox->isEditable();
    else
        comboOption.editable = true;

    const QSize comboSize = QProxyStyle::sizeFromContents(CT_ComboBox, &comboOption, QSize(), widget);
    comboOption.rect = QRect(QPoint(), comboSize);
    return QProxyStyle::subElementRect(SE_ComboBoxLayoutItem, &comboOption, widget).height();
}

int SentryStyle::nativeSpinBoxHeight(const QStyleOption* option, const QWidget* widget) const
{
    QStyleOptionSpinBox spinOption;
    if (widget) {
        spinOption.initFrom(widget);
    } else if (option) {
        spinOption.direction = option->direction;
        spinOption.fontMetrics = option->fontMetrics;
        spinOption.palette = option->palette;
        spinOption.rect = option->rect;
        spinOption.state = option->state;
    }

    const int frameWidth = QProxyStyle::pixelMetric(PM_SpinBoxFrameWidth, &spinOption, widget);
    return nativeComboBoxHeight(option, widget) + frameWidth * 2;
}
#endif

void SentryStyle::drawPrimitive(PrimitiveElement element, const QStyleOption* option,
    QPainter* painter, const QWidget* widget) const
{
#ifdef Q_OS_MACOS
    if ((element == PE_PanelLineEdit || element == PE_FrameLineEdit)
        && qobject_cast<const QLineEdit*>(widget) && isComboBoxEditor(widget)) {
        return;
    }
#endif
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void SentryStyle::polish(QWidget* widget)
{
    QProxyStyle::polish(widget);
#ifdef Q_OS_MACOS
    if (isInputWidget(widget)) {
        const int height = inputHeight(widget);
        widget->setMinimumHeight(height);
        widget->setMaximumHeight(height);
    }
#endif
}

QSize SentryStyle::sizeFromContents(ContentsType type, const QStyleOption* option,
    const QSize& contentsSize, const QWidget* widget) const
{
    QSize size = QProxyStyle::sizeFromContents(type, option, contentsSize, widget);
#ifdef Q_OS_MACOS
    switch (type) {
    case CT_LineEdit:
    case CT_ComboBox:
        size.setHeight(nativeComboBoxHeight(option, widget));
        break;
    case CT_SpinBox:
        size.setHeight(nativeSpinBoxHeight(option, widget));
        break;
    default:
        break;
    }
#endif
    return size;
}
