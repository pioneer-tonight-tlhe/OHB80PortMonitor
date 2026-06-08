#include "singleinstancechecker.h"

#include <QSharedMemory>
#include <QSystemSemaphore>
#include <QMessageBox>
#include <QApplication>
#include <QDebug>

SingleInstanceChecker::SingleInstanceChecker(const QString &appId)
    : m_sharedMemory(nullptr)
    , m_isFirstInstance(false)
    , m_appId(appId)
{
    // 创建系统信号量，用于同步多个进程的访问
    QSystemSemaphore semaphore(QString("%1_semaphore").arg(appId), 1);
    semaphore.acquire();
    
    // 创建共享内存
    m_sharedMemory = new QSharedMemory(QString("%1_shared").arg(appId));
    
    // 尝试附加到共享内存
    if (m_sharedMemory->attach()) {
        // 如果能够附加，说明已有实例在运行
        m_isFirstInstance = false;
        qDebug() << "[SingleInstanceChecker] 检测到已有实例在运行，应用ID:" << appId;
    } else {
        // 尝试创建共享内存
        if (m_sharedMemory->create(1)) {
            m_isFirstInstance = true;
            qDebug() << "[SingleInstanceChecker] 这是第一个实例，应用ID:" << appId;
        } else {
            m_isFirstInstance = false;
            qWarning() << "[SingleInstanceChecker] 创建共享内存失败，应用ID:" << appId;
        }
    }
    
    semaphore.release();
}

SingleInstanceChecker::~SingleInstanceChecker()
{
    if (m_sharedMemory) {
        if (m_isFirstInstance) {
            // 只有第一个实例需要分离共享内存
            m_sharedMemory->detach();
            qDebug() << "[SingleInstanceChecker] 第一个实例退出，释放共享内存";
        }
        delete m_sharedMemory;
        m_sharedMemory = nullptr;
    }
}

bool SingleInstanceChecker::isFirstInstance() const
{
    return m_isFirstInstance;
}

void SingleInstanceChecker::showAlreadyRunningMessage()
{
    if (!m_isFirstInstance) {
        // 显示警告信息，告知用户程序已在运行
        QMessageBox::warning(
            nullptr,
            "Application Already Running",
            "The application is already running.\n"
            "This instance will automatically exit.\n\n"
            "Please use the already running instance.",
            QMessageBox::Ok
        );
        
        qDebug() << "[SingleInstanceChecker] 检测到重复实例，自动退出当前实例";
        QApplication::quit();
    }
}

void SingleInstanceChecker::forceExit()
{
    if (!m_isFirstInstance) {
        qDebug() << "[SingleInstanceChecker] 强制退出非第一个实例";
        QApplication::exit(1);
    }
}
