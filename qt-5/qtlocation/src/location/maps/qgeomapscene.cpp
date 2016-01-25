/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2014 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
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
#include "qgeomapscene_p.h"
#include "qgeocameradata_p.h"
#include "qgeotilecache_p.h"
#include "qgeotilespec_p.h"
#include <QtPositioning/private/qdoublevector3d_p.h>
#include <QtCore/private/qobject_p.h>
#include <QtQuick/QSGSimpleTextureNode>
#include <QtQuick/QQuickWindow>
#include <cmath>

QT_BEGIN_NAMESPACE

class QGeoMapScenePrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QGeoMapScene)
public:
    QGeoMapScenePrivate();
    ~QGeoMapScenePrivate();

    QSize m_screenSize; // in pixels
    int m_tileSize; // the pixel resolution for each tile
    QGeoCameraData m_cameraData;
    QSet<QGeoTileSpec> m_visibleTiles;

    QDoubleVector3D m_cameraUp;
    QDoubleVector3D m_cameraEye;
    QDoubleVector3D m_cameraCenter;
    QMatrix4x4 m_projectionMatrix;

    // scales up the tile geometry and the camera altitude, resulting in no visible effect
    // other than to control the accuracy of the render by keeping the values in a sensible range
    double m_scaleFactor;

    // rounded down, positive zoom is zooming in, corresponding to reduced altitude
    int m_intZoomLevel;

    // mercatorToGrid transform
    // the number of tiles in each direction for the whole map (earth) at the current zoom level.
    // it is 1<<zoomLevel
    int m_sideLength;

    QHash<QGeoTileSpec, QSharedPointer<QGeoTileTexture> > m_textures;

    // tilesToGrid transform
    int m_minTileX; // the minimum tile index, i.e. 0 to sideLength which is 1<< zoomLevel
    int m_minTileY;
    int m_maxTileX;
    int m_maxTileY;
    int m_tileXWrapsBelow; // the wrap point as a tile index

    // cameraToGrid transform
    double m_mercatorCenterX; // center of camera in grid space (0 to sideLength)
    double m_mercatorCenterY;
    double m_mercatorWidth;   // width of camera in grid space (0 to sideLength)
    double m_mercatorHeight;

    // screenToWindow transform
    double m_screenOffsetX; // in pixels
    double m_screenOffsetY; // in pixels
    // cameraToScreen transform
    double m_screenWidth; // in pixels
    double m_screenHeight; // in pixels

    bool m_useVerticalLock;
    bool m_verticalLock;
    bool m_linearScaling;

    void addTile(const QGeoTileSpec &spec, QSharedPointer<QGeoTileTexture> texture);

    QDoubleVector2D itemPositionToMercator(const QDoubleVector2D &pos) const;
    QDoubleVector2D mercatorToItemPosition(const QDoubleVector2D &mercator) const;

    void setVisibleTiles(const QSet<QGeoTileSpec> &tiles);
    void removeTiles(const QSet<QGeoTileSpec> &oldTiles);
    bool buildGeometry(const QGeoTileSpec &spec, QSGGeometry::TexturedPoint2D *vertices);
    void setTileBounds(const QSet<QGeoTileSpec> &tiles);
    void setupCamera();
};

QGeoMapScene::QGeoMapScene(QObject *parent)
    : QObject(*new QGeoMapScenePrivate(),parent)
{
}

QGeoMapScene::~QGeoMapScene()
{
}

void QGeoMapScene::setUseVerticalLock(bool lock)
{
    Q_D(QGeoMapScene);
    d->m_useVerticalLock = lock;
}

void QGeoMapScene::setScreenSize(const QSize &size)
{
    Q_D(QGeoMapScene);
    d->m_screenSize = size;
}

void QGeoMapScene::setTileSize(int tileSize)
{
    Q_D(QGeoMapScene);
    d->m_tileSize = tileSize;
}

void QGeoMapScene::setCameraData(const QGeoCameraData &cameraData)
{
    Q_D(QGeoMapScene);
    d->m_cameraData = cameraData;
    d->m_intZoomLevel = static_cast<int>(std::floor(d->m_cameraData.zoomLevel()));
    float delta = cameraData.zoomLevel() - d->m_intZoomLevel;
    d->m_linearScaling = qAbs(delta) > 0.05;
    d->m_sideLength = 1 << d->m_intZoomLevel;
}

void QGeoMapScene::setVisibleTiles(const QSet<QGeoTileSpec> &tiles)
{
    Q_D(QGeoMapScene);
    d->setVisibleTiles(tiles);
}

void QGeoMapScene::addTile(const QGeoTileSpec &spec, QSharedPointer<QGeoTileTexture> texture)
{
    Q_D(QGeoMapScene);
    d->addTile(spec, texture);
}

QDoubleVector2D QGeoMapScene::itemPositionToMercator(const QDoubleVector2D &pos) const
{
    Q_D(const QGeoMapScene);
    return d->itemPositionToMercator(pos);
}

QDoubleVector2D QGeoMapScene::mercatorToItemPosition(const QDoubleVector2D &mercator) const
{
    Q_D(const QGeoMapScene);
    return d->mercatorToItemPosition(mercator);
}

bool QGeoMapScene::verticalLock() const
{
    Q_D(const QGeoMapScene);
    return d->m_verticalLock;
}

QSet<QGeoTileSpec> QGeoMapScene::texturedTiles()
{
    Q_D(QGeoMapScene);
    QSet<QGeoTileSpec> textured;
    foreach (const QGeoTileSpec &tile, d->m_textures.keys()) {
        textured += tile;
    }
    return textured;
}

QGeoMapScenePrivate::QGeoMapScenePrivate()
    : QObjectPrivate(),
      m_tileSize(0),
      m_scaleFactor(10.0),
      m_intZoomLevel(0),
      m_sideLength(0),
      m_minTileX(-1),
      m_minTileY(-1),
      m_maxTileX(-1),
      m_maxTileY(-1),
      m_tileXWrapsBelow(0),
      m_mercatorCenterX(0.0),
      m_mercatorCenterY(0.0),
      m_mercatorWidth(0.0),
      m_mercatorHeight(0.0),
      m_screenOffsetX(0.0),
      m_screenOffsetY(0.0),
      m_screenWidth(0.0),
      m_screenHeight(0.0),
      m_useVerticalLock(false),
      m_verticalLock(false),
      m_linearScaling(false)
{
}

QGeoMapScenePrivate::~QGeoMapScenePrivate()
{
}

QDoubleVector2D QGeoMapScenePrivate::itemPositionToMercator(const QDoubleVector2D &pos) const
{
    double x = m_mercatorWidth * (((pos.x() - m_screenOffsetX) / m_screenWidth) - 0.5);
    x += m_mercatorCenterX;

    if (x > 1.0 * m_sideLength)
        x -= 1.0 * m_sideLength;
    if (x < 0.0)
        x += 1.0 * m_sideLength;

    x /= 1.0 * m_sideLength;

    double y = m_mercatorHeight * (((pos.y() - m_screenOffsetY) / m_screenHeight) - 0.5);
    y += m_mercatorCenterY;
    y /= 1.0 * m_sideLength;

    return QDoubleVector2D(x, y);
}

QDoubleVector2D QGeoMapScenePrivate::mercatorToItemPosition(const QDoubleVector2D &mercator) const
{
    double mx = m_sideLength * mercator.x();

    double lb = m_mercatorCenterX - m_mercatorWidth / 2.0;
    if (lb < 0.0)
        lb += m_sideLength;
    double ub = m_mercatorCenterX + m_mercatorWidth / 2.0;
    if (m_sideLength < ub)
        ub -= m_sideLength;

    double m = (mx - m_mercatorCenterX) / m_mercatorWidth;

    double mWrapLower = (mx - m_mercatorCenterX - m_sideLength) / m_mercatorWidth;
    double mWrapUpper = (mx - m_mercatorCenterX + m_sideLength) / m_mercatorWidth;

    // correct for crossing dateline
    if (qFuzzyCompare(ub - lb + 1.0, 1.0) || (ub < lb) ) {
        if (m_mercatorCenterX < ub) {
            if (lb < mx) {
                 m = mWrapLower;
            }
        } else if (lb < m_mercatorCenterX) {
            if (mx <= ub) {
                m = mWrapUpper;
            }
        }
    }

    // apply wrapping if necessary so we don't return unreasonably large pos/neg screen positions
    // also allows map items to be drawn properly if some of their coords are out of the screen
    if ( qAbs(mWrapLower) < qAbs(m) )
        m = mWrapLower;
    if ( qAbs(mWrapUpper) < qAbs(m) )
        m = mWrapUpper;

    double x = m_screenWidth * (0.5 + m);
    double y = m_screenHeight * (0.5 + (m_sideLength * mercator.y() - m_mercatorCenterY) / m_mercatorHeight);

    return QDoubleVector2D(x + m_screenOffsetX, y + m_screenOffsetY);
}

bool QGeoMapScenePrivate::buildGeometry(const QGeoTileSpec &spec, QSGGeometry::TexturedPoint2D *vertices)
{
    int x = spec.x();

    if (x < m_tileXWrapsBelow)
        x += m_sideLength;

    if ((x < m_minTileX)
            || (m_maxTileX < x)
            || (spec.y() < m_minTileY)
            || (m_maxTileY < spec.y())
            || (spec.zoom() != m_intZoomLevel)) {
        return false;
    }

    double edge = m_scaleFactor * m_tileSize;

    double x1 = (x - m_minTileX);
    double x2 = x1 + 1.0;

    double y1 = (m_minTileY - spec.y());
    double y2 = y1 - 1.0;

    x1 *= edge;
    x2 *= edge;
    y1 *= edge;
    y2 *= edge;

    //Texture coordinate order for veritcal flip of texture
    vertices[0].set(x1, y1, 0, 0);
    vertices[1].set(x1, y2, 0, 1);
    vertices[2].set(x2, y1, 1, 0);
    vertices[3].set(x2, y2, 1, 1);

    return true;
}

void QGeoMapScenePrivate::addTile(const QGeoTileSpec &spec, QSharedPointer<QGeoTileTexture> texture)
{
    if (!m_visibleTiles.contains(spec)) // Don't add the geometry if it isn't visible
        return;

    m_textures.insert(spec, texture);
}

// return true if new tiles introduced in [tiles]
void QGeoMapScenePrivate::setVisibleTiles(const QSet<QGeoTileSpec> &tiles)
{
    Q_Q(QGeoMapScene);

    // detect if new tiles introduced
    bool newTilesIntroduced = !m_visibleTiles.contains(tiles);

    // work out the tile bounds for the new scene
    setTileBounds(tiles);

    // set up the gl camera for the new scene
    setupCamera();

    QSet<QGeoTileSpec> toRemove = m_visibleTiles - tiles;
    if (!toRemove.isEmpty())
        removeTiles(toRemove);

    m_visibleTiles = tiles;
    if (newTilesIntroduced)
        emit q->newTilesVisible(m_visibleTiles);
}

void QGeoMapScenePrivate::removeTiles(const QSet<QGeoTileSpec> &oldTiles)
{
    typedef QSet<QGeoTileSpec>::const_iterator iter;
    iter i = oldTiles.constBegin();
    iter end = oldTiles.constEnd();

    for (; i != end; ++i) {
        QGeoTileSpec tile = *i;
        m_textures.remove(tile);
    }
}

void QGeoMapScenePrivate::setTileBounds(const QSet<QGeoTileSpec> &tiles)
{
    if (tiles.isEmpty()) {
        m_minTileX = -1;
        m_minTileY = -1;
        m_maxTileX = -1;
        m_maxTileY = -1;
        return;
    }

    typedef QSet<QGeoTileSpec>::const_iterator iter;
    iter i = tiles.constBegin();
    iter end = tiles.constEnd();

    // determine whether the set of map tiles crosses the dateline.
    // A gap in the tiles indicates dateline crossing
    bool hasFarLeft = false;
    bool hasFarRight = false;
    bool hasMidLeft = false;
    bool hasMidRight = false;

    for (; i != end; ++i) {
        if ((*i).zoom() != m_intZoomLevel)
            continue;
        int x = (*i).x();
        if (x == 0)
            hasFarLeft = true;
        else if (x == (m_sideLength - 1))
            hasFarRight = true;
        else if (x == ((m_sideLength / 2) - 1)) {
            hasMidLeft = true;
        } else if (x == (m_sideLength / 2)) {
            hasMidRight = true;
        }
    }

    // if dateline crossing is detected we wrap all x pos of tiles
    // that are in the left half of the map.
    m_tileXWrapsBelow = 0;

    if (hasFarLeft && hasFarRight) {
        if (!hasMidRight) {
            m_tileXWrapsBelow = m_sideLength / 2;
        } else if (!hasMidLeft) {
            m_tileXWrapsBelow = (m_sideLength / 2) - 1;
        }
    }

    // finally, determine the min and max bounds
    i = tiles.constBegin();

    QGeoTileSpec tile = *i;

    int x = tile.x();
    if (tile.x() < m_tileXWrapsBelow)
        x += m_sideLength;

    m_minTileX = x;
    m_maxTileX = x;
    m_minTileY = tile.y();
    m_maxTileY = tile.y();

    ++i;

    for (; i != end; ++i) {
        tile = *i;
        if (tile.zoom() != m_intZoomLevel)
            continue;

        int x = tile.x();
        if (tile.x() < m_tileXWrapsBelow)
            x += m_sideLength;

        m_minTileX = qMin(m_minTileX, x);
        m_maxTileX = qMax(m_maxTileX, x);
        m_minTileY = qMin(m_minTileY, tile.y());
        m_maxTileY = qMax(m_maxTileY, tile.y());
    }
}

void QGeoMapScenePrivate::setupCamera()
{
    double f = 1.0 * qMin(m_screenSize.width(), m_screenSize.height());

    // fraction of zoom level
    double z = std::pow(2.0, m_cameraData.zoomLevel() - m_intZoomLevel) * m_tileSize;

    // calculate altitdue that allows the visible map tiles
    // to fit in the screen correctly (note that a larger f will cause
    // the camera be higher, resulting in gray areas displayed around
    // the tiles)
    double altitude = f / (2.0 * z) ;

    // mercatorWidth_ and mercatorHeight_ define the ratio for
    // mercator and screen coordinate conversion,
    // see mercatorToItemPosition() and itemPositionToMercator()
    m_mercatorHeight = m_screenSize.height() / z;
    m_mercatorWidth = m_screenSize.width() / z;

    // calculate center
    double edge = m_scaleFactor * m_tileSize;

    // first calculate the camera center in map space in the range of 0 <-> sideLength (2^z)
    QDoubleVector3D center = (m_sideLength * QGeoProjection::coordToMercator(m_cameraData.center()));

    // wrap the center if necessary (due to dateline crossing)
    if (center.x() < m_tileXWrapsBelow)
        center.setX(center.x() + 1.0 * m_sideLength);

    m_mercatorCenterX = center.x();
    m_mercatorCenterY = center.y();

    // work out where the camera center is w.r.t minimum tile bounds
    center.setX(center.x() - 1.0 * m_minTileX);
    center.setY(1.0 * m_minTileY - center.y());

    // letter box vertically
    if (m_useVerticalLock && (m_mercatorHeight > 1.0 * m_sideLength)) {
        center.setY(-1.0 * m_sideLength / 2.0);
        m_mercatorCenterY = m_sideLength / 2.0;
        m_screenOffsetY = m_screenSize.height() * (0.5 - m_sideLength / (2 * m_mercatorHeight));
        m_screenHeight = m_screenSize.height() - 2 * m_screenOffsetY;
        m_mercatorHeight = 1.0 * m_sideLength;
        m_verticalLock = true;
    } else {
        m_screenOffsetY = 0.0;
        m_screenHeight = m_screenSize.height();
        m_verticalLock = false;
    }

    if (m_mercatorWidth > 1.0 * m_sideLength) {
        m_screenOffsetX = m_screenSize.width() * (0.5 - (m_sideLength / (2 * m_mercatorWidth)));
        m_screenWidth = m_screenSize.width() - 2 * m_screenOffsetX;
        m_mercatorWidth = 1.0 * m_sideLength;
    } else {
        m_screenOffsetX = 0.0;
        m_screenWidth = m_screenSize.width();
    }

    // apply necessary scaling to the camera center
    center *= edge;

    // calculate eye

    QDoubleVector3D eye = center;
    eye.setZ(altitude * edge);

    // calculate up

    QDoubleVector3D view = eye - center;
    QDoubleVector3D side = QDoubleVector3D::normal(view, QDoubleVector3D(0.0, 1.0, 0.0));
    QDoubleVector3D up = QDoubleVector3D::normal(side, view);

    // old bearing, tilt and roll code
    //    QMatrix4x4 mBearing;
    //    mBearing.rotate(-1.0 * camera.bearing(), view);
    //    up = mBearing * up;

    //    QDoubleVector3D side2 = QDoubleVector3D::normal(up, view);
    //    QMatrix4x4 mTilt;
    //    mTilt.rotate(camera.tilt(), side2);
    //    eye = (mTilt * view) + center;

    //    view = eye - center;
    //    side = QDoubleVector3D::normal(view, QDoubleVector3D(0.0, 1.0, 0.0));
    //    up = QDoubleVector3D::normal(view, side2);

    //    QMatrix4x4 mRoll;
    //    mRoll.rotate(camera.roll(), view);
    //    up = mRoll * up;

    // near plane and far plane

    double nearPlane = 1.0;
    double farPlane = (altitude + 1.0) * edge;

    m_cameraUp = up;
    m_cameraCenter = center;
    m_cameraEye = eye;

    double aspectRatio = 1.0 * m_screenSize.width() / m_screenSize.height();
    float halfWidth = 1;
    float halfHeight = 1;
    if (aspectRatio > 1.0) {
        halfWidth *= aspectRatio;
    } else if (aspectRatio > 0.0f && aspectRatio < 1.0f) {
        halfHeight /= aspectRatio;
    }
    m_projectionMatrix.setToIdentity();
    m_projectionMatrix.frustum(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
}

class QGeoMapTileContainerNode : public QSGTransformNode
{
public:
    void addChild(const QGeoTileSpec &spec, QSGSimpleTextureNode *node)
    {
        tiles.insert(spec, node);
        appendChildNode(node);
    }
    QHash<QGeoTileSpec, QSGSimpleTextureNode *> tiles;
};

class QGeoMapRootNode : public QSGClipNode
{
public:
    QGeoMapRootNode()
        : isTextureLinear(false)
        , geometry(QSGGeometry::defaultAttributes_Point2D(), 4)
        , root(new QSGTransformNode())
        , tiles(new QGeoMapTileContainerNode())
        , wrapLeft(new QGeoMapTileContainerNode())
        , wrapRight(new QGeoMapTileContainerNode())
    {
        setIsRectangular(true);
        setGeometry(&geometry);
        root->appendChildNode(tiles);
        root->appendChildNode(wrapLeft);
        root->appendChildNode(wrapRight);
        appendChildNode(root);
    }

    ~QGeoMapRootNode()
    {
        qDeleteAll(textures.values());
    }

    void setClipRect(const QRect &rect)
    {
        if (rect != clipRect) {
            QSGGeometry::updateRectGeometry(&geometry, rect);
            QSGClipNode::setClipRect(rect);
            clipRect = rect;
            markDirty(DirtyGeometry);
        }
    }

    void updateTiles(QGeoMapTileContainerNode *root, QGeoMapScenePrivate *d, double camAdjust);

    bool isTextureLinear;

    QSGGeometry geometry;
    QRect clipRect;

    QSGTransformNode *root;

    QGeoMapTileContainerNode *tiles;        // The majority of the tiles
    QGeoMapTileContainerNode *wrapLeft;     // When zoomed out, the tiles that wrap around on the left.
    QGeoMapTileContainerNode *wrapRight;    // When zoomed out, the tiles that wrap around on the right

    QHash<QGeoTileSpec, QSGTexture *> textures;
};

static bool qgeomapscene_isTileInViewport(const QSGGeometry::TexturedPoint2D *tp, const QMatrix4x4 &matrix) {
    QPolygonF polygon; polygon.reserve(4);
    for (int i=0; i<4; ++i)
        polygon << matrix * QPointF(tp[i].x, tp[i].y);
    return QRectF(-1, -1, 2, 2).intersects(polygon.boundingRect());
}

static QVector3D toVector3D(const QDoubleVector3D& in)
{
    return QVector3D(in.x(), in.y(), in.z());
}

void QGeoMapRootNode::updateTiles(QGeoMapTileContainerNode *root,
                                  QGeoMapScenePrivate *d,
                                  double camAdjust)
{
    // Set up the matrix...
    QDoubleVector3D eye = d->m_cameraEye;
    eye.setX(eye.x() + camAdjust);
    QDoubleVector3D center = d->m_cameraCenter;
    center.setX(center.x() + camAdjust);
    QMatrix4x4 cameraMatrix;
    cameraMatrix.lookAt(toVector3D(eye), toVector3D(center), toVector3D(d->m_cameraUp));
    root->setMatrix(d->m_projectionMatrix * cameraMatrix);

    QSet<QGeoTileSpec> tilesInSG = QSet<QGeoTileSpec>::fromList(root->tiles.keys());
    QSet<QGeoTileSpec> toRemove = tilesInSG - d->m_visibleTiles;
    QSet<QGeoTileSpec> toAdd = d->m_visibleTiles - tilesInSG;

    foreach (const QGeoTileSpec &s, toRemove)
        delete root->tiles.take(s);

    for (QHash<QGeoTileSpec, QSGSimpleTextureNode *>::iterator it = root->tiles.begin();
         it != root->tiles.end(); ) {
        QSGGeometry visualGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
        QSGGeometry::TexturedPoint2D *v = visualGeometry.vertexDataAsTexturedPoint2D();
        bool ok = d->buildGeometry(it.key(), v) && qgeomapscene_isTileInViewport(v, root->matrix());
        QSGSimpleTextureNode *node = it.value();
        QSGNode::DirtyState dirtyBits = 0;

        // Check and handle changes to vertex data.
        if (ok && memcmp(node->geometry()->vertexData(), v, 4 * sizeof(QSGGeometry::TexturedPoint2D)) != 0) {
            if (v[0].x == v[3].x || v[0].y == v[3].y) { // top-left == bottom-right => invalid => remove
                ok = false;
            } else {
                memcpy(node->geometry()->vertexData(), v, 4 * sizeof(QSGGeometry::TexturedPoint2D));
                dirtyBits |= QSGNode::DirtyGeometry;
            }
        }

        if (!ok) {
            it = root->tiles.erase(it);
            delete node;
        } else {
            if (isTextureLinear != d->m_linearScaling) {
                node->setFiltering(d->m_linearScaling ? QSGTexture::Linear : QSGTexture::Nearest);
                dirtyBits |= QSGNode::DirtyMaterial;
            }
            if (dirtyBits != 0)
                node->markDirty(dirtyBits);
            it++;
        }
    }

    foreach (const QGeoTileSpec &s, toAdd) {
        QGeoTileTexture *tileTexture = d->m_textures.value(s).data();
        if (!tileTexture || tileTexture->image.isNull())
            continue;
        QSGSimpleTextureNode *tileNode = new QSGSimpleTextureNode();
        // note: setTexture will update coordinates so do it here, before we buildGeometry
        tileNode->setTexture(textures.value(s));
        Q_ASSERT(tileNode->geometry());
        Q_ASSERT(tileNode->geometry()->attributes() == QSGGeometry::defaultAttributes_TexturedPoint2D().attributes);
        Q_ASSERT(tileNode->geometry()->vertexCount() == 4);
        if (d->buildGeometry(s, tileNode->geometry()->vertexDataAsTexturedPoint2D())
                && qgeomapscene_isTileInViewport(tileNode->geometry()->vertexDataAsTexturedPoint2D(), root->matrix())) {
            tileNode->setFiltering(d->m_linearScaling ? QSGTexture::Linear : QSGTexture::Nearest);
            root->addChild(s, tileNode);
        } else {
            delete tileNode;
        }
    }
}

QSGNode *QGeoMapScene::updateSceneGraph(QSGNode *oldNode, QQuickWindow *window)
{
    Q_D(QGeoMapScene);
    float w = d->m_screenSize.width();
    float h = d->m_screenSize.height();
    if (w <= 0 || h <= 0) {
        delete oldNode;
        return 0;
    }

    QGeoMapRootNode *mapRoot = static_cast<QGeoMapRootNode *>(oldNode);
    if (!mapRoot)
        mapRoot = new QGeoMapRootNode();

    mapRoot->setClipRect(QRect(d->m_screenOffsetX, d->m_screenOffsetY, d->m_screenWidth, d->m_screenHeight));

    QMatrix4x4 itemSpaceMatrix;
    itemSpaceMatrix.scale(w / 2, h / 2);
    itemSpaceMatrix.translate(1, 1);
    itemSpaceMatrix.scale(1, -1);
    mapRoot->root->setMatrix(itemSpaceMatrix);

    QSet<QGeoTileSpec> textures = QSet<QGeoTileSpec>::fromList(mapRoot->textures.keys());
    QSet<QGeoTileSpec> toRemove = textures - d->m_visibleTiles;
    QSet<QGeoTileSpec> toAdd = d->m_visibleTiles - textures;

    foreach (const QGeoTileSpec &spec, toRemove)
        mapRoot->textures.take(spec)->deleteLater();
    foreach (const QGeoTileSpec &spec, toAdd) {
        QGeoTileTexture *tileTexture = d->m_textures.value(spec).data();
        if (!tileTexture || tileTexture->image.isNull())
            continue;
        mapRoot->textures.insert(spec, window->createTextureFromImage(tileTexture->image));
    }

    double sideLength = d->m_scaleFactor * d->m_tileSize * d->m_sideLength;
    mapRoot->updateTiles(mapRoot->tiles, d, 0);
    mapRoot->updateTiles(mapRoot->wrapLeft, d, +sideLength);
    mapRoot->updateTiles(mapRoot->wrapRight, d, -sideLength);

    mapRoot->isTextureLinear = d->m_linearScaling;

    return mapRoot;
}

QT_END_NAMESPACE
