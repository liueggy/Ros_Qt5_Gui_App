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
}

// MARK: - 激光扫描

struct LaserScan {
    let angleMin: Double
    let angleMax: Double
    let angleIncrement: Double
    let ranges: [Double]
}

// MARK: - 路径

typealias RobotPath = [Pose2D]

// MARK: - 代价地图

struct CostmapLayer: Identifiable {
    let id: String   // topic name
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

    var displayName: String {
        "VOICE \(funcId),\(cmdId)"
    }
}

// MARK: - 诊断

struct DiagnosticEntry: Identifiable {
    let id = UUID()
    let name: String
    let level: Int    // 0=OK, 1=WARN, 2=ERROR, 3=STALE
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

    var url: URL? {
        URL(string: "ws://\(host):\(port)")
    }
}
