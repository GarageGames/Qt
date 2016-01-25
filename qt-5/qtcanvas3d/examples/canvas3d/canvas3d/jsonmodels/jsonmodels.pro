!include( ../../../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

SOURCES += main.cpp

RESOURCES += qml.qrc

OTHER_FILES += doc/src/* \
               doc/images/*
