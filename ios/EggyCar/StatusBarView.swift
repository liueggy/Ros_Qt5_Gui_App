// StatusBarView.swift — 状态栏
import SwiftUI

struct StatusBarView: View {
    @EnvironmentObject var app: AppState

    var body: some View {
        HStack(spacing: 10) {
            // 连接指示
            Circle()
                .fill(connColor)
                .frame(width: 7, height: 7)

            // 电池
            HStack(spacing: 3) {
                Image(systemName: batteryIcon).foregroundStyle(batteryTint).font(.caption)
                Text(String(format: "%.0f%%", app.topics.battery.percent))
                    .font(.caption.monospacedDigit())
            }

            Divider().frame(height: 14)

            // 温湿度
            HStack(spacing: 6) {
                HStack(spacing: 2) {
                    Image(systemName: "thermometer.medium").font(.caption2).foregroundStyle(.red)
                    Text(String(format: "%.1f°C", app.topics.dht11.temperature))
                        .font(.caption.monospacedDigit())
                }
                HStack(spacing: 2) {
                    Image(systemName: "drop").font(.caption2).foregroundStyle(.blue)
                    Text(String(format: "%.0f%%", app.topics.dht11.humidity))
                        .font(.caption.monospacedDigit())
                }
            }

            // 语音通知
            if let vc = app.topics.lastVoiceCommand,
               Date().timeIntervalSince(vc.timestamp) < 5 {
                Text("🔊 \(vc.funcId),\(vc.cmdId)")
                    .font(.caption2).padding(.horizontal, 4).padding(.vertical, 1)
                    .background(.orange.opacity(0.2), in: Capsule())
            }
        }
        .padding(.horizontal, 8).padding(.vertical, 4)
        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 8))
    }

    private var connColor: Color {
        switch app.client.connectionState {
        case .connected: return .green
        case .connecting: return .yellow
        case .error: return .red
        case .disconnected: return .gray
        }
    }
    private var batteryIcon: String {
        let p = app.topics.battery.percent
        if p > 75 { return "battery.100" }
        if p > 50 { return "battery.75" }
        if p > 25 { return "battery.50" }
        if p > 10 { return "battery.25" }
        return "battery.0"
    }
    private var batteryTint: Color { app.topics.battery.percent > 20 ? .green : .red }
}
