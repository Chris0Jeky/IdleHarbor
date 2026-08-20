@echo off
setlocal
PowerShell.exe -NoLogo -NoProfile -File "%~dp0install.ps1" %*
exit /b %ERRORLEVEL%
