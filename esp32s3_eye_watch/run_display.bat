@echo off
chcp 65001 >nul
echo ========================================
echo ESP32-S3-EYE display 工程自动编译烧录
echo ========================================

:: 1. 加载ESP-IDF环境
call C:\Espressif\frameworks\esp-idf-v5.4.3\export.ps1

:: 2. 进入工程目录
cd /d D:\prj\esp-bsp-master\examples\display

:: 3. 添加LVGL依赖
echo 正在添加LVGL依赖...
idf.py add-dependency "espressif/esp_lvgl_port^2.3.1"

:: 4. 设置目标芯片
echo 正在设置目标芯片esp32s3...
idf.py set-target esp32s3

:: 5. 加载BSP配置
echo 正在加载ESP32-S3-EYE BSP配置...
idf.py -D SDKCONFIG_DEFAULTS=sdkconfig.bsp.esp32_s3_eye reconfigure

:: 6. 清理旧编译文件
echo 正在清理旧编译文件...
idf.py fullclean

:: 7. 编译工程
echo 正在编译工程...
idf.py build

:: 8. 烧录+监控
echo 正在烧录到开发板(COM3)...
idf.py -p COM3 flash monitor

pause