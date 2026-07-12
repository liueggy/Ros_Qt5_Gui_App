@echo off
setlocal
cd /d "%~dp0"
echo [INFO] Updating Ros_Qt5_Gui_App Windows binaries...
echo [INFO] Closing running Ros_Qt5_Gui_App processes...
taskkill /F /T /IM ros_qt5_gui_app.exe >nul 2>&1
rem Give Windows and antivirus scanners time to release loaded DLL handles.
timeout /t 2 /nobreak >nul
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
set "RESET_OK=0"
for /L %%I in (1,1,5) do (
  git reset --hard origin/windows-latest-bin <nul
  if not errorlevel 1 (
    set "RESET_OK=1"
    goto reset_done
  )
  echo [WARN] Files are still locked. Retrying in 2 seconds... ^(%%I/5^)
  timeout /t 2 /nobreak >nul
)
:reset_done
if "%RESET_OK%"=="0" (
  echo [ERROR] Update failed because one or more files are still in use.
  echo [ERROR] Close the app, Explorer preview windows, and antivirus scans, then retry.
  pause
  exit /b 1
)
if exist "%BACKUP_DIR%\config.json" copy /y "%BACKUP_DIR%\config.json" "config.json" >nul
if exist "%BACKUP_DIR%\state.ini" copy /y "%BACKUP_DIR%\state.ini" "state.ini" >nul
call run.bat
