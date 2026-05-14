/*
 * wifi_connect.h - WiFi 连接模块
 * @brief 提供 WiFi 连接和 NTP 时间同步功能
 */

#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include <time.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 WiFi 模块
 * @return ESP_OK 成功
 */
esp_err_t wifi_connect_init(void);

/**
 * @brief 连接到 WiFi 网络
 * @param ssid WiFi 名称
 * @param password WiFi 密码
 * @return ESP_OK 成功
 */
esp_err_t wifi_connect(const char *ssid, const char *password);

/**
 * @brief 断开 WiFi 连接
 */
void wifi_disconnect(void);

/**
 * @brief 检查 WiFi 是否已连接
 * @return true 已连接
 */
bool wifi_is_connected(void);

/**
 * @brief 初始化 NTP 时间同步
 * @param timezone 时区字符串，例如 "CST-8" 表示中国标准时间
 * @return ESP_OK 成功
 */
esp_err_t ntp_init(const char *timezone);

/**
 * @brief 获取当前时间
 * @param timeinfo 时间结构体指针
 * @return true 成功获取时间（时间有效）
 */
bool ntp_get_time(struct tm *timeinfo);

/**
 * @brief 等待 NTP 时间同步完成
 * @param timeout_ms 超时时间（毫秒）
 * @return true 同步成功
 */
bool ntp_wait_for_sync(long timeout_ms);

/**
 * @brief 获取当前时间戳
 * @return 当前时间戳
 */
time_t ntp_get_timestamp(void);

/**
 * @brief 获取 WiFi IP 地址
 * @param buf 缓冲区
 * @param buf_size 缓冲区大小
 * @return 成功返回 IP 地址字符串
 */
const char* wifi_get_ip(char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif // WIFI_CONNECT_H