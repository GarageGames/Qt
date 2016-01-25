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

#include "canvasglstatedump_p.h"
#include "activeinfo3d_p.h"
#include "canvas3d_p.h"
#include "context3d_p.h"
#include "texture3d_p.h"
#include "shader3d_p.h"
#include "program3d_p.h"
#include "buffer3d_p.h"
#include "framebuffer3d_p.h"
#include "renderbuffer3d_p.h"
#include "uniformlocation_p.h"
#include "teximage3d_p.h"
#include "arrayutils_p.h"
#include "shaderprecisionformat_p.h"
#include "enumtostringmap_p.h"
#include "canvas3dcommon_p.h"
#include "compressedtextures3tc_p.h"
#include "compressedtexturepvrtc_p.h"

#include <QtGui/QOpenGLShader>
#include <QtOpenGLExtensions/QOpenGLExtensions>
#include <QtQml/private/qv4arraybuffer_p.h>
#include <QtQml/private/qjsvalue_p.h>
#include <QtCore/private/qbytedata_p.h>

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

/*!
 * \qmltype Context3D
 * \since QtCanvas3D 1.0
 * \inqmlmodule QtCanvas3D
 * \brief Provides the 3D rendering API and context.
 *
 * An uncreatable QML type that provides a WebGL-like API that can be used to draw 3D graphics to
 * the Canvas3D element. You can get it by calling the \l{Canvas3D::getContext}{Canvas3D.getContext}
 * method.
 *
 * \sa Canvas3D
 */

CanvasContext::CanvasContext(QOpenGLContext *context, QSurface *surface, QQmlEngine *engine,
                             int width, int height, bool isES2, QObject *parent) :
    CanvasAbstractObject(parent),
    QOpenGLFunctions(context),
    m_engine(engine),
    m_v4engine(QQmlEnginePrivate::getV4Engine(engine)),
    m_unpackFlipYEnabled(false),
    m_unpackPremultiplyAlphaEnabled(false),
    m_glViewportRect(0, 0, width, height),
    m_devicePixelRatio(1.0),
    m_currentProgram(0),
    m_currentArrayBuffer(0),
    m_currentElementArrayBuffer(0),
    m_currentTexture2D(0),
    m_currentTextureCubeMap(0),
    m_currentFramebuffer(0),
    m_currentRenderbuffer(0),
    m_context(context),
    m_surface(surface),
    m_error(CANVAS_NO_ERRORS),
    m_map(EnumToStringMap::newInstance()),
    m_canvas(0),
    m_maxVertexAttribs(0),
    m_isOpenGLES2(isES2),
    m_stateDumpExt(0),
    m_standardDerivatives(0),
    m_compressedTextureS3TC(0),
    m_compressedTexturePVRTC(0)
{
    m_extensions = m_context->extensions();

    int value = 0;
    glGetIntegerv(MAX_VERTEX_ATTRIBS, &value);
    m_maxVertexAttribs = uint(value);

#ifndef QT_NO_DEBUG
    const GLubyte *version = glGetString(GL_VERSION);
    qCDebug(canvas3dinfo).nospace() << "Context3D::" << __FUNCTION__
                                    << "OpenGL version:" << (const char *)version;

    version = glGetString(GL_SHADING_LANGUAGE_VERSION);
    qCDebug(canvas3dinfo).nospace() << "Context3D::" << __FUNCTION__
                                    << "GLSL version:" << (const char *)version;

    qCDebug(canvas3dinfo).nospace() << "Context3D::" << __FUNCTION__
                                    << "EXTENSIONS: " << m_extensions;
#endif
}

/*!
 * \internal
 */
CanvasContext::~CanvasContext()
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__;
    EnumToStringMap::deleteInstance();
}

/*!
 * \internal
 */
void CanvasContext::setCanvas(Canvas *canvas)
{
    if (m_canvas != canvas) {
        if (m_canvas) {
            disconnect(m_canvas, &QQuickItem::widthChanged, this, 0);
            disconnect(m_canvas, &QQuickItem::heightChanged, this, 0);
        }

        m_canvas = canvas;
        emit canvasChanged(canvas);

        connect(m_canvas, &QQuickItem::widthChanged,
                this, &CanvasContext::drawingBufferWidthChanged);
        connect(m_canvas, &QQuickItem::heightChanged,
                this, &CanvasContext::drawingBufferHeightChanged);
    }
}

/*!
 * \qmlproperty Canvas3D Context3D::canvas
 * Holds the read only reference to the Canvas3D for this Context3D.
 */
Canvas *CanvasContext::canvas()
{
    return m_canvas;
}

/*!
 * \qmlproperty int Context3D::drawingBufferWidth
 * Holds the current read-only logical pixel width of the drawing buffer. To get the width in physical pixels
 * you need to multiply this with the \c devicePixelRatio.
 */
uint CanvasContext::drawingBufferWidth()
{
    uint width = 0;
    if (m_canvas)
        width = m_canvas->pixelSize().width() / m_devicePixelRatio;

    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(): " << width;
    return width;
}

/*!
 * \qmlproperty int Context3D::drawingBufferHeight
 * Holds the current read-only logical pixel height of the drawing buffer. To get the height in physical pixels
 * you need to multiply this with the \c devicePixelRatio.
 */
uint CanvasContext::drawingBufferHeight()
{
    uint height = 0;
    if (m_canvas)
        height = m_canvas->pixelSize().height() / m_devicePixelRatio;

    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(): " << height;
    return height;
}

/*!
 * \internal
 */
QString CanvasContext::glEnumToString(glEnums value) const
{
    return m_map->lookUp(value);
}

/*!
 * \internal
 */
void CanvasContext::logAllGLErrors(const QString &funcName)
{
    if (!canvas3dglerrors().isDebugEnabled())
        return;

    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        // Merge any GL errors with internal errors so that we don't lose them
        switch (err) {
        case GL_INVALID_ENUM:
            m_error |= CANVAS_INVALID_ENUM;
            break;
        case GL_INVALID_VALUE:
            m_error |= CANVAS_INVALID_VALUE;
            break;
        case GL_INVALID_OPERATION:
            m_error |= CANVAS_INVALID_OPERATION;
            break;
        case GL_OUT_OF_MEMORY:
            m_error |= CANVAS_OUT_OF_MEMORY;
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            m_error |= CANVAS_INVALID_FRAMEBUFFER_OPERATION;
            break;
        default:
            break;
        }

        qCWarning(canvas3dglerrors).nospace() << "Context3D::" << funcName
                                              << ": OpenGL ERROR: "
                                              << glEnumToString(CanvasContext::glEnums(err));
    }
}

/*!
 * \internal
 */
void CanvasContext::setContextAttributes(const CanvasContextAttributes &attribs)
{
    m_contextAttributes.setFrom(attribs);
}

/*!
 * \internal
 */
float CanvasContext::devicePixelRatio()
{
    return m_devicePixelRatio;
}

/*!
 * \internal
 */
void CanvasContext::setDevicePixelRatio(float ratio)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__ << "(" << ratio << ")";
    m_devicePixelRatio = ratio;
}

/*!
 * \internal
 */
QRect CanvasContext::glViewportRect() const
{
    return m_glViewportRect;
}

/*!
 * \internal
 */
GLuint CanvasContext::currentFramebuffer()
{
    if (!m_currentFramebuffer)
        return 0;

    return m_currentFramebuffer->id();
}

/*!
 * \qmlmethod Canvas3DShaderPrecisionFormat Context3D::getShaderPrecisionFormat(glEnums shadertype, glEnums precisiontype)
 * Return a new Canvas3DShaderPrecisionFormat describing the range and precision for the specified shader
 * numeric format.
 * \a shadertype Type of the shader, either \c Context3D.FRAGMENT_SHADER or
 * \c{Context3D.VERTEX_SHADER}.
 * \a precisiontype Can be \c{Context3D.LOW_FLOAT}, \c{Context3D.MEDIUM_FLOAT},
 * \c{Context3D.HIGH_FLOAT}, \c{Context3D.LOW_INT}, \c{Context3D.MEDIUM_INT} or
 * \c{Context3D.HIGH_INT}.
 *
 * \sa Canvas3DShaderPrecisionFormat
 */
/*!
 * \internal
 */
QJSValue CanvasContext::getShaderPrecisionFormat(glEnums shadertype,
                                                 glEnums precisiontype)
{
    QString str = QString(__FUNCTION__);
    str += QStringLiteral("(shaderType:")
            + glEnumToString(shadertype)
            + QStringLiteral(", precisionType:")
            + glEnumToString(precisiontype)
            + QStringLiteral(")");

    qCDebug(canvas3drendering).nospace() << "Context3D::" << str;

    GLint range[2];
    GLint precision;

    // Default values from OpenGL ES2 spec
    switch (precisiontype) {
    case LOW_INT:
    case MEDIUM_INT:
    case HIGH_INT:
        // 32-bit twos-complement integer format
        range[0] = 31;
        range[1] = 30;
        precision = 0;
        break;
    case LOW_FLOAT:
    case MEDIUM_FLOAT:
    case HIGH_FLOAT:
        // IEEE single-precision floating-point format
        range[0] = 127;
        range[1] = 127;
        precision = 23;
        break;
    default:
        range[0] = 1;
        range[1] = 1;
        precision = 1;
        m_error |= CANVAS_INVALID_ENUM;
        break;
    }

    // On desktop envs glGetShaderPrecisionFormat is part of OpenGL 4.x, so it is not necessarily
    // available. Let's just return the default values if not ES2.
    if (m_isOpenGLES2) {
        glGetShaderPrecisionFormat((GLenum)(shadertype), (GLenum)(precisiontype),
                                   range, &precision);
    }
    logAllGLErrors(str);

    CanvasShaderPrecisionFormat *format = new CanvasShaderPrecisionFormat();
    format->setPrecision(int(precision));
    format->setRangeMin(int(range[0]));
    format->setRangeMax(int(range[1]));
    return m_engine->newQObject(format);
}

/*!
 * \qmlmethod bool Context3D::isContextLost()
 * Always returns false.
 */
/*!
 * \internal
 */
bool CanvasContext::isContextLost()
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(): false";
    return false;
}

/*!
 * \qmlmethod Canvas3DContextAttributes Context3D::getContextAttributes()
 * Returns a copy of the actual context parameters that are used in the current context.
 */
/*!
 * \internal
 */
QJSValue CanvasContext::getContextAttributes()
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__ << "()";

    CanvasContextAttributes *attributes = new CanvasContextAttributes();
    attributes->setAlpha(m_contextAttributes.alpha());
    attributes->setDepth(m_contextAttributes.depth());
    attributes->setStencil(m_contextAttributes.stencil());
    attributes->setAntialias(m_contextAttributes.antialias());
    attributes->setPremultipliedAlpha(m_contextAttributes.premultipliedAlpha());
    attributes->setPreserveDrawingBuffer(m_contextAttributes.preserveDrawingBuffer());
    attributes->setPreferLowPowerToHighPerformance(
                m_contextAttributes.preferLowPowerToHighPerformance());
    attributes->setFailIfMajorPerformanceCaveat(
                m_contextAttributes.failIfMajorPerformanceCaveat());

    return m_engine->newQObject(attributes);
}

/*!
 * \qmlmethod void Context3D::flush()
 * Indicates to graphics driver that previously sent commands must complete within finite time.
 */
/*!
 * \internal
 */
void CanvasContext::flush()
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "()";
    glFlush();
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::finish()
 * Forces all previous 3D rendering commands to complete.
 */
/*!
 * \internal
 */
void CanvasContext::finish()
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "()";
    glFinish();
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod Canvas3DTexture Context3D::createTexture()
 * Create a Canvas3DTexture object and initialize a name for it as by calling \c{glGenTextures()}.
 */
/*!
 * \internal
 */
QJSValue CanvasContext::createTexture()
{
    CanvasTexture *texture = new CanvasTexture(this);
    QJSValue value = m_engine->newQObject(texture);
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "():" << value.toString();
    logAllGLErrors(__FUNCTION__);
    return value;
}

/*!
 * \qmlmethod void Context3D::deleteTexture(Canvas3DTexture texture3D)
 * Deletes the given texture as if by calling \c{glDeleteTextures()}.
 * Calling this method repeatedly on the same object has no side effects.
 * \a texture3D is the Canvas3DTexture to be deleted.
 */
/*!
 * \internal
 */
void CanvasContext::deleteTexture(QJSValue texture3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(texture:" << texture3D.toString()
                                         << ")";
    CanvasTexture *texture = getAsTexture3D(texture3D);
    if (texture) {
        if (!checkParent(texture, __FUNCTION__))
            return;
        texture->del();
        logAllGLErrors(__FUNCTION__);
    } else {
        m_error |= CANVAS_INVALID_VALUE;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID texture handle:"
                                               << texture3D.toString();
    }
}

/*!
 * \qmlmethod void Context3D::scissor(int x, int y, int width, int height)
 * Defines a rectangle that constrains the drawing.
 * \a x is theleft edge of the rectangle.
 * \a y is the bottom edge of the rectangle.
 * \a width is the width of the rectangle.
 * \a height is the height of the rectangle.
 */
/*!
 * \internal
 */
void CanvasContext::scissor(int x, int y, int width, int height)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(x:" << x
                                         << ", y:" << y
                                         << ", width:" << width
                                         << ", height:" << height
                                         << ")";

    glScissor(x, y, width, height);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::activeTexture(glEnums texture)
 * Sets the given texture unit as active. The number of texture units is implementation dependent,
 * but must be at least 8. Initially \c Context3D.TEXTURE0 is active.
 * \a texture must be one of \c Context3D.TEXTUREi values where \c i ranges from \c 0 to
 * \c{(Context3D.MAX_COMBINED_TEXTURE_IMAGE_UNITS-1)}.
 */
/*!
 * \internal
 */
void CanvasContext::activeTexture(glEnums texture)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(texture:" << glEnumToString(texture)
                                         << ")";
    glActiveTexture(GLenum(texture));
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::bindTexture(glEnums target, Canvas3DTexture texture3D)
 * Bind a Canvas3DTexture to a texturing target.
 * \a target is the target of the active texture unit to which the Canvas3DTexture will be bound.
 * Must be either \c{Context3D.TEXTURE_2D} or \c{Context3D.TEXTURE_CUBE_MAP}.
 * \a texture3D is the Canvas3DTexture to be bound.
 */
/*!
 * \internal
 */
void CanvasContext::bindTexture(glEnums target, QJSValue texture3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", texture:" << texture3D.toString()
                                         << ")";

    CanvasTexture *texture = getAsTexture3D(texture3D);
    if (target == TEXTURE_2D)
        m_currentTexture2D = texture;
    else if (target == TEXTURE_CUBE_MAP)
        m_currentTextureCubeMap = texture;

    if (texture && checkParent(texture, __FUNCTION__)) {
        if (!texture->isAlive()) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ": Trying to bind deleted texture object";
            return;
        }

        if (target == TEXTURE_2D)
            m_currentTexture2D->bind(target);
        else if (target == TEXTURE_CUBE_MAP)
            m_currentTextureCubeMap->bind(target);
    } else {
        glBindTexture(GLenum(target), 0);
    }
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \internal
 */
bool CanvasContext::isValidTextureBound(glEnums target, const QString &funcName)
{
    if (target == TEXTURE_2D) {
        if (!m_currentTexture2D) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << funcName
                                                   << ":INVALID_OPERATION:"
                                                   << "No current TEXTURE_2D bound";
            m_error |= CANVAS_INVALID_OPERATION;
            return false;
        } else if (!m_currentTexture2D->isAlive()) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << funcName
                                                   << ":INVALID_OPERATION:"
                                                   << "Currently bound TEXTURE_2D is deleted";
            m_error |= CANVAS_INVALID_OPERATION;
            return false;
        }
    } else if (target == TEXTURE_CUBE_MAP) {
        if (!m_currentTextureCubeMap) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << funcName
                                                   << ":INVALID_OPERATION:"
                                                   << "No current TEXTURE_CUBE_MAP bound";
            m_error |= CANVAS_INVALID_OPERATION;
            return false;
        } else if (!m_currentTextureCubeMap->isAlive()) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << funcName
                                                   << ":INVALID_OPERATION:"
                                                   << "Currently bound TEXTURE_CUBE_MAP is deleted";
            m_error |= CANVAS_INVALID_OPERATION;
            return false;
        }
    }

    return true;
}

bool CanvasContext::checkParent(QObject *obj, const char *function)
{
    if (obj && obj->parent() != this) {
        m_error |= CANVAS_INVALID_OPERATION;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << function
                                               << ":INVALID_OPERATION:"
                                               << "Object from wrong context";
        return false;
    }
    return true;
}

/*!
 * \internal
 *
 * Transposes matrices. \a dim is the dimensions of the square matrices in \a src.
 * A newly allocated array containing transposed matrices is returned. \a count specifies how many
 * matrices are handled.
 * Required for uniformMatrix*fv functions in ES2.
 */
float *CanvasContext::transposeMatrix(int dim, int count, float *src)
{
    float *dest = new float[dim * dim * count];

    for (int k = 0; k < count; k++) {
        const int offset = k * dim * dim;
        for (int i = 0; i < dim; i++) {
            for (int j = 0; j < dim; j++)
                dest[offset + (i * dim) + j] = src[offset + (j * dim) + i];
        }
    }

    return dest;
}

/*!
 * \internal
 *
 * Set matrix uniform values.
 */
void CanvasContext::uniformMatrixNfv(int dim, const QJSValue &location3D, bool transpose,
                                     const QJSValue &array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(dim:" << dim
                                         << ", uniformLocation:" << location3D.toString()
                                         << ", transpose:" << transpose
                                         << ", array:" << array.toString()
                                         <<")";

    if (!isOfType(location3D, "QtCanvas3D::CanvasUniformLocation")) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    CanvasUniformLocation *locationObj =
            static_cast<CanvasUniformLocation *>(location3D.toQObject());

    if (!checkParent(locationObj, __FUNCTION__))
        return;

    // Check if we have a JavaScript array
    if (array.isArray()) {
        uniformMatrixNfva(dim, locationObj, transpose, array.toVariant().toList());
        return;
    }

    int arrayLen = 0;
    float *uniformData = reinterpret_cast<float * >(
                getTypedArrayAsRawDataPtr(array, arrayLen, QV4::Heap::TypedArray::Float32Array));

    if (!m_currentProgram || !uniformData || !locationObj)
        return;

    int uniformLocation = locationObj->id();
    int numMatrices = arrayLen / (dim * dim * 4);

    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "numMatrices:" << numMatrices;

    float *transposedMatrix = 0;
    if (m_isOpenGLES2 && transpose) {
        transpose = false;
        transposedMatrix = transposeMatrix(dim, numMatrices, uniformData);
        uniformData = transposedMatrix;
    }

    switch (dim) {
    case 2:
        glUniformMatrix2fv(uniformLocation, numMatrices, transpose, uniformData);
        break;
    case 3:
        glUniformMatrix3fv(uniformLocation, numMatrices, transpose, uniformData);
        break;
    case 4:
        glUniformMatrix4fv(uniformLocation, numMatrices, transpose, uniformData);
        break;
    default:
        qWarning() << "Warning: Unsupported dim specified in" << __FUNCTION__;
        break;
    }

    logAllGLErrors(__FUNCTION__);

    delete[] transposedMatrix;
}

/*!
 * \internal
 *
 * Set matrix uniform values from JS array.
 */
void CanvasContext::uniformMatrixNfva(int dim, CanvasUniformLocation *uniformLocation,
                                     bool transpose, const QVariantList &array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(dim:" << dim
                                         << ", location3D:" << uniformLocation
                                         << ", transpose:" << transpose
                                         << ", array:" << array
                                         << ")";

    if (!m_currentProgram || !uniformLocation)
        return;

    int location3D = uniformLocation->id();
    int size = array.count();
    float *dataArray = new float[size];
    float *arrayPtr = dataArray;
    int numMatrices = size / (dim * dim);

    ArrayUtils::fillFloatArrayFromVariantList(array, arrayPtr);

    float *transposedMatrix = 0;
    if (m_isOpenGLES2 && transpose) {
        transpose = false;
        transposedMatrix = transposeMatrix(dim, numMatrices, arrayPtr);
        arrayPtr = transposedMatrix;
    }

    switch (dim) {
    case 2:
        glUniformMatrix2fv(location3D, numMatrices, transpose, arrayPtr);
        break;
    case 3:
        glUniformMatrix3fv(location3D, numMatrices, transpose, arrayPtr);
        break;
    case 4:
        glUniformMatrix4fv(location3D, numMatrices, transpose, arrayPtr);
        break;
    default:
        qWarning() << "Warning: Unsupported dim specified in" << __FUNCTION__;
        break;
    }

    logAllGLErrors(__FUNCTION__);

    delete[] dataArray;
    delete[] transposedMatrix;
}

/*!
 * \qmlmethod void Context3D::generateMipmap(glEnums target)
 * Generates a complete set of mipmaps for a texture object of the currently active texture unit.
 * \a target defines the texture target to which the texture object is bound whose mipmaps will be
 * generated. Must be either \c{Context3D.TEXTURE_2D} or \c{Context3D.TEXTURE_CUBE_MAP}.
 */
/*!
 * \internal
 */
void CanvasContext::generateMipmap(glEnums target)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ")";

    if (!isValidTextureBound(target, __FUNCTION__))
        return;

    glGenerateMipmap(target);

    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod bool Context3D::isTexture(Object anyObject)
 * Returns true if the given object is a valid Canvas3DTexture object.
 * \a anyObject is the object that is to be verified as a valid texture.
 */
/*!
 * \internal
 */
bool CanvasContext::isTexture(QJSValue anyObject)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(anyObject:" << anyObject.toString()
                                         << ")";

    CanvasTexture *texture = getAsTexture3D(anyObject);
    if (!texture || !checkParent(texture, __FUNCTION__))
        return false;

    return glIsTexture(texture->textureId());
}

/*!
 * \internal
 */
CanvasTexture *CanvasContext::getAsTexture3D(QJSValue anyObject)
{
    if (!isOfType(anyObject, "QtCanvas3D::CanvasTexture"))
        return 0;

    CanvasTexture *texture = static_cast<CanvasTexture *>(anyObject.toQObject());
    if (!texture->isAlive())
        return 0;

    return texture;
}

/*!
 * \qmlmethod void Context3D::compressedTexImage2D(glEnums target, int level, glEnums internalformat, int width, int height, int border, TypedArray pixels)
 * Specify a 2D compressed texture image.
 * \a target specifies the target texture of the active texture unit. Must be one of:
 * \c{Context3D.TEXTURE_2D},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_X}, \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_X},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Y}, \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_Y},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Z}, or \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_Z}.
 * \a level specifies the level of detail number. Level \c 0 is the base image level. Level \c n is
 * the \c{n}th mipmap reduction image.
 * \a internalformat specifies the internal format of the compressed texture.
 * \a width specifies the width of the texture image. All implementations will support 2D texture
 * images that are at least 64 texels wide and cube-mapped texture images that are at least 16
 * texels wide.
 * \a height specifies the height of the texture image. All implementations will support 2D texture
 * images that are at least 64 texels high and cube-mapped texture images that are at least 16
 * texels high.
 * \a border must be \c{0}.
 * \a pixels specifies the TypedArray containing the compressed image data.
 */
/*!
 * \internal
 */
void CanvasContext::compressedTexImage2D(glEnums target, int level, glEnums internalformat,
                                         int width, int height, int border,
                                         QJSValue pixels)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", level:" << level
                                         << ", internalformat:" << glEnumToString(internalformat)
                                         << ", width:" << width
                                         << ", height:" << height
                                         << ", border:" << border
                                         << ", pixels:" << pixels.toString()
                                         << ")";

    if (!isValidTextureBound(target, __FUNCTION__))
        return;

    int byteLen = 0;
    uchar *srcData = getTypedArrayAsRawDataPtr(pixels, byteLen, QV4::Heap::TypedArray::UInt8Array);

    if (srcData) {
        // Driver implementation will handle checking of texture
        // properties for specific compression methods
        glCompressedTexImage2D(target,
                               level,
                               internalformat,
                               width, height, border,
                               byteLen,
                               (GLvoid *)srcData );
        logAllGLErrors(__FUNCTION__);
    } else {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:pixels must be TypedArray";
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }
}

/*!
 * \qmlmethod void Context3D::compressedTexSubImage2D(glEnums target, int level, int xoffset, int yoffset, int width, int height, glEnums format, TypedArray pixels)
 * Specify a 2D compressed texture image.
 * \a target specifies the target texture of the active texture unit. Must be one of:
 * \c{Context3D.TEXTURE_2D},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_X}, \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_X},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Y}, \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_Y},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Z}, or \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_Z}.
 * \a level specifies the level of detail number. Level \c 0 is the base image level. Level \c n is
 * the \c{n}th mipmap reduction image.
 * \a xoffset Specifies a texel offset in the x direction within the texture array.
 * \a yoffset Specifies a texel offset in the y direction within the texture array.
 * \a width Width of the texture subimage.
 * \a height Height of the texture subimage.
 * \a pixels specifies the TypedArray containing the compressed image data.
 * \a format Format of the texel data given in \a pixels, must match the value
 * of \c internalFormat parameter given when the texture was created.
 * \a pixels TypedArray containing the compressed image data. If pixels is \c null.
 */
/*!
 * \internal
 */
void CanvasContext::compressedTexSubImage2D(glEnums target, int level,
                                            int xoffset, int yoffset,
                                            int width, int height,
                                            glEnums format,
                                            QJSValue pixels)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", level:" << level
                                         << ", xoffset:" << xoffset
                                         << ", yoffset:" << yoffset
                                         << ", width:" << width
                                         << ", height:" << height
                                         << ", format:" << glEnumToString(format)
                                         << ", pixels:" << pixels.toString()
                                         << ")";

    if (!isValidTextureBound(target, __FUNCTION__))
        return;

    int byteLen = 0;
    uchar *srcData = getTypedArrayAsRawDataPtr(pixels, byteLen, QV4::Heap::TypedArray::UInt8Array);

    if (srcData) {
        // Driver implementation will handle checking of texture
        // properties for specific compression methods
        glCompressedTexSubImage2D(target,
                                  level,
                                  xoffset, yoffset,
                                  width, height,
                                  format,
                                  byteLen,
                                  (GLvoid *)srcData);
        logAllGLErrors(__FUNCTION__);
    } else {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:pixels must be TypedArray";
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }
}

/*!
 * \qmlmethod void Context3D::copyTexImage2D(glEnums target, int level, glEnums internalformat, int x, int y, int width, int height, int border)
 * Copies pixels into currently bound 2D texture.
 * \a target specifies the target texture of the active texture unit. Must be \c{Context3D.TEXTURE_2D},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_X}, \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_X},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Y}, \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_Y},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Z}, or \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_Z}.
 * \a level specifies the level of detail number. Level \c 0 is the base image level. Level \c n is
 * the \c{n}th mipmap reduction image.
 * \a internalformat specifies the internal format of the texture. Must be \c{Context3D.ALPHA},
 * \c{Context3D.LUMINANCE}, \c{Context3D.LUMINANCE_ALPHA}, \c{Context3D.RGB} or \c{Context3D.RGBA}.
 * \a x specifies the window coordinate of the left edge of the rectangular region of pixels to be
 * copied.
 * \a y specifies the window coordinate of the lower edge of the rectangular region of pixels to be
 * copied.
 * \a width specifies the width of the texture image. All implementations will support 2D texture
 * images that are at least 64 texels wide and cube-mapped texture images that are at least 16
 * texels wide.
 * \a height specifies the height of the texture image. All implementations will support 2D texture
 * images that are at least 64 texels high and cube-mapped texture images that are at least 16
 * texels high.
 * \a border must be \c{0}.
 */
/*!
 * \internal
 */
void CanvasContext::copyTexImage2D(glEnums target, int level, glEnums internalformat,
                                   int x, int y, int width, int height,
                                   int border)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", level:" << level
                                         << ", internalformat:" << glEnumToString(internalformat)
                                         << ", x:" << x
                                         << ", y:" << y
                                         << ", width:" << width
                                         << ", height:" << height
                                         << ", border:" << border
                                         << ")";

    if (!isValidTextureBound(target, __FUNCTION__))
        return;

    glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::copyTexSubImage2D(glEnums target, int level, int xoffset, int yoffset, int x, int y, int width, int height)
 * Copies to into a currently bound 2D texture subimage.
 * \a target specifies the target texture of the active texture unit. Must be
 * \c{Context3D.TEXTURE_2D},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_X}, \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_X},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Y}, \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_Y},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Z}, or \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_Z}.
 * \a level specifies the level of detail number. Level \c 0 is the base image level. Level \c n is
 * the \c{n}th mipmap reduction image.
 * \a xoffset specifies the texel offset in the x direction within the texture array.
 * \a yoffset specifies the texel offset in the y direction within the texture array.
 * \a x specifies the window coordinate of the left edge of the rectangular region of pixels to be
 * copied.
 * \a y specifies the window coordinate of the lower edge of the rectangular region of pixels to be
 * copied.
 * \a width specifies the width of the texture subimage.
 * \a height specifies the height of the texture subimage.
 */
/*!
 * \internal
 */
void CanvasContext::copyTexSubImage2D(glEnums target, int level,
                                      int xoffset, int yoffset,
                                      int x, int y,
                                      int width, int height)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", level:" << level
                                         << ", xoffset:" << xoffset
                                         << ", yoffset:" << yoffset
                                         << ", x:" << x
                                         << ", y:" << y
                                         << ", width:" << width
                                         << ", height:" << height
                                         << ")";

    if (!isValidTextureBound(target, __FUNCTION__))
        return;

    glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::texImage2D(glEnums target, int level, glEnums internalformat, int width, int height, int border, glEnums format, glEnums type, TypedArray pixels)
 * Specify a 2D texture image.
 * \a target specifies the target texture of the active texture unit. Must be one of:
 * \c{Context3D.TEXTURE_2D},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_X}, \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_X},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Y}, \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_Y},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Z}, or \c{Context3D.TEXTURE_CUBE_MAP_NEGATIVE_Z}.
 * \a level specifies the level of detail number. Level \c 0 is the base image level. Level \c n is
 * the \c{n}th mipmap reduction image.
 * \a internalformat specifies the internal format of the texture. Must be \c{Context3D.ALPHA},
 * \c{Context3D.LUMINANCE}, \c{Context3D.LUMINANCE_ALPHA}, \c{Context3D.RGB} or \c{Context3D.RGBA}.
 * \a width specifies the width of the texture image. All implementations will support 2D texture
 * images that are at least 64 texels wide and cube-mapped texture images that are at least 16
 * texels wide.
 * \a height specifies the height of the texture image. All implementations will support 2D texture
 * images that are at least 64 texels high and cube-mapped texture images that are at least 16
 * texels high.
 * \a border must be \c{0}.
 * \a format specifies the format of the texel data given in \a pixels, must match the value
 * of \a internalFormat.
 * \a type specifies the data type of the data given in \a pixels, must match the TypedArray type
 * of \a pixels. Must be \c{Context3D.UNSIGNED_BYTE}, \c{Context3D.UNSIGNED_SHORT_5_6_5},
 * \c{Context3D.UNSIGNED_SHORT_4_4_4_4} or \c{Context3D.UNSIGNED_SHORT_5_5_5_1}.
 * \a pixels specifies the TypedArray containing the image data. If pixels is \c{null}, a buffer
 * of sufficient size initialized to 0 is passed.
 */
/*!
 * \internal
 */
void CanvasContext::texImage2D(glEnums target, int level, glEnums internalformat,
                               int width, int height, int border,
                               glEnums format, glEnums type,
                               QJSValue pixels)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", level:" << level
                                         << ", internalformat:" << glEnumToString(internalformat)
                                         << ", width:" << width
                                         << ", height:" << height
                                         << ", border:" << border
                                         << ", format:" << glEnumToString(format)
                                         << ", type:" << glEnumToString(type)
                                         << ", pixels:" << pixels.toString()
                                         << ")";
    if (!isValidTextureBound(target, __FUNCTION__))
        return;

    int bytesPerPixel = 0;
    uchar *srcData = 0;
    uchar *unpackedData = 0;

    bool deleteTempPixels = false;
    if (pixels.isNull()) {
        deleteTempPixels = true;
        int size = getSufficientSize(type, width, height);
        srcData = new uchar[size];
        memset(srcData, 0, size);
    }

    switch (type) {
    case UNSIGNED_BYTE: {
        switch (format) {
        case ALPHA:
            bytesPerPixel = 1;
            break;
        case RGB:
            bytesPerPixel = 3;
            break;
        case RGBA:
            bytesPerPixel = 4;
            break;
        case LUMINANCE:
            bytesPerPixel = 1;
            break;
        case LUMINANCE_ALPHA:
            bytesPerPixel = 2;
            break;
        default:
            break;
        }

        if (bytesPerPixel == 0) {
            m_error |= CANVAS_INVALID_ENUM;
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_ENUM:Invalid format supplied "
                                                   << glEnumToString(format);
            return;
        }

        if (!srcData)
            srcData = getTypedArrayAsRawDataPtr(pixels, QV4::Heap::TypedArray::UInt8Array);

        if (!srcData) {
            qCWarning(canvas3drendering).nospace() << "Context3D::"
                                                   << __FUNCTION__
                                                   << ":INVALID_OPERATION:Expected Uint8Array,"
                                                   << " received " << pixels.toString();
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }

        unpackedData = unpackPixels(srcData, false, bytesPerPixel, width, height);
        glTexImage2D(target, level, internalformat, width, height,
                     border, format, type, unpackedData);
        logAllGLErrors(__FUNCTION__);
    }
        break;
    case UNSIGNED_SHORT_4_4_4_4:
    case UNSIGNED_SHORT_5_6_5:
    case UNSIGNED_SHORT_5_5_5_1: {
        if (!srcData)
            srcData = getTypedArrayAsRawDataPtr(pixels, QV4::Heap::TypedArray::UInt16Array);

        if (!srcData) {
            qCWarning(canvas3drendering).nospace() << "Context3D::"
                                                   << __FUNCTION__
                                                   << ":INVALID_OPERATION:Expected Uint16Array,"
                                                   << " received " << pixels.toString();
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }
        unpackedData = unpackPixels(srcData, false, 2, width, height);
        glTexImage2D(target, level, internalformat, width, height,
                     border, format, type, unpackedData);
        logAllGLErrors(__FUNCTION__);
    }
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::"
                                               << __FUNCTION__
                                               << ":INVALID_ENUM:Invalid type enum";
        m_error |= CANVAS_INVALID_ENUM;
        break;
    }

    // Delete temp data
    if (unpackedData != srcData)
        delete unpackedData;

    if (deleteTempPixels)
        delete[] srcData;
}

/*!
 * \internal
 */
uchar *CanvasContext::getTypedArrayAsRawDataPtr(const QJSValue &jsValue, int &byteLength,
                                                QV4::Heap::TypedArray::Type type)
{
    QV4::Scope scope(m_v4engine);
    QV4::Scoped<QV4::TypedArray> typedArray(scope,
                                            QJSValuePrivate::convertedToValue(m_v4engine, jsValue));

    if (!typedArray)
        return 0;

    QV4::Heap::TypedArray::Type arrayType = typedArray->arrayType();
    if (type < QV4::Heap::TypedArray::NTypes && arrayType != type)
        return 0;

    uchar *dataPtr = reinterpret_cast<uchar *>(typedArray->arrayData()->data());
    dataPtr += typedArray->d()->byteOffset;
    byteLength = typedArray->byteLength();
    return dataPtr;
}

/*!
 * \internal
 */
uchar *CanvasContext::getTypedArrayAsRawDataPtr(const QJSValue &jsValue,
                                                QV4::Heap::TypedArray::Type type)
{
    int dummy;
    return getTypedArrayAsRawDataPtr(jsValue, dummy, type);
}

/*!
 * \internal
 */
uchar *CanvasContext::getTypedArrayAsRawDataPtr(const QJSValue &jsValue, int &byteLength)
{
    return getTypedArrayAsRawDataPtr(jsValue, byteLength, QV4::Heap::TypedArray::NTypes);
}

/*!
 * \internal
 */
uchar *CanvasContext::getArrayBufferAsRawDataPtr(const QJSValue &jsValue, int &byteLength)
{
    QV4::Scope scope(m_v4engine);
    QV4::Scoped<QV4::ArrayBuffer> arrayBuffer(scope,
                                              QJSValuePrivate::convertedToValue(m_v4engine, jsValue));
    if (!arrayBuffer)
        return 0;

    uchar *dataPtr = reinterpret_cast<uchar *>(arrayBuffer->data());
    byteLength = arrayBuffer->byteLength();
    return dataPtr;
}

/*!
 * \qmlmethod void Context3D::texSubImage2D(glEnums target, int level, int xoffset, int yoffset, int width, int height, glEnums format, glEnums type, TypedArray pixels)
 * Specify a 2D texture subimage.
 * \a target Target texture of the active texture unit. Must be \c{Context3D.TEXTURE_2D},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_X}, \c{Context3D.TEXTURE_CUBE_MAP__NEGATIVE_X},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Y}, \c{Context3D.TEXTURE_CUBE_MAP__NEGATIVE_Y},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Z}, or \c{Context3D.TEXTURE_CUBE_MAP__NEGATIVE_Z}.
 * \a level Level of detail number. Level \c 0 is the base image level. Level \c n is the \c{n}th
 * mipmap reduction image.
 * \a xoffset Specifies a texel offset in the x direction within the texture array.
 * \a yoffset Specifies a texel offset in the y direction within the texture array.
 * \a width Width of the texture subimage.
 * \a height Height of the texture subimage.
 * \a format Format of the texel data given in \a pixels, must match the value
 * of \c internalFormat parameter given when the texture was created.
 * \a type Data type of the data given in \a pixels, must match the TypedArray type
 * of \a pixels. Must be \c{Context3D.UNSIGNED_BYTE}, \c{Context3D.UNSIGNED_SHORT_5_6_5},
 * \c{Context3D.UNSIGNED_SHORT_4_4_4_4} or \c{Context3D.UNSIGNED_SHORT_5_5_5_1}.
 * \a pixels TypedArray containing the image data.
 */
/*!
 * \internal
 */
void CanvasContext::texSubImage2D(glEnums target, int level,
                                  int xoffset, int yoffset,
                                  int width, int height,
                                  glEnums format, glEnums type,
                                  QJSValue pixels)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", level:" << level
                                         << ", xoffset:" << xoffset
                                         << ", yoffset:" << yoffset
                                         << ", width:" << width
                                         << ", height:" << height
                                         << ", format:" << glEnumToString(format)
                                         << ", type:" << glEnumToString(type)
                                         << ", pixels:" << pixels.toString()
                                         << ")";

    if (!isValidTextureBound(target, __FUNCTION__))
        return;

    if (pixels.isNull()) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:pixels was null";
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }

    int bytesPerPixel = 0;
    uchar *srcData = 0;
    uchar *unpackedData = 0;

    switch (type) {
    case UNSIGNED_BYTE: {
        switch (format) {
        case ALPHA:
            bytesPerPixel = 1;
            break;
        case RGB:
            bytesPerPixel = 3;
            break;
        case RGBA:
            bytesPerPixel = 4;
            break;
        case LUMINANCE:
            bytesPerPixel = 1;
            break;
        case LUMINANCE_ALPHA:
            bytesPerPixel = 2;
            break;
        default:
            break;
        }

        if (bytesPerPixel == 0) {
            m_error |= CANVAS_INVALID_ENUM;
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_ENUM:Invalid format "
                                                   << glEnumToString(format);
            return;
        }

        srcData = getTypedArrayAsRawDataPtr(pixels, QV4::Heap::TypedArray::UInt8Array);
        if (!srcData) {
            qCWarning(canvas3drendering).nospace() << "Context3D::"
                                                   << __FUNCTION__
                                                   << ":INVALID_OPERATION:Expected Uint8Array,"
                                                   << " received " << pixels.toString();
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }

        unpackedData = unpackPixels(srcData, false, bytesPerPixel, width, height);
        glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, unpackedData);
        logAllGLErrors(__FUNCTION__);
    }
        break;
    case UNSIGNED_SHORT_4_4_4_4:
    case UNSIGNED_SHORT_5_6_5:
    case UNSIGNED_SHORT_5_5_5_1: {
        srcData = getTypedArrayAsRawDataPtr(pixels, QV4::Heap::TypedArray::UInt16Array);
        if (!srcData) {
            qCWarning(canvas3drendering).nospace() << "Context3D::"
                                                   << __FUNCTION__
                                                   << ":INVALID_OPERATION:Expected Uint16Array, "
                                                   << "received " << pixels.toString();
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }
        unpackedData = unpackPixels(srcData, false, 2, width, height);
        glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, unpackedData);
        logAllGLErrors(__FUNCTION__);
    }
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:Invalid type enum";
        m_error |= CANVAS_INVALID_ENUM;
        break;
    }

    // Delete temp data
    if (unpackedData != srcData)
        delete unpackedData;
}

/*!
 * \internal
 */
uchar* CanvasContext::unpackPixels(uchar *srcData, bool useSrcDataAsDst,
                                   int bytesPerPixel, int width, int height)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(srcData:" << srcData
                                         << ", useSrcDataAsDst:" << useSrcDataAsDst
                                         << ", bytesPerPixel:" << bytesPerPixel
                                         << ", width:" << width
                                         << ", height:" << height
                                         << ")";

    // Check if no processing is needed
    if (!m_unpackFlipYEnabled || srcData == 0 || width == 0 || height == 0 || bytesPerPixel == 0)
        return srcData;

    uchar *unpackedData = srcData;
    int bytesPerRow = width * bytesPerPixel;
    if (m_unpackFlipYEnabled) {
        if (useSrcDataAsDst) {
            uchar *row = new uchar[width*bytesPerPixel];
            for (int y = 0; y < height; y++) {
                memcpy(row,
                       srcData + y * bytesPerRow,
                       bytesPerRow);
                memcpy(srcData + y * bytesPerRow,
                       srcData + (height - y - 1) * bytesPerRow,
                       bytesPerRow);
                memcpy(srcData + (height - y - 1) * bytesPerRow,
                       row,
                       bytesPerRow);
            }
        } else {
            unpackedData = new uchar[height * bytesPerRow];
            for (int y = 0; y < height; y++) {
                memcpy(unpackedData + (height - y - 1) * bytesPerRow,
                       srcData + y * bytesPerRow,
                       bytesPerRow);
            }
        }
    }

    return unpackedData;
}

/*!
 * \qmlmethod void Context3D::texImage2D(glEnums target, int level, glEnums internalformat, glEnums format, glEnums type, TextureImage texImage)
 * Uploads the given TextureImage element to the currently bound Canvas3DTexture.
 * \a target Target texture of the active texture unit. Must be \c{Context3D.TEXTURE_2D},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_X}, \c{Context3D.TEXTURE_CUBE_MAP__NEGATIVE_X},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Y}, \c{Context3D.TEXTURE_CUBE_MAP__NEGATIVE_Y},
 * \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Z}, or \c{Context3D.TEXTURE_CUBE_MAP__NEGATIVE_Z}.
 * \a level Level of detail number. Level \c 0 is the base image level. Level \c n is the \c{n}th
 * mipmap reduction image.
 * \a internalformat Internal format of the texture, conceptually the given image is first
 * converted to this format, then uploaded. Must be \c{Context3D.ALPHA}, \c{Context3D.LUMINANCE},
 * \c{Context3D.LUMINANCE_ALPHA}, \c{Context3D.RGB} or \c{Context3D.RGBA}.
 * \a format Format of the texture, must match the value of \a internalFormat.
 * \a type Type of the data, conceptually the given image is first converted to this type, then
 * uploaded. Must be \c{Context3D.UNSIGNED_BYTE}, \c{Context3D.UNSIGNED_SHORT_5_6_5},
 * \c{Context3D.UNSIGNED_SHORT_4_4_4_4} or \c{Context3D.UNSIGNED_SHORT_5_5_5_1}.
 * \a texImage A complete \c{TextureImage} loaded using the \c{TextureImageFactory}.
 */
/*!
 * \internal
 */
void CanvasContext::texImage2D(glEnums target, int level, glEnums internalformat,
                               glEnums format, glEnums type, QJSValue texImage)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", level:" << level
                                         << ", internalformat:" << glEnumToString(internalformat)
                                         << ", format:" << glEnumToString(format)
                                         << ", type:" << glEnumToString(type)
                                         << ", texImage:" << texImage.toString()
                                         << ")";

    if (!isValidTextureBound(target, __FUNCTION__))
        return;

    CanvasTextureImage *image = getAsTextureImage(texImage);
    if (!image) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:"
                                               << "Invalid texImage " << texImage.toString();
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }

    uchar *pixels = 0;
    switch (type) {
    case UNSIGNED_BYTE:
        pixels = image->convertToFormat(type, m_unpackFlipYEnabled, m_unpackPremultiplyAlphaEnabled);
        break;
    case UNSIGNED_SHORT_5_6_5:
    case UNSIGNED_SHORT_4_4_4_4:
    case UNSIGNED_SHORT_5_5_5_1:
        pixels = image->convertToFormat(type, m_unpackFlipYEnabled, m_unpackPremultiplyAlphaEnabled);
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:Invalid type enum";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    if (pixels == 0) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":Conversion of pixels to format failed.";
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    if (!m_currentTexture2D->hasSpecificName()) {
        m_currentTexture2D->setName("ImageTexture_"+image->name());
    }

    glTexImage2D(target, level, internalformat, image->width(), image->height(), 0, format, type,
                 pixels);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \internal
 */
CanvasTextureImage* CanvasContext::getAsTextureImage(QJSValue anyObject)
{
    if (!isOfType(anyObject, "QtCanvas3D::CanvasTextureImage"))
        return 0;

    CanvasTextureImage *texImage = static_cast<CanvasTextureImage *>(anyObject.toQObject());
    return texImage;
}


/*!
 * \qmlmethod void Context3D::texSubImage2D(glEnums target, int level, int xoffset, int yoffset, glEnums format, glEnums type, TextureImage texImage)
 * Uploads the given TextureImage element to the currently bound Canvas3DTexture.
 * \a target specifies the target texture of the active texture unit. Must be
 * \c{Context3D.TEXTURE_2D}, \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_X},
 * \c{Context3D.TEXTURE_CUBE_MAP__NEGATIVE_X}, \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Y},
 * \c{Context3D.TEXTURE_CUBE_MAP__NEGATIVE_Y}, \c{Context3D.TEXTURE_CUBE_MAP_POSITIVE_Z}, or
 * \c{Context3D.TEXTURE_CUBE_MAP__NEGATIVE_Z}.
 * \a level Level of detail number. Level \c 0 is the base image level. Level \c n is the \c{n}th
 * mipmap reduction image.
 * \a internalformat Internal format of the texture, conceptually the given image is first
 * converted to this format, then uploaded. Must be \c{Context3D.ALPHA}, \c{Context3D.LUMINANCE},
 * \c{Context3D.LUMINANCE_ALPHA}, \c{Context3D.RGB} or \c{Context3D.RGBA}.
 * \a format Format of the texture, must match the value of \a internalFormat.
 * \a type Type of the data, conceptually the given image is first converted to this type, then
 * uploaded. Must be \c{Context3D.UNSIGNED_BYTE}, \c{Context3D.UNSIGNED_SHORT_5_6_5},
 * \c{Context3D.UNSIGNED_SHORT_4_4_4_4} or \c{Context3D.UNSIGNED_SHORT_5_5_5_1}.
 * \a texImage A complete \c{TextureImage} loaded using the \c{TextureImageFactory}.
 */
/*!
 * \internal
 */
void CanvasContext::texSubImage2D(glEnums target, int level,
                                  int xoffset, int yoffset,
                                  glEnums format, glEnums type, QJSValue texImage)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "( target:" << glEnumToString(target)
                                         << ", level:" << level
                                         << ", xoffset:" << xoffset
                                         << ", yoffset:" << yoffset
                                         << ", format:" << glEnumToString(format)
                                         << ", type:" << glEnumToString(type)
                                         << ", texImage:" << texImage.toString()
                                         << ")";

    if (!isValidTextureBound(target, __FUNCTION__))
        return;

    CanvasTextureImage *image = getAsTextureImage(texImage);
    if (!image) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:invalid texImage "
                                               << texImage.toString();
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }

    uchar *pixels = 0;
    switch (type) {
    case UNSIGNED_BYTE:
        pixels = image->convertToFormat(type,
                                        m_unpackFlipYEnabled,
                                        m_unpackPremultiplyAlphaEnabled);
        break;
    case UNSIGNED_SHORT_5_6_5:
    case UNSIGNED_SHORT_4_4_4_4:
    case UNSIGNED_SHORT_5_5_5_1:
        pixels = image->convertToFormat(type,
                                        m_unpackFlipYEnabled,
                                        m_unpackPremultiplyAlphaEnabled);
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:Invalid type enum";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    if (pixels == 0) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":Conversion of pixels to format failed.";
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    glTexSubImage2D(target, level, xoffset, yoffset, image->width(), image->height(), format,
                    type, pixels);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::texParameterf(glEnums target, glEnums pname, float param)
 * Sets texture parameters.
 * \a target specifies the target texture of the active texture unit. Must be
 * \c{Context3D.TEXTURE_2D} or \c{Context3D.TEXTURE_CUBE_MAP}.
 * \a pname specifies the symbolic name of a texture parameter. pname can be
 * \c{Context3D.TEXTURE_MIN_FILTER}, \c{Context3D.TEXTURE_MAG_FILTER}, \c{Context3D.TEXTURE_WRAP_S} or
 * \c{Context3D.TEXTURE_WRAP_T}.
 * \a param specifies the new float value to be set to \a pname
 */
/*!
 * \internal
 */
void CanvasContext::texParameterf(glEnums target, glEnums pname, float param)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "( target:" << glEnumToString(target)
                                         << ", pname:" << glEnumToString(pname)
                                         << ", param:" << param
                                         << ")";

    if (!isValidTextureBound(target, __FUNCTION__))
        return;

    glTexParameterf(GLenum(target), GLenum(pname), GLfloat(param));
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::texParameteri(glEnums target, glEnums pname, float param)
 * Sets texture parameters.
 * \a target specifies the target texture of the active texture unit. Must be
 * \c{Context3D.TEXTURE_2D} or \c{Context3D.TEXTURE_CUBE_MAP}.
 * \a pname specifies the symbolic name of a texture parameter. pname can be
 * \c{Context3D.TEXTURE_MIN_FILTER}, \c{Context3D.TEXTURE_MAG_FILTER}, \c{Context3D.TEXTURE_WRAP_S} or
 * \c{Context3D.TEXTURE_WRAP_T}.
 * \a param specifies the new int value to be set to \a pname
 */
/*!
 * \internal
 */
void CanvasContext::texParameteri(glEnums target, glEnums pname, int param)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", pname:" << glEnumToString(pname)
                                         << ", param:" << glEnumToString(glEnums(param))
                                         << ")";

    if (!isValidTextureBound(target, __FUNCTION__))
        return;

    glTexParameteri(GLenum(target), GLenum(pname), GLint(param));
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \internal
 */
int CanvasContext::getSufficientSize(glEnums internalFormat, int width, int height)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "( internalFormat:" << glEnumToString(internalFormat)
                                         << " , width:" << width
                                         << ", height:" << height
                                         << ")";
    int bytesPerPixel = 0;
    switch (internalFormat) {
    case UNSIGNED_BYTE:
        bytesPerPixel = 4;
        break;
    case UNSIGNED_SHORT_5_6_5:
    case UNSIGNED_SHORT_4_4_4_4:
    case UNSIGNED_SHORT_5_5_5_1:
        bytesPerPixel = 2;
        break;
    default:
        break;
    }

    width = (width > 0) ? width : 0;
    height = (height > 0) ? height : 0;

    return width * height * bytesPerPixel;
}

/*!
 * \qmlmethod Canvas3DFrameBuffer Context3D::createFramebuffer()
 * Returns a created Canvas3DFrameBuffer object that is initialized with a framebuffer object name as
 * if by calling \c{glGenFramebuffers()}.
 */
/*!
 * \internal
 */
QJSValue CanvasContext::createFramebuffer()
{
    CanvasFrameBuffer *framebuffer = new CanvasFrameBuffer(this);
    QJSValue value = m_engine->newQObject(framebuffer);
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << ":" << value.toString();

    logAllGLErrors(__FUNCTION__);
    return value;
}

/*!
 * \qmlmethod void Context3D::bindFramebuffer(glEnums target, Canvas3DFrameBuffer buffer)
 * Binds the given \a buffer object to the given \a target.
 * \a target must be \c{Context3D.FRAMEBUFFER}.
 */
/*!
 * \internal
 */
void CanvasContext::bindFramebuffer(glEnums target, QJSValue buffer)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", framebuffer:" << buffer.toString() << ")";

    if (target != FRAMEBUFFER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): INVALID_ENUM:"
                                               << " bind target, must be FRAMEBUFFER";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    CanvasFrameBuffer *framebuffer = getAsFramebuffer(buffer);

    if (framebuffer && checkParent(framebuffer, __FUNCTION__))
        m_currentFramebuffer = framebuffer;
    else
        m_currentFramebuffer = 0;

    // Let canvas component figure out the exact frame buffer id to use
    m_canvas->bindCurrentRenderTarget();
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod Context3D::glEnums Context3D::checkFramebufferStatus(glEnums target)
 * Returns the completeness status of the framebuffer object.
 * \a target must be \c{Context3D.FRAMEBUFFER}.
 *
 */
/*!
 * \internal
 */
CanvasContext::glEnums CanvasContext::checkFramebufferStatus(glEnums target)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ")";
    if (target != FRAMEBUFFER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_ENUM bind target, must be FRAMEBUFFER";
        m_error |= CANVAS_INVALID_ENUM;
        return FRAMEBUFFER_UNSUPPORTED;
    }

    return glEnums(glCheckFramebufferStatus(GL_FRAMEBUFFER));
}

/*!
 * \qmlmethod void Context3D::framebufferRenderbuffer(glEnums target, glEnums attachment, glEnums renderbuffertarget, Canvas3DRenderBuffer renderbuffer3D)
 * Attaches the given \a renderbuffer3D object to the \a attachment point of the current framebuffer
 * object.
 * \a target must be \c{Context3D.FRAMEBUFFER}. \a renderbuffertarget must
 * be \c{Context3D.RENDERBUFFER}.
 */
/*!
 * \internal
 */
void CanvasContext::framebufferRenderbuffer(glEnums target, glEnums attachment,
                                            glEnums renderbuffertarget,
                                            QJSValue renderbuffer3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << "attachment:" << glEnumToString(attachment)
                                         << "renderbuffertarget:"
                                         << glEnumToString(renderbuffertarget)
                                         << ", renderbuffer3D:" << renderbuffer3D.toString()
                                         << ")";

    if (target != FRAMEBUFFER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_ENUM:bind target, must be FRAMEBUFFER";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    if (!m_currentFramebuffer) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): INVALID_OPERATION:no framebuffer bound";
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    if (attachment != COLOR_ATTACHMENT0
            && attachment != DEPTH_ATTACHMENT
            && attachment != STENCIL_ATTACHMENT
            && attachment != DEPTH_STENCIL_ATTACHMENT) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): INVALID_OPERATION:attachment must be one of "
                                               << "COLOR_ATTACHMENT0, DEPTH_ATTACHMENT, "
                                               << "STENCIL_ATTACHMENT or DEPTH_STENCIL_ATTACHMENT";
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    CanvasRenderBuffer *renderbuffer = getAsRenderbuffer3D(renderbuffer3D);
    if (renderbuffer && renderbuffertarget != RENDERBUFFER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): INVALID_OPERATION renderbuffertarget must be"
                                               << " RENDERBUFFER for non null renderbuffers";
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }
    if (!checkParent(renderbuffer, __FUNCTION__))
        return;

    GLuint renderbufferId = renderbuffer ? renderbuffer->id() : 0;
    glFramebufferRenderbuffer(GLenum(target),
                              GLenum(attachment),
                              GLenum(renderbuffertarget),
                              renderbufferId);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::framebufferTexture2D(glEnums target, glEnums attachment, glEnums textarget, Canvas3DTexture texture3D, int level)
 * Attaches the given \a renderbuffer object to the \a attachment point of the current framebuffer
 * object.
 * \a target must be \c{Context3D.FRAMEBUFFER}. \a renderbuffertarget must
 * be \c{Context3D.RENDERBUFFER}.
 */
/*!
 * \internal
 */
void CanvasContext::framebufferTexture2D(glEnums target, glEnums attachment, glEnums textarget,
                                         QJSValue texture3D, int level)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", attachment:" << glEnumToString(attachment)
                                         << ", textarget:" << glEnumToString(textarget)
                                         << ", texture:" << texture3D.toString()
                                         << ", level:" << level
                                         << ")";

    if (target != FRAMEBUFFER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): INVALID_ENUM:"
                                               << " bind target, must be FRAMEBUFFER";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    if (!m_currentFramebuffer) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): INVALID_OPERATION:"
                                               << " no current framebuffer bound";
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    if (attachment != COLOR_ATTACHMENT0 && attachment != DEPTH_ATTACHMENT
            && attachment != STENCIL_ATTACHMENT) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): INVALID_OPERATION attachment must be one of "
                                               << "COLOR_ATTACHMENT0, DEPTH_ATTACHMENT"
                                               << " or STENCIL_ATTACHMENT";
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    CanvasTexture *texture = getAsTexture3D(texture3D);
    if (texture) {
        if (!checkParent(texture, __FUNCTION__))
            return;
        if (textarget != TEXTURE_2D
                && textarget != TEXTURE_CUBE_MAP_POSITIVE_X
                && textarget != TEXTURE_CUBE_MAP_POSITIVE_Y
                && textarget != TEXTURE_CUBE_MAP_POSITIVE_Z
                && textarget != TEXTURE_CUBE_MAP_NEGATIVE_X
                && textarget != TEXTURE_CUBE_MAP_NEGATIVE_Y
                && textarget != TEXTURE_CUBE_MAP_NEGATIVE_Z) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << "(): textarget must be one of TEXTURE_2D, "
                                                   << "TEXTURE_CUBE_MAP_POSITIVE_X, "
                                                   << "TEXTURE_CUBE_MAP_POSITIVE_Y, "
                                                   << "TEXTURE_CUBE_MAP_POSITIVE_Z, "
                                                   << "TEXTURE_CUBE_MAP_NEGATIVE_X, "
                                                   << "TEXTURE_CUBE_MAP_NEGATIVE_Y or "
                                                   << "TEXTURE_CUBE_MAP_NEGATIVE_Z";
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }

        if (level != 0) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << "(): INVALID_VALUE level must be 0";
            m_error |= CANVAS_INVALID_VALUE;
            return;
        }
    }

    GLuint textureId = texture ? texture->textureId() : 0;
    m_currentFramebuffer->setTexture(texture);
    glFramebufferTexture2D(GLenum(target), GLenum(attachment), GLenum(textarget), textureId, level);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::isFramebuffer(Object anyObject)
 * Returns true if the given object is a valid Canvas3DFrameBuffer object.
 * \a anyObject is the object that is to be verified as a valid framebuffer.
 */
/*!
 * \internal
 */
bool CanvasContext::isFramebuffer(QJSValue anyObject)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "( anyObject:" << anyObject.toString()
                                         << ")";


    CanvasFrameBuffer *fbo = getAsFramebuffer(anyObject);
    if (fbo && checkParent(fbo, __FUNCTION__))
        return glIsFramebuffer(fbo->id());
    else
        return false;
}

/*!
 * \internal
 */
CanvasFrameBuffer *CanvasContext::getAsFramebuffer(QJSValue anyObject)
{
    if (!isOfType(anyObject, "QtCanvas3D::CanvasFrameBuffer"))
        return 0;

    CanvasFrameBuffer *fbo = static_cast<CanvasFrameBuffer *>(anyObject.toQObject());

    if (!fbo->isAlive())
        return 0;

    return fbo;
}

/*!
 * \qmlmethod void Context3D::deleteFramebuffer(Canvas3DFrameBuffer buffer)
 * Deletes the given framebuffer as if by calling \c{glDeleteFramebuffers()}.
 * Calling this method repeatedly on the same object has no side effects.
 * \a buffer is the Canvas3DFrameBuffer to be deleted.
 */
/*!
 * \internal
 */
void CanvasContext::deleteFramebuffer(QJSValue buffer)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "( buffer:" << buffer.toString()
                                         << ")";

    CanvasFrameBuffer *fbo = getAsFramebuffer(buffer);
    if (fbo) {
        if (!checkParent(fbo, __FUNCTION__))
            return;
        fbo->del();
        logAllGLErrors(__FUNCTION__);
    } else {
        m_error |= CANVAS_INVALID_VALUE;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): INVALID_VALUE buffer handle";
    }
}

/*!
 * \qmlmethod Canvas3DRenderBuffer Context3D::createRenderbuffer()
 * Returns a created Canvas3DRenderBuffer object that is initialized with a renderbuffer object name
 * as if by calling \c{glGenRenderbuffers()}.
 */
/*!
 * \internal
 */
QJSValue CanvasContext::createRenderbuffer()
{
    CanvasRenderBuffer *renderbuffer = new CanvasRenderBuffer(this);
    QJSValue value = m_engine->newQObject(renderbuffer);
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "():" << value.toString();
    logAllGLErrors(__FUNCTION__);
    return value;
}

/*!
 * \qmlmethod void Context3D::bindRenderbuffer(glEnums target, Canvas3DRenderBuffer renderbuffer)
 * Binds the given \a renderbuffer3D object to the given \a target.
 * \a target must be \c{Context3D.RENDERBUFFER}.
 */
/*!
 * \internal
 */
void CanvasContext::bindRenderbuffer(glEnums target, QJSValue renderbuffer3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", renderbuffer3D:" << renderbuffer3D.toString()
                                         << ")";

    if (target != RENDERBUFFER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_ENUM target must be RENDERBUFFER";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    CanvasRenderBuffer *renderbuffer = getAsRenderbuffer3D(renderbuffer3D);
    if (renderbuffer && checkParent(renderbuffer, __FUNCTION__)) {
        m_currentRenderbuffer = renderbuffer;
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer->id());
    } else {
        m_currentRenderbuffer = 0;
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::renderbufferStorage(glEnums target, glEnums internalformat, int width, int height)
 * Create and initialize a data store for the \c renderbuffer object.
 * \a target must be \c Context3D.RENDERBUFFER.
 * \a internalformat specifies the color-renderable, depth-renderable or stencil-renderable format
 * of the renderbuffer. Must be one of \c{Context3D.RGBA4}, \c{Context3D.RGB565}, \c{Context3D.RGB5_A1},
 * \c{Context3D.DEPTH_COMPONENT16} or \c{Context3D.STENCIL_INDEX8}.
 * \a width specifies the renderbuffer width in pixels.
 * \a height specifies the renderbuffer height in pixels.
 */
/*!
 * \internal
 */
void CanvasContext::renderbufferStorage(glEnums target, glEnums internalformat,
                                        int width, int height)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", internalformat:" << glEnumToString(internalformat)
                                         << ", width:" << width
                                         << ", height:" << height
                                         << ")";

    if (target != RENDERBUFFER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_ENUM target must be RENDERBUFFER";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    glRenderbufferStorage(GLenum(target), GLenum(internalformat), width, height);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod bool Context3D::isRenderbuffer(Object anyObject)
 * Returns true if the given object is a valid Canvas3DRenderBuffer object.
 * \a anyObject is the object that is to be verified as a valid renderbuffer.
 */
/*!
 * \internal
 */
bool CanvasContext::isRenderbuffer(QJSValue anyObject)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(anyObject:" << anyObject.toString()
                                         << ")";

    CanvasRenderBuffer *rbo = getAsRenderbuffer3D(anyObject);
    if (!rbo || !checkParent(rbo, __FUNCTION__))
        return false;

    return glIsRenderbuffer(rbo->id());
}

/*!
 * \internal
 */
CanvasRenderBuffer *CanvasContext::getAsRenderbuffer3D(QJSValue anyObject) const
{
    if (!isOfType(anyObject, "QtCanvas3D::CanvasRenderBuffer"))
        return 0;

    CanvasRenderBuffer *rbo = static_cast<CanvasRenderBuffer *>(anyObject.toQObject());
    if (!rbo->isAlive())
        return 0;

    return rbo;
}

/*!
 * \qmlmethod void Context3D::deleteRenderbuffer(Canvas3DRenderBuffer renderbuffer3D)
 * Deletes the given renderbuffer as if by calling \c{glDeleteRenderbuffers()}.
 * Calling this method repeatedly on the same object has no side effects.
 * \a renderbuffer3D is the Canvas3DRenderBuffer to be deleted.
 */
/*!
 * \internal
 */
void CanvasContext::deleteRenderbuffer(QJSValue renderbuffer3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(renderbuffer3D:" << renderbuffer3D.toString()
                                         << ")";

    CanvasRenderBuffer *renderbuffer = getAsRenderbuffer3D(renderbuffer3D);
    if (renderbuffer) {
        if (!checkParent(renderbuffer, __FUNCTION__))
            return;
        renderbuffer->del();
        logAllGLErrors(__FUNCTION__);
    } else {
        m_error |= CANVAS_INVALID_VALUE;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): INVALID_VALUE renderbuffer handle";
    }
}

/*!
 * \qmlmethod void Context3D::sampleCoverage(float value, bool invert)
 * Sets the multisample coverage parameters.
 * \a value specifies the floating-point sample coverage value. The value is clamped to the range
 * \c{[0, 1]} with initial value of \c{1.0}.
 * \a invert specifies if coverage masks should be inverted.
 */
/*!
 * \internal
 */
void CanvasContext::sampleCoverage(float value, bool invert)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(value:" << value
                                         << ", invert:" << invert
                                         << ")";
    glSampleCoverage(value, invert);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod Canvas3DProgram Context3D::createProgram()
 * Returns a created Canvas3DProgram object that is initialized with a program object name as if by
 * calling \c{glCreateProgram()}.
 */
/*!
 * \internal
 */
QJSValue CanvasContext::createProgram()
{
    CanvasProgram *program = new CanvasProgram(this);
    QJSValue value = m_engine->newQObject(program);
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "():" << value.toString();

    logAllGLErrors(__FUNCTION__);
    return value;
}

/*!
 * \qmlmethod bool Context3D::isProgram(Object anyObject)
 * Returns true if the given object is a valid Canvas3DProgram object.
 * \a anyObject is the object that is to be verified as a valid program.
 */
/*!
 * \internal
 */
bool CanvasContext::isProgram(QJSValue anyObject)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(anyObject:" << anyObject.toString()
                                         << ")";

    CanvasProgram *program = getAsProgram3D(anyObject);
    if (!program || !checkParent(program, __FUNCTION__))
        return false;

    return true;
}

/*!
 * \internal
 */
CanvasProgram *CanvasContext::getAsProgram3D(QJSValue anyObject, bool deadOrAlive) const
{
    if (!isOfType(anyObject, "QtCanvas3D::CanvasProgram"))
        return 0;

    CanvasProgram *program = static_cast<CanvasProgram *>(anyObject.toQObject());
    if (!deadOrAlive && !program->isAlive())
        return 0;

    return program;
}

/*!
 * \qmlmethod void Context3D::deleteProgram(Canvas3DProgram program3D)
 * Deletes the given program as if by calling \c{glDeleteProgram()}.
 * Calling this method repeatedly on the same object has no side effects.
 * \a program3D is the Canvas3DProgram to be deleted.
 */
/*!
 * \internal
 */
void CanvasContext::deleteProgram(QJSValue program3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ")";

    CanvasProgram *program = getAsProgram3D(program3D, true);

    if (program) {
        if (!checkParent(program, __FUNCTION__))
            return;
        program->del();
        logAllGLErrors(__FUNCTION__);
    } else {
        m_error |= CANVAS_INVALID_VALUE;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_VALUE program handle:"
                                               << program3D.toString();
    }
}

/*!
 * \qmlmethod void Context3D::attachShader(Canvas3DProgram program3D, Canvas3DShader shader3D)
 * Attaches the given \a shader3D object to the given \a program3D object.
 * Calling this method repeatedly on the same object has no side effects.
 */
/*!
 * \internal
 */
void CanvasContext::attachShader(QJSValue program3D, QJSValue shader3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ", shader:" << shader3D.toString()
                                         << ")";

    CanvasProgram *program = getAsProgram3D(program3D);
    CanvasShader *shader = getAsShader3D(shader3D);

    if (!program) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): Invalid program handle "
                                               << program3D.toString();
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    if (!shader) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): Invalid shader handle "
                                               << shader3D.toString();
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    if (!checkParent(program, __FUNCTION__) || !checkParent(shader, __FUNCTION__))
        return;

    program->attach(shader);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod list<Canvas3DShader> Context3D::getAttachedShaders(Canvas3DProgram program3D)
 * Returns the list of shaders currently attached to the given \a program3D.
 */
/*!
 * \internal
 */
QJSValue CanvasContext::getAttachedShaders(QJSValue program3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ")";

    int index = 0;

    CanvasProgram *program = getAsProgram3D(program3D);

    if (!program) {
        m_error |= CANVAS_INVALID_VALUE;
        return QJSValue(QJSValue::NullValue);
    }

    if (!checkParent(program, __FUNCTION__))
        return QJSValue(QJSValue::NullValue);

    QList<CanvasShader *> shaders = program->attachedShaders();

    QJSValue shaderList = m_engine->newArray(shaders.count());

    for (QList<CanvasShader *>::const_iterator iter = shaders.constBegin();
         iter != shaders.constEnd(); iter++) {
        CanvasShader *shader = *iter;
        shaderList.setProperty(index++, m_engine->newQObject((CanvasShader *)shader));
    }

    return shaderList;
}


/*!
 * \qmlmethod void Context3D::detachShader(Canvas3DProgram program, Canvas3DShader shader)
 * Detaches given shader object from given program object.
 * \a program3D specifies the program object from which to detach the shader.
 * \a shader3D specifies the shader object to detach.
 */
/*!
 * \internal
 */
void CanvasContext::detachShader(QJSValue program3D, QJSValue shader3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ", shader:" << shader3D.toString()
                                         << ")";

    CanvasProgram *program = getAsProgram3D(program3D);
    CanvasShader *shader = getAsShader3D(shader3D);

    if (!program) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): Invalid program handle "
                                               << program3D.toString();
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    if (!shader) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): Invalid shader handle "
                                               << shader3D.toString();
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    if (!checkParent(program, __FUNCTION__) || !checkParent(shader, __FUNCTION__))
        return;

    program->detach(shader);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::linkProgram(Canvas3DProgram program3D)
 * Links the given program object.
 * \a program3D specifies the program to be linked.
 */
/*!
 * \internal
 */
void CanvasContext::linkProgram(QJSValue program3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ")";

    CanvasProgram *program = getAsProgram3D(program3D);

    if (!program || !checkParent(program, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    program->link();
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::lineWidth(float width)
 * Specifies the width of rasterized lines.
 * \a width specifies the width to be used when rasterizing lines. Initial value is \c{1.0}.
 */
/*!
 * \internal
 */
void CanvasContext::lineWidth(float width)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(width:" << width
                                         << ")";
    glLineWidth(width);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::polygonOffset(float factor, float units)
 * Sets scale and units used to calculate depth values.
 * \a factor specifies the scale factor that is used to create a variable depth offset for each
 * polygon. Initial value is \c{0.0}.
 * \a units gets multiplied by an implementation-specific value to create a constant depth offset.
 * Initial value is \c{0.0}.
 */
/*!
 * \internal
 */
void CanvasContext::polygonOffset(float factor, float units)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(factor:" << factor
                                         << ", units:" << units
                                         << ")";
    glPolygonOffset(factor, units);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::pixelStorei(glEnums pname, int param)
 * Set the pixel storage modes.
 * \a pname specifies the name of the parameter to be set. \c {Context3D.PACK_ALIGNMENT} affects the
 * packing of pixel data into memory. \c {Context3D.UNPACK_ALIGNMENT} affects the unpacking of pixel
 * data from memory. \c {Context3D.UNPACK_FLIP_Y_WEBGL} is initially \c false, but once set, in any
 * subsequent calls to \l texImage2D or \l texSubImage2D, the source data is flipped along the
 * vertical axis. \c {Context3D.UNPACK_PREMULTIPLY_ALPHA_WEBGL} is initially \c false, but once set,
 * in any subsequent calls to \l texImage2D or \l texSubImage2D, the alpha channel of the source
 * data, is multiplied into the color channels during the data transfer. Initial value is \c false
 * and any non-zero value is interpreted as \c true.
 *
 * \a param specifies the value that \a pname is set to.
 */
/*!
 * \internal
 */
void CanvasContext::pixelStorei(glEnums pname, int param)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(pname:" << glEnumToString(pname)
                                         << ", param:" << param
                                         << ")";

    switch (pname) {
    case UNPACK_FLIP_Y_WEBGL:
        m_unpackFlipYEnabled = (param != 0);
        break;
    case UNPACK_PREMULTIPLY_ALPHA_WEBGL:
        m_unpackPremultiplyAlphaEnabled = (param != 0);
        break;
    case UNPACK_COLORSPACE_CONVERSION_WEBGL:
        // Intentionally ignored
        break;
    default:
        glPixelStorei(GLenum(pname), param);
        logAllGLErrors(__FUNCTION__);
        break;
    }
}

/*!
 * \qmlmethod void Context3D::hint(glEnums target, glEnums mode)
 * Set implementation-specific hints.
 * \a target \c Context3D.GENERATE_MIPMAP_HINT is accepted.
 * \a mode \c{Context3D.FASTEST}, \c{Context3D.NICEST}, and \c{Context3D.DONT_CARE} are accepted.
 */
/*!
 * \internal
 */
void CanvasContext::hint(glEnums target, glEnums mode)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ",mode:" << glEnumToString(mode) << ")";
    switch (target) {
    case FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
        if (m_standardDerivatives) {
            glHint(GLenum(target), GLenum(mode));
            logAllGLErrors(__FUNCTION__);
        } else {
            m_error |= CANVAS_INVALID_ENUM;
        }
        break;
    default:
        glHint(GLenum(target), GLenum(mode));
        logAllGLErrors(__FUNCTION__);
        break;
    }
}

/*!
 * \qmlmethod void Context3D::enable(glEnums cap)
 * Enable server side GL capabilities.
 * \a cap specifies a constant indicating a GL capability.
 */
/*!
 * \internal
 */
void CanvasContext::enable(glEnums cap)
{
    QString str = QString(__FUNCTION__);
    str += QStringLiteral("(cap:")
            + glEnumToString(cap)
            + QStringLiteral(")");

    qCDebug(canvas3drendering).nospace() << str;
    glEnable(cap);
    logAllGLErrors(str);
}

/*!
 * \qmlmethod bool Context3D::isEnabled(glEnums cap)
 * Returns whether a capability is enabled.
 * \a cap specifies a constant indicating a GL capability.
 */
/*!
 * \internal
 */
bool CanvasContext::isEnabled(glEnums cap)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(cap:" << glEnumToString(cap)
                                         << ")";
    return glIsEnabled(cap);
}

/*!
 * \qmlmethod void Context3D::disable(glEnums cap)
 * Disable server side GL capabilities.
 * \a cap specifies a constant indicating a GL capability.
 */
/*!
 * \internal
 */
void CanvasContext::disable(glEnums cap)
{
    QString str = QString(__FUNCTION__);
    str += QStringLiteral("(cap:")
            + glEnumToString(cap)
            + QStringLiteral(")");

    qCDebug(canvas3drendering).nospace() << str;
    glDisable(cap);
    logAllGLErrors(str);
}

/*!
 * \qmlmethod void Context3D::blendColor(float red, float green, float blue, float alpha)
 * Set the blend color.
 * \a red, \a green, \a blue and \a alpha specify the components of \c{Context3D.BLEND_COLOR}.
 */
/*!
 * \internal
 */
void CanvasContext::blendColor(float red, float green, float blue, float alpha)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         <<  "(red:" << red
                                          << ", green:" << green
                                          << ", blue:" << blue
                                          << ", alpha:" << alpha
                                          << ")";
    glBlendColor(red, green, blue, alpha);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::blendEquation(glEnums mode)
 * Sets the equation used for both the RGB blend equation. The alpha blend equation
 * \a mode specifies how source and destination colors are to be combined. Must be
 * \c{Context3D.FUNC_ADD}, \c{Context3D.FUNC_SUBTRACT} or \c{Context3D.FUNC_REVERSE_SUBTRACT}.
 */
/*!
 * \internal
 */
void CanvasContext::blendEquation(glEnums mode)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(mode:" << glEnumToString(mode)
                                         << ")";
    glBlendEquation(GLenum(mode));
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::blendEquationSeparate(glEnums modeRGB, glEnums modeAlpha)
 * Set the RGB blend equation and the alpha blend equation separately.
 * \a modeRGB specifies how the RGB components of the source and destination colors are to be
 * combined. Must be \c{Context3D.FUNC_ADD}, \c{Context3D.FUNC_SUBTRACT} or
 * \c{Context3D.FUNC_REVERSE_SUBTRACT}.
 * \a modeAlpha specifies how the alpha component of the source and destination colors are to be
 * combined. Must be \c{Context3D.FUNC_ADD}, \c{Context3D.FUNC_SUBTRACT}, or
 * \c{Context3D.FUNC_REVERSE_SUBTRACT}.
 */
/*!
 * \internal
 */
void CanvasContext::blendEquationSeparate(glEnums modeRGB, glEnums modeAlpha)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(modeRGB:" << glEnumToString(modeRGB)
                                         << ", modeAlpha:" << glEnumToString(modeAlpha)
                                         << ")";
    glBlendEquationSeparate(GLenum(modeRGB), GLenum(modeAlpha));
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::blendFunc(glEnums sfactor, glEnums dfactor)
 * Sets the pixel arithmetic.
 * \a sfactor specifies how the RGBA source blending factors are computed. Must be
 * \c{Context3D.ZERO}, \c{Context3D.ONE}, \c{Context3D.SRC_COLOR},
 * \c{Context3D.ONE_MINUS_SRC_COLOR},
 * \c{Context3D.DST_COLOR}, \c{Context3D.ONE_MINUS_DST_COLOR}, \c{Context3D.SRC_ALPHA},
 * \c{Context3D.ONE_MINUS_SRC_ALPHA}, \c{Context3D.DST_ALPHA}, \c{Context3D.ONE_MINUS_DST_ALPHA},
 * \c{Context3D.CONSTANT_COLOR}, \c{Context3D.ONE_MINUS_CONSTANT_COLOR},
 * \c{Context3D.CONSTANT_ALPHA},
 * \c{Context3D.ONE_MINUS_CONSTANT_ALPHA} or \c{Context3D.SRC_ALPHA_SATURATE}. Initial value is
 * \c{Context3D.ONE}.
 * \a dfactor Specifies how the RGBA destination blending factors are computed. Must be
 * \c{Context3D.ZERO}, \c{Context3D.ONE}, \c{Context3D.SRC_COLOR},
 * \c{Context3D.ONE_MINUS_SRC_COLOR},
 * \c{Context3D.DST_COLOR}, \c{Context3D.ONE_MINUS_DST_COLOR}, \c{Context3D.SRC_ALPHA},
 * \c{Context3D.ONE_MINUS_SRC_ALPHA}, \c{Context3D.DST_ALPHA}, \c{Context3D.ONE_MINUS_DST_ALPHA},
 * \c{Context3D.CONSTANT_COLOR}, \c{Context3D.ONE_MINUS_CONSTANT_COLOR},
 * \c{Context3D.CONSTANT_ALPHA} or
 * \c{Context3D.ONE_MINUS_CONSTANT_ALPHA}. Initial value is \c{Context3D.ZERO}.
 */
/*!
 * \internal
 */
void CanvasContext::blendFunc(glEnums sfactor, glEnums dfactor)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(sfactor:" << glEnumToString(sfactor)
                                         << ", dfactor:" << glEnumToString(dfactor)
                                         << ")";

    if (((sfactor == CONSTANT_COLOR || sfactor == ONE_MINUS_CONSTANT_COLOR)
         && (dfactor == CONSTANT_ALPHA || dfactor == ONE_MINUS_CONSTANT_ALPHA))
            || ((dfactor == CONSTANT_COLOR || dfactor == ONE_MINUS_CONSTANT_COLOR)
                && (sfactor == CONSTANT_ALPHA || sfactor == ONE_MINUS_CONSTANT_ALPHA))) {
        m_error |= CANVAS_INVALID_OPERATION;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_OPERATION illegal combination";
        return;
    }

    glBlendFunc(GLenum(sfactor), GLenum(dfactor));
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::blendFuncSeparate(glEnums srcRGB, glEnums dstRGB, glEnums srcAlpha, glEnums dstAlpha)
 * Sets the pixel arithmetic for RGB and alpha components separately.
 * \a srcRGB specifies how the RGB source blending factors are computed. Must be \c{Context3D.ZERO},
 * \c{Context3D.ONE}, \c{Context3D.SRC_COLOR}, \c{Context3D.ONE_MINUS_SRC_COLOR},
 * \c{Context3D.DST_COLOR},
 * \c{Context3D.ONE_MINUS_DST_COLOR}, \c{Context3D.SRC_ALPHA}, \c{Context3D.ONE_MINUS_SRC_ALPHA},
 * \c{Context3D.DST_ALPHA}, \c{Context3D.ONE_MINUS_DST_ALPHA}, \c{Context3D.CONSTANT_COLOR},
 * \c{Context3D.ONE_MINUS_CONSTANT_COLOR}, \c{Context3D.CONSTANT_ALPHA},
 * \c{Context3D.ONE_MINUS_CONSTANT_ALPHA} or \c{Context3D.SRC_ALPHA_SATURATE}. Initial value is
 * \c{Context3D.ONE}.
 * \a dstRGB Specifies how the RGB destination blending factors are computed. Must be
 * \c{Context3D.ZERO}, \c{Context3D.ONE}, \c{Context3D.SRC_COLOR}, \c{Context3D.ONE_MINUS_SRC_COLOR},
 * \c{Context3D.DST_COLOR}, \c{Context3D.ONE_MINUS_DST_COLOR}, \c{Context3D.SRC_ALPHA},
 * \c{Context3D.ONE_MINUS_SRC_ALPHA}, \c{Context3D.DST_ALPHA}, \c{Context3D.ONE_MINUS_DST_ALPHA},
 * \c{Context3D.CONSTANT_COLOR}, \c{Context3D.ONE_MINUS_CONSTANT_COLOR},
 * \c{Context3D.CONSTANT_ALPHA} or
 * \c{Context3D.ONE_MINUS_CONSTANT_ALPHA}. Initial value is \c{Context3D.ZERO}.
 * \a srcAlpha specifies how the alpha source blending factors are computed. Must be
 * \c{Context3D.ZERO}, \c{Context3D.ONE}, \c{Context3D.SRC_COLOR},
 * \c{Context3D.ONE_MINUS_SRC_COLOR},
 * \c{Context3D.DST_COLOR}, \c{Context3D.ONE_MINUS_DST_COLOR}, \c{Context3D.SRC_ALPHA},
 * \c{Context3D.ONE_MINUS_SRC_ALPHA}, \c{Context3D.DST_ALPHA}, \c{Context3D.ONE_MINUS_DST_ALPHA},
 * \c{Context3D.CONSTANT_COLOR}, \c{Context3D.ONE_MINUS_CONSTANT_COLOR},
 * \c{Context3D.CONSTANT_ALPHA},
 * \c{Context3D.ONE_MINUS_CONSTANT_ALPHA} or \c{Context3D.SRC_ALPHA_SATURATE}. Initial value is
 * \c{Context3D.ONE}.
 * \a dstAlpha Specifies how the alpha destination blending factors are computed. Must be
 * \c{Context3D.ZERO}, \c{Context3D.ONE}, \c{Context3D.SRC_COLOR},
 * \c{Context3D.ONE_MINUS_SRC_COLOR},
 * \c{Context3D.DST_COLOR}, \c{Context3D.ONE_MINUS_DST_COLOR}, \c{Context3D.SRC_ALPHA},
 * \c{Context3D.ONE_MINUS_SRC_ALPHA}, \c{Context3D.DST_ALPHA}, \c{Context3D.ONE_MINUS_DST_ALPHA},
 * \c{Context3D.CONSTANT_COLOR}, \c{Context3D.ONE_MINUS_CONSTANT_COLOR},
 * \c{Context3D.CONSTANT_ALPHA} or
 * \c{Context3D.ONE_MINUS_CONSTANT_ALPHA}. Initial value is \c{Context3D.ZERO}.
 */
/*!
 * \internal
 */
void CanvasContext::blendFuncSeparate(glEnums srcRGB, glEnums dstRGB, glEnums srcAlpha,
                                      glEnums dstAlpha)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(srcRGB:" << glEnumToString(srcRGB)
                                         << ", dstRGB:" << glEnumToString(dstRGB)
                                         << ", srcAlpha:" << glEnumToString(srcAlpha)
                                         << ", dstAlpha:" << glEnumToString(dstAlpha)
                                         << ")";

    if (((srcRGB == CONSTANT_COLOR || srcRGB == ONE_MINUS_CONSTANT_COLOR )
         && (dstRGB == CONSTANT_ALPHA || dstRGB == ONE_MINUS_CONSTANT_ALPHA ))
            || ((dstRGB == CONSTANT_COLOR || dstRGB == ONE_MINUS_CONSTANT_COLOR )
                && (srcRGB == CONSTANT_ALPHA || srcRGB == ONE_MINUS_CONSTANT_ALPHA ))) {
        m_error |= CANVAS_INVALID_OPERATION;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_OPERATION illegal combination";
        return;
    }

    glBlendFuncSeparate(GLenum(srcRGB), GLenum(dstRGB), GLenum(srcAlpha), GLenum(dstAlpha));
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod variant Context3D::getProgramParameter(Canvas3DProgram program3D, glEnums paramName)
 * Return the value for the passed \a paramName given the passed \a program3D. The type returned is
 * the natural type for the requested paramName.
 * \a paramName must be \c{Context3D.DELETE_STATUS}, \c{Context3D.LINK_STATUS},
 * \c{Context3D.VALIDATE_STATUS}, \c{Context3D.ATTACHED_SHADERS}, \c{Context3D.ACTIVE_ATTRIBUTES} or
 * \c{Context3D.ACTIVE_UNIFORMS}.
 */
/*!
 * \internal
 */
QJSValue CanvasContext::getProgramParameter(QJSValue program3D, glEnums paramName)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ", paramName:" << glEnumToString(paramName)
                                         << ")";

    CanvasProgram *program = getAsProgram3D(program3D);

    if (!program || !checkParent(program, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return QJSValue(QJSValue::NullValue);
    }

    switch(paramName) {
    case DELETE_STATUS:
        // Intentional flow through
    case LINK_STATUS:
        // Intentional flow through
    case VALIDATE_STATUS: {
        GLint value = 0;
        glGetProgramiv(program->id(), GLenum(paramName), &value);
        logAllGLErrors(__FUNCTION__);
        qCDebug(canvas3drendering).nospace() << "    getProgramParameter returns " << value;
        return QJSValue(bool(value));
    }
    case ATTACHED_SHADERS:
        // Intentional flow through
    case ACTIVE_ATTRIBUTES:
        // Intentional flow through
    case ACTIVE_UNIFORMS: {
        GLint value = 0;
        glGetProgramiv(program->id(), GLenum(paramName), &value);
        logAllGLErrors(__FUNCTION__);
        qCDebug(canvas3drendering).nospace() << "    getProgramParameter returns " << value;
        return QJSValue(value);
    }
    default: {
        m_error |= CANVAS_INVALID_ENUM;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_ENUM illegal parameter name ";
        return QJSValue(QJSValue::NullValue);
    }
    }
}

/*!
 * \qmlmethod Canvas3DShader Context3D::createShader(glEnums type)
 * Creates a shader of \a type. Must be either \c Context3D.VERTEX_SHADER or
 * \c{Context3D.FRAGMENT_SHADER}.
 */
/*!
 * \internal
 */
QJSValue CanvasContext::createShader(glEnums type)
{
    switch (type) {
    case VERTEX_SHADER:
        qCDebug(canvas3drendering).nospace() << "Context3D::createShader(VERTEX_SHADER)";
        return m_engine->newQObject(new CanvasShader(QOpenGLShader::Vertex, this));
    case FRAGMENT_SHADER:
        qCDebug(canvas3drendering).nospace() << "Context3D::createShader(FRAGMENT_SHADER)";
        return m_engine->newQObject(new CanvasShader(QOpenGLShader::Fragment, this));
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:unknown shader type:"
                                               << glEnumToString(type);
        m_error |= CANVAS_INVALID_ENUM;
        return m_engine->newObject();
    }
}

/*!
 * \qmlmethod bool Context3D::isShader(Object anyObject)
 * Returns true if the given object is a valid Canvas3DShader object.
 * \a anyObject is the object that is to be verified as a valid shader.
 */
/*!
 * \internal
 */
bool CanvasContext::isShader(QJSValue anyObject)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(anyObject:" << anyObject.toString()
                                         << ")";

    CanvasShader *shader3D = getAsShader3D(anyObject);
    if (!shader3D || !checkParent(shader3D, __FUNCTION__))
        return false;

    return glIsShader(shader3D->id());
}

/*!
 * \internal
 */
CanvasShader *CanvasContext::getAsShader3D(QJSValue shader3D, bool deadOrAlive) const
{
    if (!isOfType(shader3D, "QtCanvas3D::CanvasShader"))
        return 0;

    CanvasShader *shader = static_cast<CanvasShader *>(shader3D.toQObject());
    if (!deadOrAlive && !shader->isAlive())
        return 0;

    return shader;
}

/*!
 * \qmlmethod void Context3D::deleteShader(Canvas3DShader shader)
 * Deletes the given shader as if by calling \c{glDeleteShader()}.
 * Calling this method repeatedly on the same object has no side effects.
 * \a shader is the Canvas3DShader to be deleted.
 */
/*!
 * \internal
 */
void CanvasContext::deleteShader(QJSValue shader3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::"
                                         << __FUNCTION__
                                         << "(shader:" << shader3D.toString()
                                         << ")";

    CanvasShader *shader = getAsShader3D(shader3D, true);

    if (shader) {
        if (!checkParent(shader, __FUNCTION__))
            return;
        shader->del();
        logAllGLErrors(__FUNCTION__);
    } else {
        m_error |= CANVAS_INVALID_VALUE;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:"
                                               << "Invalid shader handle:" << shader3D.toString();
    }
}

/*!
 * \qmlmethod void Context3D::shaderSource(Canvas3DShader shader, string shaderSource)
 * Replaces the shader source code in the given shader object.
 * \a shader specifies the shader object whose source code is to be replaced.
 * \a shaderSource specifies the source code to be loaded in to the shader.
 */
/*!
 * \internal
 */
void CanvasContext::shaderSource(QJSValue shader3D, const QString &shaderSource)
{
    QString modSource = "#version 120 \n#define precision \n"+ shaderSource;

    if (m_isOpenGLES2)
        modSource = shaderSource;

    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(shader:" << shader3D.toString()
                                         << ", shaderSource"
                                         << ")" << endl << modSource << endl;

    CanvasShader *shader = getAsShader3D(shader3D);
    if (!shader) {
        m_error |= CANVAS_INVALID_OPERATION;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_OPERATION:"
                                               << "Invalid shader handle:" << shader3D.toString();
        return;
    }
    if (!checkParent(shader, __FUNCTION__))
        return;

    shader->setSourceCode(modSource);
    logAllGLErrors(__FUNCTION__);
}


/*!
 * \qmlmethod string Context3D::getShaderSource(Canvas3DShader shader)
 * Returns the source code string from the \a shader object.
 */
/*!
 * \internal
 */
QJSValue CanvasContext::getShaderSource(QJSValue shader3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(shader:" << shader3D.toString()
                                         << ")";

    CanvasShader *shader = getAsShader3D(shader3D);
    if (!shader) {
        m_error |= CANVAS_INVALID_OPERATION;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_OPERATION:"
                                               << "Invalid shader handle:" << shader3D.toString();
        return m_engine->newObject();
    }
    if (!checkParent(shader, __FUNCTION__))
        return false;

    return QJSValue(QString(shader->qOGLShader()->sourceCode()));
}

/*!
 * \qmlmethod void Context3D::compileShader(Canvas3DShader shader)
 * Compiles the given \a shader object.
 */
/*!
 * \internal
 */
void CanvasContext::compileShader(QJSValue shader3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(shader:" << shader3D.toString()
                                         << ")";
    CanvasShader *shader = getAsShader3D(shader3D);
    if (!shader) {
        m_error |= CANVAS_INVALID_OPERATION;
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_OPERATION:"
                                               << "Invalid shader handle:" << shader3D.toString();
        return;
    }
    if (!checkParent(shader, __FUNCTION__))
        return;

    shader->qOGLShader()->compileSourceCode(shader->sourceCode());
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \internal
 */
CanvasUniformLocation *CanvasContext::getAsUniformLocation3D(QJSValue anyObject) const
{
    if (!isOfType(anyObject, "QtCanvas3D::CanvasUniformLocation"))
        return 0;

    CanvasUniformLocation *uniformLocation =
            static_cast<CanvasUniformLocation *>(anyObject.toQObject());

    // TODO: Should uniform locations be killed and checked for "isAlive" when program is
    // deleted?
    return uniformLocation;
}

/*!
 * \qmlmethod void Context3D::uniform1i(Canvas3DUniformLocation location3D, int x)
 * Sets the single integer value given in \a x to the given uniform \a location3D.
 */
/*!
 * \internal
 */
void CanvasContext::uniform1i(QJSValue location3D, int x)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D.toString()
                                         << ", x:" << x
                                         << ")";
    CanvasUniformLocation *locationObj = getAsUniformLocation3D(location3D);

    if (!locationObj || !checkParent(locationObj, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    glUniform1i(locationObj->id(), x);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::uniform1iv(Canvas3DUniformLocation location3D, Int32Array array)
 * Sets the integer array given in \a array to the given uniform \a location3D.
 */
/*!
 * \internal
 */
void CanvasContext::uniform1iv(QJSValue location3D, QJSValue array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D.toString()
                                         << ", array:" << array.toString()
                                         << ")";

    CanvasUniformLocation *locationObj = getAsUniformLocation3D(location3D);

    if (!locationObj || !checkParent(locationObj, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    // Check if we have a JavaScript array
    if (array.isArray()) {
        uniform1iva(locationObj, array.toVariant().toList());
        return;
    }

    int arrayLen = 0;
    uchar *uniformData = getTypedArrayAsRawDataPtr(array, arrayLen,
                                                   QV4::Heap::TypedArray::Int32Array);

    if (!uniformData) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    arrayLen /= 4; // get value count
    glUniform1iv(locationObj->id(), arrayLen, (int *)uniformData);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::uniform1f(Canvas3DUniformLocation location3D, float x)
 * Sets the single float value given in \a x to the given uniform \a location3D.
 */
/*!
 * \internal
 */
void CanvasContext::uniform1f(QJSValue location3D, float x)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D.toString()
                                         << ", x:" << x
                                         << ")";

    CanvasUniformLocation *locationObj = getAsUniformLocation3D(location3D);
    if (!locationObj || !checkParent(locationObj, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    glUniform1f(locationObj->id(), x);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::uniform1fvt(Canvas3DUniformLocation location3D, Object array)
 * Sets the float array given in \a array to the given uniform \a location3D. \a array must be
 * a JavaScript \c Array object or a \c Float32Array object.
 */
/*!
 * \internal
 */
void CanvasContext::uniform1fv(QJSValue location3D, QJSValue array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D.toString()
                                         << ", array:" << array.toString()
                                         << ")";

    CanvasUniformLocation *locationObj = getAsUniformLocation3D(location3D);
    if (!locationObj || !checkParent(locationObj, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    // Check if we have a JavaScript array
    if (array.isArray()) {
        uniform1fva(locationObj, array.toVariant().toList());
        return;
    }

    int arrayLen = 0;
    uchar *uniformData = getTypedArrayAsRawDataPtr(array, arrayLen,
                                                   QV4::Heap::TypedArray::Float32Array);

    if (!uniformData) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    arrayLen /= 4; // get value count
    glUniform1fv(locationObj->id(), arrayLen, (float *)uniformData);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::uniform2f(Canvas3DUniformLocation location3D, float x, float y)
 * Sets the two float values given in \a x and \a y to the given uniform \a location3D.
 */
/*!
 * \internal
 */
void CanvasContext::uniform2f(QJSValue location3D, float x, float y)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D.toString()
                                         << ", x:" << x
                                         << ", y:" << y
                                         << ")";

    CanvasUniformLocation *locationObj = getAsUniformLocation3D(location3D);

    if (!locationObj || !checkParent(locationObj, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    glUniform2f(locationObj->id(), x, y);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::uniform2fv(Canvas3DUniformLocation location3D, Float32Array array)
 * Sets the float array given in \a array to the given uniform \a location3D.
 */
/*!
 * \internal
 */
void CanvasContext::uniform2fv(QJSValue location3D, QJSValue array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D.toString()
                                         << ", array:" << array.toString()
                                         << ")";

    CanvasUniformLocation *locationObj = getAsUniformLocation3D(location3D);
    if (!locationObj || !checkParent(locationObj, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    // Check if we have a JavaScript array
    if (array.isArray()) {
        uniform2fva(locationObj, array.toVariant().toList());
        return;
    }

    int arrayLen = 0;
    uchar *uniformData = getTypedArrayAsRawDataPtr(array, arrayLen,
                                                   QV4::Heap::TypedArray::Float32Array);

    if (!uniformData) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    arrayLen /= (4 * 2); // get value count
    glUniform2fv(locationObj->id(), arrayLen, (float *)uniformData);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::uniform2i(Canvas3DUniformLocation location3D, int x, int y)
 * Sets the two integer values given in \a x and \a y to the given uniform \a location3D.
 */
/*!
 * \internal
 */
void CanvasContext::uniform2i(QJSValue location3D, int x, int y)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D.toString()
                                         << ", x:" << x
                                         << ", y:" << y
                                         << ")";

    CanvasUniformLocation *locationObj = getAsUniformLocation3D(location3D);

    if (!locationObj || !checkParent(locationObj, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    glUniform2i(locationObj->id(), x, y);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::uniform2iv(Canvas3DUniformLocation location3D, Int32Array array)
 * Sets the integer array given in \a array to the given uniform \a location3D.
 */
/*!
 * \internal
 */
void CanvasContext::uniform2iv(QJSValue location3D, QJSValue array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D.toString()
                                         << ", array:" << array.toString()
                                         << ")";

    CanvasUniformLocation *locationObj = getAsUniformLocation3D(location3D);
    if (!locationObj || !checkParent(locationObj, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    // Check if we have a JavaScript array
    if (array.isArray()) {
        uniform2iva(locationObj, array.toVariant().toList());
        return;
    }

    int arrayLen = 0;
    uchar *uniformData = getTypedArrayAsRawDataPtr(array, arrayLen,
                                                   QV4::Heap::TypedArray::Int32Array);

    if (!uniformData) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    arrayLen /= (4 * 2); // get value count
    glUniform2iv(locationObj->id(), arrayLen, (int *)uniformData);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::uniform3f(Canvas3DUniformLocation location3D, float x, float y, float z)
 * Sets the three float values given in \a x , \a y and \a z to the given uniform \a location3D.
 */
/*!
 * \internal
 */
void CanvasContext::uniform3f(QJSValue location3D, float x, float y, float z)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D.toString()
                                         << ", x:" << x
                                         << ", y:" << y
                                         << ", z:" << z
                                         << ")";

    CanvasUniformLocation *locationObj = getAsUniformLocation3D(location3D);
    if (!locationObj || !checkParent(locationObj, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    glUniform3f(locationObj->id(), x, y, z);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::uniform3fv(Canvas3DUniformLocation location3D, Float32Array array)
 * Sets the float array given in \a array to the given uniform \a location3D.
 */
/*!
 * \internal
 */
void CanvasContext::uniform3fv(QJSValue location3D, QJSValue array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D.toString()
                                         << ", array:" << array.toString()
                                         << ")";

    CanvasUniformLocation *locationObj = getAsUniformLocation3D(location3D);
    if (!locationObj || !checkParent(locationObj, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    // Check if we have a JavaScript array
    if (array.isArray()) {
        uniform3fva(locationObj, array.toVariant().toList());
        return;
    }

    int arrayLen = 0;
    uchar *uniformData = getTypedArrayAsRawDataPtr(array, arrayLen,
                                                   QV4::Heap::TypedArray::Float32Array);

    if (!uniformData) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    arrayLen /= (4 * 3); // get value count
    glUniform3fv(locationObj->id(), arrayLen, (float *)uniformData);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::uniform3i(Canvas3DUniformLocation location3D, int x, int y, int z)
 * Sets the three integer values given in \a x , \a y and \a z to the given uniform \a location3D.
 */
/*!
 * \internal
 */
void CanvasContext::uniform3i(QJSValue location3D, int x, int y, int z)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D.toString()
                                         << ", x:" << x
                                         << ", y:" << y
                                         << ", z:" << z
                                         << ")";
    CanvasUniformLocation *locationObj = getAsUniformLocation3D(location3D);
    if (!locationObj || !checkParent(locationObj, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    glUniform3i(locationObj->id(), x, y, z);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::uniform3iv(Canvas3DUniformLocation location3D, Int32Array array)
 * Sets the integer array given in \a array to the given uniform \a location3D.
 */
/*!
 * \internal
 */
void CanvasContext::uniform3iv(QJSValue location3D, QJSValue array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D.toString()
                                         << ", array:" << array.toString()
                                         << ")";

    CanvasUniformLocation *locationObj = getAsUniformLocation3D(location3D);
    if (!locationObj || !checkParent(locationObj, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    // Check if we have a JavaScript array
    if (array.isArray()) {
        uniform3iva(locationObj, array.toVariant().toList());
        return;
    }

    int arrayLen = 0;
    uchar *uniformData = getTypedArrayAsRawDataPtr(array, arrayLen,
                                                   QV4::Heap::TypedArray::Int32Array);

    if (!uniformData) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    arrayLen /= (4 * 3); // get value count
    glUniform3iv(locationObj->id(), arrayLen, (int *)uniformData);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::uniform4f(Canvas3DUniformLocation location3D, float x, float y, float z, float w)
 * Sets the four float values given in \a x , \a y , \a z and \a w to the given uniform \a location3D.
 */
/*!
 * \internal
 */
void CanvasContext::uniform4f(QJSValue location3D, float x, float y, float z, float w)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D.toString()
                                         << ", x:" << x
                                         << ", y:" << y
                                         << ", z:" << z
                                         << ", w:" << w
                                         << ")";
    CanvasUniformLocation *locationObj = getAsUniformLocation3D(location3D);
    if (!locationObj || !checkParent(locationObj, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    glUniform4f(locationObj->id(), x, y, z, w);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::uniform4fv(Canvas3DUniformLocation location3D, Float32Array array)
 * Sets the float array given in \a array to the given uniform \a location3D.
 */
/*!
 * \internal
 */
void CanvasContext::uniform4fv(QJSValue location3D, QJSValue array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D.toString()
                                         << ", array:" << array.toString()
                                         << ")";

    CanvasUniformLocation *locationObj = getAsUniformLocation3D(location3D);
    if (!locationObj || !checkParent(locationObj, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    // Check if we have a JavaScript array
    if (array.isArray()) {
        uniform4fva(locationObj, array.toVariant().toList());
        return;
    }

    int arrayLen = 0;
    uchar *uniformData = getTypedArrayAsRawDataPtr(array, arrayLen,
                                                   QV4::Heap::TypedArray::Float32Array);

    if (!uniformData) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    arrayLen /= (4 * 4); // get value count
    glUniform4fv(locationObj->id(), arrayLen, (float *)uniformData);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::uniform4i(Canvas3DUniformLocation location3D, int x, int y, int z, int w)
 * Sets the four integer values given in \a x , \a y , \a z and \a w to the given uniform
 * \a location3D.
 */
/*!
 * \internal
 */
void CanvasContext::uniform4i(QJSValue location3D, int x, int y, int z, int w)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D.toString()
                                         << ", x:" << x
                                         << ", y:" << y
                                         << ", z:" << z
                                         << ", w:" << w
                                         << ")";
    CanvasUniformLocation *locationObj = getAsUniformLocation3D(location3D);
    if (!locationObj || !checkParent(locationObj, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    glUniform4i(locationObj->id(), x, y, z, w);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::uniform4iv(Canvas3DUniformLocation location3D, Int32Array array)
 * Sets the integer array given in \a array to the given uniform \a location3D.
 */
/*!
 * \internal
 */
void CanvasContext::uniform4iv(QJSValue location3D, QJSValue array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D.toString()
                                         << ", array:" << array.toString()
                                         << ")";
    CanvasUniformLocation *locationObj = getAsUniformLocation3D(location3D);
    if (!locationObj || !checkParent(locationObj, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    // Check if we have a JavaScript array
    if (array.isArray()) {
        uniform4iva(locationObj, array.toVariant().toList());
        return;
    }

    int arrayLen = 0;
    uchar *uniformData = getTypedArrayAsRawDataPtr(array, arrayLen,
                                                   QV4::Heap::TypedArray::Int32Array);

    if (!uniformData) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    arrayLen /= (4 * 4); // get value count
    glUniform4iv(locationObj->id(), arrayLen, (int *)uniformData);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \internal
 */
void CanvasContext::uniform1fva(CanvasUniformLocation *location3D, QVariantList array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D
                                         << ", array:" << array
                                         << ")";

    float *arrayData = new float[array.length()];
    ArrayUtils::fillFloatArrayFromVariantList(array, arrayData);
    glUniform1fv(location3D->id(), array.count(), arrayData);
    logAllGLErrors(__FUNCTION__);
    delete [] arrayData;
}

/*!
 * \internal
 */
void CanvasContext::uniform2fva(CanvasUniformLocation *location3D, QVariantList array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D
                                         << ", array:" << array
                                         << ")";

    float *arrayData = new float[array.length()];
    ArrayUtils::fillFloatArrayFromVariantList(array, arrayData);
    glUniform2fv(location3D->id(), array.count() / 2, arrayData);
    logAllGLErrors(__FUNCTION__);
    delete [] arrayData;
}

/*!
 * \internal
 */
void CanvasContext::uniform3fva(CanvasUniformLocation *location3D, QVariantList array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D
                                         << ", array:" << array
                                         << ")";

    float *arrayData = new float[array.length()];
    ArrayUtils::fillFloatArrayFromVariantList(array, arrayData);
    glUniform3fv(location3D->id(), array.count() / 3, arrayData);
    logAllGLErrors(__FUNCTION__);
    delete [] arrayData;
}

/*!
 * \internal
 */
void CanvasContext::uniform4fva(CanvasUniformLocation *location3D, QVariantList array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D
                                         << ", array:" << array
                                         << ")";

    float *arrayData = new float[array.count()];
    ArrayUtils::fillFloatArrayFromVariantList(array, arrayData);
    glUniform4fv(location3D->id(), array.count() / 4, arrayData);
    logAllGLErrors(__FUNCTION__);
    delete [] arrayData;
}

/*!
 * \internal
 */
void CanvasContext::uniform1iva(CanvasUniformLocation *location3D, QVariantList array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D
                                         << ", array:" << array
                                         << ")";

    int *arrayData = new int[array.length()];
    ArrayUtils::fillIntArrayFromVariantList(array, arrayData);
    glUniform1iv(location3D->id(), array.count(), arrayData);
    logAllGLErrors(__FUNCTION__);
    delete [] arrayData;
}

/*!
 * \internal
 */
void CanvasContext::uniform2iva(CanvasUniformLocation *location3D, QVariantList array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D
                                         << ", array:" << array
                                         << ")";

    int *arrayData = new int[array.length()];
    ArrayUtils::fillIntArrayFromVariantList(array, arrayData);
    glUniform2iv(location3D->id(), array.count() / 2, arrayData);
    logAllGLErrors(__FUNCTION__);
    delete [] arrayData;

}

/*!
 * \internal
 */
void CanvasContext::uniform3iva(CanvasUniformLocation *location3D, QVariantList array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D
                                         << ", array:" << array
                                         << ")";

    int *arrayData = new int[array.length()];
    ArrayUtils::fillIntArrayFromVariantList(array, arrayData);
    glUniform3iv(location3D->id(), array.count() / 3, arrayData);
    logAllGLErrors(__FUNCTION__);
    delete [] arrayData;

}

/*!
 * \internal
 */
void CanvasContext::uniform4iva(CanvasUniformLocation *location3D, QVariantList array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(location3D:" << location3D
                                         << ", array:" << array
                                         << ")";

    int *arrayData = new int[array.length()];
    ArrayUtils::fillIntArrayFromVariantList(array, arrayData);
    glUniform4iv(location3D->id(), array.length() / 4, arrayData);
    logAllGLErrors(__FUNCTION__);
    delete [] arrayData;
}

/*!
 * \qmlmethod void Context3D::vertexAttrib1f(int indx, float x)
 * Sets the single float value given in \a x to the generic vertex attribute index specified
 * by \a indx.
 */
/*!
 * \internal
 */
void CanvasContext::vertexAttrib1f(unsigned int indx, float x)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(indx:" << indx
                                         << ", x:" << x
                                         << ")";
    glVertexAttrib1f(indx, x);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::vertexAttrib1fv(int indx, Float32Array array)
 * Sets the float array given in \a array to the generic vertex attribute index
 * specified by \a indx.
 */
/*!
 * \internal
 */
void CanvasContext::vertexAttrib1fv(unsigned int indx, QJSValue array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(indx:" << indx
                                         << ", array:" << array.toString()
                                         << ")";

    // Check if we have a JavaScript array
    if (array.isArray()) {
        vertexAttrib1fva(indx, array.toVariant().toList());
        return;
    }

    uchar *attribData = getTypedArrayAsRawDataPtr(array, QV4::Heap::TypedArray::Float32Array);

    if (!attribData) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    glVertexAttrib1fv(indx, (float *)attribData);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::vertexAttrib2f(int indx, float x, float y)
 * Sets the two float values given in \a x and \a y to the generic vertex attribute index
 * specified by \a indx.
 */
/*!
 * \internal
 */
void CanvasContext::vertexAttrib2f(unsigned int indx, float x, float y)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(indx:" << indx
                                         << ", x:" << x
                                         << ", y:" << y
                                         << ")";
    glVertexAttrib2f(indx, x, y);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::vertexAttrib2fv(int indx, Float32Array array)
 * Sets the float array given in \a array to the generic vertex attribute index
 * specified by \a indx.
 */
/*!
 * \internal
 */
void CanvasContext::vertexAttrib2fv(unsigned int indx, QJSValue array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(indx:" << indx
                                         << ", array:" << array.toString()
                                         << ")";

    // Check if we have a JavaScript array
    if (array.isArray()) {
        vertexAttrib2fva(indx, array.toVariant().toList());
        return;
    }

    uchar *attribData = getTypedArrayAsRawDataPtr(array, QV4::Heap::TypedArray::Float32Array);

    if (!attribData) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    glVertexAttrib2fv(indx, (float *)attribData);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::vertexAttrib3f(int indx, float x, float y, float z)
 * Sets the three float values given in \a x , \a y and \a z to the generic vertex attribute index
 * specified by \a indx.
 */
/*!
 * \internal
 */
void CanvasContext::vertexAttrib3f(unsigned int indx, float x, float y, float z)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(indx:" << indx
                                         << ", x:" << x
                                         << ", y:" << y
                                         << ", z:" << z
                                         << ")";
    glVertexAttrib3f(indx, x, y, z);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::vertexAttrib3fv(int indx, Float32Array array)
 * Sets the float array given in \a array to the generic vertex attribute index
 * specified by \a indx.
 */
/*!
 * \internal
 */
void CanvasContext::vertexAttrib3fv(unsigned int indx, QJSValue array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(indx:" << indx
                                         << ", array:" << array.toString()
                                         << ")";

    // Check if we have a JavaScript array
    if (array.isArray()) {
        vertexAttrib3fva(indx, array.toVariant().toList());
        return;
    }

    uchar *attribData = getTypedArrayAsRawDataPtr(array, QV4::Heap::TypedArray::Float32Array);

    if (!attribData) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    glVertexAttrib3fv(indx, (float *)attribData);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::vertexAttrib4f(int indx, float x, float y, float z, float w)
 * Sets the four float values given in \a x , \a y , \a z and \a w to the generic vertex attribute
 * index specified by \a indx.
 */
/*!
 * \internal
 */
void CanvasContext::vertexAttrib4f(unsigned int indx, float x, float y, float z, float w)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(indx:" << indx
                                         << ", x:" << x
                                         << ", y:" << y
                                         << ", z:" << z
                                         << ", w:" << w
                                         << ")";
    glVertexAttrib4f(indx, x, y, z, w);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::vertexAttrib4fv(int indx, Float32Array array)
 * Sets the float array given in \a array to the generic vertex attribute index
 * specified by \a indx.
 */
/*!
 * \internal
 */
void CanvasContext::vertexAttrib4fv(unsigned int indx, QJSValue array)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(indx:" << indx
                                         << ", array:" << array.toString()
                                         << ")";

    // Check if we have a JavaScript array
    if (array.isArray()) {
        vertexAttrib4fva(indx, array.toVariant().toList());
        return;
    }

    uchar *attribData = getTypedArrayAsRawDataPtr(array, QV4::Heap::TypedArray::Float32Array);

    if (!attribData) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    glVertexAttrib4fv(indx, (float *)attribData);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod int Context3D::getShaderParameter(Canvas3DShader shader, glEnums pname)
 * Returns the value of the passed \a pname for the given \a shader.
 * \a pname must be one of \c{Context3D.SHADER_TYPE}, \c Context3D.DELETE_STATUS and
 * \c{Context3D.COMPILE_STATUS}.
 */
/*!
 * \internal
 */
QJSValue CanvasContext::getShaderParameter(QJSValue shader3D, glEnums pname)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(shader:" << shader3D.toString()
                                         << ", pname:"<< glEnumToString(pname)
                                         << ")";
    CanvasShader *shader = getAsShader3D(shader3D);
    if (!shader) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_OPERATION:"
                                               <<"Invalid shader handle:" << shader3D.toString();
        m_error |= CANVAS_INVALID_OPERATION;
        return QJSValue(QJSValue::NullValue);
    }
    if (!checkParent(shader, __FUNCTION__))
        return QJSValue(QJSValue::NullValue);

    switch (pname) {
    case SHADER_TYPE: {
        GLint shaderType = 0;
        glGetShaderiv(shader->qOGLShader()->shaderId(), GL_SHADER_TYPE, &shaderType);
        logAllGLErrors(__FUNCTION__);
        return QJSValue(glEnums(shaderType));
    }
    case DELETE_STATUS: {
        bool isDeleted = !shader->isAlive();
        qCDebug(canvas3drendering).nospace() << "    getShaderParameter returns " << isDeleted;
        return (isDeleted ? QJSValue(bool(true)) : QJSValue(bool(false)));
    }
    case COMPILE_STATUS: {
        bool isCompiled = shader->qOGLShader()->isCompiled();
        qCDebug(canvas3drendering).nospace() << "    getShaderParameter returns " << isCompiled;
        return (isCompiled ? QJSValue(bool(true)) : QJSValue(bool(false)));
    }
    default: {
        qCWarning(canvas3drendering).nospace() << "getShaderParameter():UNSUPPORTED parameter name "
                                               << glEnumToString(pname);
        m_error |= CANVAS_INVALID_ENUM;
        return QJSValue(QJSValue::NullValue);
    }
    }
}

/*!
 * \qmlmethod Canvas3DBuffer Context3D::createBuffer()
 * Creates a Canvas3DBuffer object and initializes it with a buffer object name as if \c glGenBuffers()
 * was called.
 */
/*!
 * \internal
 */
QJSValue CanvasContext::createBuffer()
{
    CanvasBuffer *newBuffer = new CanvasBuffer(this);
    logAllGLErrors(__FUNCTION__);
    m_idToCanvasBufferMap[newBuffer->id()] = newBuffer;

    QJSValue value = m_engine->newQObject(newBuffer);
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << ":" << value.toString() << " = " << newBuffer;
    return value;
}

/*!
 * \qmlmethod Canvas3DUniformLocation Context3D::getUniformLocation(Canvas3DProgram program3D, string name)
 * Returns Canvas3DUniformLocation object that represents the location3D of a specific uniform variable
 * with the given \a name within the given \a program3D object.
 * Returns \c null if name doesn't correspond to a uniform variable.
 */
/*!
 * \internal
 */
QJSValue CanvasContext::getUniformLocation(QJSValue program3D, const QString &name)
{
    CanvasProgram *program = getAsProgram3D(program3D);
    if (!program) {
        qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                             << "(program3D:" << program3D.toString()
                                             << ", name:" << name
                                             << "):-1";
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << " WARNING:Invalid Canvas3DProgram reference " << program;
        m_error |= CANVAS_INVALID_OPERATION;
        return 0;
    }
    if (!checkParent(program, __FUNCTION__))
        return 0;

    int index = program->uniformLocation(name);
    logAllGLErrors(__FUNCTION__);
    if (index < 0) {
        return 0;
    }

    CanvasUniformLocation *location3D = new CanvasUniformLocation(index, this);
    location3D->setName(name);
    QJSValue value = m_engine->newQObject(location3D);
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ", name:" << value.toString()
                                         << "):" << location3D;

    return value;
}

/*!
 * \qmlmethod int Context3D::getAttribLocation(Canvas3DProgram program3D, string name)
 * Returns location3D of the given attribute variable \a name in the given \a program3D.
 */
/*!
 * \internal
 */
int CanvasContext::getAttribLocation(QJSValue program3D, const QString &name)
{
    CanvasProgram *program = getAsProgram3D(program3D);
    if (!program) {
        qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                             << "(program3D:" << program3D.toString()
                                             << ", name:" << name
                                             << "):-1";
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID Canvas3DProgram reference " << program;
        m_error |= CANVAS_INVALID_OPERATION;
        return -1;
    } else {
        if (!checkParent(program, __FUNCTION__))
            return -1;
        qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                             << "(program3D:" << program3D.toString()
                                             << ", name:" << name
                                             << "):" << program->attributeLocation(name);
    }

    return program->attributeLocation(name);
}

/*!
 * \qmlmethod void Context3D::bindAttribLocation(Canvas3DProgram program3D, int index, string name)
 * Binds the attribute \a index with the attribute variable \a name in the given \a program3D.
 */
/*!
 * \internal
 */
void CanvasContext::bindAttribLocation(QJSValue program3D, int index, const QString &name)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ", index:" << index
                                         << ", name:" << name
                                         << ")";

    CanvasProgram *program = getAsProgram3D(program3D);
    if (!program) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID Canvas3DProgram reference " << program;
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }
    if (!checkParent(program, __FUNCTION__))
        return;

    program->bindAttributeLocation(index, name);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::enableVertexAttribArray(int index)
 * Enables the vertex attribute array at \a index.
 */
/*!
 * \internal
 */
void CanvasContext::enableVertexAttribArray(int index)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(index:" << index
                                         << ")";
    glEnableVertexAttribArray(index);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::disableVertexAttribArray(int index)
 * Disables the vertex attribute array at \a index.
 */
/*!
 * \internal
 */
void CanvasContext::disableVertexAttribArray(int index)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(index:" << index
                                         << ")";
    glDisableVertexAttribArray(index);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::uniformMatrix2fv(Canvas3DUniformLocation location3D, bool transpose, Value array)
 * Converts the float array given in \a array to a 2x2 matrix and sets it to the given
 * uniform at \a uniformLocation. Applies \a transpose if set to \c{true}.
 */
/*!
 * \internal
 */
void CanvasContext::uniformMatrix2fv(QJSValue location3D, bool transpose, QJSValue array)
{
    uniformMatrixNfv(2, location3D, transpose, array);
}

/*!
 * \qmlmethod void Context3D::uniformMatrix3fv(Canvas3DUniformLocation location3D, bool transpose, Value array)
 * Converts the float array given in \a array to a 3x3 matrix and sets it to the given
 * uniform at \a uniformLocation. Applies \a transpose if set to \c{true}.
 */
/*!
 * \internal
 */
void CanvasContext::uniformMatrix3fv(QJSValue location3D, bool transpose, QJSValue array)
{
    uniformMatrixNfv(3, location3D, transpose, array);
}

/*!
 * \qmlmethod void Context3D::uniformMatrix4fv(Canvas3DUniformLocation location3D, bool transpose, Value array)
 * Converts the float array given in \a array to a 4x4 matrix and sets it to the given
 * uniform at \a uniformLocation. Applies \a transpose if set to \c{true}.
 */
/*!
 * \internal
 */
void CanvasContext::uniformMatrix4fv(QJSValue location3D, bool transpose, QJSValue array)
{
    uniformMatrixNfv(4, location3D, transpose, array);
}

/*!
 * \qmlmethod void Context3D::vertexAttribPointer(int indx, int size, glEnums type, bool normalized, int stride, long offset)
 * Sets the currently bound array buffer to the vertex attribute at the index passed at \a indx.
 * \a size is the number of components per attribute. \a stride specifies the byte offset between
 * consecutive vertex attributes. \a offset specifies the byte offset to the first vertex attribute
 * in the array. If int values should be normalized, set \a normalized to \c{true}.
 *
 * \a type specifies the element type and can be one of:
 * \list
 * \li \c{Context3D.BYTE}
 * \li \c{Context3D.UNSIGNED_BYTE}
 * \li \c{Context3D.SHORT}
 * \li \c{Context3D.UNSIGNED_SHORT}
 * \li \c{Context3D.FLOAT}
 * \endlist
 */
/*!
 * \internal
 */
void CanvasContext::vertexAttribPointer(int indx, int size, glEnums type,
                                        bool normalized, int stride, long offset)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(indx:" << indx
                                         << ", size: " << size
                                         << ", type:" << glEnumToString(type)
                                         << ", normalized:" << normalized
                                         << ", stride:" << stride
                                         << ", offset:" << offset
                                         << ")";

    if (!m_currentArrayBuffer) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_OPERATION:"
                                               << " No ARRAY_BUFFER currently bound";
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    if (offset < 0) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:"
                                               << "Offset must be positive, was "
                                               << offset;
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }

    if (stride > 255) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:"
                                               << "stride must be less than 255, was "
                                               << stride;
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }

    // Verify offset follows the rules of the spec
    switch (type) {
    case BYTE:
    case UNSIGNED_BYTE:
        break;
    case SHORT:
    case UNSIGNED_SHORT:
        if (offset % 2 != 0) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_OPERATION:"
                                                   << "offset with UNSIGNED_SHORT"
                                                   << "type must be multiple of 2";
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }
        if (stride % 2 != 0) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_OPERATION:"
                                                   << "stride with UNSIGNED_SHORT"
                                                   << "type must be multiple of 2";
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }
        break;
    case FLOAT:
        if (offset % 4 != 0) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_OPERATION:"
                                                   << "offset with FLOAT type "
                                                   << "must be multiple of 4";
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }
        if (stride % 4 != 0) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_OPERATION:"
                                                   << "stride with FLOAT type must "
                                                   << "be multiple of 4";
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:"
                                               << "Invalid type enumeration of "
                                               << glEnumToString(type);
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    glVertexAttribPointer(indx, size, GLenum(type), normalized, stride, (GLvoid *)offset);
    logAllGLErrors(__FUNCTION__);
}


/*!
 * \qmlmethod void Context3D::bufferData(glEnums target, long size, glEnums usage)
 * Sets the size of the \a target buffer to \a size. Target buffer must be either
 * \c{Context3D.ARRAY_BUFFER} or \c{Context3D.ELEMENT_ARRAY_BUFFER}. \a usage sets the usage pattern
 * of the data, and must be one of \c{Context3D.STREAM_DRAW}, \c{Context3D.STATIC_DRAW}, or
 * \c{Context3D.DYNAMIC_DRAW}.
 */
/*!
 * \internal
 */
void CanvasContext::bufferData(glEnums target, long size, glEnums usage)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", size:" << size
                                         << ", usage:" << glEnumToString(usage)
                                         << ")";

    switch (target) {
    case ARRAY_BUFFER:
        if (!m_currentArrayBuffer) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_OPERATION:"
                                                   << "called with no ARRAY_BUFFER bound";
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }
        break;
    case ELEMENT_ARRAY_BUFFER:
        if (!m_currentElementArrayBuffer) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_OPERATION:"
                                                   << "called with no ELEMENT_ARRAY_BUFFER bound";
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:Unknown target";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    glBufferData(GLenum(target), size, (GLvoid *) 0, GLenum(usage));
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::bufferData(glEnums target, value data, glEnums usage)
 * Writes the \a data held in \c{ArrayBufferView} or \c{ArrayBuffer} to the \a target buffer.
 * Target buffer must be either \c{Context3D.ARRAY_BUFFER } or \c{Context3D.ELEMENT_ARRAY_BUFFER}.
 * \a usage sets the usage pattern of the data, and must be one of \c{Context3D.STREAM_DRAW},
 * \c{Context3D.STATIC_DRAW}, or \c{Context3D.DYNAMIC_DRAW}.
 */
/*!
 * \internal
 */
void CanvasContext::bufferData(glEnums target, QJSValue data, glEnums usage)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", data:" << data.toString()
                                         << ", usage:" << glEnumToString(usage)
                                         << ")";

    if (data.isNull()) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_VALUE:Called with null data";
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }

    if (target != ARRAY_BUFFER && target != ELEMENT_ARRAY_BUFFER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:Target must be either ARRAY_BUFFER"
                                               << " or ELEMENT_ARRAY_BUFFER.";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    int arrayLen = 0;
    uchar *srcData = getTypedArrayAsRawDataPtr(data, arrayLen);

    if (!srcData)
        srcData = getArrayBufferAsRawDataPtr(data, arrayLen);

    if (srcData) {
        glBufferData(GLenum(target), arrayLen, (GLvoid *)srcData, GLenum(usage));
        logAllGLErrors(__FUNCTION__);
    } else {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:data must be either"
                                               << " TypedArray or ArrayBuffer";
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }
}

/*!
 * \qmlmethod void Context3D::bufferSubData(glEnums target, int offset, value data)
 * Writes the \a data held in \c{ArrayBufferView} or \c{ArrayBuffer} starting from \a offset to the
 * \a target buffer. Target buffer must be either \c Context3D.ARRAY_BUFFER or
 * \c{Context3D.ELEMENT_ARRAY_BUFFER}.
 */
/*!
 * \internal
 */
void CanvasContext::bufferSubData(glEnums target, int offset, QJSValue data)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", offset:"<< offset
                                         << ", data:" << data.toString()
                                         << ")";

    if (target != ARRAY_BUFFER && target != ELEMENT_ARRAY_BUFFER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:Target must be either ARRAY_BUFFER"
                                               << " or ELEMENT_ARRAY_BUFFER.";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    if (data.isNull()) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": INVALID_VALUE:Called with null data";
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }

    int arrayLen = 0;
    uchar *srcData = getTypedArrayAsRawDataPtr(data, arrayLen);

    if (!srcData)
        srcData = getArrayBufferAsRawDataPtr(data, arrayLen);

    if (srcData) {
        glBufferSubData(GLenum(target), offset, arrayLen, (GLvoid *)srcData);
        logAllGLErrors(__FUNCTION__);
    } else {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:data must be either"
                                               << " TypedArray or ArrayBuffer";
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }
}

/*!
 * \qmlmethod value Context3D::getBufferParameter(glEnums target, glEnums pname)
 * Returns the value for the passed \a pname of the \a target. Target
 * must be either \c Context3D.ARRAY_BUFFER or \c{Context3D.ELEMENT_ARRAY_BUFFER}. pname must be
 * either \c Context3D.BUFFER_SIZE or \c{Context3D.BUFFER_USAGE}.
 */
/*!
 * \internal
 */
QJSValue CanvasContext::getBufferParameter(glEnums target, glEnums pname)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", pname" << glEnumToString(pname)
                                         << ")";

    if (target != ARRAY_BUFFER && target != ELEMENT_ARRAY_BUFFER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:target must be either ARRAY_BUFFER"
                                               << " or ELEMENT_ARRAY_BUFFER.";
        m_error |= CANVAS_INVALID_ENUM;
        return QJSValue(QJSValue::NullValue);
    }

    switch (pname) {
    case BUFFER_SIZE:
    case BUFFER_USAGE:
        GLint data;
        glGetBufferParameteriv(GLenum(target), GLenum(pname), &data);
        logAllGLErrors(__FUNCTION__);

        return QJSValue(data);
    default:
        break;
    }

    qCWarning(canvas3drendering).nospace() << "getBufferParameter():INVALID_ENUM:Unknown pname";
    m_error |= CANVAS_INVALID_ENUM;
    return QJSValue(QJSValue::NullValue);
}

/*!
 * \qmlmethod bool Context3D::isBuffer(Object anyObject)
 * Returns \c true if the given \a anyObect is a valid Canvas3DBuffer object,
 * \c false otherwise.
 */
/*!
 * \internal
 */
bool CanvasContext::isBuffer(QJSValue anyObject)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(anyObject:" << anyObject.toString()
                                         << ")";

    CanvasBuffer *buffer = getAsBuffer3D(anyObject);
    if (!buffer || !checkParent(buffer, __FUNCTION__))
        return false;

    return glIsBuffer(buffer->id());
}

/*!
 * \internal
 */
CanvasBuffer *CanvasContext::getAsBuffer3D(QJSValue anyObject) const
{
    if (!isOfType(anyObject, "QtCanvas3D::CanvasBuffer"))
        return 0;

    CanvasBuffer *buffer = static_cast<CanvasBuffer *>(anyObject.toQObject());

    if (!buffer->isAlive())
        return 0;

    return buffer;
}

/*!
 * \qmlmethod void Context3D::deleteBuffer(Canvas3DBuffer buffer3D)
 * Deletes the \a buffer3D. Has no effect if the Canvas3DBuffer object has been deleted already.
 */
/*!
 * \internal
 */
void CanvasContext::deleteBuffer(QJSValue buffer3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(buffer:" << buffer3D.toString()
                                         << ")";
    CanvasBuffer *bufferObj = getAsBuffer3D(buffer3D);
    if (!bufferObj) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ": WARNING invalid buffer target"
                                               << buffer3D.toString();
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }
    if (!checkParent(bufferObj, __FUNCTION__))
        return;

    m_idToCanvasBufferMap.remove(bufferObj->id());
    bufferObj->del();
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod glEnums Context3D::getError()
 * Returns the error value, if any.
 */
/*!
 * \internal
 */
CanvasContext::glEnums CanvasContext::getError()
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__;

    // Merge any GL errors with internal errors
    switch (glGetError()) {
    case GL_NO_ERROR:
        break;
    case GL_INVALID_ENUM:
        m_error |= CANVAS_INVALID_ENUM;
        break;
    case GL_INVALID_VALUE:
        m_error |= CANVAS_INVALID_VALUE;
        break;
    case GL_INVALID_OPERATION:
        m_error |= CANVAS_INVALID_OPERATION;
        break;
    case GL_OUT_OF_MEMORY:
        m_error |= CANVAS_OUT_OF_MEMORY;
        break;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        m_error |= CANVAS_INVALID_FRAMEBUFFER_OPERATION;
        break;
#if defined(GL_STACK_OVERFLOW)
    case GL_STACK_OVERFLOW:
        qCWarning(canvas3dglerrors).nospace() << "Context3D::" << __FUNCTION__
                                              << ":GL_STACK_OVERFLOW error ignored";
        break;
#endif
#if defined(GL_STACK_UNDERFLOW)
    case GL_STACK_UNDERFLOW:
        qCWarning(canvas3dglerrors).nospace() << "Context3D::" << __FUNCTION__
                                              << ": GL_CANVAS_STACK_UNDERFLOW error ignored";
        break;
#endif
    default:
        break;
    }

    glEnums retVal = NO_ERROR;
    if (m_error != CANVAS_NO_ERRORS) {
        // Return set error flags one by one and clear the flags.
        // Note that stack overflow/underflow flags are never returned.
        if ((m_error & CANVAS_INVALID_ENUM) != 0) {
            retVal = INVALID_ENUM;
            m_error &= ~(CANVAS_INVALID_ENUM);
        } else if ((m_error & CANVAS_INVALID_VALUE) != 0) {
            retVal = INVALID_VALUE;
            m_error &= ~(CANVAS_INVALID_VALUE);
        }else if ((m_error & CANVAS_INVALID_OPERATION) != 0) {
            retVal = INVALID_OPERATION;
            m_error &= ~(CANVAS_INVALID_OPERATION);
        } else if ((m_error & CANVAS_OUT_OF_MEMORY) != 0) {
            retVal = OUT_OF_MEMORY;
            m_error &= ~(CANVAS_OUT_OF_MEMORY);
        } else if ((m_error & CANVAS_INVALID_FRAMEBUFFER_OPERATION) != 0) {
            retVal = INVALID_FRAMEBUFFER_OPERATION;
            m_error &= ~(CANVAS_INVALID_FRAMEBUFFER_OPERATION);
        }
    }

    return retVal;
}

/*!
 * \qmlmethod variant Context3D::getParameter(glEnums pname)
 * Returns the value for the given \a pname.
 */
/*!
 * \internal
 */
QJSValue CanvasContext::getParameter(glEnums pname)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "( pname:" << glEnumToString(pname)
                                         << ")";

    switch (pname) {
    // GLint values
    // Intentional flow through
    case MAX_COMBINED_TEXTURE_IMAGE_UNITS:
    case MAX_FRAGMENT_UNIFORM_VECTORS:
    case MAX_RENDERBUFFER_SIZE:
    case MAX_VARYING_VECTORS:
    case MAX_VERTEX_ATTRIBS:
    case MAX_VERTEX_TEXTURE_IMAGE_UNITS:
    case PACK_ALIGNMENT:
    case SAMPLE_BUFFERS:
    case SAMPLES:
    case STENCIL_BACK_REF:
    case STENCIL_CLEAR_VALUE:
    case STENCIL_REF:
    case SUBPIXEL_BITS:
    case UNPACK_ALIGNMENT:
    case RED_BITS:
    case GREEN_BITS:
    case BLUE_BITS:
    case ALPHA_BITS:
    case DEPTH_BITS:
    case STENCIL_BITS:
    case MAX_TEXTURE_IMAGE_UNITS:
    case MAX_TEXTURE_SIZE:
    case MAX_CUBE_MAP_TEXTURE_SIZE:
#if defined(QT_OPENGL_ES_2)
    case MAX_VERTEX_UNIFORM_VECTORS:
#endif
    {
        GLint value;
        glGetIntegerv(pname, &value);
        logAllGLErrors(__FUNCTION__);
        return QJSValue(int(value));
    }
        // GLuint values
        // Intentional flow through
    case STENCIL_BACK_VALUE_MASK:
    case STENCIL_BACK_WRITEMASK:
    case STENCIL_VALUE_MASK:
    case STENCIL_WRITEMASK:
    {
        GLint value;
        glGetIntegerv(pname, &value);
        logAllGLErrors(__FUNCTION__);
        return QJSValue(uint(value));
    }

    case FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
        if (m_standardDerivatives) {
            GLint value;
            glGetIntegerv(pname, &value);
            logAllGLErrors(__FUNCTION__);
            return QJSValue(value);
        }
        m_error |= CANVAS_INVALID_ENUM;
        return QJSValue(QJSValue::NullValue);

#if !defined(QT_OPENGL_ES_2)
    case MAX_VERTEX_UNIFORM_VECTORS: {
        GLint value;
        glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &value);
        logAllGLErrors(__FUNCTION__);
        qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__ << "():" << value;
        return QJSValue(value);
    }
#endif

        // GLboolean values
        // Intentional flow through
    case BLEND:
    case CULL_FACE:
    case DEPTH_TEST:
    case DEPTH_WRITEMASK:
    case DITHER:
    case POLYGON_OFFSET_FILL:
    case SAMPLE_COVERAGE_INVERT:
    case SCISSOR_TEST:
    case STENCIL_TEST: {
        GLboolean value;
        glGetBooleanv(pname, &value);
        logAllGLErrors(__FUNCTION__);
        return QJSValue(bool(value));
    }
    case UNPACK_FLIP_Y_WEBGL:
        return QJSValue(m_unpackFlipYEnabled);
    case UNPACK_PREMULTIPLY_ALPHA_WEBGL:
        return QJSValue(m_unpackPremultiplyAlphaEnabled);

        // GLenum values
        // Intentional flow through
    case ACTIVE_TEXTURE:
    case BLEND_DST_ALPHA:
    case BLEND_DST_RGB:
    case BLEND_EQUATION_ALPHA:
    case BLEND_EQUATION_RGB:
    case BLEND_SRC_ALPHA:
    case BLEND_SRC_RGB:
    case CULL_FACE_MODE:
    case DEPTH_FUNC:
    case FRONT_FACE:
    case GENERATE_MIPMAP_HINT:
    case STENCIL_BACK_FAIL:
    case STENCIL_BACK_FUNC:
    case STENCIL_BACK_PASS_DEPTH_FAIL:
    case STENCIL_BACK_PASS_DEPTH_PASS:
    case STENCIL_FAIL:
    case STENCIL_FUNC:
    case STENCIL_PASS_DEPTH_FAIL:
    case STENCIL_PASS_DEPTH_PASS: {
        GLint value;
        glGetIntegerv(pname, &value);
        logAllGLErrors(__FUNCTION__);
        return QJSValue(glEnums(value));
    }

    case IMPLEMENTATION_COLOR_READ_FORMAT:
        return QJSValue(QJSValue::UndefinedValue);
    case IMPLEMENTATION_COLOR_READ_TYPE:
        return QJSValue(QJSValue::UndefinedValue);
    case UNPACK_COLORSPACE_CONVERSION_WEBGL:
        return QJSValue(BROWSER_DEFAULT_WEBGL);

        // Float32Array (with 2 elements)
        // Intentional flow through
    case ALIASED_LINE_WIDTH_RANGE:
    case ALIASED_POINT_SIZE_RANGE:
    case DEPTH_RANGE: {
        QV4::Scope scope(m_v4engine);
        QV4::Scoped<QV4::ArrayBuffer> buffer(scope,
                                             m_v4engine->memoryManager->alloc<QV4::ArrayBuffer>(
                                                 m_v4engine,
                                                 sizeof(float) * 2));
        glGetFloatv(pname, (float *) buffer->data());
        logAllGLErrors(__FUNCTION__);

        QV4::ScopedFunctionObject constructor(scope,
                                              m_v4engine->typedArrayCtors[
                                              QV4::Heap::TypedArray::Float32Array]);
        QV4::ScopedCallData callData(scope, 1);
        callData->args[0] = buffer;
        return QJSValue(m_v4engine, constructor->construct(callData));
    }

        // Float32Array (with 4 values)
        // Intentional flow through
    case BLEND_COLOR:
    case COLOR_CLEAR_VALUE: {
        QV4::Scope scope(m_v4engine);
        QV4::Scoped<QV4::ArrayBuffer> buffer(scope,
                                             m_v4engine->memoryManager->alloc<QV4::ArrayBuffer>(
                                                 m_v4engine,
                                                 sizeof(float) * 4));
        glGetFloatv(pname, (float *) buffer->data());
        logAllGLErrors(__FUNCTION__);

        QV4::ScopedFunctionObject constructor(scope,
                                              m_v4engine->typedArrayCtors[
                                              QV4::Heap::TypedArray::Float32Array]);
        QV4::ScopedCallData callData(scope, 1);
        callData->args[0] = buffer;
        return QJSValue(m_v4engine, constructor->construct(callData));
    }

        // Int32Array (with 2 elements)
    case MAX_VIEWPORT_DIMS: {
        QV4::Scope scope(m_v4engine);
        QV4::Scoped<QV4::ArrayBuffer> buffer(scope,
                                             m_v4engine->memoryManager->alloc<QV4::ArrayBuffer>(
                                                 m_v4engine,
                                                 sizeof(int) * 2));
        glGetIntegerv(pname, (int *) buffer->data());
        logAllGLErrors(__FUNCTION__);

        QV4::ScopedFunctionObject constructor(scope,
                                              m_v4engine->typedArrayCtors[
                                              QV4::Heap::TypedArray::Int32Array]);
        QV4::ScopedCallData callData(scope, 1);
        callData->args[0] = buffer;
        return QJSValue(m_v4engine, constructor->construct(callData));
    }
        // Int32Array (with 4 elements)
        // Intentional flow through
    case SCISSOR_BOX:
    case VIEWPORT: {
        QV4::Scope scope(m_v4engine);
        QV4::Scoped<QV4::ArrayBuffer> buffer(scope,
                                             m_v4engine->memoryManager->alloc<QV4::ArrayBuffer>(
                                                 m_v4engine,
                                                 sizeof(int) * 4));
        glGetIntegerv(pname, (int *) buffer->data());
        logAllGLErrors(__FUNCTION__);

        QV4::ScopedFunctionObject constructor(scope,
                                              m_v4engine->typedArrayCtors[
                                              QV4::Heap::TypedArray::Int32Array]);
        QV4::ScopedCallData callData(scope, 1);
        callData->args[0] = buffer;
        return QJSValue(m_v4engine, constructor->construct(callData));
    }

        // sequence<GLboolean> (with 4 values)
        // Intentional flow through
    case COLOR_WRITEMASK: {
        GLboolean values[4];
        glGetBooleanv(COLOR_WRITEMASK, values);
        logAllGLErrors(__FUNCTION__);
        QJSValue arrayValue = m_engine->newArray(4);
        arrayValue.setProperty(0, bool(values[0]));
        arrayValue.setProperty(1, bool(values[1]));
        arrayValue.setProperty(2, bool(values[2]));
        arrayValue.setProperty(3, bool(values[3]));
        return arrayValue;
    }

        // GLfloat values
        // Intentional flow through
    case DEPTH_CLEAR_VALUE:
    case LINE_WIDTH:
    case POLYGON_OFFSET_FACTOR:
    case POLYGON_OFFSET_UNITS:
    case SAMPLE_COVERAGE_VALUE: {
        GLfloat value;
        glGetFloatv(pname, &value);
        logAllGLErrors(__FUNCTION__);
        return QJSValue(float(value));
    }

        // DomString values
        // Intentional flow through
    case RENDERER:
    case SHADING_LANGUAGE_VERSION:
    case VENDOR:
    case VERSION: {
        const GLubyte *text = glGetString(pname);
        logAllGLErrors(__FUNCTION__);
        QString qtext = QString::fromLatin1((const char *)text);
        qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__ << "():" << qtext;
        return QJSValue(qtext);
    }
    case UNMASKED_VENDOR_WEBGL: {
        const GLubyte *text = glGetString(GL_VENDOR);
        logAllGLErrors(__FUNCTION__);
        QString qtext = QString::fromLatin1((const char *)text);
        qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__ << "():" << qtext;
        return QJSValue(qtext);
    }
    case UNMASKED_RENDERER_WEBGL: {
        const GLubyte *text = glGetString(GL_VENDOR);
        logAllGLErrors(__FUNCTION__);
        QString qtext = QString::fromLatin1((const char *)text);
        qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__ << "():" << qtext;
        return QJSValue(qtext);
    }
    case COMPRESSED_TEXTURE_FORMATS: {
        GLint numFormats;
        glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &numFormats);
        if (numFormats > 0) {
            QV4::Scope scope(m_v4engine);
            QV4::Scoped<QV4::ArrayBuffer> buffer(scope,
                                                 m_v4engine->memoryManager->alloc<QV4::ArrayBuffer>(
                                                     m_v4engine,
                                                     sizeof(int) * numFormats));
            glGetIntegerv(pname, (int *) buffer->data());
            logAllGLErrors(__FUNCTION__);

            QV4::ScopedFunctionObject constructor(scope,
                                                  m_v4engine->typedArrayCtors[
                                                  QV4::Heap::TypedArray::UInt32Array]);
            QV4::ScopedCallData callData(scope, 1);
            callData->args[0] = buffer;
            return QJSValue(m_v4engine, constructor->construct(callData));
        }
    }
    case FRAMEBUFFER_BINDING: {
        return m_engine->newQObject(m_currentFramebuffer);
    }
    case RENDERBUFFER_BINDING: {
        return m_engine->newQObject(m_currentRenderbuffer);
    }
    case CURRENT_PROGRAM: {
        return m_engine->newQObject(m_currentProgram);
    }
    case ELEMENT_ARRAY_BUFFER_BINDING: {
        return m_engine->newQObject(m_currentElementArrayBuffer);
    }
    case ARRAY_BUFFER_BINDING: {
        return m_engine->newQObject(m_currentArrayBuffer);
    }
    case TEXTURE_BINDING_2D: {
        return m_engine->newQObject(m_currentTexture2D);
    }
    case TEXTURE_BINDING_CUBE_MAP: {
        return m_engine->newQObject(m_currentTextureCubeMap);
    }
    default: {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << "(): UNIMPLEMENTED PARAMETER NAME"
                                               << glEnumToString(pname);
        return QJSValue(QJSValue::NullValue);
    }
    }

    return QJSValue(QJSValue::NullValue);
}

/*!
 * \qmlmethod string Context3D::getShaderInfoLog(Canvas3DShader shader)
 * Returns the info log string of the given \a shader.
 */
/*!
 * \internal
 */
QJSValue CanvasContext::getShaderInfoLog(QJSValue shader3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(shader3D:" << shader3D.toString()
                                         << ")";
    CanvasShader *shader = getAsShader3D(shader3D);
    if (!shader) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << " WARNING: invalid shader handle:"
                                               << shader3D.toString();
        m_error |= CANVAS_INVALID_OPERATION;
        return m_engine->newObject();
    }
    if (!checkParent(shader, __FUNCTION__))
        return m_engine->newObject();

    return QJSValue(shader->qOGLShader()->log());
}

/*!
 * \qmlmethod string Context3D::getProgramInfoLog(Canvas3DProgram program3D)
 * Returns the info log string of the given \a program3D.
 */
/*!
 * \internal
 */
QJSValue CanvasContext::getProgramInfoLog(QJSValue program3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ")";
    CanvasProgram *program = getAsProgram3D(program3D);

    if (!program) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << " WARNING: invalid program handle:"
                                               << program3D.toString();
        m_error |= CANVAS_INVALID_OPERATION;
        return m_engine->newObject();
    }
    if (!checkParent(program, __FUNCTION__))
        return m_engine->newObject();

    return QJSValue(program->log());
}

/*!
 * \qmlmethod void Context3D::bindBuffer(glEnums target, Canvas3DBuffer buffer3D)
 * Binds the given \a buffer3D to the given \a target. Target must be either
 * \c Context3D.ARRAY_BUFFER or \c{Context3D.ELEMENT_ARRAY_BUFFER}. If the \a buffer3D given is
 * \c{null}, the current buffer bound to the target is unbound.
 */
/*!
 * \internal
 */
void CanvasContext::bindBuffer(glEnums target, QJSValue buffer3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target:" << glEnumToString(target)
                                         << ", buffer:" << buffer3D.toString()
                                         << ")";

    if (target != ARRAY_BUFFER && target != ELEMENT_ARRAY_BUFFER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:target must be either "
                                               << "ARRAY_BUFFER or ELEMENT_ARRAY_BUFFER.";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    CanvasBuffer *buffer = getAsBuffer3D(buffer3D);
    if (buffer && checkParent(buffer, __FUNCTION__)) {
        if (target == ARRAY_BUFFER) {
            if (buffer->target() == CanvasBuffer::UNINITIALIZED)
                buffer->setTarget(CanvasBuffer::ARRAY_BUFFER);

            if (buffer->target() != CanvasBuffer::ARRAY_BUFFER) {
                qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                       << ":INVALID_OPERATION:can't rebind "
                                                       << "ELEMENT_ARRAY_BUFFER as ARRAY_BUFFER";
                m_error |= CANVAS_INVALID_OPERATION;
                return;
            }
            m_currentArrayBuffer = buffer;
        } else {
            if (buffer->target() == CanvasBuffer::UNINITIALIZED)
                buffer->setTarget(CanvasBuffer::ELEMENT_ARRAY_BUFFER);

            if (buffer->target() != CanvasBuffer::ELEMENT_ARRAY_BUFFER) {
                qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                       << ":INVALID_OPERATION:can't rebind "
                                                       << "ARRAY_BUFFER as ELEMENT_ARRAY_BUFFER";
                m_error |= CANVAS_INVALID_OPERATION;
                return;
            }
            m_currentElementArrayBuffer = buffer;
        }
        glBindBuffer(GLenum(target), buffer->id());
        logAllGLErrors(__FUNCTION__);
    } else {
        glBindBuffer(GLenum(target), 0);
        logAllGLErrors(__FUNCTION__);
    }
}

// TODO: Is this function useful? We don't offer a way to query the status.
/*!
 * \qmlmethod void Context3D::validateProgram(Canvas3DProgram program3D)
 * Validates the given \a program3D. The validation status is stored into the state of the shader
 * program container in \a program3D.
 */
/*!
 * \internal
 */
void CanvasContext::validateProgram(QJSValue program3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString() << ")";

    CanvasProgram *program = getAsProgram3D(program3D);
    if (!program) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }
    if (checkParent(program, __FUNCTION__)) {
        program->validateProgram();
        logAllGLErrors(__FUNCTION__);
    }
}

/*!
 * \qmlmethod void Context3D::useProgram(Canvas3DProgram program)
 * Installs the given \a program3D as a part of the current rendering state.
 */
/*!
 * \internal
 */
void CanvasContext::useProgram(QJSValue program3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString() << ")";

    CanvasProgram *program = getAsProgram3D(program3D);
    m_currentProgram = program;

    if (!program) {
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    if (!checkParent(program, __FUNCTION__))
        return;

    glUseProgram(program->id());
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::clear(glEnums flags)
 * Clears the given \a flags.
 */
/*!
 * \internal
 */
void CanvasContext::clear(glEnums flags)
{
    if (!canvas3drendering().isDebugEnabled()) {
        QString flagStr;
        if (flags && COLOR_BUFFER_BIT != 0)
            flagStr.append(" COLOR_BUFFER_BIT ");

        if (flags && DEPTH_BUFFER_BIT != 0)
            flagStr.append(" DEPTH_BUFFER_BIT ");

        if (flags && STENCIL_BUFFER_BIT != 0)
            flagStr.append(" STENCIL_BUFFER_BIT ");

        qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                             << "(flags:" << flagStr << ")";
    }

    glClear(flags);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::cullFace(glEnums mode)
 * Sets the culling to \a mode. Must be one of \c{Context3D.FRONT}, \c{Context3D.BACK},
 * or \c{Context3D.FRONT_AND_BACK}. Defaults to \c{Context3D.BACK}.
 */
/*!
 * \internal
 */
void CanvasContext::cullFace(glEnums mode)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(mode:" << glEnumToString(mode) << ")";
    glCullFace(mode);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::frontFace(glEnums mode)
 * Sets the front face drawing to \a mode. Must be either \c Context3D.CW
 * or \c{Context3D.CCW}. Defaults to \c{Context3D.CCW}.
 */
/*!
 * \internal
 */
void CanvasContext::frontFace(glEnums mode)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(mode:" << glEnumToString(mode) << ")";
    glFrontFace(mode);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::depthMask(bool flag)
 * Enables or disables the depth mask based on \a flag. Defaults to \c{true}.
 */
/*!
 * \internal
 */
void CanvasContext::depthMask(bool flag)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(flag:" << flag << ")";
    if (flag)
        glDepthMask(GL_TRUE);
    else
        glDepthMask(GL_FALSE);

    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::depthFunc(glEnums func)
 * Sets the depth function to \a func. Must be one of \c{Context3D.NEVER}, \c{Context3D.LESS},
 * \c{Context3D.EQUAL}, \c{Context3D.LEQUAL}, \c{Context3D.GREATER}, \c{Context3D.NOTEQUAL},
 * \c{Context3D.GEQUAL}, or \c{Context3D.ALWAYS}. Defaults to \c{Context3D.LESS}.
 */
/*!
 * \internal
 */
void CanvasContext::depthFunc(glEnums func)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(func:" << glEnumToString(func) << ")";
    glDepthFunc(GLenum(func));
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::depthRange(float zNear, float zFar)
 * Sets the depth range between \a zNear and \a zFar. Values are clamped to \c{[0, 1]}. \a zNear
 * must be less or equal to \a zFar. zNear Range defaults to \c{[0, 1]}.
 */
/*!
 * \internal
 */
void CanvasContext::depthRange(float zNear, float zFar)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(zNear:" << zNear
                                         << ", zFar:" << zFar <<  ")";
    glDepthRangef(GLclampf(zNear), GLclampf(zFar));
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::clearStencil(int stencil)
 * Sets the clear value for the stencil buffer to \a stencil. Defaults to \c{0}.
 */
/*!
 * \internal
 */
void CanvasContext::clearStencil(int stencil)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(stencil:" << stencil << ")";
    glClearStencil(stencil);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::colorMask(bool maskRed, bool maskGreen, bool maskBlue, bool maskAlpha)
 * Enables or disables the writing of colors to the frame buffer based on \a maskRed, \a maskGreen,
 * \a maskBlue and \a maskAlpha. All default to \c{true}.
 */
/*!
 * \internal
 */
void CanvasContext::colorMask(bool maskRed, bool maskGreen, bool maskBlue, bool maskAlpha)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(maskRed:" << maskRed
                                         << ", maskGreen:" << maskGreen
                                         << ", maskBlue:" << maskBlue
                                         << ", maskAlpha:" << maskAlpha  <<  ")";
    glColorMask(maskRed, maskGreen, maskBlue, maskAlpha);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::clearDepth(float depth)
 * Sets the clear value for the depth buffer to \a depth. Must be between \c{[0, 1]}. Defaults
 * to \c{1}.
 */
/*!
 * \internal
 */
void CanvasContext::clearDepth(float depth)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(depth:" << depth << ")";
    glClearDepthf(depth);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::clearColor(float red, float green, float blue, float alpha)
 * Sets the clear values for the color buffers with \a red, \a green, \a blue and \a alpha. Values
 * must be between \c{[0, 1]}. All default to \c{0}.
 */
/*!
 * \internal
 */
void CanvasContext::clearColor(float red, float green, float blue, float alpha)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         <<  "(red:" << red
                                          << ", green:" << green
                                          << ", blue:" << blue
                                          << ", alpha:" << alpha << ")";
    glClearColor(red, green, blue, alpha);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod Context3D::viewport(int x, int y, int width, int height)
 * Defines the affine transformation from normalized x and y device coordinates to window
 * coordinates within the drawing buffer.
 * \a x defines the left edge of the viewport.
 * \a y defines the bottom edge of the viewport.
 * \a width defines the width of the viewport.
 * \a height defines the height of the viewport.
 */
/*!
 * \internal
 */
void CanvasContext::viewport(int x, int y, int width, int height)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         <<  "(x:" << x
                                          << ", y:" << y
                                          << ", width:" << width
                                          << ", height:" << height << ")";
    glViewport(x, y, width, height);
    logAllGLErrors(__FUNCTION__);
    m_glViewportRect.setX(x);
    m_glViewportRect.setY(y);
    m_glViewportRect.setWidth(width);
    m_glViewportRect.setHeight(height);
}

/*!
 * \qmlmethod void Context3D::drawArrays(glEnums mode, int first, int count)
 * Renders the geometric primitives held in the currently bound array buffer starting from \a first
 * up to \a count using \a mode for drawing. Mode can be one of:
 * \list
 * \li \c{Context3D.POINTS}
 * \li \c{Context3D.LINES}
 * \li \c{Context3D.LINE_LOOP}
 * \li \c{Context3D.LINE_STRIP}
 * \li \c{Context3D.TRIANGLES}
 * \li \c{Context3D.TRIANGLE_STRIP}
 * \li \c{Context3D.TRIANGLE_FAN}
 * \endlist
 */
/*!
 * \internal
 */
void CanvasContext::drawArrays(glEnums mode, int first, int count)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(mode:" << glEnumToString(mode)
                                         << ", first:" << first
                                         << ", count:" << count << ")";
    glDrawArrays(mode, first, count);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::drawElements(glEnums mode, int count, glEnums type, long offset)
 * Renders the number of geometric elements given in \a count held in the currently bound element
 * array buffer using \a mode for drawing. Mode can be one of:
 * \list
 * \li \c{Context3D.POINTS}
 * \li \c{Context3D.LINES}
 * \li \c{Context3D.LINE_LOOP}
 * \li \c{Context3D.LINE_STRIP}
 * \li \c{Context3D.TRIANGLES}
 * \li \c{Context3D.TRIANGLE_STRIP}
 * \li \c{Context3D.TRIANGLE_FAN}
 * \endlist
 *
 * \a type specifies the element type and can be one of:
 * \list
 * \li \c Context3D.UNSIGNED_BYTE
 * \li \c{Context3D.UNSIGNED_SHORT}
 * \endlist
 *
 * \a offset specifies the location3D where indices are stored.
 */
/*!
 * \internal
 */
void CanvasContext::drawElements(glEnums mode, int count, glEnums type, long offset)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(mode:" << glEnumToString(mode)
                                         << ", count:" << count
                                         << ", type:" << glEnumToString(type)
                                         << ", offset:" << offset << ")";
    if (!m_currentElementArrayBuffer) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_OPERATION: "
                                               << "No ELEMENT_ARRAY_BUFFER currently bound";
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    // Verify offset follows the rules of the spec
    switch (type) {
    case UNSIGNED_SHORT:
        if (offset % 2 != 0) {
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_OPERATION: "
                                                   << "Offset with UNSIGNED_SHORT"
                                                   << "type must be multiple of 2";
            m_error |= CANVAS_INVALID_OPERATION;
            return;
        }
    case UNSIGNED_BYTE:
        break;
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM: "
                                               << "Invalid type enumeration of "
                                               << glEnumToString(type);
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    glDrawElements(GLenum(mode), count, GLenum(type), (GLvoid*)offset);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::readPixels(int x, int y, long width, long height, glEnums format, glEnums type, ArrayBufferView pixels)
 * Returns the pixel data in the rectangle specified by \a x, \a y, \a width and \a height of the
 * frame buffer in \a pixels using \a format (must be \c{Context3D.RGBA}) and \a type
 * (must be \c{Context3D.UNSIGNED_BYTE}).
 */
/*!
 * \internal
 */
void CanvasContext::readPixels(int x, int y, long width, long height, glEnums format, glEnums type,
                               QJSValue pixels)
{
    if (format != RGBA) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:format must be RGBA.";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    if (type != UNSIGNED_BYTE) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:type must be UNSIGNED_BYTE.";
        m_error |= CANVAS_INVALID_ENUM;
        return;
    }

    if (pixels.isNull()) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:pixels was null.";
        m_error |= CANVAS_INVALID_VALUE;
        return;
    }

    uchar *bufferPtr = getTypedArrayAsRawDataPtr(pixels, QV4::Heap::TypedArray::UInt8Array);
    if (!bufferPtr) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_OPERATION:pixels must be Uint8Array.";
        m_error |= CANVAS_INVALID_OPERATION;
        return;
    }

    // Zero out the buffer (WebGL conformance requires pixels outside the framebuffer to be 0)
    memset(bufferPtr, 0, width * height * 4);

    // Check if the buffer is antialiased. If it is, we need to blit to the final buffer before
    // reading the value.
    if (m_contextAttributes.antialias() && !m_currentFramebuffer) {
        GLuint readFbo = m_canvas->resolveMSAAFbo();
        glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
    }

    glReadPixels(x, y, width, height, format, type, bufferPtr);

    if (m_contextAttributes.antialias() && !m_currentFramebuffer)
        m_canvas->bindCurrentRenderTarget();

    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod Canvas3DActiveInfo Context3D::getActiveAttrib(Canvas3DProgram program3D, uint index)
 * Returns information about the given active attribute variable defined by \a index for the given
 * \a program3D.
 * \sa Canvas3DActiveInfo
 */
/*!
 * \internal
 */
CanvasActiveInfo *CanvasContext::getActiveAttrib(QJSValue program3D, uint index)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ", index:" << index << ")";

    CanvasProgram *program = getAsProgram3D(program3D);
    if (!program || !checkParent(program, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return 0;
    }

    char *name = new char[512];
    GLsizei length = 0;
    int size = 0;
    GLenum type = 0;
    glGetActiveAttrib(program->id(), index, 512, &length, &size, &type, name);
    logAllGLErrors(__FUNCTION__);
    QString strName(name);
    delete [] name;
    return new CanvasActiveInfo(size, CanvasContext::glEnums(type), strName);
}

/*!
 * \qmlmethod Canvas3DActiveInfo Context3D::getActiveUniform(Canvas3DProgram program3D, uint index)
 * Returns information about the given active uniform variable defined by \a index for the given
 * \a program3D.
 * \sa Canvas3DActiveInfo
 */
/*!
 * \internal
 */
CanvasActiveInfo *CanvasContext::getActiveUniform(QJSValue program3D, uint index)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program3D:" << program3D.toString()
                                         << ", index:" << index << ")";

    CanvasProgram *program = getAsProgram3D(program3D);
    if (!program || !checkParent(program, __FUNCTION__)) {
        m_error |= CANVAS_INVALID_OPERATION;
        return 0;
    }

    char *name = new char[512];
    GLsizei length = 0;
    int size = 0;
    GLenum type = 0;
    glGetActiveUniform(program->id(), index, 512, &length, &size, &type, name);
    logAllGLErrors(__FUNCTION__);
    QString strName(name);
    delete [] name;
    return new CanvasActiveInfo(size, CanvasContext::glEnums(type), strName);
}

/*!
 * \qmlmethod void Context3D::stencilFunc(glEnums func, int ref, uint mask)
 * Sets front and back function \a func and reference value \a ref for stencil testing.
 * Also sets the \a mask value that is ANDed with both reference and stored stencil value when
 * the test is done. \a func is initially set to \c{Context3D.ALWAYS} and can be one of:
 * \list
 * \li \c{Context3D.NEVER}
 * \li \c{Context3D.LESS}
 * \li \c{Context3D.LEQUAL}
 * \li \c{Context3D.GREATER}
 * \li \c{Context3D.GEQUAL}
 * \li \c{Context3D.EQUAL}
 * \li \c{Context3D.NOTEQUAL}
 * \li \c{Context3D.ALWAYS}
 * \endlist
 */
/*!
 * \internal
 */
void CanvasContext::stencilFunc(glEnums func, int ref, uint mask)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(func:" <<  glEnumToString(func)
                                         << ", ref:" << ref
                                         << ", mask:" << mask
                                         << ")";

    glStencilFunc(GLenum(func), ref, mask);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::stencilFuncSeparate(glEnums face, glEnums func, int ref, uint mask)
 * Sets front and/or back (defined by \a face) function \a func and reference value \a ref for
 * stencil testing. Also sets the \a mask value that is ANDed with both reference and stored stencil
 * value when the test is done. \a face can be one of:
 * \list
 * \li \c{Context3D.FRONT}
 * \li \c{Context3D.BACK}
 * \li \c{Context3D.FRONT_AND_BACK}
 * \endlist
 * \a func is initially set to \c{Context3D.ALWAYS} and can be one of:
 * \list
 * \li \c{Context3D.NEVER}
 * \li \c{Context3D.LESS}
 * \li \c{Context3D.LEQUAL}
 * \li \c{Context3D.GREATER}
 * \li \c{Context3D.GEQUAL}
 * \li \c{Context3D.EQUAL}
 * \li \c{Context3D.NOTEQUAL}
 * \li \c{Context3D.ALWAYS}
 * \endlist
 */
/*!
 * \internal
 */
void CanvasContext::stencilFuncSeparate(glEnums face, glEnums func, int ref, uint mask)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(face:" <<  glEnumToString(face)
                                         << ", func:" <<  glEnumToString(func)
                                         << ", ref:" << ref
                                         << ", mask:" << mask
                                         << ")";
    glStencilFuncSeparate(GLenum(face), GLenum(func), ref, mask);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::stencilMask(uint mask)
 * Controls the front and back writing of individual bits in the stencil planes. \a mask defines
 * the bit mask to enable and disable writing of individual bits in the stencil planes.
 */
/*!
 * \internal
 */
void CanvasContext::stencilMask(uint mask)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(mask:" << mask
                                         << ")";
    glStencilMask(mask);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::stencilMaskSeparate(glEnums face, uint mask)
 * Controls the front and/or back writing (defined by \a face) of individual bits in the stencil
 * planes. \a mask defines the bit mask to enable and disable writing of individual bits in the
 * stencil planes. \a face can be one of:
 * \list
 * \li \c{Context3D.FRONT}
 * \li \c{Context3D.BACK}
 * \li \c{Context3D.FRONT_AND_BACK}
 * \endlist
 */
/*!
 * \internal
 */
void CanvasContext::stencilMaskSeparate(glEnums face, uint mask)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(face:" <<  glEnumToString(face)
                                         << ", mask:" << mask
                                         << ")";
    glStencilMaskSeparate(GLenum(face), mask);
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::stencilOp(glEnums sfail, glEnums zfail, glEnums zpass)
 * Sets the front and back stencil test actions for failing the test \a zfail and passing the test
 * \a zpass. \a sfail, \a zfail and \a zpass are initially set to \c{Context3D.KEEP} and can be one
 * of:
 * \list
 * \li \c{Context3D.KEEP}
 * \li \c{Context3D.ZERO}
 * \li \c{Context3D.GL_REPLACE}
 * \li \c{Context3D.GL_INCR}
 * \li \c{Context3D.GL_INCR_WRAP}
 * \li \c{Context3D.GL_DECR}
 * \li \c{Context3D.GL_DECR_WRAP}
 * \li \c{Context3D.GL_INVERT}
 * \endlist
 */
/*!
 * \internal
 */
void CanvasContext::stencilOp(glEnums sfail, glEnums zfail, glEnums zpass)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(sfail:" <<  glEnumToString(sfail)
                                         << ", zfail:" <<  glEnumToString(zfail)
                                         << ", zpass:" << glEnumToString(zpass)
                                         << ")";
    glStencilOp(GLenum(sfail), GLenum(zfail), GLenum(zpass));
    logAllGLErrors(__FUNCTION__);
}

/*!
 * \qmlmethod void Context3D::stencilOpSeparate(glEnums face, glEnums fail, glEnums zfail, glEnums zpass)
 * Sets the front and/or back (defined by \a face) stencil test actions for failing the test
 * \a zfail and passing the test \a zpass. \a face can be one of:
 * \list
 * \li \c{Context3D.FRONT}
 * \li \c{Context3D.BACK}
 * \li \c{Context3D.FRONT_AND_BACK}
 * \endlist
 *
 * \a sfail, \a zfail and \a zpass are initially set to \c{Context3D.KEEP} and can be one of:
 * \list
 * \li \c{Context3D.KEEP}
 * \li \c{Context3D.ZERO}
 * \li \c{Context3D.GL_REPLACE}
 * \li \c{Context3D.GL_INCR}
 * \li \c{Context3D.GL_INCR_WRAP}
 * \li \c{Context3D.GL_DECR}
 * \li \c{Context3D.GL_DECR_WRAP}
 * \li \c{Context3D.GL_INVERT}
 * \endlist
 */
/*!
 * \internal
 */
void CanvasContext::stencilOpSeparate(glEnums face, glEnums fail, glEnums zfail, glEnums zpass)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(face:" <<  glEnumToString(face)
                                         << ", fail:" <<  glEnumToString(fail)
                                         << ", zfail:" <<  glEnumToString(zfail)
                                         << ", zpass:" << glEnumToString(zpass)
                                         << ")";
    glStencilOpSeparate(GLenum(face), GLenum(fail), GLenum(zfail), GLenum(zpass));
    logAllGLErrors(__FUNCTION__);
}


/*!
 * \qmlmethod void Context3D::vertexAttrib1fva(int indx, list<variant> values)
 * Sets the array of float values given in \a values to the generic vertex attribute index
 * specified by \a indx.
 */
/*!
 * \internal
 */
void CanvasContext::vertexAttrib1fva(uint indx, QVariantList values)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(indx" << indx
                                         << ", values:" << values
                                         << ")";

    int size = values.count();
    float *arrayData = new float[size];

    ArrayUtils::fillFloatArrayFromVariantList(values, arrayData);

    glVertexAttrib1fv(indx, arrayData);
    logAllGLErrors(__FUNCTION__);

    delete [] arrayData;
}

/*!
 * \qmlmethod void Context3D::vertexAttrib2fva(int indx, list<variant> values)
 * Sets the array of float values given in \a values to the generic vertex attribute index
 * specified by \a indx.
 */
/*!
 * \internal
 */
void CanvasContext::vertexAttrib2fva(uint indx, QVariantList values)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(indx" << indx
                                         << ", values:" << values
                                         << ")";

    int size = values.count();
    float *arrayData = new float[size];

    ArrayUtils::fillFloatArrayFromVariantList(values, arrayData);

    glVertexAttrib2fv(indx, arrayData);
    logAllGLErrors(__FUNCTION__);

    delete [] arrayData;
}

/*!
 * \qmlmethod void Context3D::vertexAttrib3fva(int indx, list<variant> values)
 * Sets the array of float values given in \a values to the generic vertex attribute index
 * specified by \a indx.
 */
/*!
 * \internal
 */
void CanvasContext::vertexAttrib3fva(uint indx, QVariantList values)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(indx" << indx
                                         << ", values:" << values
                                         << ")";

    int size = values.count();
    float *arrayData = new float[size];

    ArrayUtils::fillFloatArrayFromVariantList(values, arrayData);

    glVertexAttrib3fv(indx, arrayData);
    logAllGLErrors(__FUNCTION__);

    delete [] arrayData;
}

/*!
 * \qmlmethod void Context3D::vertexAttrib4fva(int indx, list<variant> values)
 * Sets the array of float values given in \a values to the generic vertex attribute index
 * specified by \a indx.
 */
/*!
 * \internal
 */
void CanvasContext::vertexAttrib4fva(uint indx, QVariantList values)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(indx" << indx
                                         << ", values:" << values
                                         << ")";

    int size = values.count();
    float *arrayData = new float[size];

    ArrayUtils::fillFloatArrayFromVariantList(values, arrayData);

    glVertexAttrib4fv(indx, arrayData);
    logAllGLErrors(__FUNCTION__);

    delete [] arrayData;
}

/*!
 * \qmlmethod int Context3D::getFramebufferAttachmentParameter(glEnums target, glEnums attachment, glEnums pname)
 * Returns information specified by \a pname about given \a attachment of a framebuffer object
 * bound to the given \a target.
 */
/*!
 * \internal
 */
QJSValue CanvasContext::getFramebufferAttachmentParameter(glEnums target, glEnums attachment,
                                                          glEnums pname)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target" << glEnumToString(target)
                                         << ", attachment:" << glEnumToString(attachment)
                                         << ", pname:" << glEnumToString(pname)
                                         << ")";
    GLint parameter;
    glGetFramebufferAttachmentParameteriv(target, attachment, pname, &parameter);
    logAllGLErrors(__FUNCTION__);

    if (m_error != CANVAS_NO_ERRORS)
        return QJSValue(QJSValue::NullValue);

    switch (pname) {
    case FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        return QJSValue(glEnums(parameter));
    case FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
    {
        QJSValue retval;
        // Check FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, and choose the type based on it
        GLint type;
        glGetFramebufferAttachmentParameteriv(target, attachment,
                                              FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
        logAllGLErrors(__FUNCTION__);
        if (type == TEXTURE)
            retval = m_engine->newQObject(m_currentFramebuffer->texture());
        else
            retval = m_engine->newQObject(m_currentRenderbuffer);
        return retval;
    }
    case FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
    case FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
        return QJSValue(parameter);
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:invalid pname "
                                               << glEnumToString(pname);
        m_error |= CANVAS_INVALID_ENUM;
        return QJSValue(QJSValue::NullValue);
    }
}

/*!
 * \qmlmethod int Context3D::getRenderbufferParameter(glEnums target, glEnums pname)
 * Returns information specified by \a pname of a renderbuffer object
 * bound to the given \a target.
 */
/*!
 * \internal
 */
QJSValue CanvasContext::getRenderbufferParameter(glEnums target, glEnums pname)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target" << glEnumToString(target)
                                         << ", pname:" << glEnumToString(pname)
                                         << ")";

    GLint parameter;
    glGetRenderbufferParameteriv(target, pname, &parameter);
    logAllGLErrors(__FUNCTION__);

    if (m_error != CANVAS_NO_ERRORS)
        return QJSValue(QJSValue::NullValue);

    switch (pname) {
    case RENDERBUFFER_INTERNAL_FORMAT:
        return QJSValue(glEnums(parameter));
    case RENDERBUFFER_WIDTH:
    case RENDERBUFFER_HEIGHT:
    case RENDERBUFFER_RED_SIZE:
    case RENDERBUFFER_GREEN_SIZE:
    case RENDERBUFFER_BLUE_SIZE:
    case RENDERBUFFER_ALPHA_SIZE:
    case RENDERBUFFER_DEPTH_SIZE:
    case RENDERBUFFER_STENCIL_SIZE:
        return QJSValue(parameter);
    default:
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:invalid pname "
                                               << glEnumToString(pname);
        m_error |= CANVAS_INVALID_ENUM;
        return QJSValue(QJSValue::NullValue);
    }
}

/*!
 * \qmlmethod variant Context3D::getTexParameter(glEnums target, glEnums pname)
 * Returns parameter specified by the \a pname of a texture object
 * bound to the given \a target. \a pname must be one of:
 * \list
 * \li \c{Context3D.TEXTURE_MAG_FILTER}
 * \li \c{Context3D.TEXTURE_MIN_FILTER}
 * \li \c{Context3D.TEXTURE_WRAP_S}
 * \li \c{Context3D.TEXTURE_WRAP_T}
 * \endlist
 */
/*!
 * \internal
 */
QJSValue CanvasContext::getTexParameter(glEnums target, glEnums pname)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(target" << glEnumToString(target)
                                         << ", pname:" << glEnumToString(pname)
                                         << ")";

    GLint parameter = 0;
    if (isValidTextureBound(target, __FUNCTION__)) {
        switch (pname) {
        case TEXTURE_MAG_FILTER:
        case TEXTURE_MIN_FILTER:
        case TEXTURE_WRAP_S:
        case TEXTURE_WRAP_T:
            glGetTexParameteriv(target, pname, &parameter);
            logAllGLErrors(__FUNCTION__);
            return QJSValue(parameter);
        default:
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_ENUM:invalid pname "
                                                   << glEnumToString(pname)
                                                   << " must be one of: TEXTURE_MAG_FILTER, "
                                                   << "TEXTURE_MIN_FILTER, TEXTURE_WRAP_S"
                                                   << " or TEXTURE_WRAP_T";
            m_error |= CANVAS_INVALID_ENUM;
            return QJSValue(QJSValue::NullValue);
        }
    }
    return QJSValue(QJSValue::NullValue);
}


/*!
 * \qmlmethod int Context3D::getVertexAttribOffset(int index, glEnums pname)
 * Returns the offset of the specified generic vertex attribute pointer \a index. \a pname must be
 * \c{Context3D.VERTEX_ATTRIB_ARRAY_POINTER}
 * \list
 * \li \c{Context3D.TEXTURE_MAG_FILTER}
 * \li \c{Context3D.TEXTURE_MIN_FILTER}
 * \li \c{Context3D.TEXTURE_WRAP_S}
 * \li \c{Context3D.TEXTURE_WRAP_T}
 * \endlist
 */
/*!
 * \internal
 */
uint CanvasContext::getVertexAttribOffset(uint index, glEnums pname)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(index" << index
                                         << ", pname:" << glEnumToString(pname)
                                         << ")";

    uint offset = 0;
    if (pname != VERTEX_ATTRIB_ARRAY_POINTER) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_ENUM:pname must be "
                                               << "VERTEX_ATTRIB_ARRAY_POINTER";
        m_error |= CANVAS_INVALID_ENUM;
        return 0;
    }

    if (index >= m_maxVertexAttribs) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:index must be smaller than "
                                               << m_maxVertexAttribs;
        m_error |= CANVAS_INVALID_VALUE;
        return 0;
    }

    glGetVertexAttribPointerv(index, GLenum(pname), (GLvoid**) &offset);
    logAllGLErrors(__FUNCTION__);
    return offset;
}

/*!
 * \qmlmethod variant Context3D::getVertexAttrib(int index, glEnums pname)
 * Returns the requested parameter \a pname of the specified generic vertex attribute pointer
 * \a index. The type returned is dependent on the requested \a pname, as shown in the table:
 * \table
 * \header
 *   \li pname
 *   \li Returned Type
 * \row
 *   \li \c{Context3D.VERTEX_ATTRIB_ARRAY_BUFFER_BINDING}
 *   \li \c{Canvas3DBuffer}
 * \row
 *   \li \c{Context3D.VERTEX_ATTRIB_ARRAY_ENABLED}
 *   \li \c{boolean}
 * \row
 *   \li \c{Context3D.VERTEX_ATTRIB_ARRAY_SIZE}
 *   \li \c{int}
 * \row
 *   \li \c{Context3D.VERTEX_ATTRIB_ARRAY_STRIDE}
 *   \li \c{int}
 * \row
 *   \li \c{Context3D.VERTEX_ATTRIB_ARRAY_TYPE}
 *   \li \c{glEnums}
 * \row
 *   \li \c{Context3D.VERTEX_ATTRIB_ARRAY_NORMALIZED}
 *   \li \c{boolean}
 * \row
 *   \li \c{Context3D.CURRENT_VERTEX_ATTRIB}
 *   \li \c{Float32Array} (with 4 elements)
 *  \endtable
 */
/*!
 * \internal
 */
QJSValue CanvasContext::getVertexAttrib(uint index, glEnums pname)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(index" << index
                                         << ", pname:" << glEnumToString(pname)
                                         << ")";

    if (index >= MAX_VERTEX_ATTRIBS) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_VALUE:index must be smaller than "
                                               << "MAX_VERTEX_ATTRIBS = " << MAX_VERTEX_ATTRIBS;
        m_error |= CANVAS_INVALID_VALUE;
    } else {
        switch (pname) {
        case VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: {
            GLint value = 0;
            glGetVertexAttribiv(index, GLenum(pname), &value);
            logAllGLErrors(__FUNCTION__);
            if (value == 0 || !m_idToCanvasBufferMap.contains(value))
                return QJSValue(QJSValue::NullValue);

            return m_engine->newQObject(m_idToCanvasBufferMap[value]);
        }
        case VERTEX_ATTRIB_ARRAY_ENABLED: {
            GLint value = 0;
            glGetVertexAttribiv(index, GLenum(pname), &value);
            logAllGLErrors(__FUNCTION__);
            return QJSValue(bool(value));
        }
        case VERTEX_ATTRIB_ARRAY_SIZE: {
            GLint value = 0;
            glGetVertexAttribiv(index, GLenum(pname), &value);
            logAllGLErrors(__FUNCTION__);
            return QJSValue(value);
        }
        case VERTEX_ATTRIB_ARRAY_STRIDE: {
            GLint value = 0;
            glGetVertexAttribiv(index, GLenum(pname), &value);
            logAllGLErrors(__FUNCTION__);
            return QJSValue(value);
        }
        case VERTEX_ATTRIB_ARRAY_TYPE: {
            GLint value = 0;
            glGetVertexAttribiv(index, GLenum(pname), &value);
            logAllGLErrors(__FUNCTION__);
            return QJSValue(value);
        }
        case VERTEX_ATTRIB_ARRAY_NORMALIZED: {
            GLint value = 0;
            glGetVertexAttribiv(index, GLenum(pname), &value);
            logAllGLErrors(__FUNCTION__);
            return QJSValue(bool(value));
        }
        case CURRENT_VERTEX_ATTRIB: {
            QV4::Scope scope(m_v4engine);
            QV4::Scoped<QV4::ArrayBuffer> buffer(scope,
                                                 m_v4engine->memoryManager->alloc<QV4::ArrayBuffer>(
                                                     m_v4engine,
                                                     sizeof(float) * 4));

            glGetVertexAttribfv(index, GLenum(pname), (float *) buffer->data());
            logAllGLErrors(__FUNCTION__);

            QV4::ScopedFunctionObject constructor(scope,
                                                  m_v4engine->typedArrayCtors[
                                                  QV4::Heap::TypedArray::Float32Array]);
            QV4::ScopedCallData callData(scope, 1);
            callData->args[0] = buffer;
            return QJSValue(m_v4engine, constructor->construct(callData));
        }
        default:
            qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                                   << ":INVALID_ENUM:pname " << pname;
            m_error |= CANVAS_INVALID_ENUM;
        }
    }

    return QJSValue(QJSValue::NullValue);
}

/*!
 * \qmlmethod variant Context3D::getUniform(Canvas3DProgram program, Canvas3DUniformLocation location3D)
 * Returns the uniform value at the given \a location3D in the \a program.
 * The type returned is dependent on the uniform type, as shown in the table:
 * \table
 * \header
 *   \li Uniform Type
 *   \li Returned Type
 * \row
 *   \li boolean
 *   \li boolean
 * \row
 *   \li int
 *   \li int
 * \row
 *   \li float
 *   \li float
 * \row
 *   \li vec2
 *   \li Float32Array (with 2 elements)
 * \row
 *   \li vec3
 *   \li Float32Array (with 3 elements)
 * \row
 *   \li vec4
 *   \li Float32Array (with 4 elements)
 * \row
 *   \li ivec2
 *   \li Int32Array (with 2 elements)
 * \row
 *   \li ivec3
 *   \li Int32Array (with 3 elements)
 * \row
 *   \li ivec4
 *   \li Int32Array (with 4 elements)
 * \row
 *   \li bvec2
 *   \li sequence<boolean> (with 2 elements)
 * \row
 *   \li bvec3
 *   \li sequence<boolean> (with 3 elements)
 * \row
 *   \li bvec4
 *   \li sequence<boolean> (with 4 elements)
 * \row
 *   \li mat2
 *   \li Float32Array (with 4 elements)
 * \row
 *   \li mat3
 *   \li Float32Array (with 9 elements)
 * \row
 *   \li mat4
 *   \li Float32Array (with 16 elements)
 * \row
 *   \li sampler2D
 *   \li int
 * \row
 *   \li samplerCube
 *   \li int
 *  \endtable
 */
/*!
 * \internal
 */
QJSValue CanvasContext::getUniform(QJSValue program3D, QJSValue location3D)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(program" << program3D.toString()
                                         << ", location3D:" << location3D.toString()
                                         << ")";

    CanvasProgram *program = getAsProgram3D(program3D);
    CanvasUniformLocation *location = getAsUniformLocation3D(location3D);

    if (!program) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_OPERATION:No program was specified";
        m_error |= CANVAS_INVALID_OPERATION;
        return QJSValue(QJSValue::UndefinedValue);

    } else  if (!location) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_OPERATION:No location3D was specified";
        m_error |= CANVAS_INVALID_OPERATION;
        return QJSValue(QJSValue::UndefinedValue);
    }

    if (!checkParent(program, __FUNCTION__) || !checkParent(location, __FUNCTION__))
        return QJSValue(QJSValue::UndefinedValue);

    uint programId = program->id();
    uint locationId = location->id();
    int type = location->type();

    if (type < 0) {
        // Resolve location type.
        // There is no easy way to determine this, as the active uniform
        // indices do not usually match the uniform locations. We must query
        // active uniforms until we hit the one we want. This is obviously
        // extremely inefficient, but luckily getUniform is not something most
        // users typically need or use.

        const int maxCharCount = 512;
        GLsizei length;
        GLint size;
        GLenum glType;
        char nameBuf[maxCharCount];
        GLint uniformCount = 0;
        glGetProgramiv(programId, GL_ACTIVE_UNIFORMS, &uniformCount);
        // Strip any [] from the uniform name, unless part of struct
        QByteArray strippedName = location->name().toLatin1();
        int idx = strippedName.indexOf('[');
        if (idx >= 0) {
            // Don't truncate if part of struct
            if (strippedName.indexOf('.') == -1)
                strippedName.truncate(idx);
        }
        for (int i = 0; i < uniformCount; i++) {
            nameBuf[0] = '\0';
            glGetActiveUniform(programId, i, maxCharCount, &length, &size, &glType, nameBuf);
            QByteArray activeName(nameBuf, length);
            idx = activeName.indexOf('[');
            if (idx >= 0) {
                // Don't truncate if part of struct
                if (activeName.indexOf('.') == -1)
                    activeName.truncate(idx);
            }

            if (activeName == strippedName) {
                type = glType;
                location->setType(type);
                break;
            }
        }
    }

    if (type < 0) {
        qCWarning(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                               << ":INVALID_OPERATION:Uniform type could not be determined";
        m_error |= CANVAS_INVALID_OPERATION;
        return QJSValue(QJSValue::UndefinedValue);
    } else {
        int numValues = 4;
        switch (type) {
        case SAMPLER_2D:
            // Intentional flow through
        case SAMPLER_CUBE:
            // Intentional flow through
        case INT: {
            GLint value = 0;
            glGetUniformiv(programId, locationId, &value);
            logAllGLErrors(__FUNCTION__);
            return QJSValue(value);
        }
        case FLOAT: {
            GLfloat value = 0;
            glGetUniformfv(programId, locationId, &value);
            logAllGLErrors(__FUNCTION__);
            return QJSValue(value);
        }
        case BOOL: {
            GLint value = 0;
            glGetUniformiv(programId, locationId, &value);
            logAllGLErrors(__FUNCTION__);
            return QJSValue(bool(value));
        }
        case INT_VEC2:
            numValues--;
            // Intentional flow through
        case INT_VEC3:
            numValues--;
            // Intentional flow through
        case INT_VEC4: {
            QV4::Scope scope(m_v4engine);
            QV4::Scoped<QV4::ArrayBuffer> buffer(scope,
                                                 m_v4engine->memoryManager->alloc<QV4::ArrayBuffer>(
                                                     m_v4engine,
                                                     sizeof(int) * numValues));
            glGetUniformiv(programId, locationId, (int *) buffer->data());
            logAllGLErrors(__FUNCTION__);

            QV4::ScopedFunctionObject constructor(scope,
                                                  m_v4engine->typedArrayCtors[
                                                  QV4::Heap::TypedArray::Int32Array]);
            QV4::ScopedCallData callData(scope, 1);
            callData->args[0] = buffer;
            return QJSValue(m_v4engine, constructor->construct(callData));
        }
        case FLOAT_VEC2:
            numValues--;
            // Intentional flow through
        case FLOAT_VEC3:
            numValues--;
            // Intentional flow through
        case FLOAT_VEC4: {
            QV4::Scope scope(m_v4engine);
            QV4::Scoped<QV4::ArrayBuffer> buffer(scope,
                                                 m_v4engine->memoryManager->alloc<QV4::ArrayBuffer>(
                                                     m_v4engine,
                                                     sizeof(float) * numValues));
            glGetUniformfv(programId, locationId, (float *) buffer->data());
            logAllGLErrors(__FUNCTION__);

            QV4::ScopedFunctionObject constructor(scope,
                                                  m_v4engine->typedArrayCtors[
                                                  QV4::Heap::TypedArray::Float32Array]);
            QV4::ScopedCallData callData(scope, 1);
            callData->args[0] = buffer;
            return QJSValue(m_v4engine, constructor->construct(callData));
        }
        case BOOL_VEC2:
            numValues--;
            // Intentional flow through
        case BOOL_VEC3:
            numValues--;
            // Intentional flow through
        case BOOL_VEC4: {
            GLint *value = new GLint[numValues];
            QJSValue array = m_engine->newArray(numValues);

            glGetUniformiv(programId, locationId, value);
            logAllGLErrors(__FUNCTION__);

            for (int i = 0; i < numValues; i++)
                array.setProperty(i, bool(value[i]));

            return array;
        }
        case FLOAT_MAT2:
            numValues--;
            // Intentional flow through
        case FLOAT_MAT3:
            numValues--;
            // Intentional flow through
        case FLOAT_MAT4: {
            numValues = numValues * numValues;


            QV4::Scope scope(m_v4engine);
            QV4::Scoped<QV4::ArrayBuffer> buffer(scope,
                                                 m_v4engine->memoryManager->alloc<QV4::ArrayBuffer>(
                                                     m_v4engine,
                                                     sizeof(float) * numValues));
            glGetUniformfv(programId, locationId, (float *) buffer->data());
            logAllGLErrors(__FUNCTION__);

            QV4::ScopedFunctionObject constructor(scope,
                                                  m_v4engine->typedArrayCtors[
                                                  QV4::Heap::TypedArray::Float32Array]);
            QV4::ScopedCallData callData(scope, 1);
            callData->args[0] = buffer;
            return QJSValue(m_v4engine, constructor->construct(callData));
        }
        default:
            break;
        }
    }
    return QJSValue(QJSValue::UndefinedValue);
}

/*!
 * \qmlmethod list<variant> Context3D::getSupportedExtensions()
 * Returns an array of the extension strings supported by this implementation
 */
/*!
 * \internal
 */
QVariantList CanvasContext::getSupportedExtensions()
{
    qCDebug(canvas3drendering).nospace() << Q_FUNC_INFO;

    QVariantList list;
    list.append(QVariant::fromValue(QStringLiteral("QTCANVAS3D_gl_state_dump")));

    if (!m_isOpenGLES2 ||
            (m_context->format().majorVersion() >= 3
             || m_extensions.contains("OES_standard_derivatives"))) {
        list.append(QVariant::fromValue(QStringLiteral("OES_standard_derivatives")));
    }

    if (m_extensions.contains("GL_EXT_texture_compression_s3tc"))
        list.append(QVariant::fromValue(QStringLiteral("WEBGL_compressed_texture_s3tc")));

    if (m_extensions.contains("IMG_texture_compression_pvrtc"))
        list.append(QVariant::fromValue(QStringLiteral("WEBGL_compressed_texture_pvrtc")));

    return list;
}

/*!
 * \internal
 */
bool CanvasContext::isOfType(const QJSValue &value, const char *classname) const
{
    if (!value.isQObject()) {
        return false;
    }

    QObject *obj = value.toQObject();

    if (!obj)
        return false;

    if (!obj->inherits(classname)) {
        return false;
    }

    return true;
}

/*!
 * \qmlmethod variant Context3D::getExtension(string name)
 * \return object if given \a name matches a supported extension.
 * Otherwise returns \c{null}. The returned object may contain constants and/or functions provided
 * by the extension, but at minimum a unique object is returned.
 * Case-insensitive \a name of the extension to be returned.
 */
/*!
 * \internal
 */
QVariant CanvasContext::getExtension(const QString &name)
{
    qCDebug(canvas3drendering).nospace() << "Context3D::" << __FUNCTION__
                                         << "(name:" << name
                                         << ")";

    QString upperCaseName = name.toUpper();

    if (upperCaseName == QStringLiteral("QTCANVAS3D_GL_STATE_DUMP")) {
        if (!m_stateDumpExt)
            m_stateDumpExt = new CanvasGLStateDump(m_context, this);
        return QVariant::fromValue(m_stateDumpExt);
    } else if (upperCaseName == QStringLiteral("OES_STANDARD_DERIVATIVES") &&
               m_extensions.contains("OES_standard_derivatives")) {
        if (!m_standardDerivatives)
            m_standardDerivatives = new QObject(this);
        return QVariant::fromValue(m_standardDerivatives);
    } else if (upperCaseName == QStringLiteral("WEBGL_COMPRESSED_TEXTURE_S3TC") &&
               m_extensions.contains("GL_EXT_texture_compression_s3tc")) {
        if (!m_compressedTextureS3TC)
            m_compressedTextureS3TC = new CompressedTextureS3TC(this);
        return QVariant::fromValue(m_compressedTextureS3TC);
    } else if (upperCaseName == QStringLiteral("WEBGL_COMPRESSED_TEXTURE_PVRTC") &&
               m_extensions.contains("IMG_texture_compression_pvrtc")) {
        if (!m_compressedTexturePVRTC)
            m_compressedTexturePVRTC = new CompressedTexturePVRTC(this);
        return QVariant::fromValue(m_compressedTexturePVRTC);
    }

    return QVariant(QVariant::Int);
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
