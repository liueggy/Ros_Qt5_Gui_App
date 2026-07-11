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
#include <QColor>
#include <QPalette>
#include <QScreen>
#include <QSettings>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QLockFile>
#include <QMessageBox>
#include <QThread>
#include <QTimer>
#include <csignal>
#include <iostream>
#include "logger/logger.h"
#include "mainwindow.h"
#include "widgets/ui_style.h"

namespace {

constexpr int kRestartForThemeChange = 773;

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

void ApplyApplicationPalette(QApplication* app) {
  QPalette palette;
  palette.setColor(QPalette::Window, QColor(UiStyle::Palette::Background));
  palette.setColor(QPalette::WindowText, QColor(UiStyle::Palette::Text));
  palette.setColor(QPalette::Base, QColor(UiStyle::Palette::Surface));
  palette.setColor(QPalette::AlternateBase, QColor(UiStyle::Palette::SurfaceAlt));
  palette.setColor(QPalette::Text, QColor(UiStyle::Palette::Text));
  palette.setColor(QPalette::Button, QColor(UiStyle::Palette::Surface));
  palette.setColor(QPalette::ButtonText, QColor(UiStyle::Palette::Text));
  palette.setColor(QPalette::Highlight, QColor(UiStyle::Palette::Primary));
  palette.setColor(QPalette::HighlightedText,
                   QColor(UiStyle::Palette::TextOnPrimary));
  palette.setColor(QPalette::Disabled, QPalette::Text,
                   QColor(UiStyle::Palette::DisabledText));
  palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                   QColor(UiStyle::Palette::DisabledText));
  palette.setColor(QPalette::Disabled, QPalette::Base,
                   QColor(UiStyle::Palette::DisabledBg));
  app->setPalette(palette);
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
  QLockFile single_instance_lock(
      QDir::temp().absoluteFilePath(QStringLiteral("ros_qt5_gui_app.lock")));
  // A previous abnormal/slow startup can leave a short-lived lock behind on
  // Windows, which made the app look like it "flashed and exited" for several
  // attempts. Keep the single-instance guard, but recover stale locks quickly.
  single_instance_lock.setStaleLockTime(2000);
  if (!single_instance_lock.tryLock(300)) {
    single_instance_lock.removeStaleLockFile();
  }
  if (!single_instance_lock.isLocked() && !single_instance_lock.tryLock(300)) {
    QMessageBox::information(nullptr, QStringLiteral("程序已在运行"),
                             QStringLiteral("ROS Qt5 控制端已经启动，请切换到现有窗口。"));
    return 0;
  }
  ApplyApplicationFont(&a);
  g_app = &a;

  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  int result = 0;
  do {
    QSettings appearance_settings(QStringLiteral("state.ini"), QSettings::IniFormat);
    UiStyle::SetDarkTheme(
        appearance_settings.value(QStringLiteral("appearance/darkTheme"), false).toBool());
    ApplyApplicationPalette(&a);
    a.setStyleSheet(UiStyle::ApplicationStyleSheet());
    MainWindow main_window;
    main_window.show();
    LOG_INFO("ros_qt5_gui_app init!");
    result = a.exec();
  } while (result == kRestartForThemeChange);
  return result;
}
