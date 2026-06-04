# Eggy ROS Car iOS App — 移植计划

## 一、Qt 客户端功能全景

### 1. 通信层 (21 个 ROS 话题)

| 方向 | 话题 | 消息类型 | 功能 |
|---|---|---|---|
| **订阅** | `/map` | OccupancyGrid | 占据栅格地图 |
| **订阅** | `/odom` | Odometry | 里程计/机器人位姿 |
| **订阅** | `/scan` | LaserScan | 激光扫描点云 |
| **订阅** | `/plan` | Path | 全局规划路径 |
| **订阅** | `/local_plan` | Path | 局部规划路径 |
| **订阅** | `/global_costmap/costmap` | OccupancyGrid | 全局代价地图 |
| **订阅** | `/local_costmap/costmap` | OccupancyGrid | 局部代价地图 |
| **订阅** | `/local_costmap/published_footprint` | PolygonStamped | 机器人足迹 |
| **订阅** | `/battery` | BatteryState | 电池电量/电压 |
| **订阅** | `/diagnostics` | DiagnosticArray | 系统诊断信息 |
| **订阅** | `/camera/front/image/compressed` | CompressedImage | 摄像头画面 |
| **订阅** | `/stm32/dht11/temperature` | Float32 | 温度 |
| **订阅** | `/stm32/dht11/humidity` | Float32 | 湿度 |
| **订阅** | `/stm32/voice_command` | String | 语音命令 |
| **订阅** | `/eggy/command/response` | String | 命令中心响应 |
| **订阅** | `/eggy/command/status` | String | 命令中心状态 |
| **订阅** | `/map/topology` | String | 拓扑地图 |
| **发布** | `/goal_pose` | PoseStamped | 导航目标点 |
| **发布** | `/initialpose` | PoseStamped | 重定位位姿 |
| **发布** | `/cmd_vel` | Twist | 速度控制 |
| **发布** | `/eggy/command/request` | String | 命令中心请求 |
| **发布** | `/map/topology/update` | String | 拓扑地图更新 |
| 双向 | `/tf`, `/tf_static` | TFMessage | 坐标变换 |

### 2. 显示层 (地图渲染)

- 占据栅格地图渲染（PGM → Canvas 绘制）
- 机器人位姿箭头
- 激光扫描点云
- 全局/局部路径线
- 全局/局部代价地图（半透明叠加）
- 机器人足迹多边形
- 摄像头实时画面
- 拓扑地图节点和连线
- 地图编辑（画笔、橡皮、直线、区域、拓扑连线）

### 3. 控制层

- 虚拟摇杆（8 方向 + 停止）
- 方向键控制（8 方向按钮 + 键盘快捷键）
- 线速度/角速度滑块
- 停止按钮
- 点击地图设目标点
- 导航目标点列表

### 4. 监控层

- 电池电量条 + 电压数字
- 温湿度实时显示
- 语音命令通知弹窗
- 诊断树（树形结构，分级过滤，搜索）
- 速度仪表盘（Speed + Gear）

### 5. 命令中心

- 预设命令（状态查询、清代价地图、摄像头开关、建图开关）
- 参数读取/写入（dynamic_reconfigure）
- 导航速度档位（低速/均衡/快速/高速）
- 本地地图上传 + AMCL 切换
- 自定义 JSON 发送/响应日志

### 6. 设置

- ROSBridge IP/端口配置
- 图层显隐开关 + 话题名自定义
- 摄像头话题配置
- 机器人外形配置（多边形/椭圆、颜色、透明度）
- 地图路径配置

---

## 二、iOS App 架构设计

### 技术栈

| 层 | 技术选型 | 说明 |
|---|---|---|
| **UI 框架** | SwiftUI + SwiftUIX | 声明式 UI，SwiftUIX 补充原生缺失组件 |
| **ROS 通信** | Starscream (WebSocket) + 自写协议层 | 连接 ROSBridge websocket 9090 |
| **地图渲染** | MetalKit / Core Graphics | 高性能 2D 栅格地图渲染 |
| **状态管理** | Combine + ObservableObject | 响应式数据流 |
| **架构模式** | MVVM | Model-View-ViewModel |
| **网络层** | URLConnection / async-await | 配置下载等 |

### 项目结构

```
EggyCar/
├── App/                      # App 入口
│   ├── EggyCarApp.swift
│   └── AppState.swift        # 全局状态容器
├── Communication/            # ROSBridge 通信层
│   ├── ROSBridgeClient.swift       # WebSocket 连接管理
│   ├── ROSBridgeProtocol.swift     # 消息编解码 (JSON)
│   ├── TopicManager.swift          # 话题订阅/发布管理
│   └── MessageTypes.swift          # ROS 消息类型定义
├── Models/                   # 数据模型
│   ├── OccupancyGrid.swift         # 占据栅格地图
│   ├── Pose.swift                  # 位姿/四元数
│   ├── Path.swift                  # 路径
│   ├── LaserScan.swift             # 激光扫描
│   ├── Diagnostic.swift            # 诊断信息
│   └── ...
├── ViewModels/               # MVVM ViewModel
│   ├── MapViewModel.swift          # 地图+显示
│   ├── ControlViewModel.swift      # 速度控制
│   ├── StatusViewModel.swift       # 状态监控
│   ├── CommandCenterViewModel.swift # 命令中心
│   └── SettingsViewModel.swift     # 设置
├── Views/                    # SwiftUI 视图
│   ├── MainTabView.swift           # 主 Tab 页
│   ├── Map/
│   │   ├── MapCanvasView.swift     # 地图 Canvas 渲染
│   │   ├── LayerOverlay.swift      # 图层叠加
│   │   └── MapEditToolbar.swift    # 地图编辑工具
│   ├── Control/
│   │   ├── JoystickView.swift      # 虚拟摇杆
│   │   ├── SpeedControlView.swift  # 速度滑块
│   │   └── NavGoalListView.swift   # 目标点列表
│   ├── Monitor/
│   │   ├── StatusBarView.swift     # 状态栏 (电池+温湿度+语音)
│   │   ├── DiagnosticView.swift    # 诊断树
│   │   ├── CameraView.swift        # 摄像头画面
│   │   └── DashboardView.swift     # 速度仪表盘
│   ├── CommandCenter/
│   │   ├── CommandCenterView.swift # 命令中心主页
│   │   └── ParamEditorView.swift   # 参数编辑器
│   └── Settings/
│       ├── ConnectionSettingsView.swift
│       └── DisplaySettingsView.swift
├── Utils/                    # 工具类
│   ├── CoordinateTransform.swift   # ROS↔屏幕坐标变换
│   ├── ColorMap.swift              # 代价地图颜色映射
│   └── UserDefaults+.swift
└── Resources/                # 资源
    ├── Assets.xcassets
    └── Localizable.strings
```

---

## 三、分阶段实施计划

### Phase 1 — 通信 + 基础地图 (1~2 周)

**目标**: 能连接 ROSBridge、看到地图和机器人位置

- [ ] 项目初始化 (SwiftUI App, 依赖配置)
- [ ] ROSBridge WebSocket 客户端 (Starscream)
- [ ] ROSBridge 协议层 (subscribe/unpublish/publish JSON 编解码)
- [ ] 占据栅格地图解码 (nav_msgs/OccupancyGrid)
- [ ] 地图 Canvas 渲染 (Core Graphics)
- [ ] 机器人位姿箭头渲染
- [ ] 连接设置页 (IP/端口输入, 连接状态)
- [ ] 基础 Tab 结构 (地图 / 控制 / 监控 / 设置)

**里程碑**: 打开 App → 输入 IP → 看到地图和机器人位置 ✅

### Phase 2 — 图层 + 状态监控 (1 周)

**目标**: 地图上叠加所有图层，状态栏显示实时信息

- [ ] 激光扫描点云渲染
- [ ] 全局/局部路径线渲染
- [ ] 代价地图渲染 (半透明颜色映射)
- [ ] 机器人足迹多边形
- [ ] TF 坐标变换处理 (map→odom→base_link)
- [ ] 电池电量显示
- [ ] DHT11 温湿度显示
- [ ] 语音命令通知 Banner
- [ ] 手势缩放/平移地图

**里程碑**: 地图全图层显示正常，状态栏实时更新 ✅

### Phase 3 — 遥控 + 导航 (1 周)

**目标**: 能远程控制小车，点击地图导航

- [ ] 虚拟摇杆 (SwiftUI 手势实现)
- [ ] 线速度/角速度滑块
- [ ] 速度控制发布 (/cmd_vel)
- [ ] 点击地图设导航目标 (/goal_pose)
- [ ] 导航目标点列表
- [ ] 停止按钮 (紧急)

**里程碑**: 摇杆控制小车移动，点击地图导航 ✅

### Phase 4 — 命令中心 + 诊断 (0.5~1 周)

**目标**: 完整的命令控制和系统诊断

- [ ] 命令中心预设命令
- [ ] 参数读取/写入
- [ ] 速度档位切换
- [ ] 自定义 JSON 发送
- [ ] 诊断树 (按级别分组，可展开/过滤)
- [ ] 摄像头实时画面

**里程碑**: 命令中心功能与 Qt 端对齐 ✅

### Phase 5 — 高级功能 + 打磨 (1 周)

**目标**: 完善体验，准备发布

- [ ] 拓扑地图显示/编辑
- [ ] 地图编辑 (画笔/橡皮/直线) — 可选，优先级低
- [ ] 本地地图上传
- [ ] iPad 分屏适配
- [ ] 深色模式适配
- [ ] 设置持久化 (UserDefaults)
- [ ] 连接断开自动重连
- [ ] 启动页 + App Icon

---

## 四、功能优先级矩阵

| 功能 | 优先级 | 复杂度 | 说明 |
|---|---|---|---|
| ROSBridge 连接 | P0 | 中 | 一切的基础 |
| 占据栅格地图 | P0 | 高 | 核心显示，Canvas/Metal 渲染 |
| 机器人位姿 | P0 | 低 | 箭头叠加 |
| 激光扫描 | P0 | 低 | 点叠加 |
| 电池/温湿度 | P0 | 低 | 文字显示 |
| 速度控制 (摇杆) | P0 | 中 | 触摸手势 |
| 导航目标点 | P0 | 中 | 点击地图 + 发布 |
| 路径/代价地图 | P1 | 中 | 半透明叠加 |
| 诊断 | P1 | 中 | 树形 UI |
| 命令中心 | P1 | 中 | JSON 通信 |
| 摄像头画面 | P1 | 低 | 图片解码显示 |
| 语音通知 | P1 | 低 | Banner |
| 拓扑地图 | P2 | 高 | 复杂数据结构 |
| 地图编辑 | P2 | 高 | 高级功能 |

---

## 五、关键技术决策

### 1. ROSBridge 协议

Qt 端用的是 `rosbridge2cpp` (C++)，iOS 端需要自己实现 ROSBridge JSON 协议：

```swift
// 订阅
{"op": "subscribe", "topic": "/map", "type": "nav_msgs/OccupancyGrid", "throttle_rate": 100}
// 发布
{"op": "publish", "topic": "/cmd_vel", "msg": {"linear": {"x": 0.5}, "angular": {"z": 0.3}}}
// 取消订阅
{"op": "unsubscribe", "topic": "/map"}
```

### 2. 地图渲染方案

| 方案 | 优点 | 缺点 |
|---|---|---|
| **Core Graphics (CGContext)** | 简单，足够 1Hz 地图 | 大地图可能卡 |
| **Metal** | 高性能，可 shader 加速 | 复杂度高 |
| **SpriteScene** | 简单易用 | 灵活度低 |

**推荐**: Phase 1 用 Core Graphics，Phase 2 如果性能不够再迁移 Metal。

### 3. 网络库选择

**Starscream** — 最成熟的 iOS WebSocket 库，支持 TLS、重连、ping/pong，Swift 原生 API。

### 4. SwiftUIX 的价值

- `ActivityIndicator` (原生无)
- `SearchBar` (早期 iOS 无)
- `TextView` (多行文本输入)
- 更好的 `List` 和 `ScrollView` 行为
- `CocoaList` (UICollectionView wrapper)
- 适配低版本 iOS 的 backport

---

## 六、与 Qt 端的对应关系

| Qt 组件 | iOS 对应 |
|---|---|
| `DisplayManager` | `MapViewModel` + `MapCanvasView` |
| `RosbridgeComm` | `ROSBridgeClient` + `TopicManager` |
| `SpeedCtrlWidget` | `JoystickView` + `SpeedControlView` |
| `DashBoard` | `DashboardView` (SwiftUI Canvas) |
| `NavGoalTableView` | `NavGoalListView` |
| `CommandCenterWidget` | `CommandCenterView` |
| `DiagnosticDockWidget` | `DiagnosticView` |
| `DisplayConfigWidget` | `DisplaySettingsView` |
| `JoyStick` | `JoystickView` (SwiftUI 手势) |
| `mainwindow.cpp` 状态栏 | `StatusBarView` |
