// SettingsView.swift — 设置页
import SwiftUI

struct SettingsView: View {
    @EnvironmentObject var app: AppState

    var body: some View {
        List {
            // 连接状态
            Section("连接") {
                HStack {
                    Text("状态")
                    Spacer()
                    switch app.client.connectionState {
                    case .disconnected:
                        Text("未连接").foregroundStyle(.secondary)
                    case .connecting:
                        ProgressView().scaleEffect(0.8)
                        Text("连接中...")
                    case .connected:
                        Image(systemName: "checkmark.circle.fill").foregroundStyle(.green)
                        Text("已连接")
                    case .error(let msg):
                        Image(systemName: "xmark.circle.fill").foregroundStyle(.red)
                        Text(msg).lineLimit(1)
                    }
                }

                LabeledContent("ROSBridge", value: "\(app.config.host):\(app.config.port)")

                Button("修改连接") {
                    app.showConnectionSheet = true
                }

                if app.client.connectionState == .connected {
                    Button("断开连接", role: .destructive) {
                        app.disconnect()
                    }
                }
            }

            // 关于
            Section("关于") {
                LabeledContent("版本", value: "1.0.0")
                LabeledContent("协议", value: "ROSBridge WebSocket")
                LabeledContent("ROS 话题数", value: "21")
            }
        }
        .listStyle(.insetGrouped)
    }
}
