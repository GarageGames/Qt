QT += quick-private positioning-private qml-private core-private

INCLUDEPATH *= $$PWD

HEADERS += qdeclarativeposition_p.h \
           qdeclarativepositionsource_p.h \
           locationsingleton.h \
           qdeclarativegeocoordinateanimation_p.h

SOURCES += qdeclarativeposition.cpp \
           positioning.cpp \
           qdeclarativepositionsource.cpp \
           locationsingleton.cpp \
           qdeclarativegeocoordinateanimation.cpp

load(qml_plugin)

OTHER_FILES += \
    plugin.json \
    qmldir
