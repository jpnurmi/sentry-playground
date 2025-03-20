#include "qtopenglwindow.h"

#include <QtGui/qpainter.h>
#include <QtGui/qscreen.h>

static const char *vertexShaderSource = "attribute highp vec4 posAttr;\n"
                                        "attribute lowp vec4 colAttr;\n"
                                        "varying lowp vec4 col;\n"
                                        "uniform highp mat4 matrix;\n"
                                        "void main() {\n"
                                        "   col = colAttr;\n"
                                        "   gl_Position = matrix * posAttr;\n"
                                        "}\n";

static const char *fragmentShaderSource = "varying lowp vec4 col;\n"
                                          "void main() {\n"
                                          "   gl_FragColor = col;\n"
                                          "}\n";

QtOpenGLWindow::QtOpenGLWindow(QWindow *parent)
    : QWindow(parent)
{
    setSurfaceType(QWindow::OpenGLSurface);
    setTitle("QtOpenGL - Sentry Playground");
    setAnimating(true);
    resize(520, 360);
}

void QtOpenGLWindow::render(QPainter *painter)
{
    const qreal retinaScale = devicePixelRatio();
    glViewport(0, 0, width() * retinaScale, height() * retinaScale);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    m_program->bind();

    QMatrix4x4 matrix;
    matrix.perspective(60.0f, 4.0f / 3.0f, 0.1f, 100.0f);
    matrix.translate(0, 0, -2);
    matrix.rotate(100.0f * m_frame / screen()->refreshRate(), 0, 1, 0);

    m_program->setUniformValue(m_matrixUniform, matrix);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    m_program->release();

    ++m_frame;
}

void QtOpenGLWindow::initialize()
{
    static const GLfloat vertices_colors[] = { +0.0f, +0.707f, 1.0f, 0.0f, 0.0f,
                                              -0.5f, -0.500f, 0.0f, 1.0f, 0.0f,
                                              +0.5f, -0.500f, 0.0f, 0.0f, 1.0f };

    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(vertices_colors, sizeof(vertices_colors));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), nullptr);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat),
                          reinterpret_cast<void *>(2 * sizeof(GLfloat)));

    m_program = new QOpenGLShaderProgram(this);
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    m_program->bindAttributeLocation("posAttr", 0);
    m_program->bindAttributeLocation("colAttr", 1);
    m_program->link();
    m_program->bind();

    m_matrixUniform = m_program->uniformLocation("matrix");
    Q_ASSERT(m_matrixUniform != -1);
}

void QtOpenGLWindow::render()
{
    if (!m_device)
        m_device = new QOpenGLPaintDevice;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    m_device->setSize(size() * devicePixelRatio());
    m_device->setDevicePixelRatio(devicePixelRatio());

    QPainter painter(m_device);
    render(&painter);
}

void QtOpenGLWindow::renderLater()
{
    requestUpdate();
}

bool QtOpenGLWindow::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::UpdateRequest:
        renderNow();
        return true;
    default:
        return QWindow::event(event);
    }
}

void QtOpenGLWindow::exposeEvent(QExposeEvent *event)
{
    Q_UNUSED(event);

    if (isExposed())
        renderNow();
}

void QtOpenGLWindow::renderNow()
{
    if (!isExposed())
        return;

    bool needsInitialize = false;

    if (!m_context) {
        m_context = new QOpenGLContext(this);
        m_context->setFormat(requestedFormat());
        m_context->create();

        needsInitialize = true;
    }

    m_context->makeCurrent(this);

    if (needsInitialize) {
        initializeOpenGLFunctions();
        initialize();
    }

    render();

    m_context->swapBuffers(this);

    if (m_animating)
        renderLater();
}

void QtOpenGLWindow::setAnimating(bool animating)
{
    m_animating = animating;

    if (animating)
        renderLater();
}
