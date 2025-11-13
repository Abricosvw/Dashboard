@echo off
REM Quick build without full reconfigure
call "C:\esp-idf-v5.3.3\export.bat" >nul 2>&1
cd /d "%~dp0"
idf.py build
pause





