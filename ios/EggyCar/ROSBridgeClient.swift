// ROSBridgeClient.swift — ROSBridge WebSocket 客户端 (含手动重连)
import Foundation
import Combine

enum ConnectionState: Equatable {
    case disconnected
    case connecting
    case connected
    case error(String)
}

final class ROSBridgeClient: ObservableObject {
    @Published var connectionState: ConnectionState = .disconnected
    @Published var lastConfig = ConnectionConfig()

    private var webSocket: URLSessionWebSocketTask?
    private var session: URLSession?
    private var receiveHandler: ((String) -> Void)?
    private var pingTimer: DispatchSourceTimer?
    private var reconnectWorkItem: DispatchWorkItem?
    private var shouldAutoReconnect = true

    // MARK: - 连接

    func connect(config: ConnectionConfig, onMessage: @escaping (String) -> Void) {
        lastConfig = config
        receiveHandler = onMessage
        shouldAutoReconnect = true
        guard let url = config.url else { connectionState = .error("无效 URL"); return }
        disconnect(immediate: true)
        connectionState = .connecting

        let sessionConfig = URLSessionConfiguration.default
        sessionConfig.waitsForConnectivity = false
        sessionConfig.timeoutIntervalForResource = 30
        session = URLSession(configuration: sessionConfig, delegate: nil, delegateQueue: .main)
        webSocket = session?.webSocketTask(with: URLRequest(url: url, timeoutInterval: 10))
        webSocket?.resume()

        receiveNext()
        startPing()

        // 连接超时检测
        DispatchQueue.main.asyncAfter(deadline: .now() + 2) { [weak self] in
            guard let self, case .connecting = connectionState else { return }
            connectionState = .connected
        }
    }

    func disconnect(immediate: Bool = false) {
        shouldAutoReconnect = !immediate
        stopPing()
        reconnectWorkItem?.cancel()
        reconnectWorkItem = nil
        webSocket?.cancel(with: .normalClosure, reason: nil)
        webSocket = nil
        session?.invalidateAndCancel()
        session = nil
        if immediate { connectionState = .disconnected }
    }

    /// 手动重连 (可选完成回调)
    func reconnect(completion: (() -> Void)? = nil) {
        guard let handler = receiveHandler else { return }
        let cfg = lastConfig
        disconnect(immediate: true)
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) { [weak self] in
            self?.connect(config: cfg, onMessage: handler)
            completion?()
        }
    }

    // MARK: - 发送

    func send(_ text: String) {
        guard webSocket != nil else { return }
        webSocket?.send(.string(text)) { [weak self] error in
            if let error {
                print("[ROSBridge] send error: \(error)")
                DispatchQueue.main.async { self?.handleDisconnect("发送失败") }
            }
        }
    }

    func subscribe(_ topic: String, type: String, throttleRate: Int? = nil, queueLength: Int? = nil) {
        let msg = ROSSubscribe(topic, type: type, throttleRate: throttleRate, queueLength: queueLength)
        if let data = try? JSONEncoder().encode(msg), let json = String(data: data, encoding: .utf8) { send(json) }
    }

    func unsubscribe(_ topic: String) {
        let msg = ROSUnsubscribe(topic: topic)
        if let data = try? JSONEncoder().encode(msg), let json = String(data: data, encoding: .utf8) { send(json) }
    }

    func publish(topic: String, msg: [String: Any]) {
        let wrapper: [String: Any] = ["op": "publish", "topic": topic, "msg": msg]
        if let data = try? JSONSerialization.data(withJSONObject: wrapper),
           let json = String(data: data, encoding: .utf8) { send(json) }
    }

    // MARK: - 接收

    private func receiveNext() {
        webSocket?.receive { [weak self] result in
            guard let self else { return }
            switch result {
            case .success(let message):
                switch message {
                case .string(let text): DispatchQueue.main.async { self.receiveHandler?(text) }
                case .data(let data):
                    if let text = String(data: data, encoding: .utf8) {
                        DispatchQueue.main.async { self.receiveHandler?(text) }
                    }
                @unknown default: break
                }
                self.receiveNext()
            case .failure:
                DispatchQueue.main.async { self.handleDisconnect("连接断开") }
            }
        }
    }

    // MARK: - 心跳

    private func startPing() {
        stopPing()
        let timer = DispatchSource.makeTimerSource(queue: .global(qos: .utility))
        timer.schedule(deadline: .now() + 5, repeating: 10)
        timer.setEventHandler { [weak self] in
            self?.webSocket?.sendPing { error in
                if let error { print("[ROSBridge] ping: \(error.localizedDescription)") }
            }
        }
        timer.resume()
        pingTimer = timer
    }

    private func stopPing() {
        pingTimer?.cancel()
        pingTimer = nil
    }

    // MARK: - 断开处理

    private func handleDisconnect(_ reason: String) {
        guard connectionState != .disconnected else { return }
        connectionState = .error(reason)
        webSocket?.cancel(with: .goingAway, reason: nil)
        webSocket = nil
        guard shouldAutoReconnect else { return }
        let work = DispatchWorkItem { [weak self] in
            guard let self, let handler = receiveHandler else { return }
            connect(config: lastConfig, onMessage: handler)
        }
        reconnectWorkItem = work
        DispatchQueue.main.asyncAfter(deadline: .now() + 3, execute: work)
    }
}
