@echo off
setlocal
cd /d "%~dp0"
echo [INFO] Updating Ros_Qt5_Gui_App Windows binaries...
set "BACKUP_DIR=%TEMP%\ros_qt5_gui_app_local"
if not exist "%BACKUP_DIR%" mkdir "%BACKUP_DIR%"
if exist "config.json" copy /y "config.json" "%BACKUP_DIR%\config.json" >nul
if exist "state.ini" copy /y "state.ini" "%BACKUP_DIR%\state.ini" >nul
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
if exist "%BACKUP_DIR%\config.json" copy /y "%BACKUP_DIR%\config.json" "config.json" >nul
if exist "%BACKUP_DIR%\state.ini" copy /y "%BACKUP_DIR%\state.ini" "state.ini" >nul
call run.bat
