-- ============================================
-- 设备参数日志查询语句
-- ============================================

-- ============================================
-- 1. 分页条件查询
-- ============================================
-- 1.1.条件：
-- 1.1.1.qr_code
-- 1.1.2.record_time（时间区间）

-- 1.1.查询某一页中所有满足条件的记录（先简单分页，再过滤）
-- ID=query_page_with_conditions
-- 参数（共7个，按?顺序绑定）：
--   page_size, page_offset,
--   qr_code, qr_code,
--   start_time, start_time, end_time
-- 说明：每个条件参数需绑定两次（一次用于NULL检查，一次用于值比较）；
--      如果某个条件不需要应用，对应位置传NULL即可
SELECT * FROM (
    SELECT * FROM device_param_log
    LIMIT ? OFFSET ?
) AS page_data
WHERE (? IS NULL OR qr_code = ?)
  AND (? IS NULL OR record_time BETWEEN ? AND ?);

-- ============================================
-- 2. 总记录数（无条件）
-- ============================================
-- ID=query_total_count
-- 直接读取计数缓存表，避免对大表 COUNT(*)
SELECT total_count FROM log_record_count WHERE table_name = 'device_param_log';

-- ============================================
-- 3. 分页条件查询的总记录数
-- ============================================
-- ID=query_total_count_with_conditions
-- 参数（共5个，按?顺序绑定）：
--   qr_code, qr_code,
--   start_time, start_time, end_time
SELECT COUNT(*) AS total_count FROM device_param_log
WHERE (? IS NULL OR qr_code = ?)
  AND (? IS NULL OR record_time BETWEEN ? AND ?);

-- ============================================
-- 3. 插入一条语句
-- ============================================
-- ID=insert_record
-- 参数：qr_code, record_time, inlet_pressure, outlet_pressure, inlet_flow, humidity, temperature, foup_status
INSERT INTO device_param_log (qr_code, record_time, inlet_pressure, outlet_pressure, inlet_flow, humidity, temperature, foup_status, user_permission)
VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);

-- ============================================
-- 4. 批量删除某个时间区间的记录
-- ============================================
-- ID=delete_by_time_range
-- 参数：start_time, end_time
DELETE FROM device_param_log
WHERE record_time BETWEEN ? AND ?;

-- ============================================
-- 5. 查询数据库拥有几个月的日志
-- ============================================
-- ID=query_month_range
-- 查询最早和最晚时间，手动计算月份差
SELECT MIN(record_time) AS earliest_time,
    MAX(record_time) AS latest_time,
    date(MIN(record_time)) AS earliest_date
FROM device_param_log;
