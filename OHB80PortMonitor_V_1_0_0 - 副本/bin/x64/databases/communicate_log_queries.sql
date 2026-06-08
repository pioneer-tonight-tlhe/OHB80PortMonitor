-- ============================================
-- 通讯日志查询语句
-- ============================================

-- ============================================
-- 1. 分页条件查询
-- ============================================
-- 1.1.条件：
-- 1.1.1.command_id
-- 1.1.2.qr_code
-- 1.1.3.exec_status
-- 1.1.4.retry_count
-- 1.1.5.send_time（时间区间）

-- 1.1.查询某一页中所有满足条件的记录（先按条件过滤，再按 send_time 排序并分页）
-- ID=query_page_with_conditions
-- 参数（共13个，按?顺序绑定）：
--   command_id, command_id,
--   qr_code, qr_code,
--   exec_status, exec_status,
--   retry_count, retry_count,
--   start_time, start_time, end_time,
--   page_size, page_offset
-- 说明：每个条件参数需绑定两次（一次用于NULL检查，一次用于值比较）；
--      如果某个条件不需要应用，对应位置传NULL即可。
--      {ORDER} 由调用方替换为 ASC 或 DESC，以控制按 send_time 的排序方向。

SELECT * FROM communicate_log
WHERE (? IS NULL OR command_id = ?)
  AND (? IS NULL OR qr_code = ?)
  AND (? IS NULL OR exec_status = ?)
  AND (? IS NULL OR retry_count = ?)
  AND (? IS NULL OR send_time BETWEEN ? AND ?)
ORDER BY send_time {ORDER}
LIMIT ? OFFSET ?;

-- ============================================
-- 2. 总记录数（无条件）
-- ============================================
-- ID=query_total_count
-- 直接读取计数缓存表，避免对大表 COUNT(*)
SELECT total_count FROM log_record_count WHERE table_name = 'communicate_log';

-- ============================================
-- 3. 分页条件查询的总记录数
-- ============================================
-- ID=query_total_count_with_conditions
-- 参数（共11个，按?顺序绑定）：
--   command_id, command_id,
--   qr_code, qr_code,
--   exec_status, exec_status,
--   retry_count, retry_count,
--   start_time, start_time, end_time

SELECT COUNT(*) AS total_count FROM communicate_log
WHERE (? IS NULL OR command_id = ?)
  AND (? IS NULL OR qr_code = ?)
  AND (? IS NULL OR exec_status = ?)
  AND (? IS NULL OR retry_count = ?)
  AND (? IS NULL OR send_time BETWEEN ? AND ?);

-- ============================================
-- 3. 插入一条语句
-- ============================================
-- ID=insert_record
-- 参数：send_time, response_time, command_id, qr_code, exec_status, retry_count, send_frame, response_frame, description

INSERT INTO communicate_log (send_time, response_time, command_id, qr_code, exec_status, retry_count, send_frame, response_frame, description, user_permission)
VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);

-- ============================================
-- 4. 批量删除某个时间区间的记录
-- ============================================
-- ID=delete_by_time_range
-- 参数：start_time, end_time

DELETE FROM communicate_log
WHERE send_time BETWEEN ? AND ?;

-- ============================================
-- 5. 查询数据库拥有几个月的日志
-- ============================================
-- ID=query_month_range
-- 查询最早和最晚时间，手动计算月份差

SELECT MIN(send_time) AS earliest_time,
    MAX(send_time) AS latest_time,
    date(MIN(send_time)) AS earliest_date
FROM communicate_log;
