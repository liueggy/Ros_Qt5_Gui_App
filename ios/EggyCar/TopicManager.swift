// TopicManager.swift — 话题订阅管理 & 数据分发
import Foundation
import Combine

final class TopicManager: ObservableObject {
    // Published state
    @Published var map: OccupancyGrid?
    @Published var robotPose: Pose2D = .init()
    @Published var odom: (pose: Pose2D, twist: RobotSpeed) = (.init(), .init())
    @Published var laserScan: LaserScan?
    @Published var globalPath: RobotPath = []
    @Published var localPath: RobotPath = []
    @Published var battery: BatteryState = .init()
    @Published var dht11: DHT11Data = .init()
    @Published var lastVoiceCommand: VoiceCommand?
    @Published var diagnostics: [DiagnosticEntry] = []
    @Published var footprint: PolygonStamped = .init(points: [])
    @Published var globalCostmap: OccupancyGrid?
    @Published var localCostmap: OccupancyGrid?

    private let client: ROSBridgeClient
    private var subscribed = false

    init(client: ROSBridgeClient) {
        self.client = client
    }

    // MARK: - Subscribe All

    func subscribeAll() {
        guard !subscribed else { return }
        subscribed = true

        client.subscribe("/map", type: "nav_msgs/OccupancyGrid", throttleRate: 500)
        client.subscribe("/odom", type: "nav_msgs/Odometry", throttleRate: 50)
        client.subscribe("/scan", type: "sensor_msgs/LaserScan", throttleRate: 100)
        client.subscribe("/plan", type: "nav_msgs/Path", throttleRate: 500)
        client.subscribe("/local_plan", type: "nav_msgs/Path", throttleRate: 200)
        client.subscribe("/global_costmap/costmap", type: "nav_msgs/OccupancyGrid", throttleRate: 2000)
        client.subscribe("/local_costmap/costmap", type: "nav_msgs/OccupancyGrid", throttleRate: 1000)
        client.subscribe("/local_costmap/published_footprint", type: "geometry_msgs/PolygonStamped", throttleRate: 200)
        client.subscribe("/battery", type: "sensor_msgs/BatteryState", throttleRate: 2000)
        client.subscribe("/diagnostics", type: "diagnostic_msgs/DiagnosticArray", throttleRate: 2000)
        client.subscribe("/stm32/dht11/temperature", type: "std_msgs/Float32")
        client.subscribe("/stm32/dht11/humidity", type: "std_msgs/Float32")
        client.subscribe("/stm32/voice_command", type: "std_msgs/String")
    }

    // MARK: - Unsubscribe All

    func unsubscribeAll() {
        guard subscribed else { return }
        subscribed = false
        client.unsubscribe("/map")
        client.unsubscribe("/odom")
        client.unsubscribe("/scan")
        client.unsubscribe("/plan")
        client.unsubscribe("/local_plan")
        client.unsubscribe("/global_costmap/costmap")
        client.unsubscribe("/local_costmap/costmap")
        client.unsubscribe("/local_costmap/published_footprint")
        client.unsubscribe("/battery")
        client.unsubscribe("/diagnostics")
        client.unsubscribe("/stm32/dht11/temperature")
        client.unsubscribe("/stm32/dht11/humidity")
        client.unsubscribe("/stm32/voice_command")
    }

    // MARK: - Handle Incoming Message

    func handleMessage(_ text: String) {
        guard let data = text.data(using: .utf8),
              let json = try? JSONDecoder().decode(ROSMessage.self, from: data),
              json.op == "publish",
              let topic = json.topic,
              let msgDict = json.msg?.dict
        else { return }

        switch topic {
        case "/map":
            if let grid = ROSMessageDecoder.decodeOccupancyGrid(msgDict) {
                map = grid
            }

        case "/odom":
            if let result = ROSMessageDecoder.decodeOdom(msgDict) {
                let pose = Pose2D(x: result.pose.x, y: result.pose.y, theta: result.pose.theta)
                let speed = RobotSpeed(vx: result.twist.vx, vy: result.twist.vy, wz: result.twist.wz)
                robotPose = pose
                odom = (pose, speed)
            }

        case "/scan":
            if let scan = ROSMessageDecoder.decodeLaserScan(msgDict) {
                laserScan = scan
            }

        case "/plan":
            globalPath = ROSMessageDecoder.decodePath(msgDict)

        case "/local_plan":
            localPath = ROSMessageDecoder.decodePath(msgDict)

        case "/battery":
            if let b = ROSMessageDecoder.decodeBatteryState(msgDict) {
                battery = BatteryState(percent: b.percent, voltage: b.voltage)
            }

        case "/stm32/dht11/temperature":
            if let v = msgDict["data"]?.double {
                dht11.temperature = v
                dht11.lastUpdate = Date()
            }

        case "/stm32/dht11/humidity":
            if let v = msgDict["data"]?.double {
                dht11.humidity = v
                dht11.lastUpdate = Date()
            }

        case "/stm32/voice_command":
            if let jsonStr = msgDict["data"]?.string {
                let parts = jsonStr.components(separatedBy: "\"")
                // Quick parse: {"func":"00","cmd":"04"}
                let cleaned = jsonStr.replacingOccurrences(of: "{\"func\":\"", with: "")
                    .replacingOccurrences(of: "\",\"cmd\":\"", with: ",")
                    .replacingOccurrences(of: "\"}", with: "")
                let fc = cleaned.components(separatedBy: ",")
                if fc.count == 2 {
                    lastVoiceCommand = VoiceCommand(funcId: fc[0], cmdId: fc[1], timestamp: Date())
                }
            }

        case "/diagnostics":
            diagnostics = ROSMessageDecoder.decodeDiagnosticArray(msgDict)

        case "/local_costmap/published_footprint":
            if let poly = msgDict["polygon"], let points = poly["points"]?.array {
                footprint.points = points.compactMap { p in
                    guard let x = p["x"]?.double, let y = p["y"]?.double else { return nil }
                    return (x, y)
                }
            }

        case "/global_costmap/costmap":
            globalCostmap = ROSMessageDecoder.decodeOccupancyGrid(msgDict)

        case "/local_costmap/costmap":
            localCostmap = ROSMessageDecoder.decodeOccupancyGrid(msgDict)

        default:
            break
        }
    }
}
