#ifndef SENTRYSTYLE_H
#define SENTRYSTYLE_H

#include <QtWidgets/qproxystyle.h>

class SentryStyle : public QProxyStyle
{
public:
    explicit SentryStyle(QStyle* style = nullptr);

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

#endif // SENTRYSTYLE_H
