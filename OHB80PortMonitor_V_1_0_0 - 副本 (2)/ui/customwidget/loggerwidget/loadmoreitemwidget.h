#pragma once
#include <QWidget>
#include <QVBoxLayout>

class QProgressBar;
class QLabel;
class QPushButton;

// ====================================================================
// LoadMoreItemWidget —— 嵌入在 QListWidget 中的"加载更多"按钮行
// 状态机：Idle → Loading → Done
// ====================================================================
class LoadMoreItemWidget : public QWidget
{
    Q_OBJECT
public:
    enum class Position { Top, Bottom };
    explicit LoadMoreItemWidget(Position pos, QWidget *parent = nullptr);

    void setIdle();
    void setLoading();
    void setProgress(int percent);         // 更新进度条（0~100）
    void setDone(const QString &timeHint); // 显示首条/末条记录时间

    Position position() const { return m_pos; }

signals:
    void clicked();

private:
    Position    m_pos;
    QPushButton *m_btn       = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel       *m_label    = nullptr;
    QVBoxLayout  *m_layout   = nullptr;
};
