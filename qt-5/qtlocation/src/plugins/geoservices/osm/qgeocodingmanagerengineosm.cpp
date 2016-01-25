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

#include "qgeocodingmanagerengineosm.h"
#include "qgeocodereplyosm.h"

#include <QtCore/QVariantMap>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QLocale>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtPositioning/QGeoCoordinate>
#include <QtPositioning/QGeoAddress>
#include <QtPositioning/QGeoShape>
#include <QtPositioning/QGeoRectangle>

QT_BEGIN_NAMESPACE

static QString addressToQuery(const QGeoAddress &address)
{
    return address.street() + QStringLiteral(", ") +
           address.district() + QStringLiteral(", ") +
           address.city() + QStringLiteral(", ") +
           address.state() + QStringLiteral(", ") +
           address.country();
}

static QString boundingBoxToLtrb(const QGeoRectangle &rect)
{
    return QString::number(rect.topLeft().longitude()) + QLatin1Char(',') +
           QString::number(rect.topLeft().latitude()) + QLatin1Char(',') +
           QString::number(rect.bottomRight().longitude()) + QLatin1Char(',') +
           QString::number(rect.bottomRight().latitude());
}

QGeoCodingManagerEngineOsm::QGeoCodingManagerEngineOsm(const QVariantMap &parameters,
                                                       QGeoServiceProvider::Error *error,
                                                       QString *errorString)
:   QGeoCodingManagerEngine(parameters), m_networkManager(new QNetworkAccessManager(this))
{
    if (parameters.contains(QStringLiteral("osm.useragent")))
        m_userAgent = parameters.value(QStringLiteral("osm.useragent")).toString().toLatin1();
    else
        m_userAgent = "Qt Location based application";

    if (parameters.contains(QStringLiteral("osm.geocoding.host")))
        m_urlPrefix = parameters.value(QStringLiteral("osm.geocoding.host")).toString().toLatin1();
    else
        m_urlPrefix = QStringLiteral("http://nominatim.openstreetmap.org");

    *error = QGeoServiceProvider::NoError;
    errorString->clear();
}

QGeoCodingManagerEngineOsm::~QGeoCodingManagerEngineOsm()
{
}

QGeoCodeReply *QGeoCodingManagerEngineOsm::geocode(const QGeoAddress &address, const QGeoShape &bounds)
{
    return geocode(addressToQuery(address), -1, -1, bounds);
}

QGeoCodeReply *QGeoCodingManagerEngineOsm::geocode(const QString &address, int limit, int offset, const QGeoShape &bounds)
{
    Q_UNUSED(offset)

    QNetworkRequest request;
    request.setRawHeader("User-Agent", m_userAgent);

    QUrl url(QString("%1/search").arg(m_urlPrefix));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), address);
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("accept-language"), locale().name().left(2));
    //query.addQueryItem(QStringLiteral("countrycodes"), QStringLiteral("au,jp"));
    if (bounds.type() == QGeoShape::RectangleType) {
        query.addQueryItem(QStringLiteral("viewbox"), boundingBoxToLtrb(bounds));
        query.addQueryItem(QStringLiteral("bounded"), QStringLiteral("1"));
    }
    query.addQueryItem(QStringLiteral("polygon_geojson"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("addressdetails"), QStringLiteral("1"));
    if (limit != -1)
        query.addQueryItem(QStringLiteral("limit"), QString::number(limit));

    url.setQuery(query);
    request.setUrl(url);

    QNetworkReply *reply = m_networkManager->get(request);

    QGeoCodeReplyOsm *geocodeReply = new QGeoCodeReplyOsm(reply, this);

    connect(geocodeReply, SIGNAL(finished()), this, SLOT(replyFinished()));
    connect(geocodeReply, SIGNAL(error(QGeoCodeReply::Error,QString)),
            this, SLOT(replyError(QGeoCodeReply::Error,QString)));

    return geocodeReply;
}

QGeoCodeReply *QGeoCodingManagerEngineOsm::reverseGeocode(const QGeoCoordinate &coordinate,
                                                          const QGeoShape &bounds)
{
    Q_UNUSED(bounds)

    QNetworkRequest request;
    request.setRawHeader("User-Agent", m_userAgent);

    QUrl url(QString("%1/reverse").arg(m_urlPrefix));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("accept-language"), locale().name().left(2));
    query.addQueryItem(QStringLiteral("lat"), QString::number(coordinate.latitude()));
    query.addQueryItem(QStringLiteral("lon"), QString::number(coordinate.longitude()));
    query.addQueryItem(QStringLiteral("zoom"), QStringLiteral("18"));
    query.addQueryItem(QStringLiteral("addressdetails"), QStringLiteral("1"));

    url.setQuery(query);
    request.setUrl(url);

    QNetworkReply *reply = m_networkManager->get(request);

    QGeoCodeReplyOsm *geocodeReply = new QGeoCodeReplyOsm(reply, this);

    connect(geocodeReply, SIGNAL(finished()), this, SLOT(replyFinished()));
    connect(geocodeReply, SIGNAL(error(QGeoCodeReply::Error,QString)),
            this, SLOT(replyError(QGeoCodeReply::Error,QString)));

    return geocodeReply;
}

void QGeoCodingManagerEngineOsm::replyFinished()
{
    QGeoCodeReply *reply = qobject_cast<QGeoCodeReply *>(sender());
    if (reply)
        emit finished(reply);
}

void QGeoCodingManagerEngineOsm::replyError(QGeoCodeReply::Error errorCode, const QString &errorString)
{
    QGeoCodeReply *reply = qobject_cast<QGeoCodeReply *>(sender());
    if (reply)
        emit error(reply, errorCode, errorString);
}

QT_END_NAMESPACE
