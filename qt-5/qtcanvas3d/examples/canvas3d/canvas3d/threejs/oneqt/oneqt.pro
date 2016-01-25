!include( ../../../../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

TEMPLATE = app

QT += qml quick

SOURCES += main.cpp

OTHER_FILES += doc/src/* \
               doc/images/*

RESOURCES += oneqt.qrc

DISTFILES += \
    ImageCube.qml

ios {
    ios_icon.files = $$files($$PWD/ios/OneQtIcon*.png)
    QMAKE_BUNDLE_DATA += ios_icon
    QMAKE_INFO_PLIST = ios/Info.plist
}
