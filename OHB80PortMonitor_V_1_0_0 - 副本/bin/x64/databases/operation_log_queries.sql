-- ============================================
-- 运行日志表查询语句集合
-- ============================================

-- ============================================
-- 1. 分页查询（无范围）
-- ============================================
-- ID=query_pagination
-- 参数：page_size (每页记录数), page_offset (偏移量)
SELECT * FROM operation_log
ORDER BY occur_time DESC
LIMIT ? OFFSET ?;

-- ============================================
-- 1.2 分页查询（在时间范围内）
-- ============================================
-- ID=query_pagination_in_range
-- 参数：start_time, start_time, end_time, page_size, page_offset
-- start_time 为 NULL 时不限定范围（等价于全表分页）
SELECT * FROM operation_log
WHERE (? IS NULL OR occur_time BETWEEN ? AND ?)
ORDER BY occur_time DESC
LIMIT ? OFFSET ?;

-- ============================================
-- 2. 计算总记录数
-- ============================================
-- ID=query_total_count
-- 直接读取计数缓存表，避免对大表 COUNT(*)
SELECT total_count FROM log_record_count WHERE table_name = 'operation_log';

-- ============================================
-- 2.2 时间范围内的总记录数
-- ============================================
-- ID=query_total_count_in_range
-- 参数：start_time, start_time, end_time
SELECT COUNT(*) AS total_count FROM operation_log
WHERE (? IS NULL OR occur_time BETWEEN ? AND ?);

-- ============================================
-- 2.3 某条记录在时间范围内的页号
-- ============================================
-- ID=query_record_page_in_range
-- 参数：page_size, page_size,
--      start_time, start_time, end_time,
--      record_id, record_id, record_id
-- 说明：occur_time DESC 排序下，该记录的位置（1-based）= 范围内
-- "occur_time 更晚的记录数 + 同 occur_time 但 id >= 自己的记录数"。
SELECT (position + ? - 1) / ? AS page_number
FROM (
    SELECT COUNT(*) AS position FROM operation_log
    WHERE (? IS NULL OR occur_time BETWEEN ? AND ?)
      AND (
          occur_time > (SELECT occur_time FROM operation_log WHERE id = ?)
          OR (occur_time = (SELECT occur_time FROM operation_log WHERE id = ?)
              AND id >= ?)
      )
) AS sub;

-- ============================================
-- 3. 有条件模糊查询
-- ============================================
-- 3.1.条件：
-- 3.1.1.时间区间
-- 3.1.2.日志类型
-- 3.1.3.描述信息模糊查询

-- 3.3.1.查询第一条语句在哪一页（已废弃，保留兼容；新流程使用
-- query_first_matched_id + query_record_page_in_range 两步法。原公式按 id
-- 等距分布近似页号，在范围 + 删除/插入扰动下与 occur_time 排序结果不一致。）
-- ID=query_first_record_page
-- 参数：page_size, page_size, start_time, start_time, end_time, log_type, log_type, keyword, keyword
SELECT
    (first_id + ? - 1) / ? AS page_number
FROM (
    SELECT id AS first_id FROM operation_log
    WHERE (? IS NULL OR occur_time BETWEEN ? AND ?)
      AND (? IS NULL OR log_type = ?)
      AND (? IS NULL OR description LIKE ?)
    ORDER BY occur_time DESC
    LIMIT 1
) subquery;

-- 3.3.1b.查询第一条满足条件的记录 id（在范围 + 条件下）
-- ID=query_first_matched_id
-- 参数：start_time, start_time, end_time, log_type, log_type, keyword, keyword
SELECT id FROM operation_log
WHERE (? IS NULL OR occur_time BETWEEN ? AND ?)
  AND (? IS NULL OR log_type = ?)
  AND (? IS NULL OR description LIKE ?)
ORDER BY occur_time DESC
LIMIT 1;

-- 3.3.2.查询某一页中所有满足条件的记录
-- ID=query_page_with_conditions
-- 参数：page_size, page_offset, start_time, end_time, log_type, keyword
SELECT * FROM (
    SELECT * FROM operation_log
    ORDER BY occur_time DESC
    LIMIT ? OFFSET ?
) AS page_data
WHERE (? IS NULL OR occur_time BETWEEN ? AND ?)
  AND (? IS NULL OR log_type = ?)
  AND (? IS NULL OR description LIKE ?);

-- 3.3.3.在整个数据库中查询有条件模糊的总记录数
-- ID=query_total_count_with_conditions
-- 参数：start_time, end_time, log_type, keyword

SELECT COUNT(*) AS total_count FROM operation_log
WHERE (? IS NULL OR occur_time BETWEEN ? AND ?)
  AND (? IS NULL OR log_type = ?)
  AND (? IS NULL OR description LIKE ?)
  ORDER BY occur_time DESC;

-- 3.3.4.查询某条记录在条件结果中的位置（第几条）
-- ID=query_record_position
-- 参数：record_id, start_time, end_time, log_type, keyword
SELECT
    CASE
        WHEN (SELECT COUNT(*) FROM operation_log
              WHERE (? IS NULL OR occur_time BETWEEN ? AND ?)
                AND (? IS NULL OR log_type = ?)
                AND (? IS NULL OR description LIKE ?)
                AND id = ?) > 0
        THEN (SELECT COUNT(*) + 1 FROM operation_log
              WHERE (? IS NULL OR occur_time BETWEEN ? AND ?)
                AND (? IS NULL OR log_type = ?)
                AND (? IS NULL OR description LIKE ?)
                AND occur_time > (SELECT occur_time FROM operation_log WHERE id = ?))
        ELSE 0
    END AS position;

-- 3.3.5.查询上一条满足条件的记录 ID（按时间降序，即时间更早的记录）
-- ID=query_prev_matching_id
-- 参数：anchor_id, start_time, end_time, log_type, keyword
SELECT id AS matched_id FROM operation_log
WHERE occur_time < (SELECT occur_time FROM operation_log WHERE id = ?)
  AND (? IS NULL OR occur_time BETWEEN ? AND ?)
  AND (? IS NULL OR log_type = ?)
  AND (? IS NULL OR description LIKE ?)
ORDER BY occur_time DESC
LIMIT 1;

-- 3.3.6.查询下一条满足条件的记录 ID（按时间降序，即时间更晚的记录）
-- ID=query_next_matching_id
-- 参数：anchor_id, start_time, end_time, log_type, keyword
SELECT id AS matched_id FROM operation_log
WHERE occur_time > (SELECT occur_time FROM operation_log WHERE id = ?)
  AND (? IS NULL OR occur_time BETWEEN ? AND ?)
  AND (? IS NULL OR log_type = ?)
  AND (? IS NULL OR description LIKE ?)
ORDER BY occur_time DESC
LIMIT 1;

-- ============================================
-- 4. 插入一条语句
-- ============================================
-- ID=insert_record
-- 插入一条运行日志记录
-- 参数：occur_time, log_type, description
INSERT INTO operation_log (occur_time, log_type, description, user_permission)
VALUES (?, ?, ?, ?);

-- 示例：
-- INSERT INTO operation_log (occur_time, log_type, description)
-- VALUES ('2026-05-02 20:07:00', 1, '系统启动成功');

-- ============================================
-- 5. 批量删除某个时间区间的记录
-- ============================================
-- ID=delete_by_time_range
-- 参数：start_time, end_time
DELETE FROM operation_log
WHERE occur_time BETWEEN ? AND ?;

-- 示例：
-- DELETE FROM operation_log
-- WHERE occur_time BETWEEN '2026-01-01 00:00:00' AND '2026-01-31 23:59:59';

-- ============================================
-- 6. 查询数据库拥有几个月的日志
-- ============================================
-- ID=query_month_range
-- 查询最早和最晚时间，手动计算月份差

SELECT MIN(occur_time) AS earliest_time,
    MAX(occur_time) AS latest_time,
    date(MIN(occur_time)) AS earliest_date
FROM operation_log;

-- ============================================
-- 7. 删除指定年份的日志
-- ============================================
-- ID=delete_by_year
-- 参数：start_date (开始日期), end_date (结束日期)
DELETE FROM operation_log
WHERE occur_time BETWEEN ? AND ?;
