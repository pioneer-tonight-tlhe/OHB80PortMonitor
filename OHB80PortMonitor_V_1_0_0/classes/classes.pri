HEADERS += \
    $$PWD/alarminfo.h \
    $$PWD/alarmrecord.h \
    $$PWD/operationrecord.h \
    $$PWD/communicaterecord.h \
    $$PWD/deviceparamrecord.h \
    $$PWD/config/ohbdeviceconfiginfo.h \
    $$PWD/vefcsensormonitorrecord.h \
    $$PWD/tasks/record/setidlepurgetaskrecord.h \
    $$PWD/tasks/record/sh85selfchecktaskrecord.h \
    $$PWD/tasks/record/scheduler_task_record.h \
    $$PWD/bayofohbinfo.h \
    $$PWD/foupofohbinfo.h \
    $$PWD/setofohbinfo.h

SOURCES += \
    $$PWD/alarminfo.cpp \
    $$PWD/config/ohbdeviceconfiginfo.cpp \
    $$PWD/tasks/record/setidlepurgetaskrecord.cpp \
    $$PWD/tasks/record/sh85selfchecktaskrecord.cpp \
    $$PWD/tasks/record/scheduler_task_record.cpp \
    $$PWD/bayofohbinfo.cpp \
    $$PWD/foupofohbinfo.cpp \
    $$PWD/setofohbinfo.cpp

INCLUDEPATH += $$PWD/config
