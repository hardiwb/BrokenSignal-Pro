@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0configure-notion.ps1" %*
exit /b %ERRORLEVEL%
