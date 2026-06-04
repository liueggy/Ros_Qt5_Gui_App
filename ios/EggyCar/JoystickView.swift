// JoystickView.swift — 摇杆 (支持 vx/vz/vy)
import SwiftUI

struct JoystickView: View {
    var onMove: (_ vx: Double, _ vz: Double, _ vy: Double) -> Void
    @State private var offset: CGSize = .zero
    private let radius: CGFloat = 80

    var body: some View {
        ZStack {
            Circle().stroke(.gray.opacity(0.3), lineWidth: 2)
                .frame(width: radius * 2, height: radius * 2)
            // 十字线
            Path { p in
                p.move(to: CGPoint(x: 0, y: -radius))
                p.addLine(to: CGPoint(x: 0, y: radius))
                p.move(to: CGPoint(x: -radius, y: 0))
                p.addLine(to: CGPoint(x: radius, y: 0))
            }.stroke(.gray.opacity(0.15), lineWidth: 1)
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
                            offset = CGSize(width: clamped * cos(angle),
                                            height: clamped * sin(angle))
                            // vx = 前后(Y取反), vz = 旋转(X), vy = 横向(对角)
                            onMove(-clamped / radius, clamped * cos(angle) / radius,
                                   clamped * sin(angle) / radius)
                        }
                        .onEnded { _ in
                            offset = .zero
                            onMove(0, 0, 0)
                        }
                )
        }
    }
}
