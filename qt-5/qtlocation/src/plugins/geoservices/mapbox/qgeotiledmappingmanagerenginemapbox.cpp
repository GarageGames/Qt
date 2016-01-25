/****************************************************************************
**
** Copyright (C) 2014 Canonical Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

#include "qgeotiledmappingmanagerenginemapbox.h"
#include "qgeotilefetchermapbox.h"

#include <QtLocation/private/qgeocameracapabilities_p.h>
#include <QtLocation/private/qgeomaptype_p.h>
#include <QtLocation/private/qgeotiledmap_p.h>

QT_BEGIN_NAMESPACE

QGeoTiledMappingManagerEngineMapbox::QGeoTiledMappingManagerEngineMapbox(const QVariantMap &parameters, QGeoServiceProvider::Error *error, QString *errorString)
:   QGeoTiledMappingManagerEngine()
{
    QGeoCameraCapabilities cameraCaps;
    cameraCaps.setMinimumZoomLevel(0.0);
    cameraCaps.setMaximumZoomLevel(19.0);
    setCameraCapabilities(cameraCaps);

    setTileSize(QSize(256, 256));

    QList<QGeoMapType> mapTypes;
    mapTypes << QGeoMapType(QGeoMapType::CustomMap, tr("Custom"), tr("Mapbox custom map"), false, false, 0);
    setSupportedMapTypes(mapTypes);

    QGeoTileFetcherMapbox *tileFetcher = new QGeoTileFetcherMapbox(this);
    if (parameters.contains(QStringLiteral("useragent"))) {
        const QByteArray ua = parameters.value(QStringLiteral("useragent")).toString().toLatin1();
        tileFetcher->setUserAgent(ua);
    }
    if (parameters.contains(QStringLiteral("mapbox.map_id"))) {
        const QString id = parameters.value(QStringLiteral("mapbox.map_id")).toString();
        tileFetcher->setMapId(id);
    }
    if (parameters.contains(QStringLiteral("mapbox.format"))) {
        const QString format = parameters.value(QStringLiteral("mapbox.format")).toString();
        tileFetcher->setFormat(format);
    }
    if (parameters.contains(QStringLiteral("mapbox.access_token"))) {
        const QString token = parameters.value(QStringLiteral("mapbox.access_token")).toString();
        tileFetcher->setAccessToken(token);
    }

    setTileFetcher(tileFetcher);

    *error = QGeoServiceProvider::NoError;
    errorString->clear();
}

QGeoTiledMappingManagerEngineMapbox::~QGeoTiledMappingManagerEngineMapbox()
{
}

QGeoMap *QGeoTiledMappingManagerEngineMapbox::createMap()
{
    return new QGeoTiledMap(this, 0);
}

QT_END_NAMESPACE
