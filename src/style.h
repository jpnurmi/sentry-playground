#ifndef STYLE_H
#define STYLE_H

#include <QtCore/qstring.h>
#include <QtGui/qcolor.h>
#include <QtGui/qicon.h>
#include <QtGui/qpalette.h>
#include <QtWidgets/qproxystyle.h>

class Style : public QProxyStyle
{
public:
    explicit Style(QStyle* style = nullptr);

    static QColor blendedColor(QColor base, QColor overlay, int overlayAlpha);
    static QString cssRgb(const QColor& color);
    static QString cssRgba(QColor color, int alpha);
    static QString circularButtonStyle(const QPalette& palette);
    static QString circularButtonStyle(const QPalette& palette, const QString& extra);
    static QString segmentedButtonStyle(const QPalette& palette, const QString& extra);
    static QIcon makeArrowIcon(const QPalette& palette, qreal dpr);
    static QIcon makeBackIcon(const QPalette& palette, qreal dpr);
    static QIcon makeChevronIcon(const QPalette& palette, qreal dpr, bool expanded);
    static QIcon makeClearIcon(const QPalette& palette, qreal dpr);
    static QIcon makeEllipsisIcon(const QPalette& palette, qreal dpr);
    static QIcon makePlusIcon(const QPalette& palette, qreal dpr);

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
        QPainter* painter, const QWidget* widget = nullptr) const override;
    void polish(QWidget* widget) override;
    QSize sizeFromContents(ContentsType type, const QStyleOption* option,
        const QSize& contentsSize, const QWidget* widget) const override;

private:
#ifdef Q_OS_MACOS
    int inputHeight(const QWidget* widget) const;
    int nativeComboBoxHeight(const QStyleOption* option, const QWidget* widget) const;
    int nativeSpinBoxHeight(const QStyleOption* option, const QWidget* widget) const;
#endif
};

#endif // STYLE_H
