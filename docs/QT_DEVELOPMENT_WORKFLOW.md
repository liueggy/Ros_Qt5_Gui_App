# Qt 上位机开发、构建与验收流程

本文是 `Ros_Qt5_Gui_App` 的标准 Windows 开发流程。目标是让每次源码推送只构建一次，并由同一次 GitHub Actions 运行同时生成 ZIP Artifact 和可直接拉取的 `windows-latest-bin`。

## 流程总览

```text
本地修改源码
  -> 提交并 push origin/master
  -> GitHub Actions: Windows:Build and Publish
  -> 恢复 vcpkg、sccache、Ninja build 缓存
  -> 增量构建 + CTest
  -> 上传完整 ZIP Artifact
  -> 自动更新 windows-latest-bin
  -> Ros_Qt5_Gui_App_latest_bin/update_and_run.bat
  -> 核对 BUILD_INFO.txt 并运行 EXE 验收
```

不再需要手动触发第二条二进制构建工作流。ZIP 和二进制分支来自同一个 `build/install/bin`，避免重复编译和两个产物版本不一致。

## 1. 本地修改与提交

源码仓库远端为：

```text
origin https://github.com/liueggy/Ros_Qt5_Gui_App.git
```

开始修改前确认分支和工作区：

```powershell
cd C:\Users\CL\Desktop\ROS_Qt5\Ros_Qt5_Gui_App
git status --short --branch
git pull --ff-only origin master
```

完成修改后，可以先做本地快速检查；正式交付以 GitHub Actions 结果为准。提交时只暂存本次相关文件，避免提交 `build/`、`build_output/`、日志、配置和地图等本地产物。

```powershell
git add <本次修改的文件>
git commit
git push origin master
```

推送 `master` 会自动触发 `.github/workflows/windows_build.yaml`。只修改 Markdown、`doc/**`、`docs/**` 或 `screenshots/**` 时不会触发 Windows 构建；若文档提交也需要验证当前源码，可在 Actions 页面手动运行该工作流。

## 2. GitHub Actions 构建架构

工作流名称为 **Windows:Build and Publish**，按以下顺序执行：

1. 检出本次 commit。
2. 恢复 sccache 编译缓存。
3. 恢复 vcpkg 安装依赖缓存。
4. 恢复与当前分支和构建配置兼容的 Ninja `build/` 缓存。
5. 运行 `build.bat` 完成 CMake 配置、增量编译和安装。
6. 运行 CTest；测试失败则停止发布。
7. 保存新的 Ninja build 缓存。
8. 从 `build/install/bin` 生成完整 ZIP Artifact。
9. 从同一运行目录生成二进制分支内容并 force-push 到 `windows-latest-bin`。

### 缓存职责

| 缓存 | 用途 |
| --- | --- |
| vcpkg | 避免重复下载和构建 Qt、Boost、OpenCV 等依赖 |
| sccache | 按编译输入复用 C/C++ 目标文件 |
| Ninja build | 保留依赖图、Qt UI/MOC 生成物和链接状态，实现真正的跨 Runner 增量构建 |

UI 或布局小改动通常只重新生成相关 UI 头文件、编译受影响源文件并链接；新增功能会编译新增文件和受依赖影响的目标。修改 CMake、依赖清单或构建脚本时，缓存键会变化，工作流会扩大重建范围以保证正确性。

第一次运行新工作流没有 Ninja 历史缓存，耗时接近完整构建；后续提交才会体现增量效果。GitHub 缓存只是加速手段，任何缓存缺失时仍必须能够完整构建。

## 3. 检查远端构建

打开 GitHub 仓库的 Actions 页面，找到本次 commit 对应的 **Windows:Build and Publish**，确认以下步骤全部成功：

- Build via project script
- Test
- Package
- Upload Artifact
- Publish unpacked binaries to windows-latest-bin

必须核对 Actions 显示的 commit SHA，不能只看最新一次绿色运行。

如果构建失败，先修复源码或构建配置并重新 push。不要在失败后继续拉取二进制目录，因为 `windows-latest-bin` 只会保留上一次成功版本。

## 4. 拉取最新二进制

本机验收目录：

```text
C:\Users\CL\Desktop\ROS_Qt5\Ros_Qt5_Gui_App_latest_bin
```

在 Actions 发布成功后运行：

```powershell
cd C:\Users\CL\Desktop\ROS_Qt5\Ros_Qt5_Gui_App_latest_bin
.\update_and_run.bat
```

脚本执行以下操作：

1. 临时备份本机 `config.json` 和 `state.ini`。
2. 拉取 `origin/windows-latest-bin`。
3. 使用 `git reset --hard` 将已跟踪程序文件同步到远端成功版本。
4. 恢复本机配置。
5. 调用 `run.bat` 启动 `ros_qt5_gui_app.exe`。

不要在二进制目录直接修改程序文件，因为下一次更新会覆盖已跟踪内容。地图、日志和本机配置由二进制分支 `.gitignore` 排除。

## 5. 版本核对与验收

启动前打开 `BUILD_INFO.txt`，至少核对：

- `Source commit` 与刚推送的源码 commit 完全一致；
- `Source branch` 为 `master`；
- `Workflow run` 是刚完成的 Actions 任务；
- `Target branch` 为 `windows-latest-bin`。

建议按以下顺序验收：

1. EXE 正常启动，单实例行为正确。
2. ROSBridge 能连接、断开和重连。
3. 地图、TF、激光、路径和机器人位置正常显示。
4. 遥控、急停、重定位和导航命令有效。
5. 针对本次修改执行专项测试，特别检查异常输入和断线场景。

发现问题后回到源码仓库修复并重新 push，不直接修改二进制分支。

## 6. 发布约束

- `master` 是自动构建和二进制发布源。
- `windows-latest-bin` 是 Actions 生成的交付分支，禁止人工开发或手工提交。
- 二进制分支每次由工作流 force-push，这是设计行为。
- CTest 失败、编译失败或打包失败时不得更新二进制分支。
- `BUILD_INFO.txt` 是本地 EXE 与源码版本对应关系的依据。
- 标签 `v*` 或手动运行主工作流仍可更新 `qt-windows-latest` Release；日常验收只需要二进制分支。
