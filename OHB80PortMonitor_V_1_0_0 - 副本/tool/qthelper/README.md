# QtHelper 单实例检查类使用说明

## SingleInstanceChecker 类

用于确保软件只能被打开一次的实用工具类。

### 功能特性

- 基于共享内存和信号量的进程间同步
- 自动检测是否已有实例在运行
- 提供用户友好的提示对话框
- 支持强制退出功能
- 完整的资源管理和清理

### 使用方法

#### 1. 包含头文件

```cpp
#include "singleinstancechecker.h"
```

#### 2. 在 main.cpp 中使用

```cpp
#include <QApplication>
#include "singleinstancechecker.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    // 创建单实例检查器，使用应用程序唯一ID
    SingleInstanceChecker checker("CYTC_OHB_OneBaseDemo_V1.0.0");
    
    // 检查是否为第一个实例
    if (!checker.isFirstInstance()) {
        // 显示提示对话框，询问用户是否退出
        checker.showAlreadyRunningMessage();
        
        // 或者直接强制退出
        // checker.forceExit();
        return 0;
    }
    
    // 正常启动应用程序
    // ... 你的应用程序代码 ...
    
    return a.exec();
}
```

### API 说明

#### 构造函数
```cpp
SingleInstanceChecker(const QString &appId)
```
- `appId`: 应用程序唯一标识符，建议使用应用名称+版本号

#### 主要方法

- `bool isFirstInstance() const`: 检查是否为第一个实例
- `void showAlreadyRunningMessage()`: 显示已有实例运行的提示对话框
- `void forceExit()`: 强制退出当前实例

### 注意事项

1. **应用程序ID**: 确保每个应用程序使用唯一的ID，建议使用"应用名称_版本号"格式
2. **资源管理**: 析构函数会自动清理共享内存资源
3. **线程安全**: 使用系统信号量确保多进程访问的安全性
4. **跨平台**: 支持Windows、Linux、macOS等平台

### 示例应用程序ID

- `"MyApp_v1.0.0"`
- `"CYTC_OHB_OneBaseDemo_V1.0.0"`
- `"Company_Product_Version"`

### 日志输出

类会输出详细的调试日志，帮助开发者了解单实例检查的状态：

```
[SingleInstanceChecker] 这是第一个实例，应用ID: CYTC_OHB_OneBaseDemo_V1.0.0
[SingleInstanceChecker] 检测到已有实例在运行，应用ID: CYTC_OHB_OneBaseDemo_V1.0.0
[SingleInstanceChecker] 用户选择退出当前实例
```
