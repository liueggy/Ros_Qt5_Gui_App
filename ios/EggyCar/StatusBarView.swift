// StatusBarView.swift — 状态栏 (电池 + 温湿度 + 语音)
import SwiftUI

struct StatusBarView: View {
    @EnvironmentObject var app: AppState

    var body: some View {
        HStack(spacing: 16) {
            // 连接状态
            Circle()
                .fill(app.client.connectionState == .connected ? .green : .red)
                .frame(width: 8, height: 8)

            // 电池
            HStack(spacing: 4) {
                Image(systemName: batteryIcon)
                    .foregroundStyle(batteryColor)
                Text(String(format: "%.0f%%", app.topics.battery.percent))
                    .font(.caption.monospacedDigit())
                Text(String(format: "%.1fV", app.topics.battery.voltage))
                    .font(.caption2).foregroundStyle(.secondary)
            }

            Divider().frame(height: 16)

            // 温湿度
            HStack(spacing: 8) {
                Label(String(format: "%.1f°C", app.topics.dht11.temperature),
                      systemImage: "thermometer.medium")
                    .font(.caption.monospacedDigit())
                    .foregroundStyle(.red)

                Label(String(format: "%.0f%%", app.topics.dht11.humidity),
                      systemImage: "humidity")
                    .font(.caption.monospacedDigit())
                    .foregroundStyle(.blue)
            }

            // 语音通知
            if let vc = app.topics.lastVoiceCommand,
               Date().timeIntervalSince(vc.timestamp) < 5 {
                Text("🔊 \(vc.funcId),\(vc.cmdId)")
                    .font(.caption2)
                    .padding(.horizontal, 6)
                    .padding(.vertical, 2)
                    .background(.orange.opacity(0.2), in: Capsule())
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 6)
        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 10))
    }

    private var batteryIcon: String {
        let p = app.topics.battery.percent
        if p > 75 { return "battery.100" }
        if p > 50 { return "battery.75" }
        if p > 25 { return "battery.50" }
        if p > 10 { return "battery.25" }
        return "battery.0"
    }

    private var batteryColor: Color {
        app.topics.battery.percent > 20 ? .green : .red
    }
}
