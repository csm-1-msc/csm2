@echo off
chcp 65001 >nul
echo ========================================
echo ESP32-S3-EYE Watch 工程编译烧录
echo ========================================

:: 1. 进入工程目录
cd /d D:\MyHomework\PavementDamagesG-7\esp32s3_eye_watch

:: 2. 加载 ESP-IDF 环境
call C:\Espressif\frameworks\esp-idf-v5.4.3\export.bat

:: 3. 编译工程
echo 正在编译工程...
idf.py build

if %errorlevel% neq 0 (
    echo 编译失败！
    pause
    exit /b 1
)

:: 4. 烧录到开发板
echo 正在烧录到开发板...
idf.py -p COM3 flash

if %errorlevel% neq 0 (
    echo 烧录失败！
    pause
    exit /b 1
)

echo ========================================
echo 编译烧录完成！
echo ========================================
pause