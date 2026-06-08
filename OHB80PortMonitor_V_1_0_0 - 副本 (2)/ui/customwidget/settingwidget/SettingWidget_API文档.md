# SettingWidget 使用文档

## 概述

`SettingWidget` 和 `SettingItemWidget` 是用于创建设置界面的自定义控件组合，提供了统一的设置项管理界面。

## 类结构

### SettingWidget
设置项容器控件，用于管理多个设置项。

### SettingItemWidget  
单个设置项控件，包含标题、提示信息和操作控件区域。

## SettingWidget 接口

### 构造函数
```cpp
SettingWidget(QWidget *parent = nullptr);
```

### 主要方法

#### 设置标题
```cpp
void setTitle(const QString &title);
```
- **功能**: 设置SettingWidget的标题
- **参数**: title - 标题文本

#### 添加设置项
```cpp
void addItem(SettingItemWidget *item);
```
- **功能**: 添加一个设置项到容器中
- **参数**: item - 设置项指针

#### 移除设置项
```cpp
void removeItem(SettingItemWidget *item);
```
- **功能**: 移除指定的设置项
- **参数**: item - 要移除的设置项指针

#### 按索引移除设置项
```cpp
void removeItemAt(int index);
```
- **功能**: 移除指定索引的设置项
- **参数**: index - 设置项索引

#### 隐藏/显示设置项
```cpp
void hideItem(int index);
void showItem(int index);
```
- **功能**: 隐藏或显示指定索引的设置项
- **参数**: index - 设置项索引

#### 设置/获取设置项
```cpp
void setItem(int index, SettingItemWidget *item);
SettingItemWidget *itemAt(int index) const;
```
- **功能**: 设置或获取指定索引的设置项
- **参数**: index - 索引，item - 设置项指针

#### 获取设置项数量
```cpp
int itemCount() const;
```
- **功能**: 返回当前设置项数量
- **返回值**: 设置项数量

#### 清空所有设置项
```cpp
void clearItems();
```
- **功能**: 移除并删除所有设置项

## SettingItemWidget 接口

### 构造函数
```cpp
SettingItemWidget(QWidget *parent = nullptr);
```

### 主要方法

#### 设置标题和提示
```cpp
void setTitle(const QString &title);
void setTip(const QString &tip);
```
- **功能**: 设置设置项的标题和提示信息
- **参数**: title - 标题文本，tip - 提示文本

#### 获取标签控件
```cpp
QLabel *getTitleLabel() const;
QLabel *getTipLabel() const;
```
- **功能**: 获取标题和提示标签控件
- **返回值**: 对应的QLabel指针

#### 添加操作控件
```cpp
void addWidget(const QString &key, QWidget *widget);
```
- **功能**: 添加操作控件到设置项中
- **参数**: key - 控件的唯一标识，widget - 控件指针

#### 获取操作控件
```cpp
QWidget *getWidget(const QString &key) const;
```
- **功能**: 根据key获取对应的控件
- **参数**: key - 控件的唯一标识
- **返回值**: 控件指针，如果不存在返回nullptr

#### 设置状态
```cpp
void setStatus(const QString &errorMsg, bool success);
```
- **功能**: 设置操作状态（成功/失败）
- **参数**: errorMsg - 状态消息，success - 是否成功

#### 样式设置
```cpp
void setBackgroundColor(const QColor &color);
void setFontSize(int titleSize, int tipSize = 12);
```
- **功能**: 设置背景颜色和字体大小
- **参数**: color - 背景颜色，titleSize - 标题字体大小，tipSize - 提示字体大小

## 使用步骤

### 1. 创建SettingWidget容器
```cpp
SettingWidget *widget = new SettingWidget(parent);
widget->setTitle("配置标题");
```

### 2. 创建SettingItemWidget设置项
```cpp
SettingItemWidget *item = new SettingItemWidget(widget);
item->setTitle("设置项标题");
item->setTip("设置项提示信息");
```

### 3. 添加操作控件
```cpp
// 添加输入框
QLineEdit *lineEdit = new QLineEdit(item);
lineEdit->setPlaceholderText("请输入值");
item->addWidget("input", lineEdit);

// 添加按钮
QPushButton *button = new QPushButton("设置", item);
item->addWidget("button", button);
connect(button, &QPushButton::clicked, this, &MyClass::onButtonClicked);
```

### 4. 将设置项添加到容器
```cpp
widget->addItem(item);
```

### 5. 将容器添加到界面
```cpp
layout->addWidget(widget);
```

## 完整示例

```cpp
void MyClass::createSettingWidget()
{
    // 1. 创建容器
    SettingWidget *configWidget = new SettingWidget(this);
    configWidget->setTitle("网络配置");
    
    // 2. 创建IP地址设置项
    SettingItemWidget *ipItem = new SettingItemWidget(configWidget);
    ipItem->setTitle("IP地址");
    ipItem->setTip("设置设备的IP地址");
    
    QLineEdit *ipEdit = new QLineEdit(ipItem);
    ipEdit->setPlaceholderText("192.168.1.100");
    ipItem->addWidget("ip_edit", ipEdit);
    
    QPushButton *ipBtn = new QPushButton("设置", ipItem);
    ipItem->addWidget("ip_btn", ipBtn);
    connect(ipBtn, &QPushButton::clicked, this, &MyClass::onIpSetClicked);
    
    configWidget->addItem(ipItem);
    
    // 3. 创建端口设置项
    SettingItemWidget *portItem = new SettingItemWidget(configWidget);
    portItem->setTitle("端口号");
    portItem->setTip("设置设备的端口号");
    
    QSpinBox *portSpin = new QSpinBox(portItem);
    portSpin->setRange(1, 65535);
    portSpin->setValue(8080);
    portItem->addWidget("port_spin", portSpin);
    
    QPushButton *portBtn = new QPushButton("设置", portItem);
    portItem->addWidget("port_btn", portBtn);
    connect(portBtn, &QPushButton::clicked, this, &MyClass::onPortSetClicked);
    
    configWidget->addItem(portItem);
    
    // 4. 添加到主界面
    ui->scrollAreaWidgetContents->layout()->addWidget(configWidget);
}

void MyClass::onIpSetClicked()
{
    // 获取对应的设置项
    SettingItemWidget *item = qobject_cast<SettingItemWidget*>(sender()->parent());
    if (!item) return;
    
    // 获取输入框
    QLineEdit *ipEdit = qobject_cast<QLineEdit*>(item->getWidget("ip_edit"));
    if (!ipEdit) return;
    
    QString ip = ipEdit->text();
    
    // 验证IP地址格式
    QRegExp ipRegex("^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
    if (!ipRegex.exactMatch(ip)) {
        item->setStatus("IP地址格式错误", false);
        return;
    }
    
    // TODO: 实际设置IP地址的逻辑
    
    item->setStatus("IP地址设置成功", true);
}
```

## 注意事项

1. **内存管理**: SettingWidget会自动管理其包含的SettingItemWidget的生命周期
2. **控件唯一性**: 添加控件时使用的key必须唯一，否则会覆盖之前的控件
3. **状态显示**: 使用`setStatus()`方法会自动显示状态信息并在几秒后自动隐藏
4. **样式定制**: 可以通过CSS样式表进一步定制控件外观
5. **布局管理**: 控件会自动在水平方向排列，超出部分会自动换行

## 扩展功能

### 自定义控件类型
可以添加任何继承自QWidget的控件作为操作控件：
```cpp
QComboBox *comboBox = new QComboBox(item);
comboBox->addItems({"选项1", "选项2", "选项3"});
item->addWidget("combo", comboBox);

QCheckBox *checkBox = new QCheckBox("启用功能", item);
item->addWidget("checkbox", checkBox);
```

### 动态更新控件
```cpp
// 动态更新控件值
QSpinBox *spinBox = qobject_cast<QSpinBox*>(item->getWidget("spin"));
if (spinBox) {
    spinBox->setValue(newValue);
}

// 动态禁用控件
QPushButton *button = qobject_cast<QPushButton*>(item->getWidget("button"));
if (button) {
    button->setEnabled(false);
}
```

这套控件系统提供了灵活且统一的设置界面创建方式，适用于各种配置场景。
