QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    core/vm_manager.cpp \
    core/vm_types.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    ../yakumo/kvm-manager/vm_types.h \
    core/vm_manager.h \
    core/vm_types.h \
    mainwindow.h

LIBS += -lvirt

INCLUDEPATH += $$PWD/core

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    core/.keep

