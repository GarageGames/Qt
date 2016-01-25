/****************************************************************************
 **
 ** Copyright (C) 2013 Ivan Vizir <define-true-false@yandex.com>
 ** Contact: http://www.qt.io/licensing/
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#include "qwineventfilter_p.h"
#include "qwinfunctions.h"
#include "qwinevent.h"
#include <QGuiApplication>
#include <QWindow>

#ifndef WM_DWMCOLORIZATIONCOLORCHANGED
#   define WM_DWMCOLORIZATIONCOLORCHANGED 0x0320
#endif

#ifndef WM_DWMCOMPOSITIONCHANGED
#   define WM_DWMCOMPOSITIONCHANGED       0x031E
#endif

QWinEventFilter *QWinEventFilter::instance = 0;

QWinEventFilter::QWinEventFilter() :
    tbButtonCreatedMsgId(RegisterWindowMessageW(L"TaskbarButtonCreated"))
{
}

QWinEventFilter::~QWinEventFilter()
{
    instance = 0;
}

bool QWinEventFilter::nativeEventFilter(const QByteArray &, void *message, long *result)
{
    MSG *msg = static_cast<MSG *>(message);
    bool filterOut = false;

    QEvent *event = 0;
    QWindow *window = 0;
    switch (msg->message) {
    case WM_DWMCOLORIZATIONCOLORCHANGED :
        event = new QWinColorizationChangeEvent(msg->wParam, msg->lParam);
        break;
    case WM_DWMCOMPOSITIONCHANGED :
        event = new QWinCompositionChangeEvent(QtWin::isCompositionEnabled());
        break;
    case WM_THEMECHANGED :
        event = new QWinEvent(QWinEvent::ThemeChange);
        break;
    default :
        if (tbButtonCreatedMsgId == msg->message) {
            event = new QWinEvent(QWinEvent::TaskbarButtonCreated);
            filterOut = true;
        }
        break;
    }

    if (event) {
        window = findWindow(msg->hwnd);
        if (window)
            QCoreApplication::sendEvent(window, event);
        delete event;
    }

    if (filterOut && result) {
        *result = 0;
    }

    return filterOut;
}

void QWinEventFilter::setup()
{
    if (!instance) {
        instance = new QWinEventFilter;
        qApp->installNativeEventFilter(instance);
    }
}

QWindow *QWinEventFilter::findWindow(HWND handle)
{
    const WId wid = reinterpret_cast<WId>(handle);
    foreach (QWindow *topLevel, QGuiApplication::topLevelWindows()) {
        if (topLevel->handle() && topLevel->winId() == wid)
            return topLevel;
    }
    return Q_NULLPTR;
}
