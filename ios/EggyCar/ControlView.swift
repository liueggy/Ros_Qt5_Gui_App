// ControlView.swift — 遥控页
import SwiftUI

struct ControlView: View {
    @EnvironmentObject var app: AppState
    @State private var linearSpeed: Double = 0.3
    @State private var angularSpeed: Double = 1.0
    @State private var sendTimer: Timer?

    var body: some View {
        VStack(spacing: 20) {
            // 速度显示
            let odom = app.topics.odom
            HStack(spacing: 24) {
                VStack {
                    Text("线速度").font(.caption).foregroundStyle(.secondary)
                    Text(String(format: "%.2f m/s", odom.twist.vx))
                        .font(.title3.monospacedDigit()).bold()
                }
                VStack {
                    Text("角速度").font(.caption).foregroundStyle(.secondary)
                    Text(String(format: "%.2f rad/s", odom.twist.wz))
                        .font(.title3.monospacedDigit()).bold()
                }
            }
            .padding()
            .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12))

            // 摇杆
            JoystickView { vx, vz in
                sendVelocity(vx: vx * linearSpeed, vz: vz * angularSpeed)
            }
            .frame(width: 200, height: 200)

            // 速度滑块
            VStack(spacing: 12) {
                HStack {
                    Text("线速度").frame(width: 60, alignment: .leading)
                    Slider(value: $linearSpeed, in: 0.05...2.0, step: 0.05)
                    Text(String(format: "%.2f", linearSpeed))
                        .monospacedDigit().frame(width: 40)
                }
                HStack {
                    Text("角速度").frame(width: 60, alignment: .leading)
                    Slider(value: $angularSpeed, in: 0.1...5.0, step: 0.1)
                    Text(String(format: "%.1f", angularSpeed))
                        .monospacedDigit().frame(width: 40)
                }
            }
            .padding(.horizontal)

            // 停止按钮
            Button {
                sendVelocity(vx: 0, vz: 0)
            } label: {
                Text("停止")
                    .font(.title2.bold())
                    .frame(maxWidth: .infinity)
                    .padding()
                    .background(.red, in: RoundedRectangle(cornerRadius: 12))
                    .foregroundStyle(.white)
            }
            .padding(.horizontal)
        }
        .padding(.vertical)
    }

    private func sendVelocity(vx: Double, vz: Double) {
        let msg: [String: Any] = [
            "linear": ["x": vx, "y": 0.0, "z": 0.0],
            "angular": ["x": 0.0, "y": 0.0, "z": vz]
        ]
        app.client.publish(topic: "/cmd_vel", msg: msg)
    }
}
