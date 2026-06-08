HEADERS += \
    $$PWD/quiwidget.h

SOURCES += \
    $$PWD/quiwidget.cpp

# overheadcranetrack
include ($$PWD/overheadcranetrack/overheadcranetrack.pri)
INCLUDEPATH += $$PWD/overheadcranetrack

# settingwidget
include ($$PWD/settingwidget/settingwidget.pri)
INCLUDEPATH += $$PWD/settingwidget

# debugsettingwidget
include ($$PWD/debugsettingwidget/debugsettingwidget.pri)
INCLUDEPATH += $$PWD/debugsettingwidget

# configsettingwidget
include ($$PWD/configsettingwidget/configsettingwidget.pri)
INCLUDEPATH += $$PWD/configsettingwidget

# loggerwidget
include ($$PWD/loggerwidget/loggerwidget.pri)
INCLUDEPATH += $$PWD/loggerwidget

# alarmloggerwidget [DEPRECATED] 警报生命周期现由 AlarmDispatchTask + AlarmLogDBCon 接管
# include ($$PWD/alarmloggerwidget/alarmloggerwidget.pri)
# INCLUDEPATH += $$PWD/alarmloggerwidget

# alarmlogwidget
include ($$PWD/alarmlogwidget/alarmlogwidget.pri)
INCLUDEPATH += $$PWD/alarmlogwidget

# historycalendardialog
include($$PWD/historycalendardialog/historycalendardialog.pri)
INCLUDEPATH += $$PWD/historycalendardialog

# communicateloggerwidget [DEPRECATED] 由 ComunicateLogWidget + CommunicateLogDBCon 取代
# include($$PWD/communicateloggerwidget/communicateloggerwidget.pri)
# INCLUDEPATH += $$PWD/communicateloggerwidget

# comunicatelogwidget
include($$PWD/comunicatelogwidget/comunicatelogwidget.pri)
INCLUDEPATH += $$PWD/comunicatelogwidget

# runningloggerwidget [DEPRECATED] 由 OperationDispatchTask + OperationLogDBCon 接管
# include($$PWD/runningloggerwidget/runningloggerwidget.pri)
# INCLUDEPATH += $$PWD/runningloggerwidget

# operationlogwidget
include($$PWD/operationlogwidget/operationlogwidget.pri)
INCLUDEPATH += $$PWD/operationlogwidget

# useraccountlabel
include($$PWD/useraccountlabel/useraccountlabel.pri)
INCLUDEPATH += $$PWD/useraccountlabel

#paginationwidget
include ($$PWD/paginationwidget/paginationwidget.pri)
INCLUDEPATH += $$PWD/paginationwidget

#waitdialog
include ($$PWD/waitdialog/waitdialog.pri)
INCLUDEPATH += $$PWD/waitdialog

#datetimesetdialog
include ($$PWD/datetimesetdialog/datetimesetdialog.pri)
INCLUDEPATH += $$PWD/datetimesetdialog

#modaltabledialog
include ($$PWD/modaltabledialog/modaltabledialog.pri)
INCLUDEPATH += $$PWD/modaltabledialog

#scrollingtiplabel
include ($$PWD/scrollingtiplabel/scrollingtiplabel.pri)
INCLUDEPATH += $$PWD/scrollingtiplabel

FORMS +=
