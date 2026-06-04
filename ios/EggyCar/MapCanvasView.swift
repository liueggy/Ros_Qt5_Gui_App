// MapCanvasView.swift — 地图 Canvas 渲染 (修正版)
import SwiftUI

private let kRobotRadius: CGFloat = 12
private let kArrowLength: CGFloat = 22
private let kDotSize: CGFloat = 2.5

struct MapCanvasView: View {
    @ObservedObject var topics: TopicManager

    var body: some View {
        Canvas { context, size in
            guard let map = topics.map else {
                context.fill(Path(CGRect(origin: .zero, size: size)),
                             with: .color(.black))
                return
            }
            let s = size.width / CGFloat(map.metersWidth)  // pixels per meter

            // 1. 占据栅格地图
            drawMap(context, size: size, map: map)

            // 2. 代价地图
            if let gm = topics.globalCostmap {
                drawCostmap(context, size: size, map: gm,
                            color: .blue, opacity: 0.15)
            }
            if let lm = topics.localCostmap {
                drawCostmap(context, size: size, map: lm,
                            color: .orange, opacity: 0.2)
            }

            // 机器人画布坐标
            let cx = map.toPixelX(x: topics.robotPose.x)
            let cy = map.toPixelY(y: topics.robotPose.y, canvasH: size.height)
            let theta = topics.robotPose.theta

            // 3. 激光扫描 (相对机器人绘制, 防止帧不对齐)
            if let scan = topics.laserScan {
                drawLaser(context, size: size, scan: scan,
                          cx: cx, cy: cy, theta: theta, s: s, map: map)
            }

            // 4. 全局路径
            if !topics.globalPath.isEmpty {
                drawPath(context, size: size, path: topics.globalPath, map: map,
                         color: .green, width: 3)
            }

            // 5. 局部路径
            if !topics.localPath.isEmpty {
                drawPath(context, size: size, path: topics.localPath, map: map,
                         color: .yellow, width: 2)
            }

            // 6. 机器人足迹
            if !topics.footprint.points.isEmpty {
                drawFootprint(context, size: size, footprint: topics.footprint, map: map)
            }

            // 7. 机器人 (最后画, 叠加在最上层)
            drawRobot(context, cx: cx, cy: cy, theta: theta, s: s)
        }
        .background(Color(red: 0.15, green: 0.15, blue: 0.15))
    }

    // MARK: - 占据栅格地图
    private func drawMap(_ ctx: GraphicsContext, size: CGSize, map: OccupancyGrid) {
        for y in 0..<map.height {
            for x in 0..<map.width {
                let v = map.data[y * map.width + x]
                let color: Color
                switch v {
                case -1: color = Color(red: 0.4, green: 0.4, blue: 0.4)  // 未知
                case 0...30: color = Color(red: 0.95, green: 0.95, blue: 0.95) // 空闲
                default: color = Color(red: 0.1, green: 0.1, blue: 0.1)  // 占据
                }
                let rect = CGRect(x: CGFloat(x), y: CGFloat(y), width: 1, height: 1)
                ctx.fill(Path(rect), with: .color(color))
            }
        }
    }

    // MARK: - 代价地图
    private func drawCostmap(_ ctx: GraphicsContext, size: CGSize, map: OccupancyGrid,
                             color: Color, opacity: Double) {
        for y in 0..<map.height {
            for x in 0..<map.width {
                let v = map.data[y * map.width + x]
                if v <= 0 || v == -1 { continue }
                let a = Double(v) / 100.0 * opacity
                let rect = CGRect(x: CGFloat(x), y: CGFloat(y), width: 1, height: 1)
                ctx.fill(Path(rect), with: .color(color.opacity(a)))
            }
        }
    }

    // MARK: - 路径
    private func drawPath(_ ctx: GraphicsContext, size: CGSize,
                          path: [Pose2D], map: OccupancyGrid,
                          color: Color, width: CGFloat) {
        guard path.count > 1 else { return }
        var p = Path()
        for (i, pt) in path.enumerated() {
            let sx = map.toPixelX(x: pt.x)
            let sy = map.toPixelY(y: pt.y, canvasH: size.height)
            if i == 0 { p.move(to: CGPoint(x: sx, y: sy)) }
            else { p.addLine(to: CGPoint(x: sx, y: sy)) }
        }
        ctx.stroke(p, with: .color(color), lineWidth: width)
    }

    // MARK: - 激光扫描 (相对机器人)
    private func drawLaser(_ ctx: GraphicsContext, size: CGSize,
                           scan: LaserScan,
                           cx: CGFloat, cy: CGFloat, theta: Double,
                           s: CGFloat, map: OccupancyGrid) {
        var angle = scan.angleMin
        let increments = scan.ranges.count
        var points = [CGPoint]()
        for i in 0..<min(increments, 2000) {
            let r = scan.ranges[i]
            if r.isFinite && r > 0.1 && r < scan.rangeMax * 0.95 {
                let dx = CGFloat(r) * CGFloat(cos(angle + theta))
                let dy = CGFloat(r) * CGFloat(-sin(angle + theta))
                let lx = cx + dx * s
                let ly = cy + dy * s
                points.append(CGPoint(x: lx, y: ly))
            }
            angle += scan.angleIncrement
        }
        // 批量绘制点 (性能优化)
        for pt in points {
            let dot = Path(ellipseIn: CGRect(x: pt.x - kDotSize/2, y: pt.y - kDotSize/2,
                                              width: kDotSize, height: kDotSize))
            ctx.fill(dot, with: .color(.red.opacity(0.55)))
        }
    }

    // MARK: - 机器人 (更好看的样式)
    private func drawRobot(_ ctx: GraphicsContext,
                           cx: CGFloat, cy: CGFloat, theta: Double, s: CGFloat) {
        let r = kRobotRadius
        // 机体圆 (半透明渐变效果)
        let body = Path(ellipseIn: CGRect(x: cx - r, y: cy - r,
                                           width: r * 2, height: r * 2))
        ctx.fill(body, with: .color(.cyan.opacity(0.85)))
        ctx.stroke(body, with: .color(.white.opacity(0.5)), lineWidth: 1.5)

        // 方向箭头
        let ax = cx + kArrowLength * CGFloat(cos(theta))
        let ay = cy + kArrowLength * CGFloat(-sin(theta))
        var arrow = Path()
        arrow.move(to: CGPoint(x: cx, y: cy))
        arrow.addLine(to: CGPoint(x: ax, y: ay))
        ctx.stroke(arrow, with: .color(.red), lineWidth: 3)

        // 箭头三角形
        let tipLen: CGFloat = 6
        let tipAngle: Double = 0.4
        let backwardTheta = theta + Double.pi
        var arrowHead = Path()
        let t1x = ax + tipLen * CGFloat(cos(backwardTheta + tipAngle))
        let t1y = ay + tipLen * CGFloat(-sin(backwardTheta + tipAngle))
        let t2x = ax + tipLen * CGFloat(cos(backwardTheta - tipAngle))
        let t2y = ay + tipLen * CGFloat(-sin(backwardTheta - tipAngle))
        arrowHead.move(to: CGPoint(x: ax, y: ay))
        arrowHead.addLine(to: CGPoint(x: t1x, y: t1y))
        arrowHead.addLine(to: CGPoint(x: t2x, y: t2y))
        arrowHead.closeSubpath()
        ctx.fill(arrowHead, with: .color(.red))

        // 中心点
        ctx.fill(Path(ellipseIn: CGRect(x: cx - 2.5, y: cy - 2.5,
                                         width: 5, height: 5)),
                 with: .color(.white))
    }

    // MARK: - 机器人足迹
    private func drawFootprint(_ ctx: GraphicsContext, size: CGSize,
                                footprint: PolygonStamped, map: OccupancyGrid) {
        guard footprint.points.count >= 3 else { return }
        var p = Path()
        for (i, pt) in footprint.points.enumerated() {
            let sx = map.toPixelX(x: pt.x)
            let sy = map.toPixelY(y: pt.y, canvasH: size.height)
            if i == 0 { p.move(to: CGPoint(x: sx, y: sy)) }
            else { p.addLine(to: CGPoint(x: sx, y: sy)) }
        }
        p.closeSubpath()
        ctx.stroke(p, with: .color(.mint), lineWidth: 1.5)
    }
}
