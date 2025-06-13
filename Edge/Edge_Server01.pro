QT = core network

CONFIG += c++17 cmdline

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp \
        modelaggregator.cpp \
        myedgeclient.cpp \
        myedgeserver.cpp \
        myedgestatclient.cpp \
        startmission.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    modelaggregator.h \
    myedgeclient.h \
    myedgeserver.h \
    myedgestatclient.h \
    startmission.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/lib/release/ -lQt6Mqtt
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/lib/debug/ -lQt6Mqtt
else:unix: LIBS += -L$$PWD/lib/ -lQt6Mqtt

INCLUDEPATH += $$PWD/include
DEPENDPATH += $$PWD/include


