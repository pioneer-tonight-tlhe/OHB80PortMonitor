# SettingWidget 和 SettingItemWidget 使用文档

## 概述

`SettingWidget` 和 `SettingItemWidget` 是一套用于创建配置页面的自定义 Qt 控件组合。它们提供了一个统一、美观的界面来展示和管理各种设置项。

### 控件关系

- **SettingWidget**: 设置容器控件，包含标题和多个设置项
- **SettingItemWidget**: 单个设置项控件，包含标题、提示信息、操作控件和状态反馈

## SettingItemWidget 详细说明

### 功能特性

1. **标题和提示**: 每个设置项都有标题和可选的提示文本
2. **动态控件添加**: 支持添加任意 Qt 控件（如 QLineEdit、QPushButton 等）
3. **状态反馈**: 内置成功/失败状态显示，自动定时隐藏
4. **自定义样式**: 支持设置背景颜色、字体大小等
5. **网格布局**: 使用网格布局自动排列控件

### 主要接口

#### 构造函数
```cpp
explicit SettingItemWidget(QWidget *parent = nullptr);
```

#### 设置标题和提示
```cpp
void setTitle(const QString &title);        // 设置标题
void setTip(const QString &tip);            // 设置提示信息
```

#### 添加和获取控件
```cpp
void addWidget(const QString &key, QWidget *widget);  // 添加操作控件
QWidget *getWidget(const QString &key) const;         // 根据 key 获取控件
```

**说明**:
- `key`: 控件的唯一标识符，用于后续获取控件
- `widget`: 要添加的 Qt 控件
- 自动为 QPushButton 设置固定宽度 100px
- 自动为 QLineEdit 设置固定宽度 200px

#### 样式设置
```cpp
void setBackgroundColor(const QColor &color);           // 设置背景颜色
void setFontSize(int titleSize, int tipSize = 12);     // 设置字体大小
```

#### 状态反馈
```cpp
void setStatus(const QString &errorMsg, bool success);
```

**参数说明**:
- `errorMsg`: 错误信息（仅在失败时显示）
- `success`: true 表示成功，false 表示失败

**行为**:
- 成功时：显示绿色 "✓ 成功"，5秒后自动隐藏
- 失败时：显示红色 "✗ 失败"，10秒后自动隐藏，并弹出错误对话框

#### 获取内部控件
```cpp
QLabel *getTitleLabel() const;  // 获取标题标签
QLabel *getTipLabel() const;    // 获取提示标签
```

### 布局结构

```
SettingItemWidget
├── 主垂直布局 (QVBoxLayout)
│   ├── 操作区域 (QWidget)
│   │   └── 网格布局 (QGridLayout)
│   │       ├── [0,0] 标题标签 (QLabel)
│   │       ├── [0,1] 弹簧 (QSpacerItem)
│   │       ├── [0,2] 自定义控件1
│   │       ├── [0,3] 自定义控件2
│   │       └── [0,n] 状态标签 (QLabel)
│   └── 提示标签 (QLabel)
```

---

## SettingWidget 详细说明

### 功能特性

1. **标题栏**: 带有蓝色左边框的标题栏
2. **设置项管理**: 管理多个 SettingItemWidget
3. **动态操作**: 支持添加、删除、隐藏、显示设置项
4. **索引访问**: 通过索引访问和操作设置项

### 主要接口

#### 构造函数
```cpp
explicit SettingWidget(QWidget *parent = nullptr);
```

#### 标题设置
```cpp
void setTitle(const QString &title);    // 设置标题
QLabel *titleLabel() const;             // 获取标题控件
```

#### 添加和移除设置项
```cpp
void addItem(SettingItemWidget *item);              // 添加设置项
void removeItem(SettingItemWidget *item);           // 移除设置项
void removeItemAt(int index);                       // 移除指定索引的设置项
void clearItems();                                  // 清空所有设置项
```

#### 显示和隐藏设置项
```cpp
void hideItem(int index);   // 隐藏指定索引的设置项
void showItem(int index);   // 显示指定索引的设置项
```

#### 访问设置项
```cpp
void setItem(int index, SettingItemWidget *item);   // 设置指定索引的设置项
SettingItemWidget *itemAt(int index) const;         // 获取指定索引的设置项
int itemCount() const;                              // 获取设置项数量
```

### 布局结构

```
SettingWidget
└── 主垂直布局 (QVBoxLayout)
    ├── 标题标签 (QLabel) - 固定高度 25px
    └── 设置项布局 (QVBoxLayout)
        ├── SettingItemWidget 1
        ├── SettingItemWidget 2
        └── ...
```

---

## 使用案例

### 案例一：基于 ConfigPage 的完整示例

以下是基于 `@d:\Project\qt-creator\OHB\OHBOneBaseDemo\CYTCP_OHBOneBaseDemo_V_1_0_0\ui\configpage.cpp` 的实际使用案例。

#### 1. 头文件声明 (configpage.h)

```cpp
#ifndef CONFIGPAGE_H
#define CONFIGPAGE_H

#include "settingwidget.h"
#include <QMap>
#include <QWidget>

namespace Ui {
class ConfigPage;
}

class ConfigPage : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigPage(QWidget *parent = nullptr);
    ~ConfigPage();

    void initUi();
    void initIdlePurgeSettingWidget();

private slots:
    void onContinueTimeBtnClicked();
    void onFreeTimeBtnClicked();

private:
    Ui::ConfigPage *ui;
    QMap<QString, SettingWidget*> m_settingWidgetList;
};

#endif // CONFIGPAGE_H
```

#### 2. 实现文件 (configpage.cpp)

```cpp
#include "configpage.h"
#include "ui_configpage.h"
#include "customwidget/settingitemwidget.h"
#include <QLineEdit>
#include <QPushButton>

ConfigPage::ConfigPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ConfigPage)
{
    ui->setupUi(this);
    initUi();
}

ConfigPage::~ConfigPage()
{
    delete ui;
}

void ConfigPage::initUi()
{
    initIdlePurgeSettingWidget();
    // 添加弹簧使设置项向上对齐
    ui->scrollAreaWidgetContents->layout()->addItem(
        new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding)
    );
}

void ConfigPage::initIdlePurgeSettingWidget()
{
    // 1. 创建 SettingWidget 容器
    SettingWidget *w = new SettingWidget(this);
    w->setTitle("Idle Purge Config");
    m_settingWidgetList["idle_purge"] = w;
    ui->scrollAreaWidgetContents->layout()->addWidget(w);

    // 2. 创建第一个设置项：Continue Time (充气持续时间)
    auto item = new SettingItemWidget(w);
    item->setTitle("Continue Time");
    item->setTip("Duration of continuous purge in milliseconds");
    
    // 添加输入框
    auto continue_time_input = new QLineEdit(item);
    item->addWidget("continue_time_input", continue_time_input);
    
    // 添加设置按钮
    auto continue_time_btn = new QPushButton("Set", item);
    item->addWidget("continue_time_btn", continue_time_btn);
    connect(continue_time_btn, &QPushButton::clicked, 
            this, &ConfigPage::onContinueTimeBtnClicked);
    
    // 将设置项添加到容器
    w->addItem(item);

    // 3. 创建第二个设置项：Free Time (间隔时间)
    item = new SettingItemWidget(w);
    item->setTitle("Free Time");
    item->setTip("Interval time between purge cycles in milliseconds");
    
    auto free_time_input = new QLineEdit(item);
    item->addWidget("free_time_input", free_time_input);
    
    auto free_time_btn = new QPushButton("Set", item);
    item->addWidget("free_time_btn", free_time_btn);
    connect(free_time_btn, &QPushButton::clicked, 
            this, &ConfigPage::onFreeTimeBtnClicked);
    
    w->addItem(item);
}

void ConfigPage::onContinueTimeBtnClicked()
{
    // 获取对应的 SettingItemWidget
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (btn) {
        QWidget *actionsWidget = qobject_cast<QWidget*>(btn->parent());
        if (actionsWidget) {
            SettingItemWidget *item = qobject_cast<SettingItemWidget*>(
                actionsWidget->parent()
            );
            if (item) {
                // 模拟失败场景
                item->setStatus("设置充气持续时间失败：设备连接超时", false);
                
                // 成功场景示例：
                // item->setStatus("", true);
            }
        }
    }
}

void ConfigPage::onFreeTimeBtnClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (btn) {
        QWidget *actionsWidget = qobject_cast<QWidget*>(btn->parent());
        if (actionsWidget) {
            SettingItemWidget *item = qobject_cast<SettingItemWidget*>(
                actionsWidget->parent()
            );
            if (item) {
                // 模拟失败场景
                item->setStatus("设置间隔时间失败：设备连接超时", false);
            }
        }
    }
}
```

#### 3. UI 文件结构 (configpage.ui)

在 Qt Designer 中创建以下结构：

```xml
QWidget (ConfigPage)
└── QVBoxLayout
    └── QWidget (widgetSettings)
        └── QVBoxLayout
            └── QScrollArea (scrollArea)
                └── QWidget (scrollAreaWidgetContents)
                    └── QVBoxLayout
                        <!-- SettingWidget 将动态添加到这里 -->
```

---

### 案例二：简单示例

```cpp
#include "settingwidget.h"
#include "settingitemwidget.h"
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

// 创建设置容器
SettingWidget *settingWidget = new SettingWidget(parentWidget);
settingWidget->setTitle("网络设置");

// 创建 IP 地址设置项
SettingItemWidget *ipItem = new SettingItemWidget(settingWidget);
ipItem->setTitle("IP 地址");
ipItem->setTip("请输入设备的 IP 地址");

QLineEdit *ipInput = new QLineEdit(ipItem);
ipInput->setPlaceholderText("192.168.1.100");
ipItem->addWidget("ip_input", ipInput);

QPushButton *ipBtn = new QPushButton("应用", ipItem);
ipItem->addWidget("ip_btn", ipBtn);

settingWidget->addItem(ipItem);

// 创建端口设置项
SettingItemWidget *portItem = new SettingItemWidget(settingWidget);
portItem->setTitle("端口号");
portItem->setTip("请输入设备的端口号 (1-65535)");

QSpinBox *portInput = new QSpinBox(portItem);
portInput->setRange(1, 65535);
portInput->setValue(8080);
portItem->addWidget("port_input", portInput);

QPushButton *portBtn = new QPushButton("应用", portItem);
ipItem->addWidget("port_btn", portBtn);

settingWidget->addItem(portItem);

// 连接信号槽
connect(ipBtn, &QPushButton::clicked, [ipItem, ipInput]() {
    QString ip = ipInput->text();
    if (ip.isEmpty()) {
        ipItem->setStatus("IP 地址不能为空", false);
    } else {
        // 执行设置操作...
        ipItem->setStatus("", true);
    }
});
```

---

### 案例三：动态管理设置项

```cpp
SettingWidget *settingWidget = new SettingWidget(parentWidget);
settingWidget->setTitle("高级设置");

// 添加多个设置项
for (int i = 0; i < 5; i++) {
    SettingItemWidget *item = new SettingItemWidget(settingWidget);
    item->setTitle(QString("设置项 %1").arg(i + 1));
    item->setTip(QString("这是第 %1 个设置项").arg(i + 1));
    settingWidget->addItem(item);
}

// 隐藏第 3 个设置项
settingWidget->hideItem(2);

// 显示第 3 个设置项
settingWidget->showItem(2);

// 移除第 2 个设置项
settingWidget->removeItemAt(1);

// 获取设置项数量
int count = settingWidget->itemCount();

// 访问特定设置项
SettingItemWidget *firstItem = settingWidget->itemAt(0);
if (firstItem) {
    firstItem->setTitle("修改后的标题");
}

// 清空所有设置项
settingWidget->clearItems();
```

---

## 最佳实践

### 1. 控件获取

推荐使用 key 来获取添加的控件：

```cpp
SettingItemWidget *item = new SettingItemWidget();
QLineEdit *input = new QLineEdit();
item->addWidget("my_input", input);

// 后续获取
QLineEdit *retrievedInput = qobject_cast<QLineEdit*>(
    item->getWidget("my_input")
);
```

### 2. 状态反馈

在异步操作完成后设置状态：

```cpp
void onSettingComplete(bool success, const QString &error) {
    if (success) {
        settingItem->setStatus("", true);
    } else {
        settingItem->setStatus(error, false);
    }
}
```

### 3. 布局管理

在 QScrollArea 中使用时，记得添加弹簧：

```cpp
scrollAreaLayout->addWidget(settingWidget);
scrollAreaLayout->addItem(
    new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding)
);
```

### 4. 内存管理

- SettingWidget 会自动管理其子 SettingItemWidget 的内存
- 使用 `removeItem()` 时会自动删除控件
- 不需要手动 delete 添加到 SettingWidget 中的 SettingItemWidget

---

## 样式定制

### SettingItemWidget 样式

默认样式包含底部边框：
```css
SettingItemWidget { 
    border-bottom: 1px solid rgba(0, 50, 120, 200); 
    border-radius: 6px; 
}
```

标题样式：
```css
QLabel#titleLabel { 
    font-weight: bold;
    font-size: 14px; 
}
```

提示样式：
```css
QLabel#tipLabel { 
    font-size: 12px; 
}
```

### SettingWidget 样式

标题栏样式：
```css
QLabel#titleLabel {
    font-size: 16px;
    font-weight: bold;
    color: #ABD6FF;
    padding: 4px 5px;
    background-color: rgba(0, 50, 107, 180);
    border-left: 3px solid #00B7DE;
    border-bottom: 1px solid rgba(171, 214, 255, 30);
}
```

---

## 注意事项

1. **控件宽度**: QPushButton 和 QLineEdit 会自动设置固定宽度，其他控件需要手动设置
2. **状态定时器**: 状态标签会自动隐藏，成功 5 秒，失败 10 秒
3. **父子关系**: 添加到 SettingItemWidget 的控件必须将其作为父控件
4. **索引管理**: 移除设置项后，索引会自动调整
5. **信号槽**: 记得在槽函数中正确获取 SettingItemWidget 指针

---

## 常见问题

### Q: 如何修改控件的默认宽度？

A: 在调用 `addWidget()` 之前手动设置：
```cpp
QLineEdit *input = new QLineEdit();
input->setFixedWidth(300);  // 自定义宽度
item->addWidget("input", input);
```

### Q: 如何获取用户输入的值？

A: 通过 key 获取控件后，转换为具体类型：
```cpp
QLineEdit *input = qobject_cast<QLineEdit*>(item->getWidget("my_input"));
if (input) {
    QString value = input->text();
}
```

### Q: 状态标签不显示怎么办？

A: 确保在调用 `setStatus()` 之前已经添加了至少一个控件，因为状态标签会被添加到最后一列。

### Q: 如何在按钮点击时获取对应的 SettingItemWidget？

A: 参考 ConfigPage 的实现方式：
```cpp
QPushButton *btn = qobject_cast<QPushButton*>(sender());
QWidget *actionsWidget = qobject_cast<QWidget*>(btn->parent());
SettingItemWidget *item = qobject_cast<SettingItemWidget*>(actionsWidget->parent());
```

---

## 版本信息

- **创建日期**: 2026-03-03
- **适用版本**: Qt 5.15.2+
- **项目**: CYTCP_OHBOneBaseDemo_V_1_0_0
