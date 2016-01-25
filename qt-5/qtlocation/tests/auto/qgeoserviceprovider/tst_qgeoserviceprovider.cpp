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

#include <QObject>
#include <QDebug>
#include <QTest>
#include <QtLocation/QGeoServiceProvider>

QT_USE_NAMESPACE

class tst_QGeoServiceProvider : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void tst_availableServiceProvider();
    void tst_features_data();
    void tst_features();
    void tst_misc();
    void tst_nokiaRename();
};

void tst_QGeoServiceProvider::initTestCase()
{
    /*
     * Set custom path since CI doesn't install test plugins
     */
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath()
                                     + QStringLiteral("/../../../plugins"));
}

void tst_QGeoServiceProvider::tst_availableServiceProvider()
{
    const QStringList provider = QGeoServiceProvider::availableServiceProviders();

    // Currently provided plugins
    if (provider.count() != 7)
        qWarning() << provider;
    QCOMPARE(provider.count(), 7);
    // these providers are deployed
    QVERIFY(provider.contains(QStringLiteral("mapbox")));
    QVERIFY(provider.contains(QStringLiteral("here")));
    QVERIFY(provider.contains(QStringLiteral("osm")));
    // these providers exist for unit tests only
    QVERIFY(provider.contains(QStringLiteral("geocode.test.plugin")));
    QVERIFY(provider.contains(QStringLiteral("georoute.test.plugin")));
    QVERIFY(provider.contains(QStringLiteral("qmlgeo.test.plugin")));
    QVERIFY(provider.contains(QStringLiteral("test.places.unsupported")));

}

Q_DECLARE_METATYPE(QGeoServiceProvider::MappingFeatures)
Q_DECLARE_METATYPE(QGeoServiceProvider::GeocodingFeatures)
Q_DECLARE_METATYPE(QGeoServiceProvider::RoutingFeatures)
Q_DECLARE_METATYPE(QGeoServiceProvider::PlacesFeatures)

void tst_QGeoServiceProvider::tst_features_data()
{
    QTest::addColumn<QString>("providerName");
    QTest::addColumn<QGeoServiceProvider::MappingFeatures>("mappingFeatures");
    QTest::addColumn<QGeoServiceProvider::GeocodingFeatures>("codingFeatures");
    QTest::addColumn<QGeoServiceProvider::RoutingFeatures>("routingFeatures");
    QTest::addColumn<QGeoServiceProvider::PlacesFeatures>("placeFeatures");

    QTest::newRow("invalid") << QString("non-existing-provider-name")
                             << QGeoServiceProvider::MappingFeatures(QGeoServiceProvider::NoMappingFeatures)
                             << QGeoServiceProvider::GeocodingFeatures(QGeoServiceProvider::NoGeocodingFeatures)
                             << QGeoServiceProvider::RoutingFeatures(QGeoServiceProvider::NoRoutingFeatures)
                             << QGeoServiceProvider::PlacesFeatures(QGeoServiceProvider::NoPlacesFeatures);

    QTest::newRow("mapbox") << QString("mapbox")
                            << QGeoServiceProvider::MappingFeatures(QGeoServiceProvider::OnlineMappingFeature)
                            << QGeoServiceProvider::GeocodingFeatures(QGeoServiceProvider::NoGeocodingFeatures)
                            << QGeoServiceProvider::RoutingFeatures(QGeoServiceProvider::NoRoutingFeatures)
                            << QGeoServiceProvider::PlacesFeatures(QGeoServiceProvider::NoPlacesFeatures);

    QTest::newRow("here")   << QString("here")
                            << QGeoServiceProvider::MappingFeatures(QGeoServiceProvider::OnlineMappingFeature)
                            << QGeoServiceProvider::GeocodingFeatures(QGeoServiceProvider::OnlineGeocodingFeature
                                                                      | QGeoServiceProvider::ReverseGeocodingFeature)
                            << QGeoServiceProvider::RoutingFeatures(QGeoServiceProvider::OnlineRoutingFeature
                                                                    | QGeoServiceProvider::RouteUpdatesFeature
                                                                    | QGeoServiceProvider::AlternativeRoutesFeature
                                                                    | QGeoServiceProvider::ExcludeAreasRoutingFeature)
                            << QGeoServiceProvider::PlacesFeatures(QGeoServiceProvider::OnlinePlacesFeature
                                                                   | QGeoServiceProvider::PlaceRecommendationsFeature
                                                                   | QGeoServiceProvider::SearchSuggestionsFeature
                                                                   | QGeoServiceProvider::LocalizedPlacesFeature);

    QTest::newRow("osm")    << QString("osm")
                            << QGeoServiceProvider::MappingFeatures(QGeoServiceProvider::OnlineMappingFeature)
                            << QGeoServiceProvider::GeocodingFeatures(QGeoServiceProvider::OnlineGeocodingFeature
                                                                      | QGeoServiceProvider::ReverseGeocodingFeature)
                            << QGeoServiceProvider::RoutingFeatures(QGeoServiceProvider::OnlineRoutingFeature)
                            << QGeoServiceProvider::PlacesFeatures(QGeoServiceProvider::OnlinePlacesFeature);
}

void tst_QGeoServiceProvider::tst_features()
{
    QFETCH(QString, providerName);
    QFETCH(QGeoServiceProvider::MappingFeatures, mappingFeatures);
    QFETCH(QGeoServiceProvider::GeocodingFeatures, codingFeatures);
    QFETCH(QGeoServiceProvider::RoutingFeatures, routingFeatures);
    QFETCH(QGeoServiceProvider::PlacesFeatures, placeFeatures);

    QGeoServiceProvider provider(providerName);
    QCOMPARE(provider.mappingFeatures(), mappingFeatures);
    QCOMPARE(provider.geocodingFeatures(), codingFeatures);
    QCOMPARE(provider.routingFeatures(), routingFeatures);
    QCOMPARE(provider.placesFeatures(), placeFeatures);

    if (provider.mappingFeatures() == QGeoServiceProvider::NoMappingFeatures) {
        QVERIFY(provider.mappingManager() == Q_NULLPTR);
    } else {
        // some plugins require token/access parameter
        // they return 0 but set QGeoServiceProvider::MissingRequiredParameterError
        if (provider.mappingManager() != Q_NULLPTR)
            QCOMPARE(provider.error(), QGeoServiceProvider::NoError);
        else
            QCOMPARE(provider.error(), QGeoServiceProvider::MissingRequiredParameterError);
    }

    if (provider.geocodingFeatures() == QGeoServiceProvider::NoGeocodingFeatures) {
        QVERIFY(provider.geocodingManager() == Q_NULLPTR);
    } else {
        if (provider.geocodingManager() != Q_NULLPTR)
            QVERIFY(provider.geocodingManager() != Q_NULLPTR); //pointless but we want a VERIFY here
        else
            QCOMPARE(provider.error(), QGeoServiceProvider::MissingRequiredParameterError);
    }

    if (provider.routingFeatures() == QGeoServiceProvider::NoRoutingFeatures) {
        QVERIFY(provider.routingManager() == Q_NULLPTR);
    } else {
        if (provider.routingManager() != Q_NULLPTR)
            QCOMPARE(provider.error(), QGeoServiceProvider::NoError);
        else
            QCOMPARE(provider.error(), QGeoServiceProvider::MissingRequiredParameterError);
    }

    if (provider.placesFeatures() == QGeoServiceProvider::NoPlacesFeatures) {
        QVERIFY(provider.placeManager() == Q_NULLPTR);
    } else {
        if (provider.placeManager() != Q_NULLPTR)
            QCOMPARE(provider.error(), QGeoServiceProvider::NoError);
        else
            QCOMPARE(provider.error(), QGeoServiceProvider::MissingRequiredParameterError);
    }
}

void tst_QGeoServiceProvider::tst_misc()
{
    const QStringList provider = QGeoServiceProvider::availableServiceProviders();
    QVERIFY(provider.contains(QStringLiteral("osm")));
    QVERIFY(provider.contains(QStringLiteral("geocode.test.plugin")));

    QGeoServiceProvider test_experimental(
                QStringLiteral("geocode.test.plugin"), QVariantMap(), true);
    QGeoServiceProvider test_noexperimental(
                QStringLiteral("geocode.test.plugin"), QVariantMap(), false);
    QCOMPARE(test_experimental.error(), QGeoServiceProvider::NoError);
    QCOMPARE(test_noexperimental.error(), QGeoServiceProvider::NotSupportedError);

    QGeoServiceProvider osm_experimental(
                QStringLiteral("osm"), QVariantMap(), true);
    QGeoServiceProvider osm_noexperimental(
                QStringLiteral("osm"), QVariantMap(), false);
    QCOMPARE(osm_experimental.error(), QGeoServiceProvider::NoError);
    QCOMPARE(osm_noexperimental.error(), QGeoServiceProvider::NoError);
}

void tst_QGeoServiceProvider::tst_nokiaRename()
{
    // The "nokia" plugin was renamed to "here".
    // It remains available under the name "nokia" for now
    // but is not advertised via QGeoServiceProvider::availableServiceProviders()

    QVERIFY(!QGeoServiceProvider::availableServiceProviders().contains("nokia"));
    QGeoServiceProvider provider(QStringLiteral("nokia"));
    QCOMPARE(provider.error(), QGeoServiceProvider::NoError);

}

QTEST_GUILESS_MAIN(tst_QGeoServiceProvider)

#include "tst_qgeoserviceprovider.moc"
