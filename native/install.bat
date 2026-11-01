@echo off
rem Installs ABSENToX as VST3 and CLAP. Those folders need administrator rights.
net session >nul 2>&1
if errorlevel 1 (
  echo Please right-click install.bat and choose Run as administrator.
  pause
  exit /b 1
)

set "COMMON=%CommonProgramFiles%"
if not exist "%COMMON%\VST3\" mkdir "%COMMON%\VST3"
if not exist "%COMMON%\CLAP\" mkdir "%COMMON%\CLAP"

if exist "%COMMON%\VST3\ABSENToX.vst3\" rmdir /s /q "%COMMON%\VST3\ABSENToX.vst3"
xcopy /e /i /y /q "%~dp0ABSENToX.vst3" "%COMMON%\VST3\ABSENToX.vst3" >nul
if errorlevel 1 goto failed
copy /y "%~dp0ABSENToX.clap" "%COMMON%\CLAP\ABSENToX.clap" >nul
if errorlevel 1 goto failed

echo Installed to %COMMON%\VST3 and %COMMON%\CLAP. Rescan your plugins, or restart your DAW.
pause
exit /b 0

:failed
echo Could not copy the plugin. Is your DAW still open?
pause
exit /b 1
