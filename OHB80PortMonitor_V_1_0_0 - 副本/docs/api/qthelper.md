# QtHelper API 文档

## 模块概述

QtHelper 是一个静态工具类，提供常用的 Qt 辅助功能，包括字体图标设置、屏幕几何信息获取、单实例检查、KMP 字节搜索，以及磁盘空间查询。

**特性：**
- 所有方法均为静态方法，无需实例化
- 跨平台兼容（Windows/Linux）
- 提供磁盘容量查询功能

---

## 类定义

```cpp
class QtHelper
{
public:
    QtHelper();

    // 设置字符图标
    static void setLabelIcon(QLabel *label, const QString &fontFile, int iconCode, int fontSize);

    // 获取窗口当前所在屏幕的可用区域
    static QRect getCurrentScreenAvailableGeometry(QWidget *widget);

    // 单实例检查：确保软件只能被打开一次
    static bool checkSingleInstance(const QString& appId);
    static void releaseInstance();

    // KMP 字节序列搜索（O(n+m)）
    static int kmpSearch(const QByteArray &buffer, const QByteArray &pattern);

    // 磁盘空间查询
    static qint64 diskTotalBytes(const QString &path = QString());
    static qint64 diskUsedBytes(const QString &path = QString());
};
```

---

## 公共 API

### diskTotalBytes

获取指定路径所在磁盘分区的总容量（字节）。

```cpp
static qint64 diskTotalBytes(const QString &path = QString());
```

**参数：**
- `path` (const QString&): 要查询的路径，默认为应用程序所在目录

**返回值：**
- `qint64`: 磁盘分区总容量（字节），失败返回 `-1`

**说明：**
- 使用 Qt 的 `QStorageInfo` 实现
- 当 `path` 为空时，默认查询应用程序所在目录的磁盘分区
- 无效路径或无法访问时返回 `-1` 并输出 `qWarning`

---

### diskUsedBytes

获取指定路径所在磁盘分区的已用容量（字节）。

```cpp
static qint64 diskUsedBytes(const QString &path = QString());
```

**参数：**
- `path` (const QString&): 要查询的路径，默认为应用程序所在目录

**返回值：**
- `qint64`: 磁盘分区已用容量（字节），失败返回 `-1`

**说明：**
- 已用容量 = 总容量 - 可用容量
- 使用 `QStorageInfo::bytesTotal() - QStorageInfo::bytesFree()` 计算
- 无效路径或无法访问时返回 `-1` 并输出 `qWarning`

---

## 使用示例

### 查询应用程序所在目录的磁盘信息

```cpp
#include "qthelper.h"

qint64 total = QtHelper::diskTotalBytes();
qint64 used  = QtHelper::diskUsedBytes();

if (total >= 0 && used >= 0) {
    double totalGB = total / 1024.0 / 1024.0 / 1024.0;
    double usedGB  = used  / 1024.0 / 1024.0 / 1024.0;
    qDebug() << "总容量:" << totalGB << "GB, 已用:" << usedGB << "GB";
}
```

### 查询指定盘符的磁盘信息

```cpp
#include "qthelper.h"

// 查询 D 盘
qint64 dTotal = QtHelper::diskTotalBytes("D:/");
qint64 dUsed  = QtHelper::diskUsedBytes("D:/");

if (dTotal >= 0 && dUsed >= 0) {
    QMessageBox::information(nullptr, "D 盘磁盘情况",
        QString("D 盘总容量: %1 GB\n已用: %2 GB")
            .arg(dTotal / 1024.0 / 1024.0 / 1024.0, 0, 'f', 2)
            .arg(dUsed  / 1024.0 / 1024.0 / 1024.0, 0, 'f', 2));
}
```

### 在 UI 初始化时检查磁盘空间

```cpp
// HomePage 构造函数
HomePage::HomePage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::HomePage)
{
    ui->setupUi(this);

    // 检查 D 盘磁盘空间
    qint64 totalBytes = QtHelper::diskTotalBytes("D:/");
    qint64 usedBytes  = QtHelper::diskUsedBytes("D:/");
    if (totalBytes >= 0 && usedBytes >= 0) {
        double totalGB = totalBytes / 1024.0 / 1024.0 / 1024.0;
        double usedGB  = usedBytes  / 1024.0 / 1024.0 / 1024.0;
        QMessageBox::information(this, "D 盘磁盘情况",
            QString("D 盘总容量: %1 GB\n已用: %2 GB")
                .arg(totalGB, 0, 'f', 2)
                .arg(usedGB, 0, 'f', 2));
    } else {
        QMessageBox::warning(this, "磁盘信息", "无法获取 D 盘磁盘信息");
    }

    initUI();
}
```

---

## 注意事项

1. **路径格式**：
   - Windows 使用 `D:/` 或 `D:\\`
   - Linux 使用 `/home/user`
   - 路径必须指向实际存在的目录

2. **返回值检查**：调用后应检查返回值是否为 `-1`，判断查询是否成功

3. **字节单位转换**：返回值为字节，需自行转换为 KB/MB/GB：
   ```cpp
   double KB = bytes / 1024.0;
   double MB = KB / 1024.0;
   double GB = MB / 1024.0;
   ```

4. **依赖**：需要 Qt5Core 模块（`QStorageInfo` 在 Qt5.4+ 可用）

5. **线程安全**：`QStorageInfo` 本身是线程安全的，可在任意线程调用

---

## 相关文件

- 头文件：`tool/qthelper/qthelper.h`
- 实现文件：`tool/qthelper/qthelper.cpp`
