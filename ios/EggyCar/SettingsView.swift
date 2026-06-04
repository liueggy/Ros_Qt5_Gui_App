// SettingsView.swift — 设置页 (含手动重连)
import SwiftUI

struct SettingsView: View {
    @EnvironmentObject var app: AppState

    var body: some View {
        List {
            Section("连接") {
                HStack {
                    Text("状态")
                    Spacer()
                    switch app.client.connectionState {
                    case .disconnected:
                        Image(systemName: "circle.fill").foregroundStyle(.secondary).font(.caption2)
                        Text("未连接").foregroundStyle(.secondary)
                    case .connecting:
                        ProgressView().scaleEffect(0.7)
                        Text("连接中...")
                    case .connected:
                        Image(systemName: "checkmark.circle.fill").foregroundStyle(.green)
                        Text("已连接")
                    case .error(let msg):
                        Image(systemName: "xmark.circle.fill").foregroundStyle(.red)
                        Text(msg).lineLimit(1).font(.caption)
                    }
                }
                LabeledContent("地址", value: "\(app.config.host):\(app.config.port)")

                // 重连按钮
                Button(action: { app.manualReconnect() }) {
                    HStack {
                        Spacer()
                        Label("手动重连", systemImage: "arrow.clockwise")
                            .font(.body.bold())
                        Spacer()
                    }
                }
                .buttonStyle(.borderedProminent)

                Button(action: { app.showConnectionSheet = true }) {
                    HStack {
                        Spacer()
                        Label("修改连接", systemImage: "gear")
                            .font(.body)
                        Spacer()
                    }
                }
                .buttonStyle(.bordered)

                if app.client.connectionState == .connected {
                    Button("断开连接", role: .destructive) { app.disconnect() }
                }
            }

            Section("关于") {
                LabeledContent("版本", value: "1.0.0")
                LabeledContent("协议", value: "ROSBridge")
                LabeledContent("话题数", value: "21")
            }
        }
        .listStyle(.insetGrouped)
        .navigationTitle("设置")
    }
}
