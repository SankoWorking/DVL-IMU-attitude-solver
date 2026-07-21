@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0msc_download.ps1" %*
set ERR=%ERRORLEVEL%
if not "%MSC_NO_PAUSE%"=="1" (
  echo.
  echo MSC download finished with code %ERR%.
  echo Log: %~dp0..\MDK-ARM\H743\msc_download.log
)
exit /b %ERR%
