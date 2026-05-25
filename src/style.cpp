#include "style.h"

#include <QtCore/qmath.h>
#include <QtGui/qpainter.h>
#include <QtGui/qpixmap.h>
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

Style::Style(QStyle* style)
    : QProxyStyle(style)
{
}

QColor Style::blendedColor(QColor base, QColor overlay, int overlayAlpha)
{
    const qreal alpha = overlayAlpha / 255.0;
    return QColor(
        qRound(base.red() * (1 - alpha) + overlay.red() * alpha),
        qRound(base.green() * (1 - alpha) + overlay.green() * alpha),
        qRound(base.blue() * (1 - alpha) + overlay.blue() * alpha));
}

QString Style::cssRgb(const QColor& color)
{
    return QStringLiteral("rgb(%1, %2, %3)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue());
}

QString Style::cssRgba(QColor color, int alpha)
{
    color.setAlpha(alpha);
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha());
}

QString Style::circularButtonStyle(const QPalette& palette)
{
    const QColor window = palette.color(QPalette::Window);
    const QColor text = palette.color(QPalette::WindowText);
    return QStringLiteral(
        "QToolButton {"
        " border: none; border-radius: 11px;"
        " background: %1; padding: 0; }"
        "QToolButton:hover { background: %2; }"
        "QToolButton:pressed { background: %3; }")
        .arg(cssRgb(blendedColor(window, text, 34)),
             cssRgb(blendedColor(window, text, 52)),
             cssRgb(blendedColor(window, text, 68)));
}

QString Style::circularButtonStyle(const QPalette& palette, const QString& extra)
{
    const QColor window = palette.color(QPalette::Window);
    const QColor text = palette.color(QPalette::WindowText);
    return QStringLiteral(
        "QToolButton {"
        " border: none; border-radius: 11px;"
        " background: %1; padding: 0; %4 }"
        "QToolButton:hover { background: %2; }"
        "QToolButton:pressed { background: %3; }")
        .arg(cssRgb(blendedColor(window, text, 34)),
             cssRgb(blendedColor(window, text, 52)),
             cssRgb(blendedColor(window, text, 68)),
             extra);
}

QString Style::segmentedButtonStyle(const QPalette& palette, const QString& extra)
{
    const QColor window = palette.color(QPalette::Window);
    const QColor text = palette.color(QPalette::WindowText);
    return QStringLiteral(
        "QPushButton { color: %1; font-weight: bold; background: transparent;"
        " border: 1px solid %2; padding: 3px 12px; %3 }"
        "QPushButton:checked { background: %4; color: %5; }")
        .arg(cssRgb(blendedColor(window, text, 120)),
             cssRgb(blendedColor(window, text, 70)),
             extra,
             cssRgb(blendedColor(window, text, 44)),
             cssRgb(text));
}

QIcon Style::makeArrowIcon(const QPalette& palette, qreal dpr)
{
    const int size = 16;
    QPixmap pixmap(size * dpr, size * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(palette.color(QPalette::Text), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.translate(size / 2.0, size / 2.0);
    p.drawLine(QPointF(-5, 0), QPointF(5, 0));
    p.drawLine(QPointF(5, 0), QPointF(1, -4));
    p.drawLine(QPointF(5, 0), QPointF(1, 4));
    return QIcon(pixmap);
}

QIcon Style::makeBackIcon(const QPalette& palette, qreal dpr)
{
    const int size = 16;
    QPixmap pixmap(size * dpr, size * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(palette.color(QPalette::ButtonText), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(QPointF(4.5, 8), QPointF(11.5, 8));
    p.drawLine(QPointF(7.5, 4.5), QPointF(4.5, 8));
    p.drawLine(QPointF(4.5, 8), QPointF(7.5, 11.5));
    return QIcon(pixmap);
}

QIcon Style::makeChevronIcon(const QPalette& palette, qreal dpr, bool expanded)
{
    const int size = 12;
    QPixmap pixmap(size * dpr, size * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    QColor color = palette.color(QPalette::ButtonText);
    color.setAlpha(150);
    p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (expanded) {
        p.drawLine(QPointF(3.5, 4.5), QPointF(6, 7));
        p.drawLine(QPointF(6, 7), QPointF(8.5, 4.5));
    } else {
        p.drawLine(QPointF(4.5, 3.5), QPointF(7, 6));
        p.drawLine(QPointF(7, 6), QPointF(4.5, 8.5));
    }
    return QIcon(pixmap);
}

QIcon Style::makeClearIcon(const QPalette& palette, qreal dpr)
{
    const int size = 16;
    QColor color = palette.color(QPalette::ButtonText);
    color.setAlpha(180);
    QPixmap pixmap(size * dpr, size * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(5, 5), QPointF(11, 11));
    p.drawLine(QPointF(11, 5), QPointF(5, 11));
    return QIcon(pixmap);
}

QIcon Style::makeEllipsisIcon(const QPalette& palette, qreal dpr)
{
    const int size = 16;
    QColor color = palette.color(QPalette::ButtonText);
    color.setAlpha(180);
    QPixmap pixmap(size * dpr, size * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    for (int x : { 5, 8, 11 })
        p.drawEllipse(QPointF(x, size / 2.0), 1.2, 1.2);
    return QIcon(pixmap);
}

QIcon Style::makePlusIcon(const QPalette& palette, qreal dpr)
{
    const int size = 12;
    QPixmap pixmap(size * dpr, size * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(palette.color(QPalette::Text), 1.5, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(size / 2.0, 2), QPointF(size / 2.0, size - 2));
    p.drawLine(QPointF(2, size / 2.0), QPointF(size - 2, size / 2.0));
    return QIcon(pixmap);
}

#ifdef Q_OS_MACOS
int Style::inputHeight(const QWidget* widget) const
{
    if (qobject_cast<const QAbstractSpinBox*>(widget))
        return nativeSpinBoxHeight(nullptr, widget);
    return nativeComboBoxHeight(nullptr, widget);
}

int Style::nativeComboBoxHeight(const QStyleOption* option, const QWidget* widget) const
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

int Style::nativeSpinBoxHeight(const QStyleOption* option, const QWidget* widget) const
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

void Style::drawPrimitive(PrimitiveElement element, const QStyleOption* option,
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

void Style::polish(QWidget* widget)
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

QSize Style::sizeFromContents(ContentsType type, const QStyleOption* option,
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
