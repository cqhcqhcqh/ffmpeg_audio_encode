QT       += core gui \
    quick

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    encodethread.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    encodethread.h \
    mainwindow.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

mac: {
    FFMPEG_HOME = /usr/local/ffmpeg
    QMAKE_INFO_PLIST = /Mac/Info.plist
}

win32: {
    FFMPEG_HOME = C:/Workspaces/ffmpeg-4.3.2
}

INCLUDEPATH += $${FFMPEG_HOME}/include
LIBS += -L $${FFMPEG_HOME}/lib -lavdevice -lavformat -lavutil -lavcodec
