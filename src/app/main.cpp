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
#include <QCoreApplication>
#include <QDir>
#include <QEasingCurve>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QLockFile>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QScreen>
#include <QSettings>
#include <QThread>
#include <QTimer>
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

  QSettings appearance_settings(QStringLiteral("state.ini"), QSettings::IniFormat);
  const bool dark_theme =
      appearance_settings.value(QStringLiteral("appearance/darkTheme"), false).toBool();
  const bool reduced_motion =
      appearance_settings.value(QStringLiteral("appearance/reducedMotion"), false).toBool();
  UiStyle::ApplyApplicationTheme(&a, nullptr, dark_theme);

  MainWindow main_window;
  if (!reduced_motion) {
    main_window.setWindowOpacity(0.0);
  }
  main_window.show();
  if (!reduced_motion) {
    QTimer::singleShot(0, &main_window, [&main_window]() {
      auto* reveal = new QPropertyAnimation(&main_window, "windowOpacity", &main_window);
      reveal->setDuration(180);
      reveal->setStartValue(0.0);
      reveal->setEndValue(1.0);
      reveal->setEasingCurve(QEasingCurve::OutCubic);
      reveal->start(QAbstractAnimation::DeleteWhenStopped);
    });
  }
  LOG_INFO("ros_qt5_gui_app init!");
  return a.exec();
}
