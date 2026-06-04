// MapCanvasView.swift — 地图 Canvas 渲染
import SwiftUI

struct MapCanvasView: View {
    @ObservedObject var topics: TopicManager

    var body: some View {
        Canvas { context, size in
            // 1. 占据栅格地图
            if let map = topics.map {
                drawMap(context, size: size, map: map)
            }
            // 2. 代价地图
            if let gm = topics.globalCostmap {
                drawCostmap(context, size: size, map: gm, color: .blue, opacity: 0.15)
            }
            if let lm = topics.localCostmap {
                drawCostmap(context, size: size, map: lm, color: .orange, opacity: 0.2)
            }
            // 3. 全局路径
            if !topics.globalPath.isEmpty {
                drawPath(context, size: size, path: topics.globalPath, color: .green, width: 3)
            }
            // 4. 局部路径
            if !topics.localPath.isEmpty {
                drawPath(context, size: size, path: topics.localPath, color: .yellow, width: 2)
            }
            // 5. 激光扫描
            if let scan = topics.laserScan {
                drawLaser(context, size: size, scan: scan, pose: topics.robotPose)
            }
            // 6. 机器人足迹
            if !topics.footprint.points.isEmpty {
                drawFootprint(context, size: size, footprint: topics.footprint)
            }
            // 7. 机器人位姿
            drawRobot(context, size: size, pose: topics.robotPose)
        }
        .background(Color.black)
    }

    // MARK: - 绘制方法

    private func drawMap(_ ctx: GraphicsContext, size: CGSize, map: OccupancyGrid) {
        let res = map.resolution
        let pw = size.width / CGFloat(map.width)
        let ph = size.height / CGFloat(map.height)

        for y in 0..<map.height {
            for x in 0..<map.width {
                let val = map.data[y * map.width + x]
                let color: Color
                if val == -1 { color = Color(red: 0.5, green: 0.5, blue: 0.5) }
                else if val > 50 { color = .black }
                else { color = .white }

                let rect = CGRect(x: CGFloat(x) * pw, y: CGFloat(y) * ph,
                                  width: pw + 0.5, height: ph + 0.5)
                ctx.fill(Path(rect), with: .color(color))
            }
        }
    }

    private func drawCostmap(_ ctx: GraphicsContext, size: CGSize, map: OccupancyGrid,
                             color: Color, opacity: Double) {
        let pw = size.width / CGFloat(map.width)
        let ph = size.height / CGFloat(map.height)

        for y in 0..<map.height {
            for x in 0..<map.width {
                let val = map.data[y * map.width + x]
                if val <= 0 || val == -1 { continue }
                let alpha = Double(val) / 100.0 * opacity
                let rect = CGRect(x: CGFloat(x) * pw, y: CGFloat(y) * ph,
                                  width: pw + 0.5, height: ph + 0.5)
                ctx.fill(Path(rect), with: .color(color.opacity(alpha)))
            }
        }
    }

    private func drawPath(_ ctx: GraphicsContext, size: CGSize, path: [Pose2D],
                          color: Color, width: CGFloat) {
        guard path.count > 1, let map = topics.map else { return }
        var p = Path()
        let s = size.width / (CGFloat(map.width) * CGFloat(map.resolution))
        for (i, pt) in path.enumerated() {
            let sx = CGFloat(pt.x - map.originX) * s
            let sy = size.height - CGFloat(pt.y - map.originY) * s
            if i == 0 { p.move(to: CGPoint(x: sx, y: sy)) }
            else { p.addLine(to: CGPoint(x: sx, y: sy)) }
        }
        ctx.stroke(p, with: .color(color), lineWidth: width)
    }

    private func drawLaser(_ ctx: GraphicsContext, size: CGSize, scan: LaserScan, pose: Pose2D) {
        guard let map = topics.map else { return }
        let s = size.width / (CGFloat(map.width) * CGFloat(map.resolution))
        let cx = CGFloat(pose.x - map.originX) * s
        let cy = size.height - CGFloat(pose.y - map.originY) * s

        var angle = scan.angleMin
        for range in scan.ranges {
            if range.isFinite && range > 0.1 && range < 20 {
                let lx = cx + CGFloat(range) * cos(Double(angle) + pose.theta) * s
                let ly = cy - CGFloat(range) * sin(Double(angle) + pose.theta) * s
                let dot = Path(ellipseIn: CGRect(x: lx - 1.5, y: ly - 1.5, width: 3, height: 3))
                ctx.fill(dot, with: .color(.red.opacity(0.6)))
            }
            angle += scan.angleIncrement
        }
    }

    private func drawRobot(_ ctx: GraphicsContext, size: CGSize, pose: Pose2D) {
        guard let map = topics.map else { return }
        let s = size.width / (CGFloat(map.width) * CGFloat(map.resolution))
        let cx = CGFloat(pose.x - map.originX) * s
        let cy = size.height - CGFloat(pose.y - map.originY) * s
        let r: CGFloat = 10

        // 机器人圆
        let circle = Path(ellipseIn: CGRect(x: cx - r, y: cy - r, width: r * 2, height: r * 2))
        ctx.fill(circle, with: .color(.cyan.opacity(0.8)))

        // 朝向箭头
        let arrowLen: CGFloat = 20
        let ax = cx + arrowLen * CGFloat(cos(pose.theta))
        let ay = cy - arrowLen * CGFloat(sin(pose.theta))
        var arrow = Path()
        arrow.move(to: CGPoint(x: cx, y: cy))
        arrow.addLine(to: CGPoint(x: ax, y: ay))
        ctx.stroke(arrow, with: .color(.red), lineWidth: 2.5)
    }

    private func drawFootprint(_ ctx: GraphicsContext, size: CGSize, footprint: PolygonStamped) {
        guard let map = topics.map, footprint.points.count >= 3 else { return }
        let s = size.width / (CGFloat(map.width) * CGFloat(map.resolution))
        var p = Path()
        for (i, pt) in footprint.points.enumerated() {
            let sx = CGFloat(pt.x - map.originX) * s
            let sy = size.height - CGFloat(pt.y - map.originY) * s
            if i == 0 { p.move(to: CGPoint(x: sx, y: sy)) }
            else { p.addLine(to: CGPoint(x: sx, y: sy)) }
        }
        p.closeSubpath()
        ctx.stroke(p, with: .color(.mint), lineWidth: 1.5)
    }
}
