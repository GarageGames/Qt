/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#ifndef QGEOTILEDMAPPINGMANAGERENGINE_TEST_H
#define QGEOTILEDMAPPINGMANAGERENGINE_TEST_H

#include <QtCore/QLocale>
#include <QtLocation/QGeoServiceProvider>
#include <QtLocation/private/qgeotiledmappingmanagerengine_p.h>
#include <QtLocation/private/qgeotiledmapreply_p.h>
#include <QtLocation/private/qgeomaptype_p.h>
#include <QtLocation/private/qgeocameracapabilities_p.h>

#include "qgeotiledmap_test.h"
#include "qgeotilefetcher_test.h"

QT_USE_NAMESPACE

class QGeoTiledMappingManagerEngineTest: public QGeoTiledMappingManagerEngine
{
Q_OBJECT
public:
    QGeoTiledMappingManagerEngineTest(const QVariantMap &parameters,
        QGeoServiceProvider::Error *error, QString *errorString) :
        QGeoTiledMappingManagerEngine()
    {
        Q_UNUSED(error)
        Q_UNUSED(errorString)

        setLocale(QLocale (QLocale::German, QLocale::Germany));
        QGeoCameraCapabilities capabilities;
        capabilities.setMinimumZoomLevel(0.0);
        capabilities.setMaximumZoomLevel(20.0);
        capabilities.setSupportsBearing(true);
        setCameraCapabilities(capabilities);

        setTileSize(QSize(256, 256));

        QList<QGeoMapType> mapTypes;
        mapTypes << QGeoMapType(QGeoMapType::StreetMap, tr("StreetMap"), tr("StreetMap"), false, false, 1);
        mapTypes << QGeoMapType(QGeoMapType::SatelliteMapDay, tr("SatelliteMapDay"), tr("SatelliteMapDay"), false, false, 2);
        mapTypes << QGeoMapType(QGeoMapType::CycleMap, tr("CycleMap"), tr("CycleMap"), false, false, 3);
        setSupportedMapTypes(mapTypes);

        QGeoTileFetcherTest *fetcher = new QGeoTileFetcherTest(this);
        fetcher->setParams(parameters);
        fetcher->setTileSize(QSize(256, 255));
        setTileFetcher(fetcher);
    }

    QGeoMap *createMap()
    {
        return new QGeoTiledMapTest(this);;
    }

};

#endif
