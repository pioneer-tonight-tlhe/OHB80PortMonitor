# UserManager 用户权限管理模块

## 目录

1. [模块概述](#1-模块概述)
2. [架构设计](#2-架构设计)
3. [权限枚举](#3-权限枚举-userpermission)
4. [数据存储格式](#4-数据存储格式-userinfouini)
5. [公开接口详解](#5-公开接口详解)
6. [信号详解](#6-信号详解)
7. [内部实现说明](#7-内部实现说明)
8. [使用案例](#8-使用案例)

---

## 1. 模块概述

`UserManager` 是一个基于 Qt 的**单例用户权限管理类**，核心职责：

- 验证用户账号密码（从 `userinfo.ini` 读取）
- 根据当前登录用户的权限级别，**自动控制已注册 UI 控件的显示/隐藏**
- 提供用户增删改接口（需要 Debug 及以上权限）

模块由两个文件组成：

| 文件 | 职责 |
|------|------|
| `usermanager.h` | 权限枚举定义、类声明、公开接口 |
| `usermanager.cpp` | 所有接口的实现、配置文件读写 |

---

## 2. 架构设计

```
┌─────────────────────────────────────────────────────┐
│                    UserManager（单例）               │
│                                                     │
│  m_currentPermission  ←  login() / logout()         │
│  m_currentUser                                      │
│                                                     │
│  m_widgetPermMap                                    │
│  QMap<UserPermission, QVector<QPointer<QWidget>>>   │
│  ┌──────────┬───────────────────────────────────┐   │
│  │ Guest(0) │ [btnNavHome, ...]                 │   │
│  │ Normal(1)│ [btnNavConfig, ...]               │   │
│  │ Debug(2) │ [btnNavDebug, btnUserManage, ...] │   │
│  │ ...      │ ...                               │   │
│  └──────────┴───────────────────────────────────┘   │
│                                                     │
│  updateWidgetVisibility()                           │
│   └─ upperBound(currentPermission)                  │
│       ├─ [begin, cutoff)  → show()                  │
│       └─ [cutoff, end)    → hide()                  │
└─────────────────────────────────────────────────────┘
         │ signals
         ▼
  loginSuccess / loginFailed / logoutSuccess / permissionChanged
```

### 关键设计决策

**① 单例模式**  
整个程序只需一个 `UserManager` 实例，任何地方通过 `UserManager::instance()` 获取，无需传递指针。

**② `QMap<UserPermission, QVector<QPointer<QWidget>>>`**  
- **Key**（`UserPermission`）：该组控件可见所需的最低权限  
- **Value**（`QVector<QPointer<QWidget>>`）：该权限级别下所有注册的控件  
- 使用 `QPointer` 而非裸指针：当控件被销毁（如对话框关闭）时，`QPointer` 自动置 `null`，避免野指针崩溃

**③ `upperBound` 分段**  
`QMap` 按 key 升序排列，利用 `upperBound(currentPermission)` 一次定位分界点，分为"显示段"和"隐藏段"，语义清晰。

---

## 3. 权限枚举 `UserPermission`

```cpp
enum class UserPermission {
    Guest    = 0,   // 游客（未登录）
    Normal   = 1,   // 普通用户
    Debug    = 2,   // 调试用户
    Engineer = 3,   // 工程师
    Root     = 4    // root 用户
};
```

- 数值越大，权限越高
- 可以直接用 `>=` / `<=` 比较，例如 `currentPermission >= UserPermission::Debug`
- 登出后自动恢复为 `Guest(0)`

### 各权限可访问的界面

| 权限 | 首页 | 配置页 | 调试页 | 用户管理 |
|------|------|--------|--------|---------|
| Guest | ✓ | ✗ | ✗ | ✗ |
| Normal | ✓ | ✓ | ✗ | ✗ |
| Debug | ✓ | ✓ | ✓ | ✓ |
| Engineer | ✓ | ✓ | ✓ | ✓ |
| Root | ✓ | ✓ | ✓ | ✓ |

---

## 4. 数据存储格式 `userinfo.ini`

文件路径：`<应用程序目录>/userinfo.ini`

每个用户占一个 Section，Section 名即用户名：

```ini
[root]
password=root123
permission=4

[engineer1]
password=eng123
permission=3

[debug1]
password=dbg123
permission=2

[user1]
password=user123
permission=1
```

- `password`：明文密码
- `permission`：对应 `UserPermission` 枚举的整数值

> 首次运行时若文件不存在，`initDefaultConfig()` 自动生成以上四个默认用户。

---

## 5. 公开接口详解

### 5.1 `instance()` — 获取单例

```cpp
static UserManager* instance();
```

返回全局唯一的 `UserManager` 实例，首次调用时自动创建并初始化（读取/生成配置文件）。

```cpp
UserManager* mgr = UserManager::instance();
```

---

### 5.2 `registerWidget()` — 注册控件

```cpp
void registerWidget(QWidget* widget, UserPermission minPermission);
```

将一个 UI 控件注册到权限表，并立即根据**当前权限**设置其可见性。

| 参数 | 说明 |
|------|------|
| `widget` | 要受权限控制的控件指针（为 null 时忽略） |
| `minPermission` | 该控件可见所需的最低权限 |

**注意**：使用 `QPointer` 存储，控件销毁后自动从有效控件中排除，无需手动注销。

```cpp
// 导航按钮：普通用户及以上才能看到配置页按钮
UserManager::instance()->registerWidget(btnNavConfig, UserPermission::Normal);

// 用户管理按钮：调试用户及以上才可见
UserManager::instance()->registerWidget(btnUserManage, UserPermission::Debug);
```

---

### 5.3 `login()` — 用户登录

```cpp
bool login(const QString& username, const QString& password);
```

从 `userinfo.ini` 验证账号密码，成功后更新当前权限并刷新所有注册控件的可见性。

| 返回值 | 说明 |
|--------|------|
| `true` | 登录成功，同时发射 `loginSuccess` 和 `permissionChanged` 信号 |
| `false` | 用户不存在或密码错误，同时发射 `loginFailed` 信号 |

**内部流程**：
```
读取 userinfo.ini 中 username 的 password 和 permission
  ├─ password 为空 → emit loginFailed("用户不存在") → return false
  ├─ password 不匹配 → emit loginFailed("密码错误") → return false
  └─ 验证通过
       ├─ 写入 m_currentUser / m_currentPermission
       ├─ updateWidgetVisibility()   ← 批量刷新控件可见性
       ├─ emit loginSuccess(...)
       └─ emit permissionChanged(...)
```

---

### 5.4 `logout()` — 用户登出

```cpp
void logout();
```

清除当前用户信息，权限恢复为 `Guest(0)`，刷新所有控件可见性。

**内部流程**：
```
m_currentUser.clear()
m_currentPermission = Guest
updateWidgetVisibility()   ← 将 Normal+ 的控件全部隐藏
emit logoutSuccess()
emit permissionChanged(Guest)
```

---

### 5.5 `addUser()` — 添加用户

```cpp
bool addUser(const QString& username, const QString& password, UserPermission permission);
```

向 `userinfo.ini` 新增一条用户记录。

**前置条件**：当前登录用户权限 ≥ `Debug`，否则直接返回 `false`。

| 参数 | 说明 |
|------|------|
| `username` | 新用户名（不能与已有用户重复） |
| `password` | 初始密码 |
| `permission` | 新用户的权限级别 |

---

### 5.6 `removeUser()` — 删除用户

```cpp
bool removeUser(const QString& username);
```

从 `userinfo.ini` 删除指定用户的整个 Section。

**前置条件**：当前登录用户权限 ≥ `Debug`。  
用户不存在时返回 `false`。

---

### 5.7 `modifyUser()` — 修改密码

```cpp
bool modifyUser(const QString& username, const QString& newPassword);
```

修改指定用户的密码字段。

**前置条件**：当前登录用户权限 ≥ `Debug`。

---

### 5.8 `setUserPermission()` — 修改权限

```cpp
bool setUserPermission(const QString& username, UserPermission newPermission);
```

修改指定用户的权限级别（`permission` 字段）。

**前置条件**：当前登录用户权限 ≥ `Debug`。

---

### 5.9 `currentUser()` — 当前用户名

```cpp
QString currentUser() const;
```

返回当前登录的用户名，未登录时返回空字符串 `""`。

---

### 5.10 `currentPermission()` — 当前权限

```cpp
UserPermission currentPermission() const;
```

返回当前权限级别，未登录时为 `UserPermission::Guest`。

---

### 5.11 `allUsers()` — 所有用户列表

```cpp
QStringList allUsers() const;
```

读取 `userinfo.ini` 中所有 Section 名（即用户名列表），常用于用户管理界面的列表刷新。

---

### 5.12 `userPermission()` — 查询指定用户权限

```cpp
UserPermission userPermission(const QString& username) const;
```

读取配置文件中指定用户的权限级别，用户不存在时返回 `UserPermission::Guest`。

---

## 6. 信号详解

| 信号 | 触发时机 | 参数 |
|------|---------|------|
| `loginSuccess(username, permission)` | 登录验证通过后 | 用户名、权限级别 |
| `loginFailed(reason)` | 登录验证失败后 | 失败原因字符串 |
| `logoutSuccess()` | 登出完成后 | 无 |
| `permissionChanged(permission)` | 权限发生变化时（登录/登出均会触发） | 新的权限级别 |

> `loginSuccess` 和 `permissionChanged` 在同一次 `login()` 调用中**都会**发射，前者携带用户名信息，后者用于需要监听权限变化但不关心用户名的场合。

---

## 7. 内部实现说明

### `updateWidgetVisibility()` — 核心可见性刷新

```cpp
void UserManager::updateWidgetVisibility()
{
    const auto cutoff = m_widgetPermMap.upperBound(m_currentPermission);

    for (auto it = m_widgetPermMap.constBegin(); it != cutoff; ++it) {
        for (QWidget* w : it.value()) {
            if (w) w->show();   // QPointer 为 null 时 if(w) 为 false，安全跳过
        }
    }
    for (auto it = cutoff; it != m_widgetPermMap.constEnd(); ++it) {
        for (QWidget* w : it.value()) {
            if (w) w->hide();
        }
    }
}
```

`QMap` 按 key（`UserPermission`）升序排列，`upperBound(currentPermission)` 返回第一个 key 严格大于当前权限的迭代器，将 map 一分为二：

```
Guest(0) Normal(1) Debug(2) Engineer(3) Root(4)
|─────────────────|cutoff|──────────────────────|
     show()                      hide()
（当 currentPermission == Normal 时）
```

### `initDefaultConfig()` — 默认配置初始化

仅在 `userinfo.ini` 不存在时执行，写入四个默认账号：

| 用户名 | 密码 | 权限 |
|--------|------|------|
| root | root123 | Root(4) |
| engineer1 | eng123 | Engineer(3) |
| debug1 | dbg123 | Debug(2) |
| user1 | user123 | Normal(1) |

---

## 8. 使用案例

### 完整集成步骤

**Step 1：在主窗口构造函数中连接信号**

```cpp
// mainwindow.cpp
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setupUi();   // 先创建所有控件

    UserManager* mgr = UserManager::instance();
    connect(mgr, &UserManager::loginSuccess,
            this, &MainWindow::onLoginSuccess);
    connect(mgr, &UserManager::loginFailed,
            this, &MainWindow::onLoginFailed);
    connect(mgr, &UserManager::logoutSuccess,
            this, &MainWindow::onLogoutSuccess);
    connect(mgr, &UserManager::permissionChanged,
            this, &MainWindow::onPermissionChanged);

    registerPermissionWidgets();  // 注册需要权限控制的控件
}
```

**Step 2：注册控件**

```cpp
void MainWindow::registerPermissionWidgets()
{
    UserManager* mgr = UserManager::instance();

    // 导航按钮
    mgr->registerWidget(m_btnNavHome,    UserPermission::Guest);    // 始终可见
    mgr->registerWidget(m_btnNavConfig,  UserPermission::Normal);   // Normal+ 可见
    mgr->registerWidget(m_btnNavDebug,   UserPermission::Debug);    // Debug+  可见

    // 功能按钮
    mgr->registerWidget(m_btnUserManage, UserPermission::Debug);    // Debug+  可见
}
```

**Step 3：调用登录**

```cpp
void MainWindow::onLoginClicked()
{
    LoginDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    // login() 内部自动完成控件可见性刷新
    UserManager::instance()->login(dlg.username(), dlg.password());
}

void MainWindow::onLoginSuccess(const QString& username, UserPermission permission)
{
    m_labelStatus->setText(QString("当前用户：%1").arg(username));
    m_btnLogin->hide();
    m_btnLogout->show();
}

void MainWindow::onLoginFailed(const QString& reason)
{
    QMessageBox::warning(this, "登录失败", reason);
}
```

**Step 4：调用登出**

```cpp
void MainWindow::onLogoutClicked()
{
    UserManager::instance()->logout();
    // logout() 内部自动将 Normal+ 的控件全部隐藏
}

void MainWindow::onLogoutSuccess()
{
    m_labelStatus->setText("当前用户：未登录");
    m_btnLogin->show();
    m_btnLogout->hide();
}
```

**Step 5：在弹窗中注册按钮（注意生命周期）**

```cpp
// UserManageDialog 构造函数中注册按钮
UserManageDialog::UserManageDialog(QWidget* parent) : QDialog(parent)
{
    // ... 创建按钮 ...

    UserManager* mgr = UserManager::instance();
    mgr->registerWidget(m_addBtn,    UserPermission::Debug);
    mgr->registerWidget(m_removeBtn, UserPermission::Debug);
    // 对话框关闭销毁按钮时，QPointer 自动置 null，不会崩溃
}
```

### 运行效果

```
程序启动
  └─ 权限: Guest → 仅显示"首页"按钮

用户输入 user1 / user123 登录
  └─ 权限: Normal → 显示"首页"+"配置"按钮

用户输入 debug1 / dbg123 登录
  └─ 权限: Debug → 显示全部导航按钮 + "用户管理"按钮

用户点击登出
  └─ 权限: Guest → 恢复只显示"首页"按钮
```
