// MainTabView.swift — 主 Tab 界面
import SwiftUI

struct MainTabView: View {
    @EnvironmentObject var app: AppState
    @State private var selectedTab = 0

    var body: some View {
        TabView(selection: $selectedTab) {
            NavigationStack {
                MapView()
                    .navigationTitle("地图")
                    .navigationBarTitleDisplayMode(.inline)
            }
            .tabItem { Label("地图", systemImage: "map") }
            .tag(0)

            NavigationStack {
                ControlView()
                    .navigationTitle("控制")
                    .navigationBarTitleDisplayMode(.inline)
            }
            .tabItem { Label("控制", systemImage: "gamecontroller") }
            .tag(1)

            NavigationStack {
                MonitorView()
                    .navigationTitle("监控")
                    .navigationBarTitleDisplayMode(.inline)
            }
            .tabItem { Label("监控", systemImage: "heart.text.square") }
            .tag(2)

            NavigationStack {
                SettingsView()
                    .navigationTitle("设置")
                    .navigationBarTitleDisplayMode(.inline)
            }
            .tabItem { Label("设置", systemImage: "gearshape") }
            .tag(3)
        }
        .tint(.blue)
        .sheet(isPresented: $app.showConnectionSheet) {
            ConnectionSheet()
                .environmentObject(app)
        }
    }
}
