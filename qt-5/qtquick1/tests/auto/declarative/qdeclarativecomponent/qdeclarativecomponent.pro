CONFIG += testcase
TARGET = tst_qdeclarativecomponent

QT += testlib declarative
QT += script network widgets
macx:CONFIG -= app_bundle

SOURCES += tst_qdeclarativecomponent.cpp

RESOURCES += data/data.qrc

CONFIG += parallel_test

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
