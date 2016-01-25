SOURCES += osx/uistrings.cpp
PRIVATE_HEADERS += osx/uistrings_p.h

CONFIG(osx) {
    PRIVATE_HEADERS += osx/osxbtutility_p.h \
                       osx/osxbtdevicepair_p.h \
                       osx/osxbtdeviceinquiry_p.h \
                       osx/osxbtconnectionmonitor_p.h \
                       osx/osxbtsdpinquiry_p.h \
                       osx/osxbtrfcommchannel_p.h \
                       osx/osxbtl2capchannel_p.h \
                       osx/osxbtchanneldelegate_p.h \
                       osx/osxbtservicerecord_p.h \
                       osx/osxbtsocketlistener_p.h \
                       osx/osxbtobexsession_p.h \
                       osx/osxbtledeviceinquiry_p.h \
                       osx/corebluetoothwrapper_p.h \
                       osx/osxbtcentralmanager_p.h

    OBJECTIVE_SOURCES += osx/osxbtutility.mm \
                         osx/osxbtdevicepair.mm \
                         osx/osxbtdeviceinquiry.mm \
                         osx/osxbtconnectionmonitor.mm \
                         osx/osxbtsdpinquiry.mm \
                         osx/osxbtrfcommchannel.mm \
                         osx/osxbtl2capchannel.mm \
                         osx/osxbtchanneldelegate.mm \
                         osx/osxbtservicerecord.mm \
                         osx/osxbtsocketlistener.mm \
                         osx/osxbtobexsession.mm \
                         osx/osxbtledeviceinquiry.mm \
                         osx/osxbtcentralmanager.mm
} else {
    PRIVATE_HEADERS += osx/osxbtutility_p.h \
                       osx/osxbtledeviceinquiry_p.h \
                       osx/corebluetoothwrapper_p.h \
                       osx/osxbtcentralmanager_p.h

    OBJECTIVE_SOURCES += osx/osxbtutility.mm \
                         osx/osxbtledeviceinquiry.mm \
                         osx/osxbtcentralmanager.mm
}
