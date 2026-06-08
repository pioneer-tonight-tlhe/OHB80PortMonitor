HEADERS += \
    $$PWD/modbustcpmastermanager.h \
    $$PWD/modbuslogger.h \
    $$PWD/modbustcpmasterpool.h

SOURCES += \
    $$PWD/modbustcpmastermanager.cpp \
    $$PWD/modbustcpmasterpool.cpp


#modbuscommand
include ($$PWD/modbuscommand/modbuscommand.pri)
INCLUDEPATH += $$PWD/modbuscommand

#modbustcpmaster
include ($$PWD/modbustcpmaster/modbustcpmaster.pri)
INCLUDEPATH += $$PWD/modbustcpmaster
