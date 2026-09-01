@echo off
setlocal

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0start-expense-server.ps1" -Lan -EnableNotion %*

if errorlevel 1 (
  echo.
  echo Expense server stopped with an error.
  pause
)

endlocal