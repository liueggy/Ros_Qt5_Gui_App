// ConnectionSheet.swift — 连接配置弹窗 (优化版)
import SwiftUI

struct ConnectionSheet: View {
    @EnvironmentObject var app: AppState
    @Environment(\.dismiss) var dismiss
    @State private var host = "192.168.31.50"
    @State private var port = "9090"

    var body: some View {
        NavigationStack {
            Form {
                Section("ROSBridge 服务器") {
                    HStack {
                        Image(systemName: "network")
                            .foregroundStyle(.secondary)
                        TextField("IP", text: $host)
                            .textContentType(.URL)
                            .autocorrectionDisabled()
                            .textInputAutocapitalization(.never)
                    }
                    HStack {
                        Image(systemName: "number")
                            .foregroundStyle(.secondary)
                        TextField("端口", text: $port)
                            .keyboardType(.numberPad)
                    }
                }
                Section {
                    Button(action: connect) {
                        HStack {
                            Spacer()
                            Image(systemName: "antenna.radiowaves.left.and.right")
                            Text("连接").bold()
                            Spacer()
                        }
                    }
                    .foregroundStyle(.white)
                    .listRowBackground(Color.blue)
                }
                Section {
                    Text("提示：确保小车 ROSBridge 已启动\n（默认端口 9090）")
                        .font(.caption2).foregroundStyle(.secondary)
                }
            }
            .navigationTitle("连接设置")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("取消") { dismiss() }
                }
            }
            .onAppear { host = app.config.host; port = "\(app.config.port)" }
        }
    }

    private func connect() {
        app.config.host = host
        app.config.port = Int(port) ?? 9090
        app.connect()
        dismiss()
    }
}
