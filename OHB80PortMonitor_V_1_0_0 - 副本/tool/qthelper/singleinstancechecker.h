#ifndef SINGLEINSTANCECHECKER_H
#define SINGLEINSTANCECHECKER_H

#include <QString>

class QSharedMemory;
class QSystemSemaphore;

/**
 * 单实例检查类
 * 用于确保软件只能被打开一次
 */
class SingleInstanceChecker
{
public:
    /**
     * 构造函数
     * @param appId 应用程序唯一标识符
     */
    explicit SingleInstanceChecker(const QString &appId);
    
    /**
     * 析构函数
     */
    ~SingleInstanceChecker();
    
    /**
     * 检查是否为第一个实例
     * @return true 如果是第一个实例，false 如果已有实例在运行
     */
    bool isFirstInstance() const;
    
    /**
     * 显示已有实例运行的提示对话框
     * 如果不是第一个实例，会显示提示并询问用户是否退出
     */
    void showAlreadyRunningMessage();
    
    /**
     * 强制退出当前实例
     */
    void forceExit();
    
private:
    QSharedMemory *m_sharedMemory;      // 共享内存用于单实例检查
    bool m_isFirstInstance;            // 是否为第一个实例
    QString m_appId;                   // 应用程序ID
};

#endif // SINGLEINSTANCECHECKER_H
