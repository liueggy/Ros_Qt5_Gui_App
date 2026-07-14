# Eggy Robot Qt5 控制台

这是 Eggy 移动机器人项目的 Qt5 上位机。它运行在 Windows 或 Linux 电脑上，通过 ROSBridge WebSocket 连接 Firefly/RK3568 板端 ROS1 系统，用于地图、定位、导航、底盘控制、摄像头和巡检任务的可视化与操作。

本仓库是基于上游 ROS Qt GUI 项目的项目化改造版本，当前产品化重点是 **Eggy 小车 + ROS1 板端 + Qt Windows 控制端**。板端配套代码位于 [firefly-catkin-ws-backup](https://github.com/liueggy/firefly-catkin-ws-backup)。

## 当前能力

- **三种运行 profile**：建图 `mapping`、固定地图导航 `navigation`、视觉巡检 `inspection`。
- **地图与定位**：加载/保存地图、OccupancyGrid、AMCL 位姿、TF 机器人位置、激光雷达、全局/局部路径和代价地图。
- **统一任务协议**：单点导航和多点任务链统一使用 mission 请求；路线长度决定单点或多点，`inspection.enabled` 决定是否在到点后执行视觉搜索和 AI 分析。
- **巡检任务**：支持点位编辑、路线保存/加载、循环执行、是否返航和 AI 巡检开关，并显示任务进度、状态和结果。
- **底盘控制**：命令控制、摇杆控制、八方向键盘控制；Qt 应用前台任意面板均可使用 Q/W/E/A/S/D/Z/X/C 控制，文本编辑框保留正常输入行为。
- **软件急停**：按钮或空格发送锁存式 `/eggy/emergency_stop`；板端仲裁器在所有速度源之上输出零速度并取消正在执行的统一任务。软件急停不能替代硬件急停。
- **通信与诊断**：ROSBridge 断线重连、命令中心状态、能力/profile 状态、请求 ID、任务状态/结果和底盘速度仲裁状态。
- **设备数据显示**：电池、机器人轮廓、相机画面、激光雷达、地图和规划结果。

## 通信方式

在 Qt 的连接设置中选择对应通道：

| 通道 | 用途 |
| --- | --- |
| ROS1 / ROS2 | 适用于电脑本机或已配置对应 ROS 环境的开发场景 |
| ROSBridge | 通过 WebSocket 连接板端的 `rosbridge_websocket`，默认端口 `9090` |
| Tailscale ROSBridge | 与普通 ROSBridge 使用相同协议，通过 Tailscale 分配的 `100.x` 地址连接；小车可使用 4G 网络，不要求与电脑连接同一个 Wi-Fi |

板端 ROS1 启动 ROSBridge 的示例：

```bash
roslaunch rosbridge_server rosbridge_websocket.launch
```

使用 Tailscale 时，电脑和小车都加入同一个 Tailnet，并在 Qt 中填写小车的 Tailscale IP 和 `9090` 端口。网络穿透只解决连接路径，板端仍需启动 ROS、Eggy 系统和 ROSBridge。

## 跨端关键接口

Qt 不直接拼接底盘或任务实现，而是通过通信层发送稳定契约。当前主要接口包括：

```text
/eggy/mission/request       单点/多点导航与巡检任务
/eggy/mission/status        任务受理、导航、巡检、取消和错误状态
/eggy/mission/result        任务结果
/cmd_vel/manual             Qt 手动速度
/cmd_vel/navigation         普通导航速度
/cmd_vel/mission            任务链速度
/cmd_vel/safety             安全速度源
/cmd_vel                    板端唯一仲裁后的底盘速度
/eggy/emergency_stop        锁存式软件急停 Bool
/eggy/command/status        板端能力、profile 和运行状态
```

所有任务请求都应带 `schema_version`、`request_id` 和 `mission_type`。任务状态/结果通过同一 `request_id` 关联，Qt 不把 `accepted` 误认为任务完成。

## 下载 Windows 版本

前往 [Releases](https://github.com/liueggy/Ros_Qt5_Gui_App/releases) 下载最新的 `ros_qt5_gui_app_windows_x64.zip`，解压后运行 `run.bat` 或主程序。

首次使用前：

1. 确认板端 ROS、Eggy 系统节点和 ROSBridge 已启动。
2. 在 Qt 连接设置中选择 ROSBridge 或 Tailscale ROSBridge。
3. 填写板端 IP、端口和需要显示的话题。
4. 先确认命令中心状态为 ready，再发送速度或导航任务。

没有实车时，可先在 WSL2/ROS1 仿真环境中验证地图、AMCL、导航和 mission 契约；涉及底盘、相机、RKNN/NPU、Kimi 和真实急停的行为仍需实车联调。

## 从源码构建

### Windows

需要 Visual Studio C++、CMake、Ninja、Qt5 和 vcpkg。CI 使用的构建入口为：

```powershell
.\build.bat ci
```

构建完成后：

```powershell
cd build
.\start.bat
```

### Linux

需要 Qt5、CMake、Eigen、SDL2、GTest 和对应的 ROS 开发环境：

```bash
./build.sh
cd build
./start.sh
```

也可以使用 CMake 手动构建：

```bash
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

## 代码结构

```text
src/app/                    窗口、地图、速度、任务和状态交互
src/basic/                  跨层数据结构与消息 ID
src/channel/                ROS1、ROS2、ROSBridge 通信实现
src/core/                   消息总线、配置和基础框架
src/common/                 日志、工具和公共组件
src/channel/rosbridge/      WebSocket 协议、话题契约和输入校验
doc/                        使用、开发和 FAQ 文档
.github/workflows/          Windows/Linux CI 与 Windows 发布流程
```

## 安全边界

软件急停的最高优先级范围是“Qt → ROSBridge → 板端仲裁器”链路正常时的 ROS 运动控制。断网、操作系统卡死、ROS 节点退出、底盘驱动失控或电源故障时，软件指令可能无法送达；现场必须配置独立硬件急停，并在低速、可控环境中进行首次联调。

## 相关仓库

- [Eggy 板端 ROS1 工作空间](https://github.com/liueggy/firefly-catkin-ws-backup)
- [Windows 最新二进制分支](https://github.com/liueggy/Ros_Qt5_Gui_App/tree/windows-latest-bin)
- [GitHub Actions 构建记录](https://github.com/liueggy/Ros_Qt5_Gui_App/actions)

## 开源协议

本项目沿用 [MIT License](LICENSE)。
