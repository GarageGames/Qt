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

#ifndef QDECLARATIVEGEOMAPITEMVIEW_H
#define QDECLARATIVEGEOMAPITEMVIEW_H

#include <QtCore/QModelIndex>
#include <QtQml/QQmlParserStatus>
#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

class QAbstractItemModel;
class QQmlComponent;
class QQuickItem;
class QDeclarativeGeoMap;
class QDeclarativeGeoMapItemBase;

class QDeclarativeGeoMapItemView : public QObject, public QQmlParserStatus
{
    Q_OBJECT

    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(QVariant model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QQmlComponent *delegate READ delegate WRITE setDelegate NOTIFY delegateChanged)
    Q_PROPERTY(bool autoFitViewport READ autoFitViewport WRITE setAutoFitViewport NOTIFY autoFitViewportChanged)

public:
    explicit QDeclarativeGeoMapItemView(QQuickItem *parent = 0);
    ~QDeclarativeGeoMapItemView();

    QVariant model() const;
    void setModel(const QVariant &);

    QQmlComponent *delegate() const;
    void setDelegate(QQmlComponent *);

    bool autoFitViewport() const;
    void setAutoFitViewport(const bool &);

    void setMap(QDeclarativeGeoMap *);
    void repopulate();
    void removeInstantiatedItems();

    qreal zValue();
    void setZValue(qreal zValue);

    // From QQmlParserStatus
    virtual void componentComplete();
    void classBegin() {}

Q_SIGNALS:
    void modelChanged();
    void delegateChanged();
    void autoFitViewportChanged();

private:
    QDeclarativeGeoMapItemBase *createItemFromItemModel(int modelRow);

    void fitViewport();

private Q_SLOTS:
    void itemModelReset();
    void itemModelRowsInserted(const QModelIndex &index, int start, int end);
    void itemModelRowsRemoved(const QModelIndex &index, int start, int end);

private:
    bool componentCompleted_;
    QQmlComponent *delegate_;
    QAbstractItemModel *itemModel_;
    QDeclarativeGeoMap *map_;
    QList<QDeclarativeGeoMapItemBase *> mapItemList_;
    bool fitViewport_;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QDeclarativeGeoMapItemView)

#endif
