!include( ../../../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

SOURCES += main.cpp

RESOURCES += framebuffer.qrc

OTHER_FILES += qml/framebuffer/* \
               doc/src/* \
               doc/images/*

