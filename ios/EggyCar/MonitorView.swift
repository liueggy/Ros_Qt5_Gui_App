// MonitorView.swift — 监控页
import SwiftUI

struct MonitorView: View {
    @EnvironmentObject var app: AppState

    var body: some View {
        List {
            // 机器人状态
            Section("机器人") {
                let odom = app.topics.odom
                LabeledContent("X", value: String(format: "%.3f m", odom.pose.x))
                LabeledContent("Y", value: String(format: "%.3f m", odom.pose.y))
                LabeledContent("朝向", value: String(format: "%.2f°", odom.pose.theta * 180 / .pi))
                LabeledContent("线速度", value: String(format: "%.3f m/s", odom.twist.vx))
                LabeledContent("角速度", value: String(format: "%.3f rad/s", odom.twist.wz))
            }

            // 传感器
            Section("传感器") {
                LabeledContent("温度", value: String(format: "%.1f °C", app.topics.dht11.temperature))
                LabeledContent("湿度", value: String(format: "%.1f %%", app.topics.dht11.humidity))
                LabeledContent("电池电量", value: String(format: "%.0f%%", app.topics.battery.percent))
                LabeledContent("电池电压", value: String(format: "%.2f V", app.topics.battery.voltage))
            }

            // 语音命令
            Section("语音命令") {
                if let vc = app.topics.lastVoiceCommand {
                    LabeledContent("功能ID", value: vc.funcId)
                    LabeledContent("命令ID", value: vc.cmdId)
                    LabeledContent("时间", value: vc.timestamp.formatted(date: .omitted, time: .standard))
                } else {
                    Text("暂无语音命令").foregroundStyle(.secondary)
                }
            }

            // 诊断
            Section("系统诊断 (\(app.topics.diagnostics.count))") {
                if app.topics.diagnostics.isEmpty {
                    Text("暂无诊断数据").foregroundStyle(.secondary)
                } else {
                    ForEach(app.topics.diagnostics) { entry in
                        HStack {
                            Circle()
                                .fill(levelColor(entry.level))
                                .frame(width: 8, height: 8)
                            VStack(alignment: .leading, spacing: 2) {
                                Text(entry.name).font(.caption)
                                if !entry.message.isEmpty {
                                    Text(entry.message).font(.caption2)
                                        .foregroundStyle(.secondary)
                                }
                            }
                            Spacer()
                            Text(entry.levelName)
                                .font(.caption2.bold())
                                .foregroundStyle(levelColor(entry.level))
                        }
                    }
                }
            }

            // 路径信息
            Section("路径") {
                LabeledContent("全局路径点数", value: "\(app.topics.globalPath.count)")
                LabeledContent("局部路径点数", value: "\(app.topics.localPath.count)")
                LabeledContent("激光扫描点数", value: "\(app.topics.laserScan?.ranges.count ?? 0)")
            }
        }
        .listStyle(.insetGrouped)
    }

    private func levelColor(_ level: Int) -> Color {
        switch level {
        case 0: return .green
        case 1: return .orange
        case 2: return .red
        case 3: return .gray
        default: return .gray
        }
    }
}
