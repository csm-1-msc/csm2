/*
 * @file watch_ui.h
 * @brief Watch UI header file for color control
 */

#ifndef WATCH_UI_H
#define WATCH_UI_H

// External function declarations
void watch_switch_style(void);
void watch_switch_ui(void);

// Forward declaration for color_mode_t (defined in lvgl_demo_ui.c)
typedef enum {
    COLOR_MODE_AUTO = 0,   // Auto cycle through colors
    COLOR_MODE_MANUAL      // Manual color control
} color_mode_t;

color_mode_t get_color_mode(void);

#endif // WATCH_UI_H
