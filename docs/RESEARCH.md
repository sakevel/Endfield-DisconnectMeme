# 客户端登录与网络会话研究

本文档记录客户端登录弹窗控制器与网络会话底层的实现机制及 Hook 拦截契约。

---

## 1. 登录面板与弹窗机制

### 1.1 登录流程与原生控件
- 游戏登录流程由原生 C# 模块 `Entry.Beyond.dll` 驱动，而非 Lua 脚本层。
- 登录点击入口：`Beyond.Login.LoginEnterGamePanel._OnEnterGameClicked(PointerEventData)`。
- 原生弹窗管理器：`Beyond.LoginManager.AlertDialog(string, Action)` 为原生通用提示弹窗入口，复用 `assets/beyond/initialassets/prefabs/login/loginalertdialog.prefab` 预制体，保留游戏原生的排版、字体样式、动画过渡与确认回调机制。

### 1.2 登录界面生命周期
- `Beyond.Login.LoginRootPanel.Init` 与 `OnDestroy` 对应登录界面的创建与销毁。
- 在登录界面初始化或重新激活时自动恢复网络连接，确保不会将断网拦截状态带入正常的重新登录流程。

---

## 2. 网络会话与断网模拟机制

### 2.1 会话架构与分类
- 原生网络会话主要由 `Network.Beyond.dll` 与 `Gameplay.Beyond.dll` 共同管理：
  - `Beyond.Network.HGNetSession`：主游戏业务网络会话；
  - `HGNetLoggerSession`：独立的日志上报会话。
- 两者共享底层 `TcpIO` 通信类。为了精准拦截游戏数据而不影响客户端日志上传，Hook 仅针对 `HGNetSession` 关联的 I/O 实例生效。

### 2.2 数据流拦截与断网测试
- 底层 `TcpIO` 包含 `TestCloseNetIO` 与读写状态管理。
- 模组通过在主线程和 I/O 线程安全劫持连接读写入口，模拟网络丢包或超时状态：
  - 触发断网演出时，主动关闭当前网络底层套接字；
  - 原生 `HGNetBaseSession` 随后进入重连超时状态机，触发游戏原生的断线提示界面。
- 退出演出或手动重试时安全恢复套接字收发，确保玩家可重新连入服务器。
