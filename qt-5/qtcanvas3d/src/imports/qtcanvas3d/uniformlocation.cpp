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

#include "uniformlocation_p.h"

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

/*!
 * \qmltype Canvas3DUniformLocation
 * \since QtCanvas3D 1.0
 * \inqmlmodule QtCanvas3D
 * \brief Contains uniform location id.
 *
 * An uncreatable QML type that contains an uniform location id. You can get it by calling
 * the \l{Context3D::getUniformLocation()}{Context3D.getUniformLocation()} method.
 */

/*!
 * \internal
 */
CanvasUniformLocation::CanvasUniformLocation(int location, QObject *parent) :
    CanvasAbstractObject(parent),
    m_location(location),
    m_type(-1)
{
}

/*!
 * \internal
 */
CanvasUniformLocation::~CanvasUniformLocation()
{
}

/*!
 * \internal
 */
int CanvasUniformLocation::id()
{
    return m_location;
}

/*!
 * \internal
 */
int CanvasUniformLocation::type()
{
    return m_type;
}

/*!
 * \internal
 */
void CanvasUniformLocation::setType(int type)
{
    m_type = type;
}

/*!
 * \internal
 */
QDebug operator<<(QDebug dbg, const CanvasUniformLocation *uLoc)
{
    if (uLoc)
        dbg.nospace() << "Canvas3DUniformLocation("<< (void *) uLoc << ", name:"<< uLoc->name() <<", location:" << uLoc->m_location << ")";
    else
        dbg.nospace() << "Canvas3DUniformLocation("<< (void *)(uLoc) << ")";

    return dbg.maybeSpace();
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
