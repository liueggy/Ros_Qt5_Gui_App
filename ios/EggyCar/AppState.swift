// AppState.swift — 全局状态容器
import Foundation
import Combine
import SwiftUI

final class AppState: ObservableObject {
    let client = ROSBridgeClient()
    lazy var topics = TopicManager(client: client)

    @Published var config = ConnectionConfig()
    @Published var showConnectionSheet = false

    init() {
        // Auto-show connection sheet on first launch
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
        // Delay subscription until connected
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) { [weak self] in
            self?.topics.subscribeAll()
        }
        UserDefaults.standard.set(config.host, forKey: "rosbridge_host")
        UserDefaults.standard.set(config.port, forKey: "rosbridge_port")
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
