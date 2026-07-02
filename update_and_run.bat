@echo off
setlocal
cd /d "%~dp0"
echo [INFO] Updating Ros_Qt5_Gui_App Windows binaries...
git fetch --depth=1 origin windows-latest-bin
if errorlevel 1 (
  echo [ERROR] git fetch failed.
  pause
  exit /b 1
)
git reset --hard origin/windows-latest-bin
if errorlevel 1 (
  echo [ERROR] git reset failed.
  pause
  exit /b 1
)
call run.bat
