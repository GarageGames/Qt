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

#ifndef CANVASRENDERNODE_P_H
#define CANVASRENDERNODE_P_H

#include "canvas3dcommon_p.h"
#include <QtQuick/QSGSimpleTextureNode>
#include <QtQuick/QQuickWindow>
#include <QtCore/QMutex>

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

class Canvas;

class CanvasRenderNode : public QObject, public QSGSimpleTextureNode
{
    Q_OBJECT

public:
    CanvasRenderNode(Canvas *canvas, QQuickWindow *window);
    ~CanvasRenderNode();

signals:
    void textureInUse();
    void pendingNewTexture();

public slots:
    void newTexture(int id, const QSize &size);
    void prepareNode();

private:
    Canvas *m_canvas;
    int m_id;
    QSize m_size;
    QMutex m_mutex;
    QSGTexture *m_texture;
    QQuickWindow *m_window;
};

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE

#endif // CANVASRENDERNODE_P_H
