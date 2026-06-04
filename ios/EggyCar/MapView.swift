// MapView.swift — 地图显示页
import SwiftUI

struct MapView: View {
    @EnvironmentObject var app: AppState
    @State private var scale: CGFloat = 1.0
    @State private var offset: CGSize = .zero
    @State private var showLayers = false

    var body: some View {
        ZStack(alignment: .bottomTrailing) {
            // 地图画布
            ScrollView([.horizontal, .vertical]) {
                MapCanvasView(topics: app.topics)
                    .frame(width: canvasWidth, height: canvasHeight)
                    .scaleEffect(scale)
                    .offset(offset)
            }
            .gesture(magnification)
            .background(Color(.systemGroupedBackground))

            // 状态栏叠加
            VStack(spacing: 4) {
                StatusBarView()
                    .environmentObject(app)
                HStack(spacing: 12) {
                    Button { scale = min(scale * 1.5, 10) }
                        label: { Image(systemName: "plus.magnifyingglass").font(.title3) }
                    Button { scale = max(scale / 1.5, 0.3) }
                        label: { Image(systemName: "minus.magnifyingglass").font(.title3) }
                    Button { scale = 1.0; offset = .zero }
                        label: { Image(systemName: "arrow.up.left.and.down.right.magnifyingglass").font(.title3) }
                }
                .padding(8)
                .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 10))
            }
            .padding()
        }
    }

    private var canvasWidth: CGFloat {
        guard let map = app.topics.map else { return 400 }
        return CGFloat(map.width) * CGFloat(map.resolution) * 200
    }
    private var canvasHeight: CGFloat {
        guard let map = app.topics.map else { return 400 }
        return CGFloat(map.height) * CGFloat(map.resolution) * 200
    }

    private var magnification: some Gesture {
        MagnificationGesture()
            .onChanged { value in scale = max(0.3, min(10, value)) }
    }
}
