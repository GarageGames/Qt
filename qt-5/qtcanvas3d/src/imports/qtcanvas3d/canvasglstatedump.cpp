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
#include "enumtostringmap_p.h"

#include <QDebug>
#include <QColor>

#define BOOL_TO_STR(a) a ? "true" : "false"

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

/*!
   \qmltype GLStateDumpExt
   \since QtCanvas3D 1.0
   \inqmlmodule QtCanvas3D
   \brief Provides means to print current GL driver state info.

   An uncreatable QML type that provides an extension API that can be used to the dump current OpenGL
   driver state as a string that can be then, for example, be printed on the console log. You can get it by
   calling \l{Context3D::getExtension}{Context3D.getExtension} with "QTCANVAS3D_gl_state_dump"
   as parameter.

   Typical usage could be something like this:
   \code
    // Declare the variable to contain the extension
    var stateDumpExt;
    .
    .
    // After the context has been created from Canvas3D, get the extension
    stateDumpExt = gl.getExtension("QTCANVAS3D_gl_state_dump");
    .
    .
    // When you want to print the current GL state with everything enabled
    // Check that you indeed have a valid extension (for portability) then use it
    if (stateDumpExt)
        log("GL STATE DUMP:\n"+stateDumpExt.getGLStateDump(stateDumpExt.DUMP_FULL));
    \endcode

   \sa Context3D
 */
CanvasGLStateDump::CanvasGLStateDump(QOpenGLContext *context, QObject *parent) :
    QObject(parent),
    QOpenGLFunctions(context),
    m_map(EnumToStringMap::newInstance())
{
    m_isOpenGLES2 = context->isOpenGLES();
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &m_maxVertexAttribs);
}

/*!
 * \internal
 */
CanvasGLStateDump::~CanvasGLStateDump()
{
    EnumToStringMap::deleteInstance();
    m_map = 0;
}

/*!
 * \internal
 */
QString CanvasGLStateDump::getGLArrayObjectDump(int target, int arrayObject, int type)
{
    if (!arrayObject)
        return "no buffer bound";

    QString stateDumpStr;
    glBindBuffer(target, arrayObject);

    GLint size;
    glGetBufferParameteriv(target, GL_BUFFER_SIZE, &size);

    if (type == GL_FLOAT) {
        stateDumpStr.append("ARRAY_BUFFER_TYPE......................FLOAT\n");

        stateDumpStr.append("ARRAY_BUFFER_SIZE......................");
        stateDumpStr.append(QString::number(size));
        stateDumpStr.append("\n");

    } else if (type == GL_UNSIGNED_SHORT) {
        stateDumpStr.append("ARRAY_BUFFER_TYPE......................UNSIGNED_SHORT\n");

        stateDumpStr.append("ARRAY_BUFFER_SIZE......................");
        stateDumpStr.append(QString::number(size));
        stateDumpStr.append("\n");
    }

    return stateDumpStr;
}

/*!
 * \qmlmethod string GLStateDumpExt::getGLStateDump(stateDumpEnums options)
 * \return OpenGL driver state with given options as a human readable string that can be printed.
 * Optional paremeter \a options may contain bitfields masked together from following options:
 * \list
 * \li \c{GLStateDumpExt.DUMP_BASIC_ONLY} Includes only very basic OpenGL state information.
 * \li \c{GLStateDumpExt.DUMP_VERTEX_ATTRIB_ARRAYS_BIT} Includes all vertex attribute array
 * information.
 * \li \c{GLStateDumpExt.DUMP_VERTEX_ATTRIB_ARRAYS_BUFFERS_BIT} Includes size and type
 * from all currently active vertex attribute arrays (including the currently bound element array)
 * to verify that there are actual values in the array.
 * \li \c{GLStateDumpExt.DUMP_FULL} Includes everything.
 * \endlist
 */
QString CanvasGLStateDump::getGLStateDump(CanvasGLStateDump::stateDumpEnums options)
{
#if !defined(QT_OPENGL_ES_2)
    GLint drawFramebuffer;
    GLint readFramebuffer;
    GLboolean polygonOffsetLineEnabled;
    GLboolean polygonOffsetPointEnabled;
    GLint boundVertexArray;
#endif

    QString stateDumpStr;
    GLint renderbuffer;
    GLfloat clearColor[4];
    GLfloat clearDepth;
    GLboolean isBlendingEnabled = glIsEnabled(GL_BLEND);
    GLboolean isDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLint depthFunc;
    GLboolean isDepthWriteEnabled;
    GLint currentProgram;
    GLint *vertexAttribArrayEnabledStates = new GLint[m_maxVertexAttribs];
    GLint *vertexAttribArrayBoundBuffers = new GLint[m_maxVertexAttribs];
    GLint *vertexAttribArraySizes = new GLint[m_maxVertexAttribs];
    GLint *vertexAttribArrayTypes = new GLint[m_maxVertexAttribs];
    GLint *vertexAttribArrayNormalized = new GLint[m_maxVertexAttribs];
    GLint *vertexAttribArrayStrides = new GLint[m_maxVertexAttribs];
    GLint activeTexture;
    GLint texBinding2D;
    GLint arrayBufferBinding;
    GLint frontFace;
    GLboolean isCullFaceEnabled = glIsEnabled(GL_CULL_FACE);
    GLint cullFaceMode;
    GLint blendEquationRGB;
    GLint blendEquationAlpha;

    GLint blendDestAlpha;
    GLint blendDestRGB;
    GLint blendSrcAlpha;
    GLint blendSrcRGB;
    GLint scissorBox[4];
    GLboolean isScissorTestEnabled = glIsEnabled(GL_SCISSOR_TEST);
    GLint boundElementArrayBuffer;
    GLboolean polygonOffsetFillEnabled;
    GLfloat polygonOffsetFactor;
    GLfloat polygonOffsetUnits;

#if !defined(QT_OPENGL_ES_2)
    if (!m_isOpenGLES2) {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
        glGetBooleanv(GL_POLYGON_OFFSET_LINE, &polygonOffsetLineEnabled);
        glGetBooleanv(GL_POLYGON_OFFSET_POINT, &polygonOffsetPointEnabled);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &boundVertexArray);
    }
#endif

    glGetBooleanv(GL_DEPTH_WRITEMASK, &isDepthWriteEnabled);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
    glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth);
    glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
    glGetBooleanv(GL_POLYGON_OFFSET_FILL, &polygonOffsetFillEnabled);
    glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &polygonOffsetFactor);
    glGetFloatv(GL_POLYGON_OFFSET_UNITS, &polygonOffsetUnits);

    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texBinding2D );
    glGetIntegerv(GL_FRONT_FACE, &frontFace);
    glGetIntegerv(GL_CULL_FACE_MODE, &cullFaceMode);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRGB);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDestAlpha);
    glGetIntegerv(GL_BLEND_DST_RGB, &blendDestRGB);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
    glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRGB);
    glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &boundElementArrayBuffer);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBufferBinding);

#if !defined(QT_OPENGL_ES_2)
    if (!m_isOpenGLES2) {
        stateDumpStr.append("GL_DRAW_FRAMEBUFFER_BINDING.....");
        stateDumpStr.append(QString::number(drawFramebuffer));
        stateDumpStr.append("\n");

        stateDumpStr.append("GL_READ_FRAMEBUFFER_BINDING.....");
        stateDumpStr.append(QString::number(readFramebuffer));
        stateDumpStr.append("\n");
    }
#endif

    stateDumpStr.append("GL_RENDERBUFFER_BINDING.........");
    stateDumpStr.append(QString::number(renderbuffer));
    stateDumpStr.append("\n");

    stateDumpStr.append("GL_SCISSOR_TEST.................");
    stateDumpStr.append(BOOL_TO_STR(isScissorTestEnabled));
    stateDumpStr.append("\n");

    stateDumpStr.append("GL_SCISSOR_BOX..................");
    stateDumpStr.append(QString::number(scissorBox[0]));
    stateDumpStr.append(", ");
    stateDumpStr.append(QString::number(scissorBox[1]));
    stateDumpStr.append(", ");
    stateDumpStr.append(QString::number(scissorBox[2]));
    stateDumpStr.append(", ");
    stateDumpStr.append(QString::number(scissorBox[3]));
    stateDumpStr.append("\n");

    stateDumpStr.append("GL_COLOR_CLEAR_VALUE............");
    stateDumpStr.append("r:");
    stateDumpStr.append(QString::number(clearColor[0]));
    stateDumpStr.append(" g:");
    stateDumpStr.append(QString::number(clearColor[1]));
    stateDumpStr.append(" b:");
    stateDumpStr.append(QString::number(clearColor[2]));
    stateDumpStr.append(" a:");
    stateDumpStr.append(QString::number(clearColor[3]));
    stateDumpStr.append("\n");

    stateDumpStr.append("GL_DEPTH_CLEAR_VALUE............");
    stateDumpStr.append(QString::number(clearDepth));
    stateDumpStr.append("\n");

    stateDumpStr.append("GL_BLEND........................");
    stateDumpStr.append(BOOL_TO_STR(isBlendingEnabled));
    stateDumpStr.append("\n");

    stateDumpStr.append("GL_BLEND_EQUATION_RGB...........");
    stateDumpStr.append(m_map->lookUp(blendEquationRGB));
    stateDumpStr.append("\n");

    stateDumpStr.append("GL_BLEND_EQUATION_ALPHA.........");
    stateDumpStr.append(m_map->lookUp(blendEquationAlpha));
    stateDumpStr.append("\n");

    stateDumpStr.append("GL_DEPTH_TEST...................");
    stateDumpStr.append(BOOL_TO_STR(isDepthTestEnabled));
    stateDumpStr.append("\n");

    stateDumpStr.append("GL_DEPTH_FUNC...................");
    stateDumpStr.append(m_map->lookUp(depthFunc));
    stateDumpStr.append("\n");

    stateDumpStr.append("GL_DEPTH_WRITEMASK..............");
    stateDumpStr.append(BOOL_TO_STR(isDepthWriteEnabled));
    stateDumpStr.append("\n");

    stateDumpStr.append("GL_POLYGON_OFFSET_FILL..........");
    stateDumpStr.append(BOOL_TO_STR(polygonOffsetFillEnabled));
    stateDumpStr.append("\n");

#if !defined(QT_OPENGL_ES_2)
    if (!m_isOpenGLES2) {
        stateDumpStr.append("GL_POLYGON_OFFSET_LINE..........");
        stateDumpStr.append(BOOL_TO_STR(polygonOffsetLineEnabled));
        stateDumpStr.append("\n");

        stateDumpStr.append("GL_POLYGON_OFFSET_POINT.........");
        stateDumpStr.append(BOOL_TO_STR(polygonOffsetPointEnabled));
        stateDumpStr.append("\n");
    }
#endif

    stateDumpStr.append("GL_POLYGON_OFFSET_FACTOR........");
    stateDumpStr.append(QString::number(polygonOffsetFactor));
    stateDumpStr.append("\n");

    stateDumpStr.append("GL_POLYGON_OFFSET_UNITS.........");
    stateDumpStr.append(QString::number(polygonOffsetUnits));
    stateDumpStr.append("\n");

    stateDumpStr.append("GL_CULL_FACE....................");
    stateDumpStr.append(BOOL_TO_STR(isCullFaceEnabled));
    stateDumpStr.append("\n");

    stateDumpStr.append("GL_CULL_FACE_MODE...............");
    stateDumpStr.append(m_map->lookUp(cullFaceMode));
    stateDumpStr.append("\n");

    stateDumpStr.append("GL_FRONT_FACE...................");
    stateDumpStr.append(m_map->lookUp(frontFace));
    stateDumpStr.append("\n");

    stateDumpStr.append("GL_CURRENT_PROGRAM..............");
    stateDumpStr.append(QString::number(currentProgram));
    stateDumpStr.append("\n");

    stateDumpStr.append("GL_ACTIVE_TEXTURE...............");
    stateDumpStr.append(m_map->lookUp(activeTexture));
    stateDumpStr.append("\n");

    stateDumpStr.append("GL_TEXTURE_BINDING_2D...........");
    stateDumpStr.append(QString::number(texBinding2D));
    stateDumpStr.append("\n");

    stateDumpStr.append("GL_ELEMENT_ARRAY_BUFFER_BINDING.");
    stateDumpStr.append(QString::number(boundElementArrayBuffer));
    stateDumpStr.append("\n");

    stateDumpStr.append("GL_ARRAY_BUFFER_BINDING.........");
    stateDumpStr.append(QString::number(arrayBufferBinding));
    stateDumpStr.append("\n");

#if !defined(QT_OPENGL_ES_2)
    if (!m_isOpenGLES2) {
        stateDumpStr.append("GL_VERTEX_ARRAY_BINDING.........");
        stateDumpStr.append(QString::number(boundVertexArray));
        stateDumpStr.append("\n");
    }
#endif

    if (options && DUMP_VERTEX_ATTRIB_ARRAYS_BIT) {
        for (int i = 0; i < m_maxVertexAttribs;i++) {
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &vertexAttribArrayEnabledStates[i]);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &vertexAttribArrayBoundBuffers[i]);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &vertexAttribArraySizes[i]);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, &vertexAttribArrayTypes[i]);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &vertexAttribArrayNormalized[i]);
            glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &vertexAttribArrayStrides[i]);
        }


        for (int i = 0; i < m_maxVertexAttribs;i++) {
            stateDumpStr.append("GL_VERTEX_ATTRIB_ARRAY_");
            stateDumpStr.append(QString::number(i));
            stateDumpStr.append("\n");

            stateDumpStr.append("GL_VERTEX_ATTRIB_ARRAY_ENABLED.........");
            stateDumpStr.append(BOOL_TO_STR(vertexAttribArrayEnabledStates[i]));
            stateDumpStr.append("\n");

            stateDumpStr.append("GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING..");
            stateDumpStr.append(QString::number(vertexAttribArrayBoundBuffers[i]));
            stateDumpStr.append("\n");

            stateDumpStr.append("GL_VERTEX_ATTRIB_ARRAY_SIZE............");
            stateDumpStr.append(QString::number(vertexAttribArraySizes[i]));
            stateDumpStr.append("\n");

            stateDumpStr.append("GL_VERTEX_ATTRIB_ARRAY_TYPE............");
            stateDumpStr.append(m_map->lookUp(vertexAttribArrayTypes[i]));
            stateDumpStr.append("\n");

            stateDumpStr.append("GL_VERTEX_ATTRIB_ARRAY_NORMALIZED......");
            stateDumpStr.append(QString::number(vertexAttribArrayNormalized[i]));
            stateDumpStr.append("\n");

            stateDumpStr.append("GL_VERTEX_ATTRIB_ARRAY_STRIDE..........");
            stateDumpStr.append(QString::number(vertexAttribArrayStrides[i]));
            stateDumpStr.append("\n");
        }
    }

    if (options && DUMP_VERTEX_ATTRIB_ARRAYS_BUFFERS_BIT) {
        if (boundElementArrayBuffer != 0) {
            stateDumpStr.append("GL_ELEMENT_ARRAY_BUFFER................");
            stateDumpStr.append(QString::number(boundElementArrayBuffer));
            stateDumpStr.append("\n");

            stateDumpStr.append(getGLArrayObjectDump(GL_ELEMENT_ARRAY_BUFFER,
                                                     boundElementArrayBuffer,
                                                     GL_UNSIGNED_SHORT));
        }

        for (int i = 0; i < m_maxVertexAttribs;i++) {
            if (vertexAttribArrayEnabledStates[i]) {
                stateDumpStr.append("GL_ARRAY_BUFFER........................");
                stateDumpStr.append(QString::number(vertexAttribArrayBoundBuffers[i]));
                stateDumpStr.append("\n");

                stateDumpStr.append(getGLArrayObjectDump(GL_ARRAY_BUFFER,
                                                         vertexAttribArrayBoundBuffers[i],
                                                         vertexAttribArrayTypes[i]));
            }
        }
    }


    delete[] vertexAttribArrayEnabledStates;
    delete[] vertexAttribArrayBoundBuffers;
    delete[] vertexAttribArraySizes;
    delete[] vertexAttribArrayTypes;
    delete[] vertexAttribArrayNormalized;
    delete[] vertexAttribArrayStrides;

    return stateDumpStr;
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
