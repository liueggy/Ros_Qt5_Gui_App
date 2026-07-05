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
#include <QScreen>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QLabel>
#include <QLockFile>
#include <QMessageBox>
#include <QMovie>
#include <QPixmap>
#include <QSplashScreen>
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


QWidget* CreateOptionalSplashWindow(QMovie** movie_out) {
  const QString gif_path = QDir(QCoreApplication::applicationDirPath())
                               .filePath(QStringLiteral("assets/splash/startup.gif"));
  if (!QFileInfo::exists(gif_path)) {
    const QString source_tree_gif = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("../assets/splash/startup.gif"));
    if (!QFileInfo::exists(source_tree_gif)) {
      return nullptr;
    }
  }
  const QString resolved_path = QFileInfo::exists(gif_path)
                                    ? gif_path
                                    : QDir(QCoreApplication::applicationDirPath())
                                          .filePath(QStringLiteral("../assets/splash/startup.gif"));
  auto* splash = new QWidget(nullptr, Qt::SplashScreen | Qt::FramelessWindowHint);
  splash->setAttribute(Qt::WA_DeleteOnClose);
  auto* label = new QLabel(splash);
  auto* movie = new QMovie(resolved_path, QByteArray(), splash);
  label->setMovie(movie);
  QObject::connect(movie, &QMovie::frameChanged, splash, [splash, movie](int) {
    const QSize frame_size = movie->currentPixmap().size();
    if (frame_size.isValid() && splash->size() != frame_size) {
      splash->resize(frame_size);
      const QRect screen = QApplication::primaryScreen()
                               ? QApplication::primaryScreen()->availableGeometry()
                               : QRect(0, 0, 1280, 720);
      splash->move(screen.center() - QPoint(splash->width() / 2, splash->height() / 2));
    }
  });
  label->setAlignment(Qt::AlignCenter);
  label->resize(420, 260);
  splash->resize(label->size());
  movie->start();
  splash->show();
  if (movie_out) {
    *movie_out = movie;
  }
  return splash;
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
  a.setStyleSheet(UiStyle::ApplicationStyleSheet());
  g_app = &a;

  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  QMovie* splash_movie = nullptr;
  QWidget* splash_window = CreateOptionalSplashWindow(&splash_movie);
  MainWindow main_window;
  main_window.show();
  if (splash_window) {
    QTimer::singleShot(800, splash_window, &QWidget::close);
  }
  LOG_INFO("ros_qt5_gui_app init!");
  return a.exec();
}
