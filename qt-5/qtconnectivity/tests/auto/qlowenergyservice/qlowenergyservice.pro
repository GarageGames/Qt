SOURCES += tst_qlowenergyservice.cpp
TARGET = tst_qlowenergyservice
CONFIG += testcase

QT = core bluetooth testlib
blackberry {
    LIBS += -lbtapi
}

