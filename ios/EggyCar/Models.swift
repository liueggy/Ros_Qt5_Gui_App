// Models.swift — 数据模型定义
import Foundation
import Combine

// MARK: - 几何

struct Pose2D: Equatable {
    var x: Double = 0
    var y: Double = 0
    var theta: Double = 0
}

struct RobotSpeed: Equatable {
    var vx: Double = 0
    var vy: Double = 0
    var wz: Double = 0
}

// MARK: - 占据栅格地图
struct OccupancyGrid {
    let width: Int
    let height: Int
    let resolution: Double
    let originX: Double
    let originY: Double
    let data: [Int8]

    var metersWidth: Double { Double(width) * resolution }
    var metersHeight: Double { Double(height) * resolution }

    // 地图坐标 → 像素坐标 (y-up ROS → y-down iOS)
    func toPixelX(x: Double) -> CGFloat { CGFloat((x - originX) / resolution) }
    func toPixelY(y: Double, canvasH: CGFloat) -> CGFloat { canvasH - CGFloat((y - originY) / resolution) }
}

// MARK: - 激光扫描
struct LaserScan {
    let angleMin: Double
    let angleMax: Double
    let angleIncrement: Double
    let ranges: [Double]
    let rangeMax: Double

    init(angleMin: Double, angleMax: Double, angleIncrement: Double, ranges: [Double], rangeMax: Double = 12.0) {
        self.angleMin = angleMin
        self.angleMax = angleMax
        self.angleIncrement = angleIncrement
        self.ranges = ranges
        self.rangeMax = rangeMax
    }
}

// MARK: - 路径
typealias RobotPath = [Pose2D]

// MARK: - 代价地图
struct CostmapLayer: Identifiable {
    let id: String
    var grid: OccupancyGrid?
}

// MARK: - 机器人足迹
struct PolygonStamped {
    var points: [(x: Double, y: Double)]
}

// MARK: - 电池状态
struct BatteryState: Equatable {
    var percent: Double = 0
    var voltage: Double = 0
}

// MARK: - DHT11
struct DHT11Data: Equatable {
    var temperature: Double = 0
    var humidity: Double = 0
    var lastUpdate: Date = .distantPast
}

// MARK: - 语音命令
struct VoiceCommand: Equatable, Identifiable {
    let id = UUID()
    let funcId: String
    let cmdId: String
    let timestamp: Date
    var displayName: String { "VOICE \(funcId),\(cmdId)" }
}

// MARK: - 诊断
struct DiagnosticEntry: Identifiable {
    let id = UUID()
    let name: String
    let level: Int
    let message: String
    var levelName: String {
        switch level {
        case 0: return "OK"
        case 1: return "WARN"
        case 2: return "ERROR"
        case 3: return "STALE"
        default: return "?"
        }
    }
}

// MARK: - 连接配置
struct ConnectionConfig: Equatable {
    var host: String = "192.168.31.50"
    var port: Int = 9090
    var url: URL? { URL(string: "ws://\(host):\(port)") }
}

// MARK: - 控制模式
enum ControlMode: String, CaseIterable, Identifiable {
    case diff = "差速"
    case omni = "全向"
    var id: Self { self }
}

// MARK: - 激光偏移 (相对于 base_link, 米)
let kLaserOffsetX: Double = 0.0  // RPLIDAR A1 通常装在中心附近
let kLaserOffsetY: Double = 0.0
