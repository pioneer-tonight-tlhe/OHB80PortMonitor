loggermanange
include ($$PWD/loggermananger/loggermananger.pri)
INCLUDEPATH += $$PWD/loggermananger

# qthelper
include ($$PWD/qthelper/qthelper.pri)
INCLUDEPATH += $$PWD/qthelper

# binfilereader
include ($$PWD/binfilereader/binfilereader.pri)
INCLUDEPATH += $$PWD/binfilereader

# communicationrecorder - 通讯记录采集器（节流器），根据 Foup 状态对 Modbus 通讯指令上报频率进行节流（工作中 1s，空闲 3s），减轻 UI 日志写入压力
include ($$PWD/communicationrecorder/communicationrecorder.pri)
INCLUDEPATH += $$PWD/communicationrecorder

# usermanager 已迁移至 data/usermanager

# defer - RAII 模式，在作用域结束时自动执行回调函数
include ($$PWD/defer/defer.pri)
INCLUDEPATH += $$PWD/defer

# iocsv - CSV 文件读写工具
include ($$PWD/iocsv/iocsv.pri)
INCLUDEPATH += $$PWD/iocsv
