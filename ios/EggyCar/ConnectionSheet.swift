// ConnectionSheet.swift — 连接配置弹窗
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
                    TextField("IP 地址", text: $host)
                        .textContentType(.URL)
                        .autocorrectionDisabled()
                        .textInputAutocapitalization(.never)
                    TextField("端口", text: $port)
                        .keyboardType(.numberPad)
                }

                Section {
                    Button {
                        app.config.host = host
                        app.config.port = Int(port) ?? 9090
                        app.connect()
                        dismiss()
                    } label: {
                        HStack {
                            Spacer()
                            Text("连接").bold()
                            Spacer()
                        }
                    }
                }
            }
            .navigationTitle("连接设置")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("取消") { dismiss() }
                }
            }
            .onAppear {
                host = app.config.host
                port = "\(app.config.port)"
            }
        }
        .presentationDetents([.medium])
    }
}
