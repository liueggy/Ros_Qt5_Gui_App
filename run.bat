@echo off
setlocal
cd /d "%~dp0"
if not exist "ros_qt5_gui_app.exe" (
  echo [ERROR] ros_qt5_gui_app.exe not found in %CD%
  pause
  exit /b 1
)
"ros_qt5_gui_app.exe"
set "APP_EXIT=%ERRORLEVEL%"
if not "%APP_EXIT%"=="0" (
  echo [ERROR] ros_qt5_gui_app.exe exited with code %APP_EXIT%.
  pause
)
exit /b %APP_EXIT%
