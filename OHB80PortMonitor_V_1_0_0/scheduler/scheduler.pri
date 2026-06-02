INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/scheduler_task.h \
    $$PWD/scheduler.h \
    $$PWD/tasks/alarmlogquerytask.h \
    $$PWD/tasks/communicatelogquerytask.h \
    $$PWD/tasks/monitor_data_task/monitor_data_task.h \
    $$PWD/tasks/monitor_data_task/monitor_data_task_logger.h \
    $$PWD/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_task.h \
    $$PWD/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_task_logger.h \
    $$PWD/tasks/operationlogquerytask.h \
    $$PWD/tasks/send_command_task.h \
    $$PWD/tasks/init_check_task.h \
    $$PWD/tasks/network_status_task/network_status_task.h \
    $$PWD/tasks/network_status_task/network_status_task_logger.h \
    $$PWD/tasks/network_status_task/network_status_task_qrcode_logger.h \
    $$PWD/tasks/set_firmware_config_task.h \
    $$PWD/tasks/firmware_upgrade_task.h \
    $$PWD/tasks/set_idle_purge_task.h \
    $$PWD/tasks/set_pneumatic_valve_pressure_task.h \
    $$PWD/tasks/sh85selfchecktask/sh85_self_check_task.h \
    $$PWD/tasks/sh85selfchecktask/sh85_periodic_self_check_task.h \
    $$PWD/tasks/sh85selfchecktask/sh85_self_check_log_helper.h \
    $$PWD/tasks/set_humidity_offset_task.h \
    $$PWD/tasks/set_purge_flow_task.h \
    $$PWD/tasks/set_vefc_gas_type_task.h \
    $$PWD/tasks/set_ui_refresh_time_task.h \
    $$PWD/tasks/read_vefc_flow_unit_medium_status_task.h \
    $$PWD/tasks/alarm_dispatch_task/alarm_dispatch_task.h \
    $$PWD/tasks/alarm_dispatch_task/alarm_dispatch_task_logger.h \
    $$PWD/tasks/operation_dispatch_task.h \
    $$PWD/tasks/user_management_task.h

SOURCES += \
    $$PWD/scheduler.cpp \
    $$PWD/tasks/alarmlogquerytask.cpp \
    $$PWD/tasks/communicatelogquerytask.cpp \
    $$PWD/tasks/monitor_data_task/monitor_data_task.cpp \
    $$PWD/tasks/monitor_data_task/monitor_data_task_logger.cpp \
    $$PWD/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_task.cpp \
    $$PWD/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_task_logger.cpp \
    $$PWD/tasks/operationlogquerytask.cpp \
    $$PWD/tasks/send_command_task.cpp \
    $$PWD/tasks/init_check_task.cpp \
    $$PWD/tasks/network_status_task/network_status_task.cpp \
    $$PWD/tasks/network_status_task/network_status_task_logger.cpp \
    $$PWD/tasks/network_status_task/network_status_task_qrcode_logger.cpp \
    $$PWD/tasks/set_firmware_config_task.cpp \
    $$PWD/tasks/firmware_upgrade_task.cpp \
    $$PWD/tasks/set_idle_purge_task.cpp \
    $$PWD/tasks/set_pneumatic_valve_pressure_task.cpp \
    $$PWD/tasks/sh85selfchecktask/sh85_self_check_task.cpp \
    $$PWD/tasks/sh85selfchecktask/sh85_periodic_self_check_task.cpp \
    $$PWD/tasks/sh85selfchecktask/sh85_self_check_log_helper.cpp \
    $$PWD/tasks/set_humidity_offset_task.cpp \
    $$PWD/tasks/set_purge_flow_task.cpp \
    $$PWD/tasks/set_vefc_gas_type_task.cpp \
    $$PWD/tasks/set_ui_refresh_time_task.cpp \
    $$PWD/tasks/read_vefc_flow_unit_medium_status_task.cpp \
    $$PWD/tasks/alarm_dispatch_task/alarm_dispatch_task.cpp \
    $$PWD/tasks/alarm_dispatch_task/alarm_dispatch_task_logger.cpp \
    $$PWD/tasks/operation_dispatch_task.cpp \
    $$PWD/tasks/user_management_task.cpp
