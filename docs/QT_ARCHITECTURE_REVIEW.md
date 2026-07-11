# Qt 架构、性能与界面审阅报告

> 审阅日期：2026-07-11。范围：`src/` 下 153 个 C++/Qt 文件。方法：CodeGraph 全量索引、Qt 确定性规则扫描、六方向深度审查、QWidget UI 审计。本文是只读审阅结论，不表示问题已经修复。

## Qt Code Review Report

**Scope**: files: `src/**/*.cpp`, `src/**/*.cc`, `src/**/*.h`, `src/**/*.hpp`  
**Files reviewed**: 153  
**Issues found**: 14（4 个自研代码 lint 重点，10 个深度分析确认项）

---

### Lint findings

确定性扫描产生 76 条原始提示。Vendored RapidJSON 和 Easylogging++ 的提示不纳入产品整改；`QString::arg()` 多参数重载等已人工确认的扫描器误报也不纳入问题数。以下是对自研代码有直接维护价值的重点。

#### [L-001] Getter 命名不符合 Qt API 习惯

- **File**: `src/app/widgets/joystick.h:53`
- **Rule**: API-5
- **Finding**: 普通只读 getter 使用 `getKeyNum` 前缀；Qt API 通常直接使用名词形式。
- **Mitigation**: 在兼容窗口内引入无 `get` 的 const getter，并逐步迁移调用方。

#### [L-002] 只读遍历可能触发 Qt 隐式共享容器分离

- **File**: `src/basic/map/topology_map.h:104,135`
- **Rule**: PAT-12
- **Finding**: 非 const range-for 引用用于读取 Qt/共享数据时可能触发 detach。
- **Mitigation**: 对只读路径使用 `const auto&`/`std::as_const`；确实需要修改时保留非 const 并加意图注释。

#### [L-003] ROSBridge 只读遍历可能产生无谓复制或 detach

- **File**: `src/channel/rosbridge/rosbridge_comm.cpp:131`
- **Rule**: PAT-12
- **Finding**: 通信热路径使用非 const range-for 引用。
- **Mitigation**: 确认循环不修改元素后改为 const 遍历，并用 profiler 对比高频消息处理成本。

#### [L-004] 本地时间用于日志/状态路径

- **File**: `src/app/mainwindow.cpp:2004,2077`; `src/app/widgets/command_center_widget.cpp:455`; `src/app/widgets/terminal_widget.cpp:119`
- **Rule**: DEP-11
- **Finding**: `QDateTime::currentDateTime()` 受时区/DST 影响，且比 UTC 获取更昂贵。
- **Mitigation**: 内部时间戳统一保存 UTC，仅在显示边界按 `QLocale` 转本地时间。

---

### Deep analysis findings

#### [D-001] ROSBridge 共享传输状态存在数据竞争和 UAF 风险

- **File**: `src/channel/rosbridge/rosbridge_comm.cpp:168-219,509-616`
- **Category**: Thread Safety
- **Confidence**: 96/100
- **Finding**: `websocket_connection_`、`ros_bridge_`、订阅/发布容器在连接线程、重连线程、处理线程和 socket 回调线程间读写；只有状态标志和局部重连逻辑受同步保护。并发 reset 与读取可形成数据竞争或 use-after-free。
- **Trace**: `Start()` 启动 `ConnectAsync`；错误回调可启动 `ReconnectLoop`；`Stop()`/重连路径清理 transport；`Process()` 同时解引用 `ros_bridge_`。
- **Mitigation**: 首选单一 I/O 所有者线程和事件队列；否则以同一 mutex 覆盖全部 transport 指针与容器，并在 teardown 前停止生产者、join 线程，再释放资源。

#### [D-002] 异步地图渲染捕获裸 this

- **File**: `src/app/display/display_occ_map.cpp:46-89`
- **Category**: Thread Safety
- **Confidence**: 93/100
- **Finding**: 未跟踪的 `QtConcurrent::run` 捕获 `this`，计算完成后又排队调用该对象；对象提前析构会留下悬空接收者/捕获。并发地图任务还可能乱序完成，以旧地图覆盖新地图。
- **Trace**: 订阅触发 `ParseOccupyMap()`，后台任务捕获 `this`，没有持有 `QFuture`、取消/等待机制或 generation 检查。
- **Mitigation**: 由对象持有 future/watcher，析构时取消并等待；使用 `QPointer` 防护，同时增加递增 generation 丢弃过期结果。

#### [D-003] 导航任务表模型泄漏

- **File**: `src/app/widgets/nav_goal_table_view.cpp:15-18,49`
- **Category**: Ownership & Lifecycle
- **Confidence**: 98/100
- **Finding**: `QStandardItemModel` 无 parent，`setModel()` 不转移所有权，析构为空。
- **Trace**: 构造函数 `new QStandardItemModel()` -> `setModel()`；没有 delete 或 QObject parent。
- **Mitigation**: 以 `this` 为 parent，或使用明确的 RAII 所有权。

#### [D-004] ROS1 TF listener 泄漏

- **File**: `src/channel/ros1/rosnode.cpp:56,187`
- **Category**: Ownership & Lifecycle
- **Confidence**: 99/100
- **Finding**: `init()` 分配 `tf::TransformListener`，析构未释放；反复关闭/打开通道会累积。
- **Trace**: `ChannelManager::CloseChannel()` 删除 channel 对象，但 `RosNode::~RosNode()` 为空。
- **Mitigation**: 使用 `std::unique_ptr`，或在析构/Stop 中释放；Start 重入前清理旧状态。

#### [D-005] ROS2 executor 泄漏且可被重复启动覆盖

- **File**: `src/channel/ros2/rclcomm.cpp:43-48,211-214`
- **Category**: Ownership & Lifecycle
- **Confidence**: 99/100
- **Finding**: `MultiThreadedExecutor` 由裸指针持有，默认析构，Stop 只 shutdown，未 cancel/remove/reset。
- **Trace**: Start 分配 executor；Process 依赖它；关闭通道删除 RclComm 时无释放路径。
- **Mitigation**: 改为 `unique_ptr`；Stop 按 cancel、remove_node、reset 顺序完成收尾，并防止重复 Start。

#### [D-006] 高频 display getter 缺少 const 且返回字符串副本

- **File**: `src/app/display/virtual_display.h:165,173`; `src/app/display/virtual_display.cpp:93`
- **Category**: API & C++ Correctness
- **Confidence**: 88/100
- **Finding**: `GetDisplayName/GetDisplayType` 是只读操作，但方法非 const、按值返回 `std::string`；scene/factory/topology 热路径反复复制。
- **Trace**: CodeGraph 显示两个 getter 被多处显示管理路径调用。
- **Mitigation**: 方法标记 const，并在生命周期允许时返回 `const std::string&`；只在跨所有权边界复制。

#### [D-007] 配置写入失败可能被报告为成功

- **File**: `src/common/config/config_manager.cc:9-24,68-72`
- **Category**: Error Handling
- **Confidence**: 98/100
- **Finding**: 写入只验证文件打开，未检查 write/flush/close；`StoreConfigUnlocked()` 又无条件返回 true。
- **Trace**: `StoreConfig()` -> `StoreConfigUnlocked()` -> `writeStringToFile()`，失败状态没有完整向上传递。
- **Mitigation**: 检查完整流状态并传播 false；配置持久化改为同目录临时文件写入、flush 后原子替换。

#### [D-008] 任务链保存失败仍显示成功

- **File**: `src/app/mainwindow.cpp:1368-1373`
- **Category**: Error Handling
- **Confidence**: 99/100
- **Finding**: UI 忽略 `SaveTaskChain()` 的 bool 结果，无条件显示保存成功。
- **Trace**: 保存按钮 lambda -> `SaveTaskChain()` -> `writeStringToFile()`；返回值在 UI 边界丢失。
- **Mitigation**: 分支处理成功/失败，错误提示包含路径、原因和恢复建议。

#### [D-009] 导入任务链失败会先清空现有内容

- **File**: `src/app/widgets/nav_goal_table_view.cpp:140-172`; `src/app/mainwindow.cpp:1346-1355`
- **Category**: Error Handling
- **Confidence**: 94/100
- **Finding**: 在验证文件和 JSON 前先清空模型；解析失败返回 false，但调用者忽略，用户数据从当前视图消失且无提示。
- **Trace**: `LoadTaskChain()` 首先 `removeRows()`，随后才读文件/反序列化。
- **Mitigation**: 先解析到临时 `TaskChain` 并完整验证，成功后一次性替换模型；调用方处理 false。

#### [D-010] MessageBus 对每个订阅者复制大型消息

- **File**: `src/core/framework/message_bus.h:118-143`
- **Category**: Performance & Quality
- **Confidence**: 93/100
- **Finding**: `Publish` 为每个订阅者分别 `make_shared<T>(data)`；地图、激光、图像相关大消息成本随订阅数线性增加。
- **Trace**: `PUBLISH` -> `MessageBus::Publish` -> 循环中为每个 callback 建立 payload 副本 -> GUI 队列执行。
- **Mitigation**: 每次发布只创建一个 `shared_ptr<const T>` 不可变快照供所有订阅者共享，或让调用方直接发布不可变 shared_ptr。

---

### Investigation targets (human verification needed)

#### [I-001] MainWindow 是二次开发冲突热点

- **File**: `src/app/mainwindow.cpp:484-2210`
- **Category**: Performance & Quality
- **Confidence**: 79/100
- **Finding**: 单文件约 2200 行，同时承担通道连接、消息解析、Dock 构建、导航、重定位、巡检、图像和状态持久化。
- **Unverified because**: 是否拆分需要结合团队变更频率与功能路线判断。
- **How to verify**: 统计近 20 次功能提交对 MainWindow 的触碰率、冲突率和测试覆盖，再确定 feature controller/presenter 边界。

#### [I-002] 平滑图像模式可能反复执行高成本缩放

- **File**: `src/app/widgets/ratio_layouted_frame.cpp:137-155`
- **Category**: Performance & Quality
- **Confidence**: 76/100
- **Finding**: `smoothImage_` 启用时可能在每次 paintEvent 做 SmoothTransformation 和分配。
- **Unverified because**: 当前默认 false，静态调用链未确认产品运行时会开启。
- **How to verify**: 实际开启对应显示选项，用 Qt Creator Analyzer/采样 profiler 观察 paintEvent；若为热点，按源图 revision + 目标 size 缓存缩放结果。

#### [I-003] 跨线程 QTimer::singleShot 的 Qt5 基线语义

- **File**: `src/core/framework/callback_executor.h:18-28`
- **Category**: Thread Safety
- **Confidence**: 74/100
- **Finding**: 使用带 context 的 `QTimer::singleShot` 从 std::thread 派发 GUI 回调。
- **Unverified because**: 需结合最低 Qt5 版本验证 functor overload 的队列和退出期取消语义。
- **How to verify**: 在最低支持 Qt5 上做工作线程发布、应用退出竞态测试；若不确定，统一使用显式 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`。

---

### Summary

| Category | Lint | Deep | Investigate | Total |
| --- | ---: | ---: | ---: | ---: |
| Model Contracts | 0 | 0 | 0 | 0 |
| Ownership & Lifecycle | 0 | 3 | 0 | 3 |
| Thread Safety | 0 | 2 | 1 | 3 |
| API & C++ Correctness | 1 | 1 | 0 | 2 |
| Error Handling | 0 | 3 | 0 | 3 |
| Performance & Quality | 3 | 1 | 2 | 6 |
| **Total** | **4** | **10** | **3** | **17** |

置信度低于 60 的候选已抑制。60–79 仅列为人工调查项。

## UI/UX 与布局审计

### 已有良好基础

- `ui_style.h/.cpp` 已建立角色化颜色、字号、间距、圆角和控件高度 token，避免了完全散乱的 QSS。
- 主窗口采用 Advanced Docking System，面板可以重排、浮动和持久化，适合运维类桌面工具。
- 主体最小窗口为 960×640，并根据屏幕可用区域居中/最大化，具备基本桌面适配能力。
- 关键按钮普遍有 hover/pressed/focus 状态，主字号基线为 16px。

### Critical

#### [UI-C01] 物理控制缺少统一安全交互契约

机器人遥控、导航、重定位和巡检会触发真实执行器，但确认、执行中、超时、未确认、撤销/急停反馈分散在不同 widget 和 MainWindow 分支中。应建立统一 `CommandState`（idle/pending/acknowledged/running/succeeded/failed/timed-out/cancelled），所有危险动作必须显示命令是否已被板端确认，急停保持全局可见且键盘可达。

#### [UI-C02] 无障碍元数据基本缺失

源码未发现系统性 `accessibleName/accessibleDescription` 设置。图标按钮、地图工具、窗口控制和状态色块对屏幕阅读器不可解释。应为所有无文字交互控件设置可访问名称/描述，检查 Tab 顺序和可见焦点，并确保连接、告警、成功状态不是仅靠颜色表达。

### Warning

#### [UI-W01] MainWindow 动态拼装 UI，组件边界不稳定

`mainwindow.ui` 基本只有空布局，实际 UI 在 `MainWindow::setupUi()` 中构建 700+ 行。新增组件通常需要同时修改创建、Dock 放置、信号连接、状态恢复和成员字段，容易产生合并冲突。建议将每个 feature panel 封装为独立 QWidget + controller，并用声明式 `DockDescriptor` 注册标题、默认区域、最小尺寸、可见性和持久化 key。

#### [UI-W02] 固定像素尺寸较多

状态 pill、工具按钮、表格列、设置导航和多个标签使用 `setFixedSize/Width/Height`。在 Windows 125%/150% DPI、系统大字体或较长翻译文本下可能截断。仅图标和确需固定的仪表可保留固定尺寸；文字控件改用 sizePolicy、minimumSizeHint、layout stretch 和基于字体度量的最小宽度。

#### [UI-W03] 样式 token 尚未完全收口

虽然已有 Palette，但 `ui_style.cpp` 和业务 widget 仍存在硬编码 hex、rgba、圆角和字号。应禁止业务层新增原始颜色；补齐 overlay、selected、focus、AI uncertainty、critical alarm 等语义 token，并提供 light/dark 两套 palette 的切换接口。

#### [UI-W04] Dock 布局版本与功能注册耦合

持久化布局会受到 dock objectName、插入顺序和组件增减影响。已有 layout version 是正确方向，但应把 dock ID 设为稳定常量，为缺失/新增 dock 提供迁移和默认布局重建，避免新功能上线后被旧 state 隐藏。

### Opportunity

- 将 `DisplayConfigWidget` 的长页面拆为连接、显示、地图、图像、主题等独立设置页，每页可单独测试和扩展。
- 为耗时连接、地图加载、巡检/AI 阶段提供非阻塞、持续可见的进度与取消入口；AI 结果明确标注置信度和“需复核”。
- 建立 UI 自动化基线：至少覆盖 960×640、1920×1080@100%、1920×1080@150%、大字体，以及关键 dock 的保存/恢复截图。
- 为 Widget 工厂、Dock 注册、消息到 ViewModel/Presenter 的映射建立测试，降低新增组件对 MainWindow 的直接修改量。

## 推荐治理顺序

1. **P0 正确性**：修复 D-001、D-002 的并发/UAF；补齐危险命令确认与 ACK 状态。
2. **P1 生命周期与数据安全**：修复 D-003～D-005、D-007～D-009，并增加失败路径测试。
3. **P1 性能**：让 MessageBus 共享不可变 payload；用 profiler 验证地图、激光、图像刷新。
4. **P2 架构**：先提取 `ConnectionController`、`InspectionController`、`DockRegistry`，逐步缩小 MainWindow；不要一次性重写。
5. **P2 UI 规范**：清理固定尺寸和业务层硬编码 QSS，补齐 accessibility、DPI/大字体/键盘测试。

## CodeGraph 基线

本次已执行 `codegraph index .` 全量重建：157 个文件、4,263 个节点、9,243 条边，数据库约 11.98 MB。后续小改动使用 `codegraph sync .`；大规模移动/重构后重新执行 `codegraph index .`。
