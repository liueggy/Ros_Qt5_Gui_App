// JoystickView.swift — 虚拟摇杆
import SwiftUI

struct JoystickView: View {
    var onMove: (_ vx: Double, _ vz: Double) -> Void

    @State private var offset: CGSize = .zero
    @GestureState private var isDragging = false

    private let radius: CGFloat = 80

    var body: some View {
        ZStack {
            // 底盘
            Circle()
                .stroke(.gray.opacity(0.4), lineWidth: 2)
                .frame(width: radius * 2, height: radius * 2)

            // 方向十字线
            Path { p in
                p.move(to: CGPoint(x: 0, y: -radius))
                p.addLine(to: CGPoint(x: 0, y: radius))
                p.move(to: CGPoint(x: -radius, y: 0))
                p.addLine(to: CGPoint(x: radius, y: 0))
            }
            .stroke(.gray.opacity(0.2), lineWidth: 1)

            // 摇杆头
            Circle()
                .fill(.blue.gradient)
                .frame(width: 50, height: 50)
                .shadow(radius: 3)
                .offset(offset)
                .gesture(
                    DragGesture(minimumDistance: 0)
                        .onChanged { value in
                            let dx = value.translation.width
                            let dy = value.translation.height
                            let dist = sqrt(dx * dx + dy * dy)
                            let clamped = min(dist, radius)
                            let angle = atan2(dy, dx)
                            let cx = clamped * cos(angle)
                            let cy = clamped * sin(angle)
                            offset = CGSize(width: cx, height: cy)
                            // vx = 前后 (y轴取反), vz = 左右
                            onMove(Double(-cy / radius), Double(cx / radius))
                        }
                        .onEnded { _ in
                            offset = .zero
                            onMove(0, 0)
                        }
                )
        }
    }
}
