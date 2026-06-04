// EggyCarApp.swift — App 入口
import SwiftUI

@main
struct EggyCarApp: App {
    @StateObject private var appState = AppState()

    var body: some Scene {
        WindowGroup {
            MainTabView()
                .environmentObject(appState)
                .onAppear {
                    appState.loadSavedConfig()
                    if !appState.showConnectionSheet {
                        appState.connect()
                    }
                }
        }
    }
}
