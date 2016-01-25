/****************************************************************************
**
** Copyright (C) 2013 Petroules Corporation.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtMacExtras module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
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
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qmacfunctions.h"
#include "qmacfunctions_p.h"

#import <UIKit/UIKit.h>

QT_BEGIN_NAMESPACE

namespace QtMac
{

CGContextRef currentCGContext()
{
    return UIGraphicsGetCurrentContext();
}

/*!
    \fn void QtMac::setApplicationIconBadgeNumber(int number)

    Sets the value shown on the application icon a.k.a badge to \a number.

    Unlike its OS X counterpart, only numbers can be used.

    \sa applicationIconBadgeNumber(), setBadgeLabelText()
*/
void setApplicationIconBadgeNumber(int number)
{
    [[UIApplication sharedApplication] setApplicationIconBadgeNumber:number];
}

/*!
    \fn int QtMac::applicationIconBadgeNumber()

    Returns the value of the application icon a.k.a badge.

    \sa setApplicationIconBadgeNumber(), badgeLabelText()
*/
int applicationIconBadgeNumber()
{
    return [[UIApplication sharedApplication] applicationIconBadgeNumber];
}

} // namespace QtMac

QT_END_NAMESPACE
