-- 运行日志表

CREATE TABLE IF NOT EXISTS operation_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    occur_time TEXT NOT NULL,
    log_type INTEGER NOT NULL CHECK(log_type IN (0, 1, 2)),
    description TEXT NOT NULL,
    user_permission INTEGER NOT NULL DEFAULT 0
);

-- 索引
-- 单列索引：用于简单查询
CREATE INDEX IF NOT EXISTS idx_operation_log_occur_time ON operation_log(occur_time);
CREATE INDEX IF NOT EXISTS idx_operation_log_log_type ON operation_log(log_type);

-- 复合索引：用于有条件查询 (log_type + occur_time DESC)，覆盖最常用的查询模式
CREATE INDEX IF NOT EXISTS idx_operation_log_type_time ON operation_log(log_type, occur_time DESC);

-- 复合索引：用于时间区间查询 (occur_time + log_type)，优化 BETWEEN 查询
CREATE INDEX IF NOT EXISTS idx_operation_log_time_type ON operation_log(occur_time, log_type);

-- ============================================
-- 通讯日志表
-- ============================================

CREATE TABLE IF NOT EXISTS communicate_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    send_time TEXT NOT NULL,
    response_time TEXT,
    command_id TEXT NOT NULL,
    qr_code TEXT NOT NULL,
    exec_status INTEGER NOT NULL CHECK(exec_status IN (0, 1, 2, 3)),
    retry_count INTEGER NOT NULL DEFAULT 0,
    send_frame BLOB NOT NULL,
    response_frame BLOB,
    description TEXT NOT NULL,
    user_permission INTEGER NOT NULL DEFAULT 0
);

-- 通讯日志索引
-- 单列索引
CREATE INDEX IF NOT EXISTS idx_communicate_log_send_time ON communicate_log(send_time);
CREATE INDEX IF NOT EXISTS idx_communicate_log_command_id ON communicate_log(command_id);
CREATE INDEX IF NOT EXISTS idx_communicate_log_qr_code ON communicate_log(qr_code);
CREATE INDEX IF NOT EXISTS idx_communicate_log_exec_status ON communicate_log(exec_status);

-- 复合索引：按设备 + 时间查询
CREATE INDEX IF NOT EXISTS idx_communicate_log_qr_time ON communicate_log(qr_code, send_time DESC);

-- 复合索引：按状态 + 时间查询
CREATE INDEX IF NOT EXISTS idx_communicate_log_status_time ON communicate_log(exec_status, send_time DESC);

-- 复合索引：时间区间 + 状态查询
CREATE INDEX IF NOT EXISTS idx_communicate_log_time_status ON communicate_log(send_time, exec_status);


-- ============================================
-- 警报日志表
-- ============================================

CREATE TABLE IF NOT EXISTS alarm_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    alarm_level INTEGER NOT NULL,
    occur_time TEXT NOT NULL,
    qr_code TEXT NOT NULL,
    alarm_type TEXT NOT NULL,
    is_resolved INTEGER NOT NULL DEFAULT 2 CHECK(is_resolved IN (0, 1, 2)),
    resolve_time TEXT,
    description TEXT NOT NULL,
    user_permission INTEGER NOT NULL DEFAULT 0
);

-- 警报日志索引
-- 单列索引
CREATE INDEX IF NOT EXISTS idx_alarm_log_alarm_level ON alarm_log(alarm_level);
CREATE INDEX IF NOT EXISTS idx_alarm_log_occur_time ON alarm_log(occur_time);
CREATE INDEX IF NOT EXISTS idx_alarm_log_qr_code ON alarm_log(qr_code);
CREATE INDEX IF NOT EXISTS idx_alarm_log_alarm_type ON alarm_log(alarm_type);
CREATE INDEX IF NOT EXISTS idx_alarm_log_is_resolved ON alarm_log(is_resolved);
CREATE INDEX IF NOT EXISTS idx_alarm_log_resolve_time ON alarm_log(resolve_time);

-- 复合索引：按设备 + 发生时间查询
CREATE INDEX IF NOT EXISTS idx_alarm_log_qr_occur_time ON alarm_log(qr_code, occur_time DESC);

-- 复合索引：按警报级别 + 发生时间查询
CREATE INDEX IF NOT EXISTS idx_alarm_log_level_occur_time ON alarm_log(alarm_level, occur_time DESC);

-- 复合索引：按警报类型 + 发生时间查询
CREATE INDEX IF NOT EXISTS idx_alarm_log_type_occur_time ON alarm_log(alarm_type, occur_time DESC);

-- 复合索引：按是否解决 + 发生时间查询
CREATE INDEX IF NOT EXISTS idx_alarm_log_resolved_occur_time ON alarm_log(is_resolved, occur_time DESC);

-- 复合索引：发生时间区间 + 是否解决
CREATE INDEX IF NOT EXISTS idx_alarm_log_occur_time_resolved ON alarm_log(occur_time, is_resolved);


-- ============================================
-- 设备参数日志表
-- ============================================
CREATE TABLE IF NOT EXISTS device_param_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    qr_code TEXT NOT NULL,                                                  -- QRCode（设备标识）
    record_time TEXT NOT NULL,                                              -- 记录时间
    inlet_pressure REAL NOT NULL,                                           -- 进气压力
    outlet_pressure REAL NOT NULL,                                          -- 出气压力
    inlet_flow REAL NOT NULL,                                               -- 进气流量
    humidity REAL NOT NULL,                                                 -- 相对湿度
    temperature REAL NOT NULL,                                              -- 温度
    foup_status INTEGER NOT NULL CHECK(foup_status IN (0, 1))               -- 0:foup out 1:foup in
);

-- 设备参数日志索引
-- 单列索引
CREATE INDEX IF NOT EXISTS idx_device_param_log_qr_code ON device_param_log(qr_code);
CREATE INDEX IF NOT EXISTS idx_device_param_log_record_time ON device_param_log(record_time);

-- 复合索引：按设备 + 记录时间查询（最常用模式）
CREATE INDEX IF NOT EXISTS idx_device_param_log_qr_record_time ON device_param_log(qr_code, record_time DESC);

-- 复合索引：记录时间区间 + 设备
CREATE INDEX IF NOT EXISTS idx_device_param_log_record_time_qr ON device_param_log(record_time, qr_code);


-- ============================================
-- 日志表总记录数统计表
-- 每次插入/删除日志记录时，与具体的日志插入/删除在同一事务中更新 total_count
-- ============================================
CREATE TABLE IF NOT EXISTS log_record_count (
    table_name  TEXT PRIMARY KEY,
    total_count INTEGER NOT NULL DEFAULT 0
);

-- 初始化/修正各日志表的当前计数（启动时执行，保证 total_count 与真实行数一致）
INSERT OR REPLACE INTO log_record_count(table_name, total_count)
VALUES ('operation_log',    (SELECT COUNT(*) FROM operation_log));

INSERT OR REPLACE INTO log_record_count(table_name, total_count)
VALUES ('communicate_log',  (SELECT COUNT(*) FROM communicate_log));

INSERT OR REPLACE INTO log_record_count(table_name, total_count)
VALUES ('alarm_log',        (SELECT COUNT(*) FROM alarm_log));

INSERT OR REPLACE INTO log_record_count(table_name, total_count)
VALUES ('device_param_log', (SELECT COUNT(*) FROM device_param_log));

-- ============================================
-- 用户权限字段迁移（老库升级用，新库会因列已存在而失败）
-- 默认值 0 对应 UserPermission::Guest
-- ============================================
ALTER TABLE operation_log    ADD COLUMN user_permission INTEGER NOT NULL DEFAULT 0;
ALTER TABLE communicate_log  ADD COLUMN user_permission INTEGER NOT NULL DEFAULT 0;
ALTER TABLE alarm_log        ADD COLUMN user_permission INTEGER NOT NULL DEFAULT 0;
ALTER TABLE device_param_log ADD COLUMN user_permission INTEGER NOT NULL DEFAULT 0;

-- ============================================
-- 移除 alarm_log.customer_visible 字段（老库降级用，新库会因列不存在而失败）
-- ============================================
ALTER TABLE alarm_log DROP COLUMN customer_visible;
