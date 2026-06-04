// ROSBridgeClient.swift — ROSBridge WebSocket 通信客户端
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

    private var webSocket: URLSessionWebSocketTask?
    private var session: URLSession?
    private var config = ConnectionConfig()
    private var receiveHandler: ((String) -> Void)?
    private var reconnectWorkItem: DispatchWorkItem?
    private var pingTimer: DispatchSourceTimer?
    private var shouldReconnect = false

    // MARK: - Connect / Disconnect

    func connect(config: ConnectionConfig, onMessage: @escaping (String) -> Void) {
        self.config = config
        self.receiveHandler = onMessage
        self.shouldReconnect = true

        guard let url = config.url else {
            connectionState = .error("Invalid URL")
            return
        }

        disconnect(immediate: true)
        connectionState = .connecting

        let sessionConfig = URLSessionConfiguration.default
        sessionConfig.waitsForConnectivity = false
        session = URLSession(configuration: sessionConfig, delegate: nil, delegateQueue: .main)

        webSocket = session?.webSocketTask(with: URLRequest(url: url, timeoutInterval: 5))
        webSocket?.resume()

        // Wait for connection
        receiveNext()
        startPing()

        DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) { [weak self] in
            guard let self else { return }
            if self.connectionState == .connecting {
                self.connectionState = .connected
            }
        }
    }

    func disconnect(immediate: Bool = false) {
        shouldReconnect = !immediate
        stopPing()
        reconnectWorkItem?.cancel()
        webSocket?.cancel(with: .normalClosure, reason: nil)
        webSocket = nil
        session?.invalidateAndCancel()
        session = nil
        if immediate {
            connectionState = .disconnected
        }
    }

    // MARK: - Send

    func send(_ text: String) {
        guard webSocket != nil else { return }
        webSocket?.send(.string(text)) { [weak self] error in
            if let error {
                print("[ROSBridge] send error: \(error)")
                DispatchQueue.main.async {
                    self?.handleDisconnect("Send failed: \(error.localizedDescription)")
                }
            }
        }
    }

    func subscribe(_ topic: String, type: String, throttleRate: Int? = nil, queueLength: Int? = nil) {
        let msg = ROSSubscribe(topic, type: type, throttleRate: throttleRate, queueLength: queueLength)
        if let data = try? JSONEncoder().encode(msg),
           let json = String(data: data, encoding: .utf8) {
            send(json)
        }
    }

    func unsubscribe(_ topic: String) {
        let msg = ROSUnsubscribe(topic: topic)
        if let data = try? JSONEncoder().encode(msg),
           let json = String(data: data, encoding: .utf8) {
            send(json)
        }
    }

    func publish(topic: String, msg: [String: Any]) {
        let wrapper: [String: Any] = ["op": "publish", "topic": topic, "msg": msg]
        if let data = try? JSONSerialization.data(withJSONObject: wrapper),
           let json = String(data: data, encoding: .utf8) {
            send(json)
        }
    }

    // MARK: - Receive Loop

    private func receiveNext() {
        webSocket?.receive { [weak self] result in
            guard let self else { return }
            switch result {
            case .success(let message):
                switch message {
                case .string(let text):
                    DispatchQueue.main.async {
                        self.receiveHandler?(text)
                    }
                case .data(let data):
                    if let text = String(data: data, encoding: .utf8) {
                        DispatchQueue.main.async {
                            self.receiveHandler?(text)
                        }
                    }
                @unknown default: break
                }
                self.receiveNext()
            case .failure(let error):
                print("[ROSBridge] receive error: \(error)")
                DispatchQueue.main.async {
                    self.handleDisconnect("Connection lost")
                }
            }
        }
    }

    // MARK: - Ping (keepalive)

    private func startPing() {
        stopPing()
        let timer = DispatchSource.makeTimerSource(queue: .global(qos: .utility))
        timer.schedule(deadline: .now() + 5, repeating: 5)
        timer.setEventHandler { [weak self] in
            self?.webSocket?.sendPing { error in
                if let error {
                    print("[ROSBridge] ping error: \(error)")
                }
            }
        }
        timer.resume()
        pingTimer = timer
    }

    private func stopPing() {
        pingTimer?.cancel()
        pingTimer = nil
    }

    // MARK: - Reconnect

    private func handleDisconnect(_ reason: String) {
        guard connectionState != .disconnected else { return }
        connectionState = .error(reason)
        webSocket?.cancel(with: .goingAway, reason: nil)
        webSocket = nil

        guard shouldReconnect else { return }
        print("[ROSBridge] will reconnect in 3s...")
        let work = DispatchWorkItem { [weak self] in
            self?.connect(config: self!.config, onMessage: self!.receiveHandler!)
        }
        reconnectWorkItem = work
        DispatchQueue.main.asyncAfter(deadline: .now() + 3, execute: work)
    }
}
