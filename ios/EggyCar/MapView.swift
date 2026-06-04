// MapView.swift — 地图显示页 (带优化手势 + 点击设点)
import SwiftUI

struct MapView: View {
    @EnvironmentObject var app: AppState
    @State private var scale: CGFloat = 1.0
    @State private var offset: CGSize = .zero
    @GestureState private var magnify: CGFloat = 1.0

    var body: some View {
        ZStack(alignment: .bottomTrailing) {
            // 地图画布 (像素坐标系: 1pt = 1 地图像素)
            let map = app.topics.map
            let canvasW = map.map { CGFloat($0.width) } ?? 300
            let canvasH = map.map { CGFloat($0.height) } ?? 300

            ScrollView([.horizontal, .vertical], showsIndicators: false) {
                MapCanvasView(topics: app.topics)
                    .frame(width: canvasW, height: canvasH)
                    .scaleEffect(scale * magnify)
                    .offset(offset)
            }
            .gesture(
                MagnificationGesture()
                    .updating($magnify) { val, state, _ in state = val }
                    .onEnded { val in scale = min(max(scale * val, 0.5), 8) }
            )
            .background(Color(red: 0.15, green: 0.15, blue: 0.15))
            .onTapGesture(count: 2) { location in
                // 双击缩放
                withAnimation(.easeInOut(duration: 0.2)) {
                    if scale > 2 { scale = 1; offset = .zero }
                    else { scale = 3 }
                }
            }

            // 底部工具叠加
            VStack(spacing: 6) {
                // 状态栏
                StatusBarView()
                    .environmentObject(app)
                HStack(spacing: 12) {
                    Button { withAnimation { scale = min(max(scale * 1.4, 0.5), 8) } }
                        label: { Image(systemName: "plus.magnifyingglass").font(.title3) }
                    Button { withAnimation { scale = min(max(scale / 1.4, 0.5), 8) } }
                        label: { Image(systemName: "minus.magnifyingglass").font(.title3) }
                    Button { withAnimation { scale = 1; offset = .zero } }
                        label: { Image(systemName: "arrow.up.left.and.down.right.magnifyingglass").font(.title3) }
                    Divider().frame(height: 20)
                    // 重定位按钮 (点击地图选点)
                    Button {
                        // 发送初始位姿 (0,0,0 让 AMCL 重定位)
                        let msg: [String: Any] = [
                            "header": ["frame_id": "map", "stamp": ["secs": 0, "nsecs": 0]],
                            "pose": [
                                "pose": [
                                    "position": ["x": 0, "y": 0, "z": 0],
                                    "orientation": ["x": 0, "y": 0, "z": 0, "w": 1]
                                ],
                                "covariance": Array(repeating: 0.5, count: 36)
                            ]
                        ]
                        app.client.publish(topic: "/initialpose", msg: msg)
                    } label: {
                        Label("重定位", systemImage: "location.fill.viewfinder")
                            .font(.caption)
                    }
                    .buttonStyle(.bordered)
                }
                .padding(8)
                .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12))
            }
            .padding(8)
        }
    }
}
