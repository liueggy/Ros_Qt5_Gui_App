@echo off
setlocal
cd /d "%~dp0"
if not exist "ros_qt5_gui_app.exe" (
  echo [ERROR] ros_qt5_gui_app.exe not found in %CD%
  pause
  exit /b 1
)
start "" "ros_qt5_gui_app.exe"
