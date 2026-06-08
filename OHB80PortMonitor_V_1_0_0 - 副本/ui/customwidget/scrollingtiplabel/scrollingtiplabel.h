#ifndef SCROLLINGTIPLABEL_H
#define SCROLLINGTIPLABEL_H

#include <QLabel>
#include <QHash>
#include <QMouseEvent>
#include <QQueue>
#include <QTimer>

#include "classes/operationrecord.h"
#include "classes/alarmrecord.h"

// ====================================================================
// ScrollingTipLabel — 滚动公告栏控件
//
//   功能概述：
//     - 实时显示操作日志和警报日志
//     - 警报日志优先级最高，优先显示警报
//     - 支持滚动显示消息
//     - 支持消除消息机制
//
//   管理对象：
//     1. m_alarmQueue: 警报队列，存放 AlarmLogDBCon 的主键 id
//     2. m_operationLog: 存放一条 OperationLogDBCon 记录（QStringList）
//
//   公开接口：
//     - submitAlarmLog(const AlarmRecord& record)
//       提交一个警报日志（使用 record.id 作为唯一标识）
//     - submitAlarmResolved(int alarmRecordId)
//       提交一个警报已解决日志
//     - submitOperationLog(const OperationRecord& record)
//       提交一个运行日志记录
//
//   私有方法：
//     - getCurrentDisplayLog()
//       获取当前待显示的日志
//
//   功能实现：
//     1. 滚动显示消息
//        - 字符串长度 > label 显示范围：滚动显示
//        - 字符串长度 <= label 显示范围：居中显示
//     2. 消除消息机制
//        - 警报已解决：从 alarm 队列中删除对应 id
//        - 提交 OperationLog：替换当前记录
//     3. 获取当前待显示日志
//        - alarm 队列不空：返回最后一个警报
//        - alarm 队列为空：返回 OperationLog 记录
// ====================================================================
class ScrollingTipLabel : public QLabel
{
    Q_OBJECT

public:
    explicit ScrollingTipLabel(QWidget *parent = nullptr);
    ~ScrollingTipLabel();

    // 提交一个警报日志（使用 (qrCode, alarmType) 复合 key 唯一标识）
    void submitAlarmLog(const AlarmRecord& record);

    // 提交一个警报已解决日志（按 (qrCode, alarmType) 移除）
    void submitAlarmResolved(const AlarmRecord& record);

    // 提交一个运行日志记录
    void submitOperationLog(const OperationRecord& record);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    // 获取当前待显示的文本
    QString getCurrentDisplayText();

    // 更新显示内容
    void updateDisplay();

    // 更新样式（根据是否显示警报）
    void updateStyle(bool isAlarm);

    // 滚动显示逻辑
    void startScrolling();
    void stopScrolling();
    void renderScrollFrame();

    // 格式化日志为显示字符串
    QString formatAlarmRecord(const AlarmRecord& record) const;
    QString formatOperationRecord(const OperationRecord& record) const;

private slots:
    void onScrollTimer();

private:
    // 管理对象
    // key 为 "qrCode|alarmType"，在 submit 与 resolve 时均能由 AlarmRecord 直接构造，
    // 避免依赖异步 DB 主键 id 导致提交/解决两端 key 对不上
    QQueue<QString>            m_alarmQueue;    // 警报队列，存放复合 key
    OperationRecord            m_operationLog;  // 当前运行日志记录
    QHash<QString, AlarmRecord> m_alarmLogs;    // key -> AlarmRecord 映射

    // 滚动相关
    QTimer          *m_scrollTimer = nullptr;
    int              m_scrollOffset = 0;
    QString          m_currentText;
    int              m_textWidth = 0;

    // 状态相关
    bool             m_isAlarmMode = false;  // 当前是否显示警报

    // 滚动配置
    static constexpr int kScrollIntervalMs = 100;  // 滚动间隔
    static constexpr int kScrollStep = 2;           // 每次滚动像素
};

#endif // SCROLLINGTIPLABEL_H
