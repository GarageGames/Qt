!include( ../../../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

SOURCES += main.cpp

RESOURCES += textureandlight.qrc

OTHER_FILES += qml/textureandlight/* \
               doc/src/* \
               doc/images/*

