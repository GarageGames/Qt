TARGET = qtgeoservices_mapbox
QT += location-private positioning-private network

PLUGIN_TYPE = geoservices
PLUGIN_CLASS_NAME = QGeoServiceProviderFactoryMapbox
load(qt_plugin)

HEADERS += \
    qgeoserviceproviderpluginmapbox.h \
    qgeotiledmappingmanagerenginemapbox.h \
    qgeotilefetchermapbox.h \
    qgeomapreplymapbox.h

SOURCES += \
    qgeoserviceproviderpluginmapbox.cpp \
    qgeotiledmappingmanagerenginemapbox.cpp \
    qgeotilefetchermapbox.cpp \
    qgeomapreplymapbox.cpp

OTHER_FILES += \
    mapbox_plugin.json

