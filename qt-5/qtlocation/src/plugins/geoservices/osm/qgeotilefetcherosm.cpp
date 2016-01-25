/****************************************************************************
**
** Copyright (C) 2013 Aaron McCarthy <mccarthy.aaron@gmail.com>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

#include "qgeotilefetcherosm.h"
#include "qgeomapreplyosm.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtLocation/private/qgeotilespec_p.h>

QT_BEGIN_NAMESPACE

QGeoTileFetcherOsm::QGeoTileFetcherOsm(QObject *parent)
:   QGeoTileFetcher(parent), m_networkManager(new QNetworkAccessManager(this)),
    m_userAgent("Qt Location based application")
{
}

void QGeoTileFetcherOsm::setUserAgent(const QByteArray &userAgent)
{
    m_userAgent = userAgent;
}

void QGeoTileFetcherOsm::setUrlPrefix(const QString &urlPrefix)
{
    m_urlPrefix = urlPrefix;
}

QGeoTiledMapReply *QGeoTileFetcherOsm::getTileImage(const QGeoTileSpec &spec)
{
    QNetworkRequest request;
    request.setRawHeader("User-Agent", m_userAgent);

    QString urlPrefix;

    switch (spec.mapId()) {
    case 1:
        urlPrefix = QStringLiteral("http://otile1.mqcdn.com/tiles/1.0.0/map/");
        break;
    case 2:
        urlPrefix = QStringLiteral("http://otile1.mqcdn.com/tiles/1.0.0/sat/");
        break;
    case 3:
        urlPrefix = QStringLiteral("http://a.tile.thunderforest.com/cycle/");
        break;
    case 4:
        urlPrefix = QStringLiteral("http://a.tile.thunderforest.com/transport/");
        break;
    case 5:
        urlPrefix = QStringLiteral("http://a.tile.thunderforest.com/transport-dark/");
        break;
    case 6:
        urlPrefix = QStringLiteral("http://a.tile.thunderforest.com/landscape/");
        break;
    case 7:
        urlPrefix = QStringLiteral("http://a.tile.thunderforest.com/outdoors/");
        break;
    case 8:
        urlPrefix = m_urlPrefix;
        break;
    default:
        qWarning("Unknown map id %d\n", spec.mapId());
    }

    request.setUrl(QUrl(urlPrefix + QString::number(spec.zoom()) + QLatin1Char('/') +
                        QString::number(spec.x()) + QLatin1Char('/') +
                        QString::number(spec.y()) + QStringLiteral(".png")));

    QNetworkReply *reply = m_networkManager->get(request);

    return new QGeoMapReplyOsm(reply, spec);
}

QT_END_NAMESPACE
