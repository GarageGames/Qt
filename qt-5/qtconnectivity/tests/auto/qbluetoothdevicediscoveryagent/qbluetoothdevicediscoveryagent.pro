SOURCES += tst_qbluetoothdevicediscoveryagent.cpp
TARGET=tst_qbluetoothdevicediscoveryagent
CONFIG += testcase

QT = core concurrent bluetooth testlib
osx:QT += widgets
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
