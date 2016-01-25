/****************************************************************************
**
** Copyright (C) 2015 Aaron McCarthy <mccarthy.aaron@gmail.com>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtFoo module of the Qt Toolkit.
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

#ifndef QPLACEMANAGERENGINEOSM_H
#define QPLACEMANAGERENGINEOSM_H

#include <QtLocation/QPlaceManagerEngine>
#include <QtLocation/QGeoServiceProvider>

QT_BEGIN_NAMESPACE

class QNetworkAccessManager;
class QNetworkReply;
class QPlaceCategoriesReplyOsm;

class QPlaceManagerEngineOsm : public QPlaceManagerEngine
{
    Q_OBJECT

public:
    QPlaceManagerEngineOsm(const QVariantMap &parameters, QGeoServiceProvider::Error *error,
                           QString *errorString);
    ~QPlaceManagerEngineOsm();

    QPlaceSearchReply *search(const QPlaceSearchRequest &request) Q_DECL_OVERRIDE;

    QPlaceReply *initializeCategories() Q_DECL_OVERRIDE;
    QString parentCategoryId(const QString &categoryId) const Q_DECL_OVERRIDE;
    QStringList childCategoryIds(const QString &categoryId) const Q_DECL_OVERRIDE;
    QPlaceCategory category(const QString &categoryId) const Q_DECL_OVERRIDE;

    QList<QPlaceCategory> childCategories(const QString &parentId) const Q_DECL_OVERRIDE;

    QList<QLocale> locales() const Q_DECL_OVERRIDE;
    void setLocales(const QList<QLocale> &locales) Q_DECL_OVERRIDE;

private slots:
    void categoryReplyFinished();
    void categoryReplyError();
    void replyFinished();
    void replyError(QPlaceReply::Error errorCode, const QString &errorString);

private:
    void fetchNextCategoryLocale();

    QNetworkAccessManager *m_networkManager;
    QByteArray m_userAgent;
    QString m_urlPrefix;
    QList<QLocale> m_locales;

    QNetworkReply *m_categoriesReply;
    QList<QPlaceCategoriesReplyOsm *> m_pendingCategoriesReply;
    QHash<QString, QPlaceCategory> m_categories;
    QHash<QString, QStringList> m_subcategories;

    QList<QLocale> m_categoryLocales;
};

QT_END_NAMESPACE

#endif // QPLACEMANAGERENGINEOSM_H
