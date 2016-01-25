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

#ifndef QCANVAS3D_PLUGIN_H
#define QCANVAS3D_PLUGIN_H

#include "canvas3d_p.h"
#include "texture3d_p.h"
#include "shader3d_p.h"
#include "context3d_p.h"
#include "program3d_p.h"
#include "buffer3d_p.h"
#include "uniformlocation_p.h"
#include "teximage3d_p.h"
#include "contextattributes_p.h"
#include "framebuffer3d_p.h"
#include "renderbuffer3d_p.h"
#include "shaderprecisionformat_p.h"
#include "activeinfo3d_p.h"
#include "canvasglstatedump_p.h"

#include <QQmlExtensionPlugin>

using namespace QtCanvas3D;

Q_DECLARE_METATYPE(CanvasBuffer)

QML_DECLARE_TYPE(Canvas)
QML_DECLARE_TYPE(CanvasContext)
QML_DECLARE_TYPE(CanvasTexture)
QML_DECLARE_TYPE(CanvasShader)
QML_DECLARE_TYPE(CanvasProgram)
QML_DECLARE_TYPE(CanvasBuffer)
QML_DECLARE_TYPE(CanvasUniformLocation)
QML_DECLARE_TYPE(CanvasTextureImage)
QML_DECLARE_TYPE(CanvasTextureImageFactory)
QML_DECLARE_TYPE(CanvasContextAttributes)
QML_DECLARE_TYPE(CanvasFrameBuffer)
QML_DECLARE_TYPE(CanvasRenderBuffer)
QML_DECLARE_TYPE(CanvasShaderPrecisionFormat)
QML_DECLARE_TYPE(CanvasActiveInfo)
QML_DECLARE_TYPE(CanvasGLStateDump)

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

class QtCanvas3DPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface")

public:
    void registerTypes(const char *uri);
};

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE

#endif // QCANVAS3D_PLUGIN_H

