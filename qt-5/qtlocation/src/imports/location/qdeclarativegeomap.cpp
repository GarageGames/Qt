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

#include "qdeclarativegeomap_p.h"
#include "qdeclarativegeomapquickitem_p.h"
#include "qdeclarativegeomapcopyrightsnotice_p.h"
#include "qdeclarativegeoserviceprovider_p.h"
#include "qdeclarativegeomaptype_p.h"
#include "qgeomapcontroller_p.h"
#include "qgeomappingmanager_p.h"
#include "qgeocameracapabilities_p.h"
#include "qgeomap_p.h"
#include <QtPositioning/QGeoCircle>
#include <QtPositioning/QGeoRectangle>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGSimpleRectNode>
#include <QtQuick/private/qquickwindow_p.h>
#include <QtQml/qqmlinfo.h>
#include <cmath>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Map
    \instantiates QDeclarativeGeoMap
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-maps
    \since Qt Location 5.0

    \brief The Map type displays a map.

    The Map type is used to display a map or image of the Earth, with
    the capability to also display interactive objects tied to the map's
    surface.

    There are a variety of different ways to visualize the Earth's surface
    in a 2-dimensional manner, but all of them involve some kind of projection:
    a mathematical relationship between the 3D coordinates (latitude, longitude
    and altitude) and 2D coordinates (X and Y in pixels) on the screen.

    Different sources of map data can use different projections, and from the
    point of view of the Map type, we treat these as one replaceable unit:
    the Map plugin. A Map plugin consists of a data source, as well as all other
    details needed to display its data on-screen.

    The current Map plugin in use is contained in the \l plugin property of
    the Map item. In order to display any image in a Map item, you will need
    to set this property. See the \l Plugin type for a description of how
    to retrieve an appropriate plugin for use.

    The geographic region displayed in the Map item is referred to as its
    viewport, and this is defined by the properties \l center, and
    \l zoomLevel. The \l center property contains a \l {coordinate}
    specifying the center of the viewport, while \l zoomLevel controls the scale of the
    map. See each of these properties for further details about their values.

    When the map is displayed, each possible geographic coordinate that is
    visible will map to some pixel X and Y coordinate on the screen. To perform
    conversions between these two, Map provides the \l toCoordinate and
    \l fromCoordinate functions, which are of general utility.

    \section2 Map Objects

    Map related objects can be declared within the body of a Map object in Qt Quick and will
    automatically appear on the Map. To add objects programmatically, first be
    sure they are created with the Map as their parent (for example in an argument to
    Component::createObject), and then call the \l addMapItem method on the Map.
    A corresponding \l removeMapItem method also exists to do the opposite and
    remove an object from the Map.

    Moving Map objects around, resizing them or changing their shape normally
    does not involve any special interaction with Map itself -- changing these
    details about a map object will automatically update the display.

    \section2 Interaction

    The Map type includes support for pinch and flick gestures to control
    zooming and panning. These are enabled by default, and available at any
    time by using the \l gesture object. The actual GestureArea is constructed
    specially at startup and cannot be replaced or destroyed. Its properties
    can be altered, however, to control its behavior.

    \section2 Performance

    Maps are rendered using OpenGL (ES) and the Qt Scene Graph stack, and as
    a result perform quite well where GL accelerated hardware is available.

    For "online" Map plugins, network bandwidth and latency can be major
    contributors to the user's perception of performance. Extensive caching is
    performed to mitigate this, but such mitigation is not always perfect. For
    "offline" plugins, the time spent retrieving the stored geographic data
    and rendering the basic map features can often play a dominant role. Some
    offline plugins may use hardware acceleration themselves to (partially)
    avert this.

    In general, large and complex Map items such as polygons and polylines with
    large numbers of vertices can have an adverse effect on UI performance.
    Further, more detailed notes on this are in the documentation for each
    map item type.

    \section2 Example Usage

    The following snippet shows a simple Map and the necessary Plugin type
    to use it. The map is centered near Brisbane, Australia, zoomed out to the
    minimum zoom level, with gesture interaction enabled.

    \code
    Plugin {
        id: somePlugin
        // code here to choose the plugin as necessary
    }

    Map {
        id: map

        plugin: somePlugin

        center {
            latitude: -27
            longitude: 153
        }
        zoomLevel: map.minimumZoomLevel

        gesture.enabled: true
    }
    \endcode

    \image api-map.png
*/

/*!
    \qmlsignal QtLocation::Map::copyrightLinkActivated(string link)

    This signal is emitted when the user clicks on a \a link in the copyright notice. The
    application should open the link in a browser or display its contents to the user.
*/

QDeclarativeGeoMap::QDeclarativeGeoMap(QQuickItem *parent)
        : QQuickItem(parent),
        m_plugin(0),
        m_serviceProvider(0),
        m_mappingManager(0),
        m_center(51.5073,-0.1277), //London city center
        m_activeMapType(0),
        m_gestureArea(0),
        m_map(0),
        m_error(QGeoServiceProvider::NoError),
        m_zoomLevel(8.0),
        m_componentCompleted(false),
        m_mappingManagerInitialized(false),
        m_pendingFitViewport(false)
{
    setAcceptHoverEvents(false);
    setAcceptedMouseButtons(Qt::LeftButton);
    setFlags(QQuickItem::ItemHasContents | QQuickItem::ItemClipsChildrenToShape);
    setFiltersChildMouseEvents(true);

    connect(this, SIGNAL(childrenChanged()), this, SLOT(onMapChildrenChanged()), Qt::QueuedConnection);

    // Create internal flickable and pinch area.
    m_gestureArea = new QDeclarativeGeoMapGestureArea(this, this);
    m_activeMapType = new QDeclarativeGeoMapType(QGeoMapType(QGeoMapType::NoMap,
                                                             tr("No Map"),
                                                             tr("No Map"), false, false, 0), this);
}

QDeclarativeGeoMap::~QDeclarativeGeoMap()
{
    if (!m_mapViews.isEmpty())
        qDeleteAll(m_mapViews);
    // remove any map items associations
    for (int i = 0; i < m_mapItems.count(); ++i) {
        if (m_mapItems.at(i))
            m_mapItems.at(i).data()->setMap(0,0);
    }
    m_mapItems.clear();

    delete m_copyrights.data();
    m_copyrights.clear();
}

/*!
    \internal
*/
void QDeclarativeGeoMap::onMapChildrenChanged()
{
    if (!m_componentCompleted || !m_mappingManagerInitialized)
        return;

    int maxChildZ = 0;
    QObjectList kids = children();
    bool foundCopyrights = false;

    for (int i = 0; i < kids.size(); ++i) {
        QDeclarativeGeoMapCopyrightNotice *copyrights = qobject_cast<QDeclarativeGeoMapCopyrightNotice *>(kids.at(i));
        if (copyrights) {
            foundCopyrights = true;
        } else {
            QDeclarativeGeoMapItemBase *mapItem = qobject_cast<QDeclarativeGeoMapItemBase *>(kids.at(i));
            if (mapItem) {
                if (mapItem->z() > maxChildZ)
                    maxChildZ = mapItem->z();
            }
        }
    }

    QDeclarativeGeoMapCopyrightNotice *copyrights = m_copyrights.data();
    // if copyrights object not found within the map's children
    if (!foundCopyrights) {
        // if copyrights object was deleted!
        if (!copyrights) {
            // create a new one and set its parent, re-assign it to the weak pointer, then connect the copyrights-change signal
            m_copyrights = new QDeclarativeGeoMapCopyrightNotice(this);
            copyrights = m_copyrights.data();
            connect(m_map, SIGNAL(copyrightsChanged(QImage)),
                    copyrights, SLOT(copyrightsChanged(QImage)));
            connect(m_map, SIGNAL(copyrightsChanged(QString)),
                    copyrights, SLOT(copyrightsChanged(QString)));
            connect(copyrights, SIGNAL(linkActivated(QString)),
                    this, SIGNAL(copyrightLinkActivated(QString)));
        } else {
            // just re-set its parent.
            copyrights->setParent(this);
        }
    }

    // put the copyrights notice object at the highest z order
    copyrights->setCopyrightsZ(maxChildZ + 1);
}


void QDeclarativeGeoMap::setError(QGeoServiceProvider::Error error, const QString &errorString)
{
    if (m_error == error && m_errorString == errorString)
        return;
    m_error = error;
    m_errorString = errorString;
    emit errorChanged();
}

/*!
    \internal
*/
void QDeclarativeGeoMap::pluginReady()
{
    m_serviceProvider = m_plugin->sharedGeoServiceProvider();
    m_mappingManager = m_serviceProvider->mappingManager();

    setError(m_serviceProvider->error(), m_serviceProvider->errorString());

    if (!m_mappingManager || m_serviceProvider->error() != QGeoServiceProvider::NoError) {
        qmlInfo(this) << QStringLiteral("Error: Plugin does not support mapping.\nError message:")
                      << m_serviceProvider->errorString();
        return;
    }

    if (!m_mappingManager->isInitialized())
        connect(m_mappingManager, SIGNAL(initialized()), this, SLOT(mappingManagerInitialized()));
    else
        mappingManagerInitialized();

    // make sure this is only called once
    disconnect(this, SLOT(pluginReady()));
}

/*!
    \internal
*/
void QDeclarativeGeoMap::componentComplete()
{
    m_componentCompleted = true;
    populateMap();
    QQuickItem::componentComplete();
}

/*!
    \internal
*/
void QDeclarativeGeoMap::mousePressEvent(QMouseEvent *event)
{
    if (isInteractive())
        m_gestureArea->handleMousePressEvent(event);
    else
        QQuickItem::mousePressEvent(event);
}

/*!
    \internal
*/
void QDeclarativeGeoMap::mouseMoveEvent(QMouseEvent *event)
{
    if (isInteractive())
        m_gestureArea->handleMouseMoveEvent(event);
    else
        QQuickItem::mouseMoveEvent(event);
}

/*!
    \internal
*/
void QDeclarativeGeoMap::mouseReleaseEvent(QMouseEvent *event)
{
    if (isInteractive()) {
        m_gestureArea->handleMouseReleaseEvent(event);
        ungrabMouse();
    } else {
        QQuickItem::mouseReleaseEvent(event);
    }
}

/*!
    \internal
*/
void QDeclarativeGeoMap::mouseUngrabEvent()
{
    if (isInteractive())
        m_gestureArea->handleMouseUngrabEvent();
    else
        QQuickItem::mouseUngrabEvent();
}

void QDeclarativeGeoMap::touchUngrabEvent()
{
    if (isInteractive())
        m_gestureArea->handleTouchUngrabEvent();
    else
        QQuickItem::touchUngrabEvent();
}

/*!
    \qmlproperty MapGestureArea QtLocation::Map::gesture

    Contains the MapGestureArea created with the Map. This covers pan, flick and pinch gestures.
    Use \c{gesture.enabled: true} to enable basic gestures, or see \l{MapGestureArea} for
    further details.
*/

QDeclarativeGeoMapGestureArea *QDeclarativeGeoMap::gesture()
{
    return m_gestureArea;
}

/*!
    \internal
*/
void QDeclarativeGeoMap::populateMap()
{
    QObjectList kids = children();
    for (int i = 0; i < kids.size(); ++i) {
        // dispatch items appropriately
        QDeclarativeGeoMapItemView *mapView = qobject_cast<QDeclarativeGeoMapItemView *>(kids.at(i));
        if (mapView) {
            m_mapViews.append(mapView);
            setupMapView(mapView);
            continue;
        }
        QDeclarativeGeoMapItemBase *mapItem = qobject_cast<QDeclarativeGeoMapItemBase *>(kids.at(i));
        if (mapItem) {
            addMapItem(mapItem);
        }
    }
}

/*!
    \internal
*/
void QDeclarativeGeoMap::setupMapView(QDeclarativeGeoMapItemView *view)
{
    Q_UNUSED(view)
    view->setMap(this);
    view->repopulate();
}

/*!
 * \internal
 */
QSGNode *QDeclarativeGeoMap::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    if (!m_map) {
        delete oldNode;
        return 0;
    }

    QSGSimpleRectNode *root = static_cast<QSGSimpleRectNode *>(oldNode);
    if (!root)
        root = new QSGSimpleRectNode(boundingRect(), QColor::fromRgbF(0.9, 0.9, 0.9));
    else
        root->setRect(boundingRect());

    QSGNode *content = root->childCount() ? root->firstChild() : 0;
    content = m_map->updateSceneGraph(content, window());
    if (content && root->childCount() == 0)
        root->appendChildNode(content);

    return root;
}


/*!
    \qmlproperty Plugin QtLocation::Map::plugin

    This property holds the plugin which provides the mapping functionality.

    This is a write-once property. Once the map has a plugin associated with
    it, any attempted modifications of the plugin will be ignored.
*/

void QDeclarativeGeoMap::setPlugin(QDeclarativeGeoServiceProvider *plugin)
{
    if (m_plugin) {
        qmlInfo(this) << QStringLiteral("Plugin is a write-once property, and cannot be set again.");
        return;
    }
    m_plugin = plugin;
    emit pluginChanged(m_plugin);

    if (m_plugin->isAttached()) {
        pluginReady();
    } else {
        connect(m_plugin, SIGNAL(attached()),
                this, SLOT(pluginReady()));
    }
}

/*!
    \internal
    this function will only be ever called once
*/
void QDeclarativeGeoMap::mappingManagerInitialized()
{
    m_mappingManagerInitialized = true;

    m_map = m_mappingManager->createMap(this);
    m_gestureArea->setMap(m_map);

    // once mappingManagerInitilized_ is set zoomLevel() returns the default initialised
    // zoom level of the map controller. Overwrite it here to whatever the user chose.
    m_map->mapController()->setZoom(m_zoomLevel);

    //The zoom level limits are only restricted by the plugins values, if the user has set a more
    //strict zoom level limit before initialization nothing is done here.
    if (m_mappingManager->cameraCapabilities().minimumZoomLevel() > m_gestureArea->minimumZoomLevel())
        setMinimumZoomLevel(m_mappingManager->cameraCapabilities().minimumZoomLevel());

    if (m_gestureArea->maximumZoomLevel() < 0
            || m_mappingManager->cameraCapabilities().maximumZoomLevel() < m_gestureArea->maximumZoomLevel())
        setMaximumZoomLevel(m_mappingManager->cameraCapabilities().maximumZoomLevel());

    m_map->setActiveMapType(QGeoMapType());

    m_copyrights = new QDeclarativeGeoMapCopyrightNotice(this);
    connect(m_map, SIGNAL(copyrightsChanged(QImage)),
            m_copyrights.data(), SLOT(copyrightsChanged(QImage)));
    connect(m_map, SIGNAL(copyrightsChanged(QString)),
            m_copyrights.data(), SLOT(copyrightsChanged(QString)));
    connect(m_copyrights.data(), SIGNAL(linkActivated(QString)),
            this, SIGNAL(copyrightLinkActivated(QString)));

    connect(m_map,
            SIGNAL(updateRequired()),
            this,
            SLOT(update()));
    connect(m_map->mapController(),
            SIGNAL(centerChanged(QGeoCoordinate)),
            this,
            SIGNAL(centerChanged(QGeoCoordinate)));
    connect(m_map->mapController(),
            SIGNAL(zoomChanged(qreal)),
            this,
            SLOT(mapZoomLevelChanged(qreal)));

    m_map->mapController()->setCenter(m_center);

    QList<QGeoMapType> types = m_mappingManager->supportedMapTypes();
    for (int i = 0; i < types.size(); ++i) {
        QDeclarativeGeoMapType *type = new QDeclarativeGeoMapType(types[i], this);
        m_supportedMapTypes.append(type);
    }

    if (!m_supportedMapTypes.isEmpty()) {
        QDeclarativeGeoMapType *type = m_supportedMapTypes.at(0);
        m_activeMapType = type;
        m_map->setActiveMapType(type->mapType());
    }

    // Map tiles are built in this call
    m_map->resize(width(), height());
    // This prefetches a buffer around the map
    m_map->prefetchData();
    m_map->update();

    emit minimumZoomLevelChanged();
    emit maximumZoomLevelChanged();
    emit supportedMapTypesChanged();
    emit activeMapTypeChanged();

    // Any map items that were added before the plugin was ready
    // need to have setMap called again
    foreach (const QPointer<QDeclarativeGeoMapItemBase> &item, m_mapItems) {
        if (item)
            item.data()->setMap(this, m_map);
    }
}

/*!
    \internal
*/
void QDeclarativeGeoMap::updateMapDisplay(const QRectF &target)
{
    Q_UNUSED(target);
    QQuickItem::update();
}


/*!
    \internal
*/
QDeclarativeGeoServiceProvider *QDeclarativeGeoMap::plugin() const
{
    return m_plugin;
}

/*!
    \internal
    Sets the gesture areas minimum zoom level. If the camera capabilities
    has been set this method honors the boundaries set by it.
*/
void QDeclarativeGeoMap::setMinimumZoomLevel(qreal minimumZoomLevel)
{
    if (m_gestureArea && minimumZoomLevel >= 0) {
        qreal oldMinimumZoomLevel = this->minimumZoomLevel();
        if (m_mappingManagerInitialized
                && minimumZoomLevel < m_mappingManager->cameraCapabilities().minimumZoomLevel()) {
            minimumZoomLevel = m_mappingManager->cameraCapabilities().minimumZoomLevel();
        }
        m_gestureArea->setMinimumZoomLevel(minimumZoomLevel);
        setZoomLevel(qBound<qreal>(minimumZoomLevel, zoomLevel(), maximumZoomLevel()));
        if (oldMinimumZoomLevel != minimumZoomLevel)
            emit minimumZoomLevelChanged();
    }
}

/*!
    \qmlproperty real QtLocation::Map::minimumZoomLevel

    This property holds the minimum valid zoom level for the map.

    The minimum zoom level is defined by the \l plugin used.
    If a plugin supporting mapping is not set, -1.0 is returned.
*/

qreal QDeclarativeGeoMap::minimumZoomLevel() const
{
    if (m_gestureArea->minimumZoomLevel() != -1)
        return m_gestureArea->minimumZoomLevel();
    else if (m_mappingManager && m_mappingManagerInitialized)
        return m_mappingManager->cameraCapabilities().minimumZoomLevel();
    else
        return -1.0;
}

/*!
    \internal
    Sets the gesture areas maximum zoom level. If the camera capabilities
    has been set this method honors the boundaries set by it.
*/
void QDeclarativeGeoMap::setMaximumZoomLevel(qreal maximumZoomLevel)
{
    if (m_gestureArea && maximumZoomLevel >= 0) {
        qreal oldMaximumZoomLevel = this->maximumZoomLevel();
        if (m_mappingManagerInitialized
                && maximumZoomLevel > m_mappingManager->cameraCapabilities().maximumZoomLevel()) {
            maximumZoomLevel = m_mappingManager->cameraCapabilities().maximumZoomLevel();
        }
        m_gestureArea->setMaximumZoomLevel(maximumZoomLevel);
        setZoomLevel(qBound<qreal>(minimumZoomLevel(), zoomLevel(), maximumZoomLevel));
        if (oldMaximumZoomLevel != maximumZoomLevel)
            emit maximumZoomLevelChanged();
    }
}

/*!
    \qmlproperty real QtLocation::Map::maximumZoomLevel

    This property holds the maximum valid zoom level for the map.

    The maximum zoom level is defined by the \l plugin used.
    If a plugin supporting mapping is not set, -1.0 is returned.
*/

qreal QDeclarativeGeoMap::maximumZoomLevel() const
{
    if (m_gestureArea->maximumZoomLevel() != -1)
        return m_gestureArea->maximumZoomLevel();
    else if (m_mappingManager && m_mappingManagerInitialized)
        return m_mappingManager->cameraCapabilities().maximumZoomLevel();
    else
        return -1.0;
}

/*!
    \qmlproperty real QtLocation::Map::zoomLevel

    This property holds the zoom level for the map.

    Larger values for the zoom level provide more detail. Zoom levels
    are always non-negative. The default value is 8.0.
*/
void QDeclarativeGeoMap::setZoomLevel(qreal zoomLevel)
{
    if (m_zoomLevel == zoomLevel || zoomLevel < 0)
        return;

    if ((zoomLevel < minimumZoomLevel()
         || (maximumZoomLevel() >= 0 && zoomLevel > maximumZoomLevel())))
        return;

    m_zoomLevel = zoomLevel;
    if (m_mappingManagerInitialized)
        m_map->mapController()->setZoom(m_zoomLevel);
    emit zoomLevelChanged(zoomLevel);
}

qreal QDeclarativeGeoMap::zoomLevel() const
{
    if (m_mappingManagerInitialized)
        return m_map->mapController()->zoom();
    else
        return m_zoomLevel;
}

/*!
    \qmlproperty coordinate QtLocation::Map::center

    This property holds the coordinate which occupies the center of the
    mapping viewport. Invalid center coordinates are ignored.

    The default value is an arbitrary valid coordinate.
*/
void QDeclarativeGeoMap::setCenter(const QGeoCoordinate &center)
{
    if (!m_mappingManagerInitialized && center == m_center)
        return;

    if (!center.isValid())
        return;

    m_center = center;

    if (m_center.isValid() && m_mappingManagerInitialized) {
        m_map->mapController()->setCenter(m_center);
        update();
    } else {
        emit centerChanged(m_center);
    }
}

QGeoCoordinate QDeclarativeGeoMap::center() const
{
    if (m_mappingManagerInitialized)
        return m_map->mapController()->center();
    else
        return m_center;
}


/*!
    \qmlproperty geoshape QtLocation::Map::visibleRegion

    This property holds the region which occupies the viewport of
    the map. The camera is positioned in the center of the shape, and
    at the largest integral zoom level possible which allows the
    whole shape to be visible on the screen. This implies that
    reading this property back shortly after having been set the
    returned area is equal or larger than the set area.

    Setting this property implicitly changes the \l center and
    \l zoomLevel of the map. Any previously set value to those
    properties will be overridden.

    This property does not provide any change notifications.

    \since 5.5
*/
void QDeclarativeGeoMap::setVisibleRegion(const QGeoShape &shape)
{
    if (shape == m_region)
        return;

    m_region = shape;
    if (!shape.isValid()) {
        // shape invalidated -> nothing to fit anymore
        m_pendingFitViewport = false;
        return;
    }

    if (!width() || !height()) {
        m_pendingFitViewport = true;
        return;
    }

    fitViewportToGeoShape();
}

QGeoShape QDeclarativeGeoMap::visibleRegion() const
{
    if (!width() || !height())
        return QGeoShape();

    QGeoCoordinate tl = m_map->itemPositionToCoordinate(QDoubleVector2D(0, 0));
    QGeoCoordinate br = m_map->itemPositionToCoordinate(QDoubleVector2D(width(), height()));

    return QGeoRectangle(tl, br);
}

void QDeclarativeGeoMap::fitViewportToGeoShape()
{
    double bboxWidth;
    double bboxHeight;
    QGeoCoordinate centerCoordinate;

    switch (m_region.type()) {
    case QGeoShape::RectangleType:
    {
        QGeoRectangle rect = m_region;
        QDoubleVector2D topLeftPoint = m_map->coordinateToItemPosition(rect.topLeft(), false);
        QDoubleVector2D botRightPoint = m_map->coordinateToItemPosition(rect.bottomRight(), false);
        bboxWidth = qAbs(topLeftPoint.x() - botRightPoint.x());
        bboxHeight = qAbs(topLeftPoint.y() - botRightPoint.y());
        centerCoordinate = rect.center();
        break;
    }
    case QGeoShape::CircleType:
    {
        QGeoCircle circle = m_region;
        centerCoordinate = circle.center();
        QGeoCoordinate edge = centerCoordinate.atDistanceAndAzimuth(circle.radius(), 90);
        QDoubleVector2D centerPoint = m_map->coordinateToItemPosition(centerCoordinate, false);
        QDoubleVector2D edgePoint = m_map->coordinateToItemPosition(edge, false);
        bboxWidth = qAbs(centerPoint.x() - edgePoint.x()) * 2;
        bboxHeight = bboxWidth;
        break;
    }
    case QGeoShape::UnknownType:
        //Fallthrough to default
    default:
        return;
    }

    // position camera to the center of bounding box
    setProperty("center", QVariant::fromValue(centerCoordinate));

    //If the shape is empty we just change centerposition, not zoom
    if (bboxHeight == 0 && bboxWidth == 0)
        return;

    // adjust zoom
    double bboxWidthRatio = bboxWidth / (bboxWidth + bboxHeight);
    double mapWidthRatio = width() / (width() + height());
    double zoomRatio;

    if (bboxWidthRatio > mapWidthRatio)
        zoomRatio = bboxWidth / width();
    else
        zoomRatio = bboxHeight / height();

    qreal newZoom = std::log10(zoomRatio) / std::log10(0.5);

    newZoom = std::floor(qMax(minimumZoomLevel(), (m_map->mapController()->zoom() + newZoom)));
    setProperty("zoomLevel", QVariant::fromValue(newZoom));
}

/*!
    \internal
*/
void QDeclarativeGeoMap::mapZoomLevelChanged(qreal zoom)
{
    if (zoom == m_zoomLevel)
        return;
    m_zoomLevel = zoom;
    emit zoomLevelChanged(m_zoomLevel);
}

/*!
    \qmlproperty list<MapType> QtLocation::Map::supportedMapTypes

    This read-only property holds the set of \l{MapType}{map types} supported by this map.

    \sa activeMapType
*/
QQmlListProperty<QDeclarativeGeoMapType> QDeclarativeGeoMap::supportedMapTypes()
{
    return QQmlListProperty<QDeclarativeGeoMapType>(this, m_supportedMapTypes);
}

/*!
    \qmlmethod coordinate QtLocation::Map::toCoordinate(QPointF position)

    Returns the coordinate which corresponds to the \a position relative to the map item.

    Returns an invalid coordinate if \a position is not within the current viewport.
*/
QGeoCoordinate QDeclarativeGeoMap::toCoordinate(const QPointF &position) const
{
    if (m_map)
        return m_map->itemPositionToCoordinate(QDoubleVector2D(position));
    else
        return QGeoCoordinate();
}

/*!
    \qmlmethod point QtLocation::Map::fromCoordinate(coordinate coordinate)

    Returns the position relative to the map item which corresponds to the \a coordinate.

    Returns an invalid QPointF if \a coordinate is not within the current viewport.
*/
QPointF QDeclarativeGeoMap::fromCoordinate(const QGeoCoordinate &coordinate) const
{
    if (m_map)
        return m_map->coordinateToItemPosition(coordinate).toPointF();
    else
        return QPointF(qQNaN(), qQNaN());
}

/*!
    \qmlmethod QtLocation::Map::toScreenPosition(coordinate coordinate)
    \obsolete

    This function is missed named and is equilavent to \l {fromCoordinate}, which should be used
    instead.
*/
QPointF QDeclarativeGeoMap::toScreenPosition(const QGeoCoordinate &coordinate) const
{
    return fromCoordinate(coordinate);
}

/*!
    \qmlmethod void QtLocation::Map::pan(int dx, int dy)

    Starts panning the map by \a dx pixels along the x-axis and
    by \a dy pixels along the y-axis.

    Positive values for \a dx move the map right, negative values left.
    Positive values for \a dy move the map down, negative values up.

    During panning the \l center, and \l zoomLevel may change.
*/
void QDeclarativeGeoMap::pan(int dx, int dy)
{
    if (!m_mappingManagerInitialized)
        return;
    m_map->mapController()->pan(dx, dy);
}


/*!
    \qmlmethod void QtLocation::Map::prefetchData()

    Optional hint that allows the map to prefetch during this idle period
*/
void QDeclarativeGeoMap::prefetchData()
{
    if (!m_mappingManagerInitialized)
        return;
    m_map->prefetchData();
}

/*!
    \qmlproperty string QtLocation::Map::errorString

    This read-only property holds the textual presentation of the latest mapping provider error.
    If no error has occurred, an empty string is returned.

    An empty string may also be returned if an error occurred which has no associated
    textual representation.

    \sa QGeoServiceProvider::errorString()
*/

QString QDeclarativeGeoMap::errorString() const
{
    return m_errorString;
}

/*!
    \qmlproperty enumeration QtLocation::Map::error

    This read-only property holds the last occurred mapping service provider error.

    \list
    \li Map.NoError - No error has occurred.
    \li Map.NotSupportedError -The plugin does not support mapping functionality.
    \li Map.UnknownParameterError -The plugin did not recognize one of the parameters it was given.
    \li Map.MissingRequiredParameterError - The plugin did not find one of the parameters it was expecting.
    \li Map.ConnectionError - The plugin could not connect to its backend service or database.
    \endlist

    \sa QGeoServiceProvider::Error
*/

QGeoServiceProvider::Error QDeclarativeGeoMap::error() const
{
    return m_error;
}

/*!
    \internal
*/
void QDeclarativeGeoMap::touchEvent(QTouchEvent *event)
{
    if (isInteractive()) {
        m_gestureArea->handleTouchEvent(event);
        if ( event->type() == QEvent::TouchEnd ||
             event->type() == QEvent::TouchCancel) {
            ungrabTouchPoints();
        }
    }
    //this will always ignore event so sythesized event is generated;
    QQuickItem::touchEvent(event);
}

/*!
    \internal
*/
void QDeclarativeGeoMap::wheelEvent(QWheelEvent *event)
{
    if (isInteractive())
        m_gestureArea->handleWheelEvent(event);
    else
        QQuickItem::wheelEvent(event);

}

bool QDeclarativeGeoMap::isInteractive()
{
    return (m_gestureArea->enabled() && m_gestureArea->activeGestures()) || m_gestureArea->isActive();
}

/*!
    \internal
*/
bool QDeclarativeGeoMap::childMouseEventFilter(QQuickItem *item, QEvent *event)
{
    Q_UNUSED(item)
    if (!isVisible() || !isEnabled() || !isInteractive())
        return QQuickItem::childMouseEventFilter(item, event);

    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease:
        return sendMouseEvent(static_cast<QMouseEvent *>(event));
    case QEvent::UngrabMouse: {
        QQuickWindow *win = window();
        if (!win) break;
        if (!win->mouseGrabberItem() ||
                (win->mouseGrabberItem() &&
                 win->mouseGrabberItem() != this)) {
            // child lost grab, we could even lost
            // some events if grab already belongs for example
            // in item in diffrent window , clear up states
            mouseUngrabEvent();
        }
        break;
    }
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        if (static_cast<QTouchEvent *>(event)->touchPoints().count() >= 2) {
            // 1 touch point = handle with MouseEvent (event is always synthesized)
            // let the synthesized mouse event grab the mouse,
            // note there is no mouse grabber at this point since
            // touch event comes first (see Qt::AA_SynthesizeMouseForUnhandledTouchEvents)
            return sendTouchEvent(static_cast<QTouchEvent *>(event));
        }
    default:
        break;
    }
    return QQuickItem::childMouseEventFilter(item, event);
}

/*!
    \qmlmethod void QtLocation::Map::addMapItem(MapItem item)

    Adds the given \a item to the Map (for example MapQuickItem, MapCircle). If the object
    already is on the Map, it will not be added again.

    As an example, consider the case where you have a MapCircle representing your current position:

    \snippet declarative/maps.qml QtQuick import
    \snippet declarative/maps.qml QtLocation import
    \codeline
    \snippet declarative/maps.qml Map addMapItem MapCircle at current position

    \note MapItemViews cannot be added with this method.

    \sa mapItems, removeMapItem, clearMapItems
*/

void QDeclarativeGeoMap::addMapItem(QDeclarativeGeoMapItemBase *item)
{
    if (!item || item->quickMap())
        return;
    m_updateMutex.lock();
    item->setParentItem(this);
    if (m_map)
        item->setMap(this, m_map);
    m_mapItems.append(item);
    emit mapItemsChanged();
    m_updateMutex.unlock();
}

/*!
    \qmlproperty list<MapItem> QtLocation::Map::mapItems

    Returns the list of all map items in no particular order.
    These items include items that were declared statically as part of
    the type declaration, as well as dynamical items (\l addMapItem,
    \l MapItemView).

    \sa addMapItem, removeMapItem, clearMapItems
*/

QList<QObject *> QDeclarativeGeoMap::mapItems()
{
    QList<QObject *> ret;
    foreach (const QPointer<QDeclarativeGeoMapItemBase> &ptr, m_mapItems) {
        if (ptr)
            ret << ptr.data();
    }
    return ret;
}

/*!
    \qmlmethod void QtLocation::Map::removeMapItem(MapItem item)

    Removes the given \a item from the Map (for example MapQuickItem, MapCircle). If
    the MapItem does not exist or was not previously added to the map, the
    method does nothing.

    \sa mapItems, addMapItem, clearMapItems
*/
void QDeclarativeGeoMap::removeMapItem(QDeclarativeGeoMapItemBase *ptr)
{
    if (!ptr || !m_map)
        return;
    QPointer<QDeclarativeGeoMapItemBase> item(ptr);
    if (!m_mapItems.contains(item))
        return;
    m_updateMutex.lock();
    item.data()->setParentItem(0);
    item.data()->setMap(0, 0);
    // these can be optimized for perf, as we already check the 'contains' above
    m_mapItems.removeOne(item);
    emit mapItemsChanged();
    m_updateMutex.unlock();
}

/*!
    \qmlmethod void QtLocation::Map::clearMapItems()

    Removes all items from the map.

    \sa mapItems, addMapItem, removeMapItem
*/
void QDeclarativeGeoMap::clearMapItems()
{
    if (m_mapItems.isEmpty())
        return;
    m_updateMutex.lock();
    for (int i = 0; i < m_mapItems.count(); ++i) {
        if (m_mapItems.at(i)) {
            m_mapItems.at(i).data()->setParentItem(0);
            m_mapItems.at(i).data()->setMap(0, 0);
        }
    }
    m_mapItems.clear();
    emit mapItemsChanged();
    m_updateMutex.unlock();
}

/*!
    \qmlproperty MapType QtLocation::Map::activeMapType

    \brief Access to the currently active \l{MapType}{map type}.

    This property can be set to change the active \l{MapType}{map type}.
    See the \l{Map::supportedMapTypes}{supportedMapTypes} property for possible values.

    \sa MapType
*/
void QDeclarativeGeoMap::setActiveMapType(QDeclarativeGeoMapType *mapType)
{
    if (m_activeMapType->mapType() != mapType->mapType()) {
        m_activeMapType = mapType;
        if (m_map)
            m_map->setActiveMapType(mapType->mapType());
        emit activeMapTypeChanged();
    }
}

QDeclarativeGeoMapType * QDeclarativeGeoMap::activeMapType() const
{
    return m_activeMapType;
}

/*!
    \internal
*/
void QDeclarativeGeoMap::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    if (!m_mappingManagerInitialized)
        return;

    m_map->resize(newGeometry.width(), newGeometry.height());
    QQuickItem::geometryChanged(newGeometry, oldGeometry);

    /*!
        The fitViewportTo*() functions depend on a valid map geometry.
        If they were called prior to the first resize they cause
        the zoomlevel to jump to 0 (showing the world). Therefore the
        calls were queued up until now.

        Multiple fitViewportTo*() calls replace each other.
     */
    if (m_pendingFitViewport && width() && height()) {
        fitViewportToGeoShape();
        m_pendingFitViewport = false;
    }

}

// TODO Remove this function -> BC break
/*!
    \qmlmethod void QtLocation::Map::fitViewportToGeoShape(QGeoShape shape)

    \internal

    Fits the current viewport to the boundary of the shape. The camera is positioned
    in the center of the shape, and at the largest integral zoom level possible which
    allows the whole shape to be visible on screen

*/
void QDeclarativeGeoMap::fitViewportToGeoShape(const QVariant &variantShape)
{
    if (!m_map || !m_mappingManagerInitialized)
        return;

    QGeoShape shape;

    if (variantShape.userType() == qMetaTypeId<QGeoRectangle>())
        shape = variantShape.value<QGeoRectangle>();
    else if (variantShape.userType() == qMetaTypeId<QGeoCircle>())
        shape = variantShape.value<QGeoCircle>();
    else if (variantShape.userType() == qMetaTypeId<QGeoShape>())
        shape = variantShape.value<QGeoShape>();

    if (!shape.isValid())
        return;

    double bboxWidth;
    double bboxHeight;
    QGeoCoordinate centerCoordinate;

    switch (shape.type()) {
    case QGeoShape::RectangleType:
    {
        QGeoRectangle rect = shape;
        QDoubleVector2D topLeftPoint = m_map->coordinateToItemPosition(rect.topLeft(), false);
        QDoubleVector2D botRightPoint = m_map->coordinateToItemPosition(rect.bottomRight(), false);
        bboxWidth = qAbs(topLeftPoint.x() - botRightPoint.x());
        bboxHeight = qAbs(topLeftPoint.y() - botRightPoint.y());
        centerCoordinate = rect.center();
        break;
    }
    case QGeoShape::CircleType:
    {
        QGeoCircle circle = shape;
        centerCoordinate = circle.center();
        QGeoCoordinate edge = centerCoordinate.atDistanceAndAzimuth(circle.radius(), 90);
        QDoubleVector2D centerPoint = m_map->coordinateToItemPosition(centerCoordinate, false);
        QDoubleVector2D edgePoint = m_map->coordinateToItemPosition(edge, false);
        bboxWidth = qAbs(centerPoint.x() - edgePoint.x()) * 2;
        bboxHeight = bboxWidth;
        break;
    }
    case QGeoShape::UnknownType:
        //Fallthrough to default
    default:
        return;
    }

    // position camera to the center of bounding box
    setProperty("center", QVariant::fromValue(centerCoordinate));

    //If the shape is empty we just change centerposition, not zoom
    if (bboxHeight == 0 && bboxWidth == 0)
        return;

    // adjust zoom
    double bboxWidthRatio = bboxWidth / (bboxWidth + bboxHeight);
    double mapWidthRatio = width() / (width() + height());
    double zoomRatio;

    if (bboxWidthRatio > mapWidthRatio)
        zoomRatio = bboxWidth / width();
    else
        zoomRatio = bboxHeight / height();

    qreal newZoom = std::log10(zoomRatio) / std::log10(0.5);

    newZoom = std::floor(qMax(minimumZoomLevel(), (m_map->mapController()->zoom() + newZoom)));
    setProperty("zoomLevel", QVariant::fromValue(newZoom));
}

/*!
    \qmlmethod void QtLocation::Map::fitViewportToMapItems()

    Fits the current viewport to the boundary of all map items. The camera is positioned
    in the center of the map items, and at the largest integral zoom level possible which
    allows all map items to be visible on screen

*/
void QDeclarativeGeoMap::fitViewportToMapItems()
{
    if (!m_mappingManagerInitialized)
        return;
    fitViewportToMapItemsRefine(true);
}

/*!
    \internal
*/
void QDeclarativeGeoMap::fitViewportToMapItemsRefine(bool refine)
{
    if (m_mapItems.size() == 0)
        return;

    double minX = 0;
    double maxX = 0;
    double minY = 0;
    double maxY = 0;
    double topLeftX = 0;
    double topLeftY = 0;
    double bottomRightX = 0;
    double bottomRightY = 0;
    bool haveQuickItem = false;

    // find bounds of all map items
    int itemCount = 0;
    for (int i = 0; i < m_mapItems.count(); ++i) {
        if (!m_mapItems.at(i))
            continue;
        QDeclarativeGeoMapItemBase *item = m_mapItems.at(i).data();
        if (!item)
            continue;

        // skip quick items in the first pass and refine the fit later
        if (refine) {
            QDeclarativeGeoMapQuickItem *quickItem =
                    qobject_cast<QDeclarativeGeoMapQuickItem*>(item);
            if (quickItem) {
                haveQuickItem = true;
                continue;
            }
        }

        topLeftX = item->position().x();
        topLeftY = item->position().y();
        bottomRightX = topLeftX + item->width();
        bottomRightY = topLeftY + item->height();

        if (itemCount == 0) {
            minX = topLeftX;
            maxX = bottomRightX;
            minY = topLeftY;
            maxY = bottomRightY;
        } else {
            minX = qMin(minX, topLeftX);
            maxX = qMax(maxX, bottomRightX);
            minY = qMin(minY, topLeftY);
            maxY = qMax(maxY, bottomRightY);
        }
        ++itemCount;
    }

    if (itemCount == 0) {
        if (haveQuickItem)
            fitViewportToMapItemsRefine(false);
        return;
    }
    double bboxWidth = maxX - minX;
    double bboxHeight = maxY - minY;
    double bboxCenterX = minX + (bboxWidth / 2.0);
    double bboxCenterY = minY + (bboxHeight / 2.0);

    // position camera to the center of bounding box
    QGeoCoordinate coordinate;
    coordinate = m_map->itemPositionToCoordinate(QDoubleVector2D(bboxCenterX, bboxCenterY), false);
    setProperty("center", QVariant::fromValue(coordinate));

    // adjust zoom
    double bboxWidthRatio = bboxWidth / (bboxWidth + bboxHeight);
    double mapWidthRatio = width() / (width() + height());
    double zoomRatio;

    if (bboxWidthRatio > mapWidthRatio)
        zoomRatio = bboxWidth / width();
    else
        zoomRatio = bboxHeight / height();

    qreal newZoom = std::log10(zoomRatio) / std::log10(0.5);
    newZoom = std::floor(qMax(minimumZoomLevel(), (m_map->mapController()->zoom() + newZoom)));
    setProperty("zoomLevel", QVariant::fromValue(newZoom));

    // as map quick items retain the same screen size after the camera zooms in/out
    // we refine the viewport again to achieve better results
    if (refine)
        fitViewportToMapItemsRefine(false);
}

bool QDeclarativeGeoMap::sendMouseEvent(QMouseEvent *event)
{
    QPointF localPos = mapFromScene(event->windowPos());
    QQuickWindow *win = window();
    QQuickItem *grabber = win ? win->mouseGrabberItem() : 0;
    bool stealEvent = m_gestureArea->isActive();

    if ((stealEvent || contains(localPos)) && (!grabber || !grabber->keepMouseGrab())) {
        QScopedPointer<QMouseEvent> mouseEvent(QQuickWindowPrivate::cloneMouseEvent(event, &localPos));
        mouseEvent->setAccepted(false);

        switch (mouseEvent->type()) {
        case QEvent::MouseMove:
            m_gestureArea->handleMouseMoveEvent(mouseEvent.data());
            break;
        case QEvent::MouseButtonPress:
            m_gestureArea->handleMousePressEvent(mouseEvent.data());
            break;
        case QEvent::MouseButtonRelease:
            m_gestureArea->handleMouseReleaseEvent(mouseEvent.data());
            break;
        default:
            break;
        }

        stealEvent = m_gestureArea->isActive();
        grabber = win ? win->mouseGrabberItem() : 0;

        if (grabber && stealEvent && !grabber->keepMouseGrab() && grabber != this)
            grabMouse();

        if (stealEvent) {
            //do not deliver
            event->setAccepted(true);
            return true;
        } else {
            return false;
        }
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        if (win && win->mouseGrabberItem() == this)
            ungrabMouse();
    }

    return false;
}

bool QDeclarativeGeoMap::sendTouchEvent(QTouchEvent *event)
{
    QQuickWindowPrivate *win = window() ? QQuickWindowPrivate::get(window()) : 0;
    const QTouchEvent::TouchPoint &point = event->touchPoints().first();
    QQuickItem *grabber = win ? win->itemForTouchPointId.value(point.id()) : 0;

    bool stealEvent = m_gestureArea->isActive();
    bool containsPoint = contains(mapFromScene(point.scenePos()));

    if ((stealEvent || containsPoint) && (!grabber || !grabber->keepTouchGrab())) {
        QScopedPointer<QTouchEvent> touchEvent(new QTouchEvent(event->type(), event->device(), event->modifiers(), event->touchPointStates(), event->touchPoints()));
        touchEvent->setTimestamp(event->timestamp());
        touchEvent->setAccepted(false);

        m_gestureArea->handleTouchEvent(touchEvent.data());
        stealEvent = m_gestureArea->isActive();
        grabber = win ? win->itemForTouchPointId.value(point.id()) : 0;

        if (grabber && stealEvent && !grabber->keepTouchGrab() && grabber != this) {
            QVector<int> ids;
            foreach (const QTouchEvent::TouchPoint &tp, event->touchPoints()) {
                if (!(tp.state() & Qt::TouchPointReleased)) {
                    ids.append(tp.id());
                }
            }
            grabTouchPoints(ids);
        }

        if (stealEvent) {
            //do not deliver
            event->setAccepted(true);
            return true;
        } else {
            return false;
        }
    }

    if (event->type() == QEvent::TouchEnd) {
        if (win && win->itemForTouchPointId.value(point.id()) == this) {
            ungrabTouchPoints();
        }
    }
    return false;
}

#include "moc_qdeclarativegeomap_p.cpp"

QT_END_NAMESPACE
