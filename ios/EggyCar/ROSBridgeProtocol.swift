// ROSBridgeProtocol.swift — ROSBridge JSON 协议定义
import Foundation

// MARK: - Outgoing Messages (→ ROSBridge)

struct ROSSubscribe: Codable {
    let op = "subscribe"
    let topic: String
    let type: String?
    let throttle_rate: Int?
    let queue_length: Int?

    init(_ topic: String, type: String? = nil, throttleRate: Int? = nil, queueLength: Int? = nil) {
        self.topic = topic
        self.type = type
        self.throttle_rate = throttleRate
        self.queue_length = queueLength
    }
}

struct ROSUnsubscribe: Codable {
    let op = "unsubscribe"
    let topic: String
}

struct ROSPublish: Codable {
    let op = "publish"
    let topic: String
    let msg: AnyCodable
}

// MARK: - Incoming Messages (← ROSBridge)

struct ROSMessage: Codable {
    let op: String
    let topic: String?
    let msg: AnyCodable?
    let id: String?
    let reason: String?
}

// MARK: - AnyCodable (type-erased JSON)

struct AnyCodable: Codable {
    let value: Any

    init(_ value: Any) { self.value = value }

    init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        if let v = try? container.decode(Bool.self) { value = v }
        else if let v = try? container.decode(Int.self) { value = v }
        else if let v = try? container.decode(Double.self) { value = v }
        else if let v = try? container.decode(String.self) { value = v }
        else if let v = try? container.decode([String: AnyCodable].self) { value = v }
        else if let v = try? container.decode([AnyCodable].self) { value = v }
        else { value = NSNull() }
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        switch value {
        case let v as Bool: try container.encode(v)
        case let v as Int: try container.encode(v)
        case let v as Double: try container.encode(v)
        case let v as Float: try container.encode(v)
        case let v as String: try container.encode(v)
        case let v as [String: AnyCodable]: try container.encode(v)
        case let v as [AnyCodable]: try container.encode(v)
        default: try container.encodeNil()
        }
    }
}

// MARK: - JSON Helpers

extension AnyCodable {
    var dict: [String: AnyCodable]? { value as? [String: AnyCodable] }
    var array: [AnyCodable]? { value as? [AnyCodable] }
    var double: Double? {
        if let v = value as? Double { return v }
        if let v = value as? Int { return Double(v) }
        if let v = value as? Float { return Double(v) }
        return nil
    }
    var int: Int? {
        if let v = value as? Int { return v }
        if let v = value as? Double { return Int(v) }
        return nil
    }
    var string: String? { value as? String }
    var bool: Bool? { value as? Bool }

    subscript(key: String) -> AnyCodable? { dict?[key] }
    subscript(index: Int) -> AnyCodable? { array?[index] }
}

// MARK: - Message decode helpers

enum ROSMessageDecoder {
    static func decodeOccupancyGrid(_ data: [String: AnyCodable]) -> OccupancyGrid? {
        guard let info = data["info"],
              let width = info["width"]?.int,
              let height = info["height"]?.int,
              let resolution = info["resolution"]?.double,
              let origin = info["origin"],
              let pos = origin["position"],
              let px = pos["x"]?.double, let py = pos["y"]?.double,
              let dataArray = data["data"]?.array
        else { return nil }

        let cells = dataArray.compactMap { $0.int?.toInt8 }
        return OccupancyGrid(width: width, height: height,
                             resolution: resolution,
                             originX: px, originY: py,
                             data: cells)
    }

    static func decodePose(_ data: [String: AnyCodable]) -> (x: Double, y: Double, theta: Double)? {
        guard let pose = data["pose"],
              let position = pose["position"],
              let x = position["x"]?.double,
              let y = position["y"]?.double,
              let orientation = pose["orientation"],
              let qz = orientation["z"]?.double,
              let qw = orientation["w"]?.double
        else { return nil }
        return (x, y, 2.0 * atan2(qz, qw))
    }

    static func decodeOdom(_ data: [String: AnyCodable]) -> (pose: (x: Double, y: Double, theta: Double),
                                                              twist: (vx: Double, vy: Double, wz: Double))? {
        guard let poseData = data["pose"],
              let poseDict = poseData.dict,
              let twistData = data["twist"] else { return nil }

        guard let poseResult = decodePose(poseDict) else { return nil }

        let linear = twistData["linear"]
        let angular = twistData["angular"]
        let vx = linear?["x"]?.double ?? 0
        let vy = linear?["y"]?.double ?? 0
        let wz = angular?["z"]?.double ?? 0

        return (poseResult, (vx, vy, wz))
    }

    static func decodeLaserScan(_ data: [String: AnyCodable]) -> LaserScan? {
        guard let angleMin = data["angle_min"]?.double,
              let angleMax = data["angle_max"]?.double,
              let angleIncrement = data["angle_increment"]?.double,
              let ranges = data["ranges"]?.array
        else { return nil }
        let rangeValues = ranges.compactMap { $0.double }
        return LaserScan(angleMin: angleMin, angleMax: angleMax,
                         angleIncrement: angleIncrement, ranges: rangeValues)
    }

    static func decodePath(_ data: [String: AnyCodable]) -> [Pose2D] {
        guard let poses = data["poses"]?.array else { return [] }
        return poses.compactMap { p in
            guard let pose = p["pose"] else { return nil }
            let result = decodePose(["pose": AnyCodable(pose.value)])
            return result.map { Pose2D(x: $0.x, y: $0.y, theta: $0.theta) }
        }
    }

    static func decodeBatteryState(_ data: [String: AnyCodable]) -> (percent: Double, voltage: Double)? {
        let percent = data["percentage"]?.double ?? 0
        let voltage = data["voltage"]?.double ?? 0
        return (percent, voltage)
    }

    static func decodeDiagnosticArray(_ data: [String: AnyCodable]) -> [DiagnosticEntry] {
        guard let statusArray = data["status"]?.array else { return [] }
        return statusArray.compactMap { s in
            guard let name = s["name"]?.string,
                  let level = s["level"]?.int else { return nil }
            let message = s["message"]?.string ?? ""
            return DiagnosticEntry(name: name, level: level, message: message)
        }
    }
}

private extension Int {
    var toInt8: Int8 {
        if self == -1 { return -1 }
        if self >= 0 && self <= 100 { return Int8(self) }
        return -1
    }
}
