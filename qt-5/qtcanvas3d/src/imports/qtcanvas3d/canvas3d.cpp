/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "canvas3d_p.h"
#include "context3d_p.h"
#include "canvas3dcommon_p.h"
#include "canvasrendernode_p.h"
#include "teximage3d_p.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlContext>
#include <QtCore/QElapsedTimer>

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(canvas3dinfo, "qt.canvas3d.info")
Q_LOGGING_CATEGORY(canvas3drendering, "qt.canvas3d.rendering")
Q_LOGGING_CATEGORY(canvas3dglerrors, "qt.canvas3d.glerrors")

/*!
 * \qmltype Canvas3D
 * \since QtCanvas3D 1.0
 * \inqmlmodule QtCanvas3D
 * \brief Canvas that provides a 3D rendering context.
 *
 * The Canvas3D is a QML element that, when placed in your Qt Quick 2 scene, allows you to
 * get a 3D rendering context and call 3D rendering API calls through that context object.
 * Use of the rendering API requires knowledge of OpenGL like rendering APIs.
 *
 * There are two functions that are called by the Canvas3D implementation:
 * \list
 * \li initializeGL is emitted before the first frame is rendered, and usually during that you get
 * the 3D context and initialize resources to be used later on during the rendering cycle.
 * \li paintGL is emitted for each frame to be rendered, and usually during that you
 * submit 3D rendering calls to draw whatever 3D content you want to be displayed.
 * \endlist
 *
 * \sa Context3D
 */

/*!
 * \internal
 */
Canvas::Canvas(QQuickItem *parent):
    QQuickItem(parent),
    m_isNeedRenderQueued(false),
    m_renderNodeReady(false),
    m_mainThread(QThread::currentThread()),
    m_contextThread(0),
    m_context3D(0),
    m_isFirstRender(true),
    m_fboSize(0, 0),
    m_initializedSize(1, 1),
    m_maxSize(0, 0),
    m_glContext(0),
    m_glContextQt(0),
    m_glContextShare(0),
    m_contextWindow(0),
    m_fps(0),
    m_frameTimeMs(0),
    m_maxSamples(0),
    m_devicePixelRatio(1.0f),
    m_isOpenGLES2(false),
    m_isSoftwareRendered(false),
    m_isContextAttribsSet(false),
    m_resizeGLQueued(false),
    m_antialiasFbo(0),
    m_renderFbo(0),
    m_displayFbo(0),
    m_oldDisplayFbo(0),
    m_offscreenSurface(0)
{
    connect(this, &QQuickItem::windowChanged, this, &Canvas::handleWindowChanged);
    connect(this, &Canvas::needRender, this, &Canvas::renderNext, Qt::QueuedConnection);
    connect(this, &QQuickItem::widthChanged, this, &Canvas::queueResizeGL, Qt::DirectConnection);
    connect(this, &QQuickItem::heightChanged, this, &Canvas::queueResizeGL, Qt::DirectConnection);
    connect(this, &QQuickItem::widthChanged, this, &Canvas::widthChanged, Qt::DirectConnection);
    connect(this, &QQuickItem::heightChanged, this, &Canvas::heightChanged, Qt::DirectConnection);
    setAntialiasing(false);

    // Set contents to false in case we are in qml designer to make component look nice
    m_runningInDesigner = QGuiApplication::applicationDisplayName() == "Qml2Puppet";
    setFlag(ItemHasContents, !m_runningInDesigner);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
    if (QCoreApplication::testAttribute(Qt::AA_UseSoftwareOpenGL))
        m_isSoftwareRendered = true;
#endif
}

/*!
 * \qmlsignal void Canvas::initializeGL()
 * Emitted once when Canvas3D is ready and OpenGL state initialization can be done by the client.
 */

/*!
 * \qmlsignal void Canvas::paintGL()
 * Emitted each time a new frame should be drawn to Canvas3D. Driven by the QML scenegraph loop.
 */

/*!
 * \internal
 */
Canvas::~Canvas()
{
    shutDown();
}

/*!
 * \internal
 */
void Canvas::shutDown()
{
    if (!m_glContext)
        return;

    disconnect(m_contextWindow, 0, this, 0);
    disconnect(this, 0, this, 0);

    m_glContext->makeCurrent(m_offscreenSurface);
    delete m_renderFbo;
    delete m_displayFbo;
    delete m_antialiasFbo;
    delete m_context3D;
    m_glContext->doneCurrent();

    qCDebug(canvas3drendering).nospace() << m_contextThread << m_mainThread;

    if (m_contextThread && m_contextThread != m_mainThread) {
        m_glContext->deleteLater();
        m_offscreenSurface->deleteLater();
    } else {
        delete m_glContext;
        delete m_offscreenSurface;
    }
    m_glContext = 0;
    m_glContextQt = 0;
    m_glContextShare->deleteLater();
    m_glContextShare = 0;
}

/*!
 * \internal
 *
 * Override QQuickItem's setWidth to be able to limit the maximum canvas size to maximum viewport
 * dimensions.
 */
void Canvas::setWidth(int width)
{
    int newWidth = width;
    int maxWidth = m_maxSize.width();
    if (maxWidth && width > maxWidth) {
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << "():"
                                             << "Maximum width exceeded. Limiting to "
                                             << maxWidth;
        newWidth = maxWidth;
    }

    QQuickItem::setWidth(qreal(newWidth));
}

/*!
 * \internal
 */
int Canvas::width()
{
    return int(QQuickItem::width());
}

/*!
 * \internal
 *
 * Override QQuickItem's setHeight to be able to limit the maximum canvas size to maximum viewport
 * dimensions.
 */
void Canvas::setHeight(int height)
{
    int newHeight = height;
    int maxHeight = m_maxSize.height();
    if (maxHeight && height > maxHeight) {
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << "():"
                                             << "Maximum height exceeded. Limiting to "
                                             << maxHeight;
        newHeight = maxHeight;
    }

    QQuickItem::setHeight(qreal(newHeight));
}

/*!
 * \internal
 */
int Canvas::height()
{
    return int(QQuickItem::height());
}

/*!
 * \qmlproperty float Canvas3D::devicePixelRatio
 * Specifies the ratio between logical pixels (used by the Qt Quick) and actual physical
 * on-screen pixels (used by the 3D rendering).
 */
float Canvas::devicePixelRatio()
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "()";
    QQuickWindow *win = window();
    if (win)
        return win->devicePixelRatio();
    else
        return 1.0f;
}


/*!
 * \qmlmethod Context3D Canvas3D::getContext(string type)
 * Returns the 3D rendering context that allows 3D rendering calls to be made.
 * The \a type parameter is ignored for now, but a string is expected to be given.
 */
/*!
 * \internal
 */
QJSValue Canvas::getContext(const QString &type)
{
    QVariantMap map;
    return getContext(type, map);
}

/*!
 * \qmlmethod Context3D Canvas3D::getContext(string type, Canvas3DContextAttributes options)
 * Returns the 3D rendering context that allows 3D rendering calls to be made.
 * The \a type parameter is ignored for now, but a string is expected to be given.
 * The \a options parameter is only parsed when the first call to getContext() is
 * made and is ignored in subsequent calls if given. If the first call is made without
 * giving the \a options parameter, then the context and render target is initialized with
 * default configuration.
 *
 * \sa Canvas3DContextAttributes, Context3D
 */
/*!
 * \internal
 */
QJSValue Canvas::getContext(const QString &type, const QVariantMap &options)
{
    Q_UNUSED(type);

    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                         << "(type:" << type
                                         << ", options:" << options
                                         << ")";

    if (!m_isContextAttribsSet) {
        // Accept passed attributes only from first call and ignore for subsequent calls
        m_isContextAttribsSet = true;
        m_contextAttribs.setFrom(options);
        qCDebug(canvas3drendering).nospace()  << "Canvas3D::" << __FUNCTION__
                                              << " Context attribs:" << m_contextAttribs;

        // If we can't do antialiasing, ensure we don't even try to enable it
        if (m_maxSamples == 0 || m_isSoftwareRendered)
            m_contextAttribs.setAntialias(false);

        // Reflect the fact that creation of stencil attachment
        // causes the creation of depth attachment as well
        if (m_contextAttribs.stencil())
            m_contextAttribs.setDepth(true);

        // Ensure ignored attributes are left to their default state
        m_contextAttribs.setPremultipliedAlpha(false);
        m_contextAttribs.setPreferLowPowerToHighPerformance(false);
        m_contextAttribs.setFailIfMajorPerformanceCaveat(false);
    }

    if (!m_context3D) {
        // Create the context using current context attributes
        updateWindowParameters();

        // Initialize the swap buffer chain
        if (m_contextAttribs.depth() && m_contextAttribs.stencil() && !m_contextAttribs.antialias())
            m_fboFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        else if (m_contextAttribs.depth() && !m_contextAttribs.antialias())
            m_fboFormat.setAttachment(QOpenGLFramebufferObject::Depth);
        else if (m_contextAttribs.stencil() && !m_contextAttribs.antialias())
            m_fboFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        else
            m_fboFormat.setAttachment(QOpenGLFramebufferObject::NoAttachment);

        if (m_contextAttribs.antialias()) {
            m_antialiasFboFormat.setSamples(m_maxSamples);

            if (m_antialiasFboFormat.samples() != m_maxSamples) {
                qCWarning(canvas3drendering).nospace() <<
                        "Canvas3D::" << __FUNCTION__ <<
                        " Failed to use " << m_maxSamples <<
                        " will use " << m_antialiasFboFormat.samples();
            }

            if (m_contextAttribs.depth() && m_contextAttribs.stencil())
                m_antialiasFboFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
            else if (m_contextAttribs.depth())
                m_antialiasFboFormat.setAttachment(QOpenGLFramebufferObject::Depth);
            else
                m_antialiasFboFormat.setAttachment(QOpenGLFramebufferObject::NoAttachment);
        }

        // Create the offscreen surface
        QSurfaceFormat surfaceFormat = m_glContextShare->format();

        if (m_isOpenGLES2) {
            // Some devices report wrong version, so force 2.0 on ES2
            surfaceFormat.setVersion(2, 0);
        } else {
            surfaceFormat.setSwapBehavior(QSurfaceFormat::SingleBuffer);
            surfaceFormat.setSwapInterval(0);
        }

        if (m_contextAttribs.alpha())
            surfaceFormat.setAlphaBufferSize(8);
        else
            surfaceFormat.setAlphaBufferSize(0);

        if (m_contextAttribs.depth())
            surfaceFormat.setDepthBufferSize(24);

        if (m_contextAttribs.stencil())
            surfaceFormat.setStencilBufferSize(8);
        else
            surfaceFormat.setStencilBufferSize(-1);

        if (m_contextAttribs.antialias())
            surfaceFormat.setSamples(m_antialiasFboFormat.samples());

        m_contextWindow = window();
        m_contextThread = QThread::currentThread();

        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << " Creating QOpenGLContext with surfaceFormat :"
                                             << surfaceFormat;
        m_glContext = new QOpenGLContext();
        m_glContext->setFormat(surfaceFormat);

        // Share with m_glContextShare which in turn shares with m_glContextQt.
        // In case of threaded rendering both of these live on the render
        // thread of the scenegraph. m_glContextQt may be current on that
        // thread at this point, which would fail the context creation with
        // some drivers. Hence the need for m_glContextShare.
        m_glContext->setShareContext(m_glContextShare);
        if (!m_glContext->create()) {
            qCWarning(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                                   << " Failed to create OpenGL context for FBO";
            return QJSValue(QJSValue::NullValue);
        }

        m_offscreenSurface = new QOffscreenSurface();
        m_offscreenSurface->setFormat(m_glContext->format());
        m_offscreenSurface->create();

        if (!m_glContext->makeCurrent(m_offscreenSurface)) {
            qCWarning(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                                   << " Failed to make offscreen surface current";
            return QJSValue(QJSValue::NullValue);
        }

        // Initialize OpenGL functions using the created GL context
        initializeOpenGLFunctions();

        // Get the maximum drawable size
        GLint viewportDims[2];
        glGetIntegerv(GL_MAX_VIEWPORT_DIMS, viewportDims);
        m_maxSize.setWidth(viewportDims[0]);
        m_maxSize.setHeight(viewportDims[1]);

        // Set the size and create FBOs
        setPixelSize(m_initializedSize);
        m_displayFbo->bind();
        glViewport(0, 0,
                   m_fboSize.width(),
                   m_fboSize.height());
        glScissor(0, 0,
                  m_fboSize.width(),
                  m_fboSize.height());
        m_renderFbo->bind();
        glViewport(0, 0,
                   m_fboSize.width(),
                   m_fboSize.height());
        glScissor(0, 0,
                  m_fboSize.width(),
                  m_fboSize.height());

#if !defined(QT_OPENGL_ES_2)
        if (!m_isOpenGLES2) {
            // Make it possible to change point primitive size and use textures with them in
            // the shaders. These are implicitly enabled in ES2.
            glEnable(GL_PROGRAM_POINT_SIZE);
            glEnable(GL_POINT_SPRITE);
        }
#endif

        // Verify that width and height are not initially too large, in case width and height
        // were set before getting GL_MAX_VIEWPORT_DIMS
        if (width() > m_maxSize.width()) {
            qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                                 << "():"
                                                 << "Maximum width exceeded. Limiting to "
                                                 << m_maxSize.width();
            QQuickItem::setWidth(m_maxSize.width());
        }
        if (height() > m_maxSize.height()) {
            qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                                 << "():"
                                                 << "Maximum height exceeded. Limiting to "
                                                 << m_maxSize.height();
            QQuickItem::setHeight(m_maxSize.height());
        }

        // Create the Context3D
        m_context3D = new CanvasContext(m_glContext, m_offscreenSurface,
                                        QQmlEngine::contextForObject(this)->engine(),
                                        m_fboSize.width(),
                                        m_fboSize.height(),
                                        m_isOpenGLES2);
        m_context3D->setCanvas(this);
        m_context3D->setDevicePixelRatio(m_devicePixelRatio);
        m_context3D->setContextAttributes(m_contextAttribs);

        emit contextChanged(m_context3D);
    }

    glFlush();
    glFinish();

    return QQmlEngine::contextForObject(this)->engine()->newQObject(m_context3D);
}

/*!
 * \qmlproperty size Canvas3D::pixelSize
 * Specifies the size of the render target surface in physical on-screen pixels used by
 * the 3D rendering.
 */
/*!
 * \internal
 */
QSize Canvas::pixelSize()
{
    return m_fboSize;
}

/*!
 * \internal
 */
void Canvas::setPixelSize(QSize pixelSize)
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                         << "(pixelSize:" << pixelSize
                                         << ")";

    if (pixelSize.width() > m_maxSize.width()) {
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << "():"
                                             << "Maximum pixel width exceeded limiting to "
                                             << m_maxSize.width();
        pixelSize.setWidth(m_maxSize.width());
    }

    if (pixelSize.height() > m_maxSize.height()) {
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << "():"
                                             << "Maximum pixel height exceeded limiting to "
                                             << m_maxSize.height();
        pixelSize.setHeight(m_maxSize.height());
    }

    if (m_fboSize == pixelSize && m_renderFbo != 0)
        return;

    m_fboSize = pixelSize;
    createFBOs();

    // Queue the pixel size signal to next repaint cycle and queue repaint
    queueResizeGL();
    emitNeedRender();
}

/*!
 * \internal
 */
void Canvas::createFBOs()
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "()";

    if (!m_glContext) {
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << " No OpenGL context created, returning";
        return;
    }

    if (!m_offscreenSurface) {
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << " No offscreen surface created, returning";
        return;
    }

    // Ensure context is current
    m_glContext->makeCurrent(m_offscreenSurface);

    // Store current clear color and the bound texture
    GLint texBinding2D;
    GLfloat clearColor[4];
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texBinding2D);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);

    // Store existing display FBO, don't delete before next updatePaintNode call
    // Store existing render and antialias FBO's for a moment so we get new id's for new ones
    m_oldDisplayFbo = m_displayFbo;
    QOpenGLFramebufferObject *renderFbo = m_renderFbo;
    QOpenGLFramebufferObject *antialiasFbo = m_antialiasFbo;

    QOpenGLFramebufferObject *dummyFbo = 0;
    if (!m_renderFbo) {
        // Create a dummy FBO to work around a weird GPU driver bug on some platforms that
        // causes the first FBO created to get corrupted in some cases.
        dummyFbo = new QOpenGLFramebufferObject(m_fboSize.width(),
                                                m_fboSize.height(),
                                                m_fboFormat);
    }

    // Create FBOs
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                         << " Creating front and back FBO's with"
                                         << " attachment format:" << m_fboFormat.attachment()
                                         << " and size:" << m_fboSize;
    m_displayFbo = new QOpenGLFramebufferObject(m_fboSize.width(),
                                                m_fboSize.height(),
                                                m_fboFormat);
    m_renderFbo  = new QOpenGLFramebufferObject(m_fboSize.width(),
                                                m_fboSize.height(),
                                                m_fboFormat);

    // Clear the FBOs to prevent random junk appearing on the screen
    // Note: Viewport may not be changed automatically
    glClearColor(0,0,0,0);
    m_displayFbo->bind();
    glClear(GL_COLOR_BUFFER_BIT);
    m_renderFbo->bind();
    glClear(GL_COLOR_BUFFER_BIT);

    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                         << " Render FBO handle:" << m_renderFbo->handle()
                                         << " isValid:" << m_renderFbo->isValid();

    if (m_contextAttribs.antialias()) {
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << "Creating MSAA buffer with "
                                             << m_antialiasFboFormat.samples() << " samples "
                                             << " and attachment format of "
                                             << m_antialiasFboFormat.attachment();
        m_antialiasFbo = new QOpenGLFramebufferObject(
                    m_fboSize.width(),
                    m_fboSize.height(),
                    m_antialiasFboFormat);
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << " Antialias FBO handle:" << m_antialiasFbo->handle()
                                             << " isValid:" << m_antialiasFbo->isValid();
        m_antialiasFbo->bind();
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // FBO ids and texture id's have been generated, we can now free the existing ones.
    delete renderFbo;
    delete antialiasFbo;

    // Store the correct texture binding
    glBindTexture(GL_TEXTURE_2D, texBinding2D);
    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);

    if (m_context3D) {
        bindCurrentRenderTarget();
        emitNeedRender();
    }

    // Get rid of the dummy FBO, it has served its purpose
    delete dummyFbo;
}

/*!
 * \internal
 */
void Canvas::handleWindowChanged(QQuickWindow *window)
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "(" << window << ")";
    if (!window)
        return;

    emitNeedRender();
}

/*!
 * \internal
 */
void Canvas::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                         << "(newGeometry:" << newGeometry
                                         << ", oldGeometry" << oldGeometry
                                         << ")";
    QQuickItem::geometryChanged(newGeometry, oldGeometry);

    m_cachedGeometry = newGeometry;

    emitNeedRender();
}

/*!
 * \internal
 */
void Canvas::itemChange(ItemChange change, const ItemChangeData &value)
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                         << "(change:" << change
                                         << ")";
    QQuickItem::itemChange(change, value);

    emitNeedRender();
}

/*!
 * \internal
 */
CanvasContext *Canvas::context()
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "()";
    return m_context3D;
}

/*!
 * \internal
 */
void Canvas::updateWindowParameters()
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "()";

    // Update the device pixel ratio
    QQuickWindow *win = window();

    if (win) {
        qreal pixelRatio = win->devicePixelRatio();
        if (pixelRatio != m_devicePixelRatio) {
            m_devicePixelRatio = pixelRatio;
            emit devicePixelRatioChanged(pixelRatio);
            queueResizeGL();
            win->update();
        }
    }

    if (m_context3D) {
        if (m_context3D->devicePixelRatio() != m_devicePixelRatio)
            m_context3D->setDevicePixelRatio(m_devicePixelRatio);
    }
}

/*!
 * \internal
 *
 * Blits the antialias fbo into the final render fbo.
 * Returns the final render fbo handle.
 */
GLuint Canvas::resolveMSAAFbo()
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                         << " Resolving MSAA from FBO:"
                                         << m_antialiasFbo->handle()
                                         << " to FBO:" << m_renderFbo->handle();
    QOpenGLFramebufferObject::blitFramebuffer(m_renderFbo, m_antialiasFbo);

    return m_renderFbo->handle();
}

/*!
 * \internal
 */
void Canvas::ready()
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "()";

    connect(window(), &QQuickWindow::sceneGraphInvalidated,
            this, &Canvas::shutDown);

    update();
}

/*!
 * \internal
 */
QSGNode *Canvas::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                         << "("
                                         << oldNode <<", " << data
                                         << ")";
    updateWindowParameters();
    m_initializedSize = boundingRect().size().toSize();
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                         << " size:" << m_initializedSize
                                         << " devicePixelRatio:" << m_devicePixelRatio;
    if (m_runningInDesigner
            || m_initializedSize.width() < 0
            || m_initializedSize.height() < 0
            || !window()) {
        delete oldNode;
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << " Returns null";
        return 0;
    }

    CanvasRenderNode *node = static_cast<CanvasRenderNode *>(oldNode);

    if (!m_glContextQt) {
        m_glContextQt = window()->openglContext();
        m_isOpenGLES2 = m_glContextQt->isOpenGLES();

        QSurfaceFormat surfaceFormat = m_glContextQt->format();
        // Some devices report wrong version, so force 2.0 on ES2
        if (m_isOpenGLES2)
            surfaceFormat.setVersion(2, 0);

        if (!m_isOpenGLES2 || surfaceFormat.majorVersion() >= 3)
            m_maxSamples = 4;

        m_glContextShare = new QOpenGLContext;
        m_glContextShare->setFormat(surfaceFormat);
        m_glContextShare->setShareContext(m_glContextQt);
        QSurface *surface = m_glContextQt->surface();
        m_glContextQt->doneCurrent();
        if (!m_glContextShare->create())
            qCWarning(canvas3drendering) << "Failed to create share context";
        m_glContextQt->makeCurrent(surface);
        ready();
        return 0;
    }

    if (!node) {
        node = new CanvasRenderNode(this, window());

        /* Set up connections to get the production of FBO textures in sync with vsync on the
         * main thread.
         *
         * When a new texture is ready on the rendering thread, we use a direct connection to
         * the texture node to let it know a new texture can be used. The node will then
         * emit pendingNewTexture which we bind to QQuickWindow::update to schedule a redraw.
         *
         * When the scene graph starts rendering the next frame, the prepareNode() function
         * is used to update the node with the new texture. Once it completes, it emits
         * textureInUse() which we connect to the FBO rendering thread's renderNext() to have
         * it start producing content into its current "back buffer".
         *
         * This FBO rendering pipeline is throttled by vsync on the scene graph rendering thread.
         */
        connect(this, &Canvas::textureReady,
                node, &CanvasRenderNode::newTexture,
                Qt::DirectConnection);

        connect(node, &CanvasRenderNode::pendingNewTexture,
                window(), &QQuickWindow::update,
                Qt::QueuedConnection);

        connect(window(), &QQuickWindow::beforeRendering,
                node, &CanvasRenderNode::prepareNode,
                Qt::DirectConnection);

        connect(node, &CanvasRenderNode::textureInUse,
                this, &Canvas::emitNeedRender,
                Qt::QueuedConnection);

        // Get the production of FBO textures started..
        emitNeedRender();

        update();
    }

    node->setRect(boundingRect());
    emitNeedRender();

    m_renderNodeReady = true;

    return node;
}

/*!
 * \internal
 */
void Canvas::bindCurrentRenderTarget()
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "()";

    if (m_context3D->currentFramebuffer() == 0) {
        // Bind default framebuffer
        if (m_antialiasFbo) {
            qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                                 << " Binding current FBO to antialias FBO:"
                                                 << m_antialiasFbo->handle();
            m_antialiasFbo->bind();
        } else {
            qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                                 << " Binding current FBO to render FBO:"
                                                 << m_renderFbo->handle();
            m_renderFbo->bind();
        }
    } else {
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << " Binding current FBO to current Context3D FBO:"
                                             << m_context3D->currentFramebuffer();
        glBindFramebuffer(GL_FRAMEBUFFER, m_context3D->currentFramebuffer());
    }
}

/*!
 * \qmlproperty int Canvas3D::fps
 * This property specifies the current frames per seconds, the value is calculated every
 * 500 ms.
 * \sa frameTimeMs
 */
uint Canvas::fps()
{
    return m_fps;
}

/*!
 * \qmlmethod int Canvas3D::frameTimeMs()
 * This method returns the number of milliseconds the last rendered frame took to process. Before
 * any frames have been rendered this method returns 0. This time is measured from the point
 * the paintGL() signal was sent to the time glFinish() returns. This excludes the time it
 * takes to present the frame on screen.
 * \sa fps
*/
int Canvas::frameTimeMs()
{
    return m_frameTimeMs;
}

/*!
 * \internal
 */
void Canvas::renderNext()
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "()";

    m_isNeedRenderQueued = false;

    updateWindowParameters();

    // Don't try to do anything before the render node has been created
    if (!m_renderNodeReady) {
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << " Render node not ready, returning";
        return;
    }

    if (!m_glContext) {
        // Call the initialize function from QML/JavaScript until it calls the getContext()
        // that in turn creates the buffers.
        // Allow the JavaScript code to call the getContext() to create the context object and FBOs
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << " Emit inigGL() signal";
        emit initializeGL();

        if (!m_isContextAttribsSet) {
            qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                                 << " Context attributes not set, returning";
            return;
        }

        if (!m_glContext) {
            qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                                 << " QOpenGLContext not created, returning";
            return;
        }
    }

    // We have a GL context, make it current
    m_glContext->makeCurrent(m_offscreenSurface);

    // Bind the correct render target FBO
    bindCurrentRenderTarget();

    // Signal changes in pixel size
    if (m_resizeGLQueued) {
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << " Emit resizeGL() signal";
        emit resizeGL(int(width()), int(height()), m_devicePixelRatio);
        m_resizeGLQueued = false;
    }

    // Ensure we have correct clip rect set in the context
    QRect viewport = m_context3D->glViewportRect();
    glViewport(viewport.x(), viewport.y(), viewport.width(), viewport.height());
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                         << " Viewport set to " << viewport;

    // Check that we're complete component before drawing
    if (!isComponentComplete()) {
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << " Component is not complete, skipping drawing";
        return;
    }

    // Check if any images are loaded and need to be notified while the correct
    // GL context is current.
    QQmlEngine *engine = QQmlEngine::contextForObject(this)->engine();
    CanvasTextureImageFactory::factory(engine)->notifyLoadedImages();

    static QElapsedTimer frameTimer;
    frameTimer.start();

    // Call render in QML JavaScript side
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                         << " Emit paintGL() signal";
    emit paintGL();

    if (m_contextAttribs.antialias())
        resolveMSAAFbo();

    // We need to flush the contents to the FBO before posting
    // the texture to the other thread, otherwise, we might
    // get unexpected results.
    glFlush();
    glFinish();

    m_frameTimeMs = frameTimer.elapsed();

    // Update frames per second reading after glFinish()
    static QElapsedTimer timer;
    static int frames;

    if (frames == 0) {
        timer.start();
    } else if (timer.elapsed() > 500) {
        qreal avgtime = timer.elapsed() / (qreal) frames;
        uint avgFps = uint(1000.0 / avgtime);
        if (avgFps != m_fps) {
            m_fps = avgFps;
            emit fpsChanged(avgFps);
        }

        timer.start();
        frames = 0;
    }
    ++frames;

    // Swap
    qSwap(m_renderFbo, m_displayFbo);
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                         << " Displaying texture:"
                                         << m_displayFbo->texture()
                                         << " from FBO:" << m_displayFbo->handle();

    if (m_contextAttribs.preserveDrawingBuffer()) {
        // Copy the content of display fbo to the render fbo
        GLint texBinding2D;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texBinding2D);

        m_displayFbo->bind();
        glBindTexture(GL_TEXTURE_2D, m_renderFbo->texture());

        glCopyTexImage2D(GL_TEXTURE_2D, 0, m_displayFbo->format().internalTextureFormat(),
                         0, 0, m_fboSize.width(), m_fboSize.height(), 0);

        glBindTexture(GL_TEXTURE_2D, texBinding2D);
    }

    // FBO ids and texture id's have been generated, we can now free the old display FBO
    delete m_oldDisplayFbo;
    m_oldDisplayFbo = 0;

    // Rebind default FBO
    QOpenGLFramebufferObject::bindDefault();

    // Notify the render node of new texture
    emit textureReady(m_displayFbo->texture(), m_fboSize, m_devicePixelRatio);
}

/*!
 * \internal
 */
void Canvas::queueResizeGL()
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "()";

    m_resizeGLQueued = true;
}

/*!
 * \internal
 */
void Canvas::emitNeedRender()
{
    qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__ << "()";

    if (m_isNeedRenderQueued) {
        qCDebug(canvas3drendering).nospace() << "Canvas3D::" << __FUNCTION__
                                             << " needRender already queued, returning";
        return;
    }

    m_isNeedRenderQueued = true;
    emit needRender();
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
