@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0keil_build.ps1" -Action build
if errorlevel 1 exit /b %ERRORLEVEL%
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0dfu_download.ps1" %*
exit /b %ERRORLEVEL%
