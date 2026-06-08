-- ============================================
-- VEFC 传感器监控查询语句
-- ============================================

-- ============================================
-- 1. 插入一条数据
-- ============================================
-- ID=insert_record
-- 参数：
--   qr_code,
--   record_timestamp,
--   gas_pressure,
--   actual_flow,
--   sensor_pressure,
--   sensor_temperature
INSERT INTO vefc_sensor_monitor (
    qr_code,
    record_timestamp,
    gas_pressure,
    actual_flow,
    sensor_pressure,
    sensor_temperature
)
VALUES (?, ?, ?, ?, ?, ?);

-- ============================================
-- 2. 删除某个时间区间的数据
-- ============================================
-- ID=delete_by_time_range
-- 参数：
--   start_timestamp,
--   end_timestamp
-- 说明：
--   使用 [start_timestamp, end_timestamp) 的毫秒时间戳区间。
DELETE FROM vefc_sensor_monitor
WHERE record_timestamp >= ?
  AND record_timestamp < ?;

-- ============================================
-- 3. 计算某一天所有字段的平均值数据
-- ============================================
-- ID=query_daily_average
-- 参数：
--   day_start_timestamp,
--   next_day_start_timestamp
-- 说明：
--   使用 [当天 00:00:00.000, 次日 00:00:00.000) 的毫秒时间戳区间。
SELECT
    COUNT(*) AS sample_count,
    MIN(record_timestamp) AS first_timestamp,
    MAX(record_timestamp) AS last_timestamp,
    AVG(gas_pressure) AS avg_gas_pressure,
    AVG(actual_flow) AS avg_actual_flow,
    AVG(sensor_pressure) AS avg_sensor_pressure,
    AVG(sensor_temperature) AS avg_sensor_temperature
FROM vefc_sensor_monitor
WHERE record_timestamp >= ?
  AND record_timestamp < ?;

-- ============================================
-- 4. 查询时间最久的一个星期记录
-- ============================================
-- ID=query_oldest_week_records
-- 参数：无
-- 说明：
--   以数据库中最早的 record_timestamp 为起点，查询 [最早时间, 最早时间 + 7天) 的记录。
--   如果当前数据跨度不足 7 天，则不返回任何记录。
WITH bounds AS (
    SELECT
        MIN(record_timestamp) AS first_timestamp,
        MAX(record_timestamp) AS last_timestamp,
        MIN(record_timestamp) + 7 * 24 * 60 * 60 * 1000 AS week_end_timestamp
    FROM vefc_sensor_monitor
)
SELECT
    v.qr_code,
    v.record_timestamp,
    v.gas_pressure,
    v.actual_flow,
    v.sensor_pressure,
    v.sensor_temperature
FROM vefc_sensor_monitor AS v
CROSS JOIN bounds AS b
WHERE b.first_timestamp IS NOT NULL
  AND b.last_timestamp >= b.week_end_timestamp
  AND v.record_timestamp >= b.first_timestamp
  AND v.record_timestamp < b.week_end_timestamp
ORDER BY v.record_timestamp ASC, v.qr_code ASC;
