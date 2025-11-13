@echo off
REM ========================================
REM ESP32 Dashboard Build Script
REM ========================================

echo.
echo =======================================
echo ESP32 Dashboard - Build Script
echo =======================================
echo.

REM Check if we're in the correct directory
if not exist "CMakeLists.txt" (
    echo ERROR: CMakeLists.txt not found!
    echo Please run this script from the project root directory.
    pause
    exit /b 1
)

REM Activate ESP-IDF environment
echo [1/3] Activating ESP-IDF environment...
call "C:\esp-idf-v5.3.3\export.bat"

if errorlevel 1 (
    echo ERROR: Failed to activate ESP-IDF environment!
    pause
    exit /b 1
)

echo.
echo [2/3] Configuring project...
idf.py reconfigure

if errorlevel 1 (
    echo ERROR: Configuration failed!
    pause
    exit /b 1
)

echo.
echo [3/3] Building project...
idf.py build

if errorlevel 1 (
    echo ERROR: Build failed!
    pause
    exit /b 1
)

echo.
echo =======================================
echo BUILD SUCCESSFUL!
echo =======================================
echo.
echo To flash to ESP32:
echo   idf.py -p COM13 flash monitor
echo.
echo Or run: flash_project.bat
echo.

pause





