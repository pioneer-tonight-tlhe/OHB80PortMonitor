QT       += core gui concurrent serialport serialbus printsupport network xml sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# 根据MinGW编译器类型自动检测架构并指定输出目录
contains(QMAKE_CXX,.*mingw.*64.*) {
    ARCH_DIR = x64
} else:contains(QMAKE_CXX,.*mingw.*) {
    ARCH_DIR = x32
} else {
    # 备用检测
    contains(QT_ARCH, x86_64) {
        ARCH_DIR = x64
    } else {
        ARCH_DIR = x32
    }
}
DESTDIR = $$PWD/bin/$$ARCH_DIR

# Windows 平台：设置应用图标
RC_ICONS = $$PWD/resource/main.ico


SOURCES += \
    main.cpp

HEADERS += \
    head.h \
    singleton.h

#app
include ($$PWD/app/app.pri)
INCLUDEPATH += $$PWD/app

#config
include ($$PWD/config/config.pri)
INCLUDEPATH += $$PWD/config

#tool
include ($$PWD/tool/tool.pri)
INCLUDEPATH += $$PWD/tool

#ui
include ($$PWD/ui/ui.pri)
INCLUDEPATH += $$PWD/ui

#scheduler
include ($$PWD/scheduler/scheduler.pri)
INCLUDEPATH += $$PWD/scheduler

#data
include ($$PWD/data/data.pri)
INCLUDEPATH += $$PWD/data

#classes，存放一些数据类对象
include ($$PWD/classes/classes.pri)
INCLUDEPATH += $$PWD/classes

#resources
RESOURCES += \
    resource/main.qrc \
    resource/qss.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
