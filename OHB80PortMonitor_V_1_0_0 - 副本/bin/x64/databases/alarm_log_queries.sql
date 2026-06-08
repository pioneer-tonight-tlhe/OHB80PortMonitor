-- ============================================
-- 警报日志查询语句
-- ============================================

-- ============================================
-- 1. 分页条件查询
-- ============================================
-- 1.1.条件：
-- 1.1.1.alarm_level
-- 1.1.2.qr_code
-- 1.1.3.alarm_type
-- 1.1.4.is_resolved
-- 1.1.5.occur_time（时间区间）

-- 1.1.查询某一页中所有满足条件的记录（按 occur_time 降序，先过滤再分页）
-- ID=query_page_with_conditions
-- 参数（共13个，按?顺序绑定）：
--   alarm_level, alarm_level,
--   qr_code, qr_code,
--   alarm_type, alarm_type,
--   is_resolved, is_resolved,
--   start_time, start_time, end_time,
--   page_size, page_offset
-- 说明：每个条件参数需绑定两次（一次用于NULL检查，一次用于值比较）；
--      如果某个条件不需要应用，对应位置传NULL即可

SELECT * FROM alarm_log
WHERE (? IS NULL OR alarm_level = ?)
  AND (? IS NULL OR qr_code = ?)
  AND (? IS NULL OR alarm_type = ?)
  AND (? IS NULL OR is_resolved = ?)
  AND (? IS NULL OR occur_time BETWEEN ? AND ?)
ORDER BY occur_time DESC
LIMIT ? OFFSET ?;

-- ============================================
-- 2. 总记录数（无条件）
-- ============================================
-- ID=query_total_count
-- 直接读取计数缓存表，避免对大表 COUNT(*)

SELECT total_count FROM log_record_count WHERE table_name = 'alarm_log';

-- ============================================
-- 3. 分页条件查询的总记录数
-- ============================================
-- ID=query_total_count_with_conditions
-- 参数（共11个，按?顺序绑定）：
--   alarm_level, alarm_level,
--   qr_code, qr_code,
--   alarm_type, alarm_type,
--   is_resolved, is_resolved,
--   start_time, start_time, end_time

SELECT COUNT(*) AS total_count FROM alarm_log
WHERE (? IS NULL OR alarm_level = ?)
  AND (? IS NULL OR qr_code = ?)
  AND (? IS NULL OR alarm_type = ?)
  AND (? IS NULL OR is_resolved = ?)
  AND (? IS NULL OR occur_time BETWEEN ? AND ?);

-- ============================================
-- 3. 插入一条语句
-- ============================================
-- ID=insert_record
-- 参数：alarm_level, occur_time, qr_code, alarm_type, is_resolved, resolve_time, description, user_permission

INSERT INTO alarm_log (alarm_level, occur_time, qr_code, alarm_type, is_resolved, resolve_time, description, user_permission)
VALUES (?, ?, ?, ?, ?, ?, ?, ?);

-- ============================================
-- 4. 批量删除某个时间区间的记录
-- ============================================
-- ID=delete_by_time_range
-- 参数：start_time, end_time

DELETE FROM alarm_log
WHERE occur_time BETWEEN ? AND ?;

-- ============================================
-- 5. 查询数据库拥有几个月的日志
-- ============================================
-- ID=query_month_range
-- 查询最早和最晚时间，手动计算月份差

SELECT MIN(occur_time) AS earliest_time,
    MAX(occur_time) AS latest_time,
    date(MIN(occur_time)) AS earliest_date
FROM alarm_log;

-- ============================================
-- 6. 把指定 (qr_code, alarm_type) 下未解决的记录标记为已解决
-- ============================================
-- ID=update_resolve
-- 参数：resolve_time, qr_code, alarm_type
-- 说明：单设备单警报类型只允许一条未解决记录，按 qr_code + alarm_type 唯一定位
UPDATE alarm_log
SET is_resolved = 1, resolve_time = ?
WHERE qr_code = ? AND alarm_type = ? AND is_resolved = 0;

