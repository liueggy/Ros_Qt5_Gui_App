// AppState.swift — 全局状态容器 (含重连订阅)
import Foundation
import Combine
import SwiftUI

final class AppState: ObservableObject {
    let client = ROSBridgeClient()
    lazy var topics = TopicManager(client: client)

    @Published var config = ConnectionConfig()
    @Published var showConnectionSheet = false

    init() {
        let launched = UserDefaults.standard.bool(forKey: "hasLaunched")
        if !launched {
            showConnectionSheet = true
            UserDefaults.standard.set(true, forKey: "hasLaunched")
        }
    }

    func connect() {
        client.connect(config: config) { [weak self] text in
            self?.topics.handleMessage(text)
        }
        // 连接后延时订阅话题
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) { [weak self] in
            self?.topics.subscribeAll()
        }
        UserDefaults.standard.set(config.host, forKey: "rosbridge_host")
        UserDefaults.standard.set(config.port, forKey: "rosbridge_port")
    }

    /// 手动重连 (取消旧订阅 → 断开 → 重连 → 重新订阅)
    func manualReconnect() {
        topics.unsubscribeAll()
        client.reconnect { [weak self] in
            // 重连成功后重新订阅
            DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) {
                self?.topics.subscribeAll()
            }
        }
    }

    func disconnect() {
        topics.unsubscribeAll()
        client.disconnect(immediate: true)
    }

    func loadSavedConfig() {
        let host = UserDefaults.standard.string(forKey: "rosbridge_host") ?? "192.168.31.50"
        let port = UserDefaults.standard.integer(forKey: "rosbridge_port")
        config.host = host
        config.port = port > 0 ? port : 9090
    }
}
