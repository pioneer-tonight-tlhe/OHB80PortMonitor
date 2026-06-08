#pragma once
#include <QDate>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMetaType>

// 历史查询参数
struct HistoryQuery {
    QDate   date;               // 必填：查询哪天的日志
    int     timeColumnIndex = -1; // "时间"列在表头中的索引（-1 = 不使用时间区间）
    QString timeFrom;           // 时间区间起点 HH:mm:ss（空 = 不限）
    QString timeTo;             // 时间区间终点 HH:mm:ss（空 = 不限）
    QString likePattern;        // LIKE 模糊匹配串（空 = 不使用）
    int     pageSize  = 50;     // 每页记录数
    int     pageIndex = 0;      // 请求第几页（0-based）
};

// 历史查询结果
struct HistoryResult {
    QVector<QStringList> records;      // 当前页的记录
    QVector<bool>        highlighted;  // 每条记录是否需要高亮（淡蓝色）
    QVector<int>         matchedGlobalIndices; // 所有匹配记录的全局索引（用于前一个/后一个导航）
    int currentPage  = 0;              // 当前页号
    int totalPages   = 0;              // 总页数
    int totalRecords = 0;              // 总记录数
};

Q_DECLARE_METATYPE(HistoryQuery)
Q_DECLARE_METATYPE(HistoryResult)
