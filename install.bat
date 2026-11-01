@echo off
rem Installs ABSENToX. Usage: install.bat ["REAPER resource folder"]
set "REAPER=%APPDATA%\REAPER"
if not "%~1"=="" set "REAPER=%~1"

if not exist "%REAPER%\Effects\" (
  echo REAPER's Effects folder was not found at: %REAPER%\Effects
  echo Open REAPER once, or pass its folder: install.bat "D:\REAPER"
  pause
  exit /b 1
)

copy /Y "%~dp0absentox.jsfx" "%REAPER%\Effects\absentox.jsfx" >nul
if errorlevel 1 (
  echo Could not copy the plugin. Is the folder read-only?
  pause
  exit /b 1
)

echo Installed to %REAPER%\Effects. Restart REAPER, or press F5 in the FX browser.
pause
