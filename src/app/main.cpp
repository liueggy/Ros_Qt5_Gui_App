/*
 * @Author: chengyangkj chengyangkj@qq.com
 * @Date: 2023-09-28 14:56:04
 * @LastEditors: chengyangkj chengyangkj@qq.com
 * @LastEditTime: 2023-10-05 11:39:01
 * @FilePath: /ROS2_Qt5_Gui_App/src/app/main.cpp
 */
#ifndef SDL_MAIN_HANDLED
  #define SDL_MAIN_HANDLED
#endif

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QLabel>
#include <QMovie>
#include <QPixmap>
#include <QSplashScreen>
#include <QThread>
#include <csignal>
#include <iostream>
#include "logger/logger.h"
#include "mainwindow.h"
#include "widgets/ui_style.h"

namespace {

void ApplyApplicationFont(QApplication* app) {
  const QStringList preferredFontFamilies = {
      "Microsoft YaHei UI",
      "Microsoft YaHei",
      "PingFang SC",
      "Noto Sans CJK SC",
      "Source Han Sans SC",
      "Segoe UI"};
  const QStringList availableFontFamilies = QFontDatabase().families();
  QString selectedFontFamily = QStringLiteral("Microsoft YaHei UI");
  for (const auto& fontFamily : preferredFontFamilies) {
    if (availableFontFamilies.contains(fontFamily)) {
      selectedFontFamily = fontFamily;
      break;
    }
  }
  QFont uiFont(selectedFontFamily, 10);
  uiFont.setStyleStrategy(QFont::PreferAntialias);
  app->setFont(uiFont);
}

}  // namespace
static QApplication* g_app = nullptr;

void signalHandler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    if (g_app) {
      g_app->exit(0);
    }
  }
}

int main(int argc, char* argv[]) {
  QApplication a(argc, argv);
  ApplyApplicationFont(&a);
  a.setStyleSheet(UiStyle::ApplicationStyleSheet());
  g_app = &a;

  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  MainWindow main_window;
  main_window.show();
  LOG_INFO("ros_qt5_gui_app init!");
  return a.exec();
}
