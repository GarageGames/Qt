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

#include "shader3d_p.h"

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

/*!
 * \qmltype Canvas3DShader
 * \since QtCanvas3D 1.0
 * \inqmlmodule QtCanvas3D
 * \brief Contains a shader.
 *
 * An uncreatable QML type that contains a shader. You can get it by calling
 * the \l{Context3D::createShader()}{Context3D.createShader()} method.
 */

/*!
 * \internal
 */
CanvasShader::CanvasShader(QOpenGLShader::ShaderType type, QObject *parent) :
    CanvasAbstractObject(parent),
    m_shader(new QOpenGLShader(type, this)),
    m_sourceCode("")
{
}

/*!
 * \internal
 */
CanvasShader::~CanvasShader()
{
    delete m_shader;
}

/*!
 * \internal
 */
GLuint CanvasShader::id()
{
    return m_shader->shaderId();
}

/*!
 * \internal
 */
bool CanvasShader::isAlive()
{
    return bool(m_shader);
}

/*!
 * \internal
 */
void CanvasShader::del()
{
    delete m_shader;
    m_shader = 0;
}

/*!
 * \internal
 */
QOpenGLShader *CanvasShader::qOGLShader()
{
    return m_shader;
}

/*!
 * \internal
 */
QString CanvasShader::sourceCode()
{
    return m_sourceCode;
}

/*!
 * \internal
 */
void CanvasShader::setSourceCode(const QString &source)
{
    m_sourceCode = source;
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
