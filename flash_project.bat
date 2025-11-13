@echo off
REM ========================================
REM ESP32 Dashboard Flash Script
REM ========================================

echo.
echo =======================================
echo ESP32 Dashboard - Flash Script
echo =======================================
echo.

REM Activate ESP-IDF environment
echo Activating ESP-IDF environment...
call "C:\esp-idf-v5.3.3\export.bat"

if errorlevel 1 (
    echo ERROR: Failed to activate ESP-IDF environment!
    pause
    exit /b 1
)

echo.
echo Flashing to ESP32 on COM13...
echo.

idf.py -p COM13 flash monitor

if errorlevel 1 (
    echo ERROR: Flash failed!
    pause
    exit /b 1
)

pause





