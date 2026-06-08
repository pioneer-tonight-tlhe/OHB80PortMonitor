#ifndef COMUNICATELOGWIDGET_H
#define COMUNICATELOGWIDGET_H

#include <QWidget>
#include <QHash>
#include <QVector>
#include <QStringList>
#include "communicaterecord.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

namespace Ui {
class ComunicateLogWidget;
}

class ComunicateLogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ComunicateLogWidget(QWidget *parent = nullptr);
    ~ComunicateLogWidget();

    // 初始化 UI 控件的可选项（下拉框选项 / 数值输入框范围 等）。
    // 由 MainWindow 在执行查询测试前调用，使界面具备可用的查询条件输入。
    void initUi();

    // 实时日志接口（类似老的 CommunicateLoggerWidget::writeLog）
    // 按 qrcode 更新对应行的内容
    void writeLog(const QString& qrcode, const QString& sendTime,
                  const QString& responseTime, const QString& commandId,
                  const QString& durationMs, const QString& execStatus,
                  const QString& retryCount, const QString& request,
                  const QString& response, const QString& description);

    // 初始化 qrcode 列表（固定行数表格）
    void initQrcodeList(const QStringList& qrcodes);

    // 根据用户权限设置列的可见性
    void updateColumnVisibility();

private slots:
    void onCheckBoxAllStateChanged(int state);
    void onSearchClicked();
    void onSetTimeClicked();
    void onPaginationPageChanged(int page);
    void onPageWithConditionsResult(const QList<CommunicateRecord>& records);
    void onTotalCountWithConditionsResult(int totalCount);
    void onLiveLogClicked(const QModelIndex& index);
    void onHistoryLogClicked(const QModelIndex& index);

private:
    Ui::ComunicateLogWidget *ui;
    int m_currentPage;
    int m_pageSize;
    int m_totalPages;
    bool m_lastQueryHadConditions;

    // 缓存最近一次查询条件，用于翻页时复用
    QString m_lastCommandId;
    QString m_lastQRCode;
    int     m_lastExecStatus;
    int     m_lastRetryCount;
    QString m_lastStartTime;
    QString m_lastEndTime;

    void submitQuery(int page);
    void setHistoryLogData(const QList<CommunicateRecord>& data);

    // 初始化 live log 表（建立 model + 表头）
    void initLiveLog();

    // 实时表格数据
    QVector<QStringList> m_liveRecords;    // 实时数据（每行一个 QStringList）
    QHash<QString, int>  m_qrcodeRow;      // qrcode → 行索引
    static const QStringList kLiveHeaders; // 列头
    static const int kDefaultRowCount = 80; // 默认行数
};

#endif // COMUNICATELOGWIDGET_H
