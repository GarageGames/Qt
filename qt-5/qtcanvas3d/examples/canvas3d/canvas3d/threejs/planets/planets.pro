!include( ../../../../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

TEMPLATE = app

QT += qml quick

SOURCES += main.cpp

OTHER_FILES += *.qml \
               planets.js \
               doc/src/* \
               doc/images/*

RESOURCES += planets.qrc

ios {
    ios_icon.files = $$files($$PWD/ios/AppIcon*.png)
    QMAKE_BUNDLE_DATA += ios_icon
    QMAKE_INFO_PLIST = ios/Info.plist
}
