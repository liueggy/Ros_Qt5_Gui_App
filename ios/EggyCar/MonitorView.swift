// MonitorView.swift — 监控页 (对齐 Qt)
import SwiftUI

struct MonitorView: View {
    @EnvironmentObject var app: AppState

    var body: some View {
        List {
            Section("机器人状态") {
                let odom = app.topics.odom
                LabeledContent("X", value: String(format: "%.3f m", odom.pose.x))
                LabeledContent("Y", value: String(format: "%.3f m", odom.pose.y))
                LabeledContent("朝向", value: String(format: "%.1f°", odom.pose.theta * 180 / .pi))
                LabeledContent("前向速度", value: String(format: "%.3f m/s", odom.twist.vx))
                LabeledContent("横向速度", value: String(format: "%.3f m/s", odom.twist.vy))
                LabeledContent("角速度", value: String(format: "%.3f rad/s", odom.twist.wz))
            }

            Section("传感器") {
                HStack {
                    Label("温度", systemImage: "thermometer.medium")
                    Spacer()
                    Text(String(format: "%.1f °C", app.topics.dht11.temperature))
                        .foregroundStyle(.red).bold().monospacedDigit()
                }
                HStack {
                    Label("湿度", systemImage: "humidity")
                    Spacer()
                    Text(String(format: "%.0f %%", app.topics.dht11.humidity))
                        .foregroundStyle(.blue).bold().monospacedDigit()
                }
                HStack {
                    Label("电池", systemImage: batteryIcon)
                    Spacer()
                    Text(String(format: "%.0f%%", app.topics.battery.percent))
                        .foregroundStyle(batteryColor).bold().monospacedDigit()
                    Text(String(format: "%.2fV", app.topics.battery.voltage))
                        .foregroundStyle(.secondary).monospacedDigit()
                }
            }

            Section("语音命令") {
                if let vc = app.topics.lastVoiceCommand,
                   Date().timeIntervalSince(vc.timestamp) < 60 {
                    LabeledContent("功能", value: vc.funcId)
                    LabeledContent("命令", value: vc.cmdId)
                    LabeledContent("时间", value: vc.timestamp.formatted(date: .omitted, time: .standard))
                } else {
                    Text("暂无语音命令").foregroundStyle(.secondary)
                }
            }

            Section("路径 / 激光") {
                LabeledContent("全局路径", value: "\(app.topics.globalPath.count) 点")
                LabeledContent("局部路径", value: "\(app.topics.localPath.count) 点")
                LabeledContent("激光帧", value: "\(app.topics.laserScan?.ranges.count ?? 0) 点")
                LabeledContent("地图尺寸",
                               value: app.topics.map.map { "\($0.width)×\($0.height)" } ?? "-")
            }

            Section("系统诊断 (\(app.topics.diagnostics.count))") {
                if app.topics.diagnostics.isEmpty {
                    Text("暂无").foregroundStyle(.secondary)
                } else {
                    // 统计
                    let ok = app.topics.diagnostics.filter { $0.level == 0 }.count
                    let warn = app.topics.diagnostics.filter { $0.level == 1 }.count
                    let err = app.topics.diagnostics.filter { $0.level >= 2 }.count
                    HStack(spacing: 16) {
                        Label("\(ok)", systemImage: "checkmark.circle").foregroundStyle(.green).font(.caption)
                        Label("\(warn)", systemImage: "exclamationmark.triangle").foregroundStyle(.orange).font(.caption)
                        Label("\(err)", systemImage: "xmark.circle").foregroundStyle(.red).font(.caption)
                    }
                    ForEach(app.topics.diagnostics.prefix(20)) { entry in
                        HStack(spacing: 6) {
                            Circle().fill(levelColor(entry.level)).frame(width: 8, height: 8)
                            VStack(alignment: .leading, spacing: 1) {
                                Text(entry.name).font(.caption).lineLimit(1)
                                if !entry.message.isEmpty {
                                    Text(entry.message).font(.caption2).foregroundStyle(.secondary).lineLimit(1)
                                }
                            }
                        }
                    }
                    if app.topics.diagnostics.count > 20 {
                        Text("还有 \(app.topics.diagnostics.count - 20) 条...")
                            .font(.caption2).foregroundStyle(.secondary)
                    }
                }
            }
        }
        .listStyle(.insetGrouped)
    }

    private var batteryIcon: String {
        let p = app.topics.battery.percent
        if p > 75 { return "battery.100" }
        if p > 50 { return "battery.75" }
        if p > 25 { return "battery.50" }
        if p > 10 { return "battery.25" }
        return "battery.0"
    }
    private var batteryColor: Color { app.topics.battery.percent > 20 ? .green : .red }
    private func levelColor(_ level: Int) -> Color {
        switch level { case 0: return .green; case 1: return .orange; case 2: return .red; default: return .gray }
    }
}
