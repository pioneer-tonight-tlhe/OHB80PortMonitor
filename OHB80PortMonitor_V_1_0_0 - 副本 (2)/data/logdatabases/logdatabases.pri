HEADERS += \
    $$PWD/databasemanager.h \
    $$PWD/dbconnectionhelper.h \
    $$PWD/dbtypes.h \
    $$PWD/logcleanupscheduler.h \
    $$PWD/sqlmapper.h

SOURCES += \
    $$PWD/databasemanager.cpp \
    $$PWD/dbconnectionhelper.cpp \
    $$PWD/logcleanupscheduler.cpp \
    $$PWD/sqlmapper.cpp

#alarmlogdb
include ($$PWD/alarmlogdb/alarmlogdb.pri)
INCLUDEPATH += $$PWD/alarmlogdb

#communicatelogdb
include ($$PWD/communicatelogdb/communicatelogdb.pri)
INCLUDEPATH += $$PWD/communicatelogdb

#deviceparamlogdb
include ($$PWD/deviceparamlogdb/deviceparamlogdb.pri)
INCLUDEPATH += $$PWD/deviceparamlogdb

#vefcsensormonitordb
include ($$PWD/vefcsensormonitordb/vefcsensormonitordb.pri)
INCLUDEPATH += $$PWD/vefcsensormonitordb

#operationlogdb
include ($$PWD/operationlogdb/operationlogdb.pri)
INCLUDEPATH += $$PWD/operationlogdb

#writesqldb
include ($$PWD/writesqldb/writesqldb.pri)
INCLUDEPATH += $$PWD/writesqldb
