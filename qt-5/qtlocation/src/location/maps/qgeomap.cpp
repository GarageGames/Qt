/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#include "qgeomap_p.h"
#include "qgeomap_p_p.h"
#include "qgeocameracapabilities_p.h"
#include "qgeomapcontroller_p.h"
#include "qgeomappingmanagerengine_p.h"

QT_BEGIN_NAMESPACE

QGeoMap::QGeoMap(QGeoMapPrivate &dd, QObject *parent)
    : QObject(dd,parent)
{
}

QGeoMap::~QGeoMap()
{
}

QGeoMapController *QGeoMap::mapController()
{
    Q_D(QGeoMap);
    if (!d->m_controller)
        d->m_controller = new QGeoMapController(this);
    return d->m_controller;
}

void QGeoMap::resize(int width, int height)
{
    Q_D(QGeoMap);
    d->resize(width, height);
    // always emit this signal to trigger items to redraw
    emit cameraDataChanged(d->m_cameraData);
}

int QGeoMap::width() const
{
    Q_D(const QGeoMap);
    return d->m_width;
}

int QGeoMap::height() const
{
    Q_D(const QGeoMap);
    return d->m_height;
}

void QGeoMap::setCameraData(const QGeoCameraData &cameraData)
{
    Q_D(QGeoMap);
    if (cameraData == d->m_cameraData)
        return;

    d->setCameraData(cameraData);

    update();

    emit cameraDataChanged(d->m_cameraData);
}

QGeoCameraData QGeoMap::cameraData() const
{
    Q_D(const QGeoMap);
    return d->m_cameraData;
}

void QGeoMap::update()
{
    emit updateRequired();
}

void QGeoMap::setActiveMapType(const QGeoMapType type)
{
    Q_D(QGeoMap);
    d->m_activeMapType = type;
    d->changeActiveMapType(type);
    d->setCameraData(d->m_cameraData);
    update();
}

const QGeoMapType QGeoMap::activeMapType() const
{
    Q_D(const QGeoMap);
    return d->m_activeMapType;
}

QString QGeoMap::pluginString()
{
    Q_D(const QGeoMap);
    return d->m_pluginString;
}

QGeoCameraCapabilities QGeoMap::cameraCapabilities()
{
    Q_D(const QGeoMap);
    if (!d->m_engine.isNull())
        return d->m_engine->cameraCapabilities();
    else
        return QGeoCameraCapabilities();
}

int QGeoMap::mapVersion()
{
    return -1;
}

void QGeoMap::prefetchData()
{

}

QGeoMapPrivate::QGeoMapPrivate(QGeoMappingManagerEngine *engine)
    : QObjectPrivate(),
      m_width(0),
      m_height(0),
      m_aspectRatio(0.0),
      m_engine(engine),
      m_controller(0),
      m_activeMapType(QGeoMapType())
{
    if (!m_engine.isNull()) {
        m_pluginString = m_engine->managerName() + QLatin1Char('_') + QString::number(m_engine->managerVersion());
    }
}

QGeoMapPrivate::~QGeoMapPrivate()
{
    // controller_ is a child of map_, don't need to delete it here

    // TODO map items are not deallocated!
    // However: how to ensure this is done in rendering thread?
}

void QGeoMapPrivate::setCameraData(const QGeoCameraData &cameraData)
{
    QGeoCameraData oldCameraData = m_cameraData;
    m_cameraData = cameraData;

    if (!m_engine.isNull()) {
        QGeoCameraCapabilities capabilities = m_engine->cameraCapabilities();
        if (m_cameraData.zoomLevel() < capabilities.minimumZoomLevel())
            m_cameraData.setZoomLevel(capabilities.minimumZoomLevel());

        if (m_cameraData.zoomLevel() > capabilities.maximumZoomLevel())
            m_cameraData.setZoomLevel(capabilities.maximumZoomLevel());

        if (!capabilities.supportsBearing())
            m_cameraData.setBearing(0.0);

        if (capabilities.supportsTilting()) {
            if (m_cameraData.tilt() < capabilities.minimumTilt())
                m_cameraData.setTilt(capabilities.minimumTilt());

            if (m_cameraData.tilt() > capabilities.maximumTilt())
                m_cameraData.setTilt(capabilities.maximumTilt());
        } else {
            m_cameraData.setTilt(0.0);
        }

        if (!capabilities.supportsRolling())
            m_cameraData.setRoll(0.0);
    }

    // Do not call this expensive function if the width is 0, since it will get called
    // anyway when it is resized to a width > 0.
    // this is mainly an optimization to the initialization of the geomap, which would otherwise
    // call changeCameraData four or more times
    if (m_width > 0)
        changeCameraData(oldCameraData);
}

void QGeoMapPrivate::resize(int width, int height)
{
    m_width = width;
    m_height = height;
    m_aspectRatio = 1.0 * m_width / m_height;
    mapResized(width, height);
    setCameraData(m_cameraData);
}

QT_END_NAMESPACE
