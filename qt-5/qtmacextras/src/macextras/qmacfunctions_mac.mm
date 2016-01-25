/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#import <Cocoa/Cocoa.h>
#import <AppKit/NSApplication.h>

#include "qmacfunctions.h"
#include "qmacfunctions_p.h"

#include <QtCore/QString>

#if QT_VERSION > QT_VERSION_CHECK(5, 0, 0)
#include <QtGui/QWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <qpa/qplatformmenu.h>
#endif

QT_BEGIN_NAMESPACE

namespace QtMac
{

/*!
    \fn NSImage* QtMac::toNSImage(const QPixmap &pixmap)

    Creates an \c NSImage equivalent to the QPixmap \a pixmap. Returns the \c NSImage handle.

    It is the caller's responsibility to release the \c NSImage data
    after use.
*/
NSImage* toNSImage(const QPixmap &pixmap)
{
    if (pixmap.isNull())
        return 0;
    CGImageRef cgimage = toCGImageRef(pixmap);
    NSBitmapImageRep *bitmapRep = [[NSBitmapImageRep alloc] initWithCGImage:cgimage];
    NSImage *image = [[NSImage alloc] init];
    [image addRepresentation:bitmapRep];
    [bitmapRep release];
    CFRelease(cgimage);
    return image;
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
/*!
    \fn bool QtMac::isMainWindow(QWindow *window)

    Returns whether the given QWindow \a window is the application's main window
*/
bool isMainWindow(QWindow *window)
{
    NSWindow *macWindow = static_cast<NSWindow*>(
        QGuiApplication::platformNativeInterface()->nativeResourceForWindow("nswindow", window));
    if (!macWindow)
        return false;

    return [macWindow isMainWindow];
}
#endif

CGContextRef currentCGContext()
{
    return reinterpret_cast<CGContextRef>([[NSGraphicsContext currentContext] graphicsPort]);
}

/*!
    \fn void QtMac::setBadgeLabelText(const QString &text)

    Sets the \a text shown on the application icon a.k.a badge.

    This is generally used with numbers (e.g. number of unread emails); it can also show a string.

    \sa badgeLabelText()
*/
void setBadgeLabelText(const QString &text)
{
    [[[NSApplication sharedApplication] dockTile] setBadgeLabel:text.toNSString()];
}

/*!
    \fn QString QtMac::badgeLabelText()

    Returns the text of the application icon a.k.a badge.

    \sa setBadgeLabelText()
*/
QString badgeLabelText()
{
    return QString::fromNSString([[[NSApplication sharedApplication] dockTile] badgeLabel]);
}

} // namespace QtMac

QT_END_NAMESPACE
