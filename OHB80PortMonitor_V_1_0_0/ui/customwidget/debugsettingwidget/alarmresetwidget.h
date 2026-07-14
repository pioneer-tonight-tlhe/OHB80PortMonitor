/*******************************************************************************************
 * @file alarmresetwidget.h
 * @author Simon <Job No.13> 2026-07-02
 *
 * @class AlarmResetWidget
 * @brief DebugPage widget for launching the background unresolved alarm reset task.
 *
 * Design goals:
 *      1. Display the current unresolved alarm count.
 *      2. Submit AlarmResetTask to the scheduler instead of modifying the database in UI.
 *      3. Show batch progress reported by the scheduler task.
 *******************************************************************************************/
#ifndef ALARMRESETWIDGET_H
#define ALARMRESETWIDGET_H

#include "settingwidget.h"

#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

class SettingItemWidget;

class AlarmResetWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit AlarmResetWidget(QWidget* parent = nullptr);
    ~AlarmResetWidget() override;

private slots:
    void refreshUnresolvedCount();
    void onStartClicked();

private:
    void initUI();
    void updateProgress(int totalCount, int resolvedCount, int remainingCount, int batchCount);
    void setControlsEnabled(bool enabled);

private:
    SettingItemWidget* m_controlItem = nullptr;
    QLabel* m_unresolvedCountLabel = nullptr;
    QLabel* m_progressLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QPushButton* m_startButton = nullptr;
    bool m_taskRunning = false;
};

#endif // ALARMRESETWIDGET_H
