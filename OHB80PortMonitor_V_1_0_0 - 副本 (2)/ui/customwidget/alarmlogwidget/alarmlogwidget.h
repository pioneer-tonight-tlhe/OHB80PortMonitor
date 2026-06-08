#ifndef ALARMLOGWIDGET_H
#define ALARMLOGWIDGET_H

#include <QWidget>
#include "alarmrecord.h"

namespace Ui {
class AlarmLogWidget;
}

class AlarmLogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AlarmLogWidget(QWidget *parent = nullptr);
    ~AlarmLogWidget();

    // 初始化 UI 控件的可选项（下拉框选项 / 数值输入框范围 等）。
    // 由 MainWindow 在执行查询测试前调用，使界面具备可用的查询条件输入。
    void initUi();

private slots:
    void onCheckBoxAllStateChanged(int state);
    void onSearchClicked();
    void onSetStartTimeClicked();
    void onSetResolvedTimeClicked();
    void onPaginationPageChanged(int page);
    void onPageWithConditionsResult(const QList<AlarmRecord>& records);
    void onTotalCountWithConditionsResult(int totalCount);
    void onRecordInserted(const AlarmRecord& record);
    void onRecordResolved(const QString& qrCode,
                          const QString& alarmType,
                          const QString& resolveTime);
    void onLiveLogClicked(const QModelIndex& index);
    void onHistoryLogClicked(const QModelIndex& index);

private:
    Ui::AlarmLogWidget *ui;

    // 分页状态
    int m_currentPage;
    int m_pageSize;
    int m_totalPages;

    // 缓存最近一次查询条件，用于翻页时复用
    int     m_lastAlarmLevel;       // -1 表示未启用
    QString m_lastQRCode;
    QString m_lastAlarmType;        // 空表示未启用，存储为 alarm_type 整数对应的字符串
    int     m_lastIsResolved;       // -1 表示未启用
    QString m_lastStartTime;
    QString m_lastEndTime;

    void submitQuery(int page);
    void setHistoryLogData(const QList<AlarmRecord>& data);

    // 初始化 live log 表（建立 model + 表头 + 订阅 DBCon::recordInserted）
    void initLiveLog();

    // 启动时从 DB 加载 is_resolved=0 的警报到 live log，
    // 让本次启动能看到上一次未解决的警报。
    void loadUnresolvedToLiveLog();

    // live log 行数上限（超过后清除所有已解决记录）
    static constexpr int kLiveLogMaxRows = 100;
};

#endif // ALARMLOGWIDGET_H
