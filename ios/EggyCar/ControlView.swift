// ControlView.swift — 遥控页 (支持差速/全向切换)
import SwiftUI

struct ControlView: View {
    @EnvironmentObject var app: AppState
    @State private var linearSpeed: Double = 0.3
    @State private var angularSpeed: Double = 1.0
    @State private var lateralSpeed: Double = 0.2
    @State private var mode: ControlMode = .diff

    var body: some View {
        VStack(spacing: 16) {
            // 模式切换
            Picker("控制模式", selection: $mode) {
                ForEach(ControlMode.allCases) { m in
                    Text(m.rawValue).tag(m)
                }
            }
            .pickerStyle(.segmented)
            .padding(.horizontal)

            // 速度状态
            let odom = app.topics.odom
            HStack(spacing: 16) {
                VStack {
                    Text("前向").font(.caption2).foregroundStyle(.secondary)
                    Text(String(format: "%.2f", odom.twist.vx))
                        .font(.title3.monospacedDigit()).bold()
                }
                if mode == .omni {
                    VStack {
                        Text("横向").font(.caption2).foregroundStyle(.secondary)
                        Text(String(format: "%.2f", odom.twist.vy))
                            .font(.title3.monospacedDigit()).bold()
                    }
                }
                VStack {
                    Text("旋转").font(.caption2).foregroundStyle(.secondary)
                    Text(String(format: "%.2f", odom.twist.wz))
                        .font(.title3.monospacedDigit()).bold()
                }
            }
            .padding(10)
            .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12))

            // 摇杆
            JoystickView { vx, vz, vy in
                switch mode {
                case .diff:
                    sendCmd(vx: vx * linearSpeed, vy: 0, vz: vz * angularSpeed)
                case .omni:
                    sendCmd(vx: vx * linearSpeed, vy: vy * lateralSpeed,
                            vz: vz * angularSpeed * 0.5)
                }
            }
            .frame(width: 200, height: 200)

            // 滑块
            VStack(spacing: 8) {
                HStack {
                    Text("线速度").frame(width: 60, alignment: .leading).font(.subheadline)
                    Slider(value: $linearSpeed, in: 0.05...2.0, step: 0.05)
                    Text(String(format: "%.2f", linearSpeed)).monospacedDigit().frame(width: 40)
                }
                if mode == .omni {
                    HStack {
                        Text("横移").frame(width: 60, alignment: .leading).font(.subheadline)
                        Slider(value: $lateralSpeed, in: 0.05...1.0, step: 0.05)
                        Text(String(format: "%.2f", lateralSpeed)).monospacedDigit().frame(width: 40)
                    }
                }
                HStack {
                    Text("角速度").frame(width: 60, alignment: .leading).font(.subheadline)
                    Slider(value: $angularSpeed, in: 0.1...5.0, step: 0.1)
                    Text(String(format: "%.1f", angularSpeed)).monospacedDigit().frame(width: 40)
                }
            }
            .padding(.horizontal)

            // 停止按钮
            Button { sendCmd(vx: 0, vy: 0, vz: 0) } label: {
                Text("紧急停止").font(.title2.bold()).frame(maxWidth: .infinity).padding()
                    .background(.red, in: RoundedRectangle(cornerRadius: 12))
                    .foregroundStyle(.white)
            }
            .padding(.horizontal)

            Text("摇杆 → 上下前后 / 左右旋转")
                .font(.caption2).foregroundStyle(.secondary)
        }
        .padding(.vertical)
    }

    private func sendCmd(vx: Double, vy: Double, vz: Double) {
        let msg: [String: Any] = [
            "linear": ["x": vx, "y": vy, "z": 0.0],
            "angular": ["x": 0.0, "y": 0.0, "z": vz]
        ]
        app.client.publish(topic: "/cmd_vel", msg: msg)
    }
}
