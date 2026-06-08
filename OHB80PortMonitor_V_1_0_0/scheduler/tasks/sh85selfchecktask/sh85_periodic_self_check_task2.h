#ifndef SH85_PERIODIC_SELF_CHECK_TASK2_H
#define SH85_PERIODIC_SELF_CHECK_TASK2_H

#include "scheduler_task.h"
#include "modbustcpmastermanager/modbustcpmaster/sh85selfchecker.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

// ====================================================================
// SH85PeriodicSelfCheckTask2 - SH85 周期自检调度任务（第二版）
//
// 设计目标：
//   1. 作为常驻调度任务，按周期筛选可执行 SH85 自检的设备。
//   2. 每轮自检并行启动各设备的 SH85SelfChecker，并统一转发 UI 所需信号。
//   3. 日志复用 SH85SelfCheckLogHelper：设备过程先聚合到内存 record，
//      设备结束时写一条完整明细，整轮结束时只写汇总/失败告警/分隔线。
// ====================================================================
class SH85PeriodicSelfCheckTask2 : public SchedulerTask
{
    Q_OBJECT

public:
    // ==================================== 构造函数 ====================================
    explicit SH85PeriodicSelfCheckTask2(QObject *parent = nullptr);
    ~SH85PeriodicSelfCheckTask2() override;


    // ============================ 基类SchedulerTask相关接口 ============================
    void start() override;
    void stop() override;

    QString taskType() const override { return QStringLiteral("SH85PeriodicSelfCheckTask2"); }
    bool isPersistent() const override { return true; }

    // 当前调度任务状态，供 UI 或调试输出展示。
    QString currentState() const;


    // ==================================== 自检开启功能 ====================================
    // 启用/停用周期自检；停用后不再触发新轮次
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }


    // ==================================== 设置自检周期 ====================================
    // 设置周期，unit 支持 s / min / hour
    void setPeriod(int value, const QString& unit);
    int periodSeconds() const { return m_periodSec; }


    // ================================== 单个设备自检功能 ==================================
    // 单设备模式：非空时只执行指定二维码设备；空字符串表示恢复全量模式。
    void setSingleDevice(const QString& qrcode);


    // ==================================== 筛选自检的设备 ====================================
    QStringList filterAvailableDevices();


private:
    // ==================================== 周期与轮次控制 ====================================
    void initPeriodTimer();
    void startAvailableDeviceChecks();
    void finishDevice(const QString& qrcode,
                      bool success,
                      SH85SelfChecker::Result result,
                      const QString& description);
    void tryFinishRound();


    // ---- checker 信号连接与异常收口 ----
    void disconnectAllCheckers();
    void appendNotSubmittedAndFinish(const QString& qrcode,
                                     SH85SelfChecker::Result result,
                                     const QString& reason);
    static QString currentTimestamp();

signals:
    // ---- 转发单设备自检信号，供 UI 显示倒计时/状态/单设备结果 ----
    void countdownTick(int remainingSeconds, const QString& masterId);
    void selfCheckerStateChanged(SH85SelfChecker::State state, const QString& masterId);
    void oneFinished(const QString& masterId, bool success, const QString& message);

    // ---- 转发自检过程中的命令/错误信号，供外层需要时订阅 ----
    void errorOccurred(SH85SelfChecker::Result result, const QString& message, const QString& masterId);
    void commandCompleted(ModbusCommand cmd, const QString& masterId);
    void commandRetrying(ModbusCommand cmd, const QString& masterId);

    // ---- 一轮自检全部结束后的轻量汇总信号 ----
    void allDevicesFinished(int totalCount, int successCount, int failureCount);

private slots:
    // ---- 周期与轮次控制 ----
    void onPeriodTimeout();

    // ---- 转发单设备自检信号，供 UI 显示倒计时/状态/单设备结果 ----
    void onCheckerCountdownTick(int remainingSeconds, const QString& masterId);
    void onCheckerStateChanged(SH85SelfChecker::State state, const QString& masterId);
    void onCheckerFinished(bool success,
                           SH85SelfChecker::Result result,
                           const QString& message,
                           const QString& masterId);


    // ---- 转发自检过程中的命令/错误信号，供外层需要时订阅 ----
    void onCheckerCommandCompleted(ModbusCommand cmd, const QString& masterId);
    void onCheckerCommandRetrying(ModbusCommand cmd, const QString& masterId);
    void onCheckerErrorOccurred(SH85SelfChecker::Result result,
                                const QString& message,
                                const QString& masterId);

private:
    // 单设备在本轮自检中的统计结果；详细过程由 m_deviceRecords 保存。
    struct DeviceResult {
        QString qrcode;
        bool participated = false;
        bool success = false;
        QString description;
    };

    // ---- 自检开启功能 ----
    bool m_enabled = true;

    // ---- 设置自检周期 ----
    // 默认半小时
    int m_periodSec = 1800;


    // ---- 单个设备自检功能 ----
    bool m_singleDeviceMode = false;
    QString m_singleDeviceQrcode;
    QStringList m_availableDevices;


    // ---- 周期与轮次控制 ----
    QTimer* m_periodTimer = nullptr;
    int m_periodRemainingCalls = 0;


    // ---- 一轮自检上下文 ----

    // 本轮自检的唯一标识，用于日志和调试时区分不同轮次。
    QString m_roundId;

    // 本轮自检开始时间，用于统计耗时或写入轮次汇总。
    QString m_roundStartTime;

    // 是否存在有效轮次上下文；为 false 时忽略迟到的 checker 信号。
    bool m_roundActive = false;

    // 本轮目标设备列表，保持固定顺序，保证汇总输出顺序稳定。
    QStringList m_roundOrderedQrcodes;

    // 本轮仍在等待完成的设备；设备结束后移除，清空即本轮结束。
    QSet<QString> m_pendingQrcodes;

    // 本轮每台设备的参与状态、成功状态和结果描述。
    QHash<QString, DeviceResult> m_roundResults;


    // ---- checker 当前阶段与连接句柄 ----
    QHash<QString, SH85SelfChecker::State> m_checkerStates;
    QList<QMetaObject::Connection> m_checkerConnections;
};

#endif // SH85_PERIODIC_SELF_CHECK_TASK2_H
