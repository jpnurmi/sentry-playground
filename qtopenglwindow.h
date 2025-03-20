#ifndef QTOPENGLWINDOW_H
#define QTOPENGLWINDOW_H

#include <QtGui/qopenglfunctions.h>
#include <QtGui/qwindow.h>
#include <QtOpenGL/qopenglbuffer.h>
#include <QtOpenGL/qopenglpaintdevice.h>
#include <QtOpenGL/qopenglshaderprogram.h>

class QtOpenGLWindow : public QWindow, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit QtOpenGLWindow(QWindow *parent = nullptr);

    virtual void render(QPainter *painter);
    virtual void render();

    virtual void initialize();

    void setAnimating(bool animating);

public slots:
    void renderLater();
    void renderNow();

protected:
    bool event(QEvent *event) override;

    void exposeEvent(QExposeEvent *event) override;

private:
    bool m_animating = false;
    GLint m_matrixUniform = 0;
    QOpenGLBuffer m_vbo;
    QOpenGLShaderProgram *m_program = nullptr;
    int m_frame = 0;

    QOpenGLContext *m_context = nullptr;
    QOpenGLPaintDevice *m_device = nullptr;
};

#endif // QTOPENGLWINDOW_H
