@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0dfu_download.ps1" %*
set ERR=%ERRORLEVEL%
if not "%DFU_NO_PAUSE%"=="1" (
  echo.
  echo DFU download finished with code %ERR%.
  echo Log: %~dp0..\MDK-ARM\H743\dfu_download.log
)
exit /b %ERR%
