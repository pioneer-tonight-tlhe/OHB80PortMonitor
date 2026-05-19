#include "communicationrecorder.h"
#include "app/shareddata.h"
#include "classes/foupofohbinfo.h"

#include <QDebug>

CommunicationRecorder::CommunicationRecorder(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(TICK_INTERVAL_MS);
    connect(m_timer, &QTimer::timeout, this, &CommunicationRecorder::onTick);
}

CommunicationRecorder::~CommunicationRecorder()
{
    stop();
}

void CommunicationRecorder::start()
{
    if (!m_timer->isActive()) {
        m_timer->start();
    }
}

void CommunicationRecorder::stop()
{
    if (m_timer->isActive()) {
        m_timer->stop();
    }
}

void CommunicationRecorder::submitCommand(const ModbusCommand& cmd, const QString& masterId)
{
    if (masterId.isEmpty()) return;

    // 检查响应时间差值必须大于0
    if (cmd.responseMs - cmd.sentMs <= 0) return;

    // 仅对 THROTTLED_COMMAND_ID 指令进行节流，其它指令直接全量发射
    if (cmd.id != QLatin1String(THROTTLED_COMMAND_ID)) {
        emit shouldEmit(cmd, masterId);
        return;
    }

    // 仅存储最新指令；确保计数器存在
    m_latestCmd[masterId] = cmd;
    if (!m_counterMs.contains(masterId)) {
        m_counterMs[masterId] = 0;
    }
}

void CommunicationRecorder::onTick()
{
    // 遍历所有计数器，累加 TICK_INTERVAL_MS，判断是否达到阈值
    for (auto it = m_counterMs.begin(); it != m_counterMs.end(); ++it) {
        const QString& masterId = it.key();
        int& counter = it.value();
        counter += TICK_INTERVAL_MS;

        // 根据 Foup 在位状态决定阈值
        FoupOfOHBInfo* foup = SharedData::getFoupByQRCode(masterId);
        int threshold = (foup && foup->foupIn()) ? THRESHOLD_FOUP_IN_MS
                                                 : THRESHOLD_FOUP_OUT_MS;

        if (counter >= threshold) {
            // 达到阈值：发射信号并重置
            auto latestIt = m_latestCmd.find(masterId);
            if (latestIt != m_latestCmd.end()) {
                emit shouldEmit(latestIt.value(), masterId);
                // 发射后移除，避免重复发射（下一次有新指令到来才会再次记录）
                m_latestCmd.erase(latestIt);
            }
            counter = 0;
        }
    }
}
