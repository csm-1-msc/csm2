/*
 * 8_sd_card_capture - SD 卡数据采集模块头文件
 * @brief SD 卡初始化、数据记录、文件管理
 */

#ifndef SD_CARD_CAPTURE_H
#define SD_CARD_CAPTURE_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SD 卡采集退出回调函数类型
 * 
 * 当用户长按按钮退出 SD 卡模式时调用此回调
 */
typedef void (*sd_capture_exit_callback_t)(void);

/**
 * @brief 初始化 SD 卡采集模块
 * 
 * @param on_exit 退出回调函数，当用户长按按钮退出时调用
 * @return esp_err_t ESP_OK 表示成功，其他表示失败
 */
esp_err_t sd_capture_init(sd_capture_exit_callback_t on_exit);

/**
 * @brief 反初始化 SD 卡采集模块
 * 
 * 停止采集、关闭文件、释放资源
 */
void sd_capture_deinit(void);

/**
 * @brief 开始数据采集
 * 
 * @return esp_err_t ESP_OK 表示成功，其他表示失败
 */
esp_err_t sd_capture_start(void);

/**
 * @brief 停止数据采集
 */
void sd_capture_stop(void);

/**
 * @brief 切换采集状态（开始/停止）
 * 
 * 如果当前在采集中则停止，否则开始采集
 */
void sd_capture_toggle(void);

/**
 * @brief 处理长按按钮事件
 * 
 * 停止采集并调用退出回调，返回到手表 UI
 */
void sd_capture_handle_long_press(void);

/**
 * @brief 检查是否正在采集
 * 
 * @return true 正在采集
 * @return false 未采集
 */
bool sd_capture_is_collecting(void);

/**
 * @brief 检查 SD 卡是否就绪
 * 
 * @return true SD 卡已挂载
 * @return false SD 卡未挂载
 */
bool sd_capture_is_sd_ready(void);

/**
 * @brief 获取已采集的样本数量
 * 
 * @return uint32_t 样本数量
 */
uint32_t sd_capture_get_sample_count(void);

/**
 * @brief 获取当前采集文件路径
 * 
 * @return const char* 文件路径，如果无活动文件则返回 NULL
 */
const char* sd_capture_get_current_file(void);

/**
 * @brief 退出 SD 卡采集模式
 * 
 * 停止采集、销毁 UI、清理资源，并调用退出回调返回手表模式
 */
void sd_capture_exit(void);

#ifdef __cplusplus
}
#endif

#endif // SD_CARD_CAPTURE_H