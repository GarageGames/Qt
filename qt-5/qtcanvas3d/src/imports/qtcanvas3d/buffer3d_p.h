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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QtCanvas3D API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#ifndef BUFFER3D_P_H
#define BUFFER3D_P_H

#include "context3d_p.h"
#include "abstractobject3d_p.h"

#include <QOpenGLFunctions>

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

class CanvasBuffer : public CanvasAbstractObject, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    enum bindTarget {
        UNINITIALIZED = 0,
        ARRAY_BUFFER,
        ELEMENT_ARRAY_BUFFER
    };

    explicit CanvasBuffer(QObject *parent = 0);
    CanvasBuffer(const CanvasBuffer& other);
    virtual ~CanvasBuffer();

    void del();
    bool isAlive();
    GLuint id();
    bindTarget target();
    void setTarget(bindTarget bindPoint);

    friend QDebug operator<< (QDebug d, const CanvasBuffer *buffer);

private:
    GLuint m_bufferId;
    bindTarget m_bindTarget;
};

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE

#endif // BUFFER3D_P_H
