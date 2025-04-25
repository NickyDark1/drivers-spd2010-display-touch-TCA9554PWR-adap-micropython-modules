/*
 * LVGL Driver for MicroPython
 * Adapted from LVGL_Driver.cpp/.h
 */

#include "py/obj.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "lvgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#define TAG "lvgl_driver"
#define LVGL_BUF_LEN (LV_HOR_RES_MAX * LV_VER_RES_MAX / 10)
#define EXAMPLE_LVGL_TICK_PERIOD_MS 2

// Forward declarations
extern mp_obj_t spd2010_display_add_window(mp_obj_t x_start_obj, mp_obj_t y_start_obj, mp_obj_t x_end_obj, mp_obj_t y_end_obj, mp_obj_t color_obj);
extern mp_obj_t spd2010_touch_get_xy(void);

// Display buffer for LVGL
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[LVGL_BUF_LEN];
static lv_color_t buf2[LVGL_BUF_LEN];
static esp_timer_handle_t lvgl_tick_timer = NULL;

// Print callback for LVGL
void lvgl_print(const char *buf) {
    // Use ESP-IDF logging instead of Serial
    ESP_LOGI(TAG, "%s", buf);
}

// Rounder callback for LVGL
void lvgl_port_rounder_callback(struct _lv_disp_drv_t *disp_drv, lv_area_t *area) {
    uint16_t x1 = area->x1;
    uint16_t x2 = area->x2;

    // Round the start of coordinate down to the nearest 4M number
    area->x1 = (x1 >> 2) << 2;

    // Round the end of coordinate up to the nearest 4N+3 number
    area->x2 = ((x2 >> 2) << 2) + 3;
}

// Display flush callback for LVGL
void lvgl_display_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    mp_obj_t args[] = {
        mp_obj_new_int(area->x1),
        mp_obj_new_int(area->y1),
        mp_obj_new_int(area->x2),
        mp_obj_new_int(area->y2),
        mp_obj_new_bytearray((area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1) * sizeof(lv_color_t), (uint8_t*)color_p)
    };
    
    spd2010_display_add_window(args[0], args[1], args[2], args[3], args[4]);
    lv_disp_flush_ready(disp_drv);
}

// Touch read callback for LVGL
void lvgl_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    mp_obj_t touch_data = spd2010_touch_get_xy();
    
    if (touch_data != mp_const_none) {
        mp_obj_t pressed_obj = mp_obj_dict_get(touch_data, mp_obj_new_str("pressed", 7));
        
        if (mp_obj_is_true(pressed_obj)) {
            mp_obj_t x_obj = mp_obj_dict_get(touch_data, mp_obj_new_str("x", 1));
            mp_obj_t y_obj = mp_obj_dict_get(touch_data, mp_obj_new_str("y", 1));
            
            data->point.x = mp_obj_get_int(x_obj);
            data->point.y = mp_obj_get_int(y_obj);
            data->state = LV_INDEV_STATE_PR;
        } else {
            data->state = LV_INDEV_STATE_REL;
        }
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// LVGL tick increment callback
void lvgl_tick_inc_cb(void *arg) {
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

// Initialize LVGL
STATIC mp_obj_t lvgl_driver_init(void) {
    // Initialize LVGL
    lv_init();
    
    // Initialize display buffer
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LVGL_BUF_LEN);

    // Initialize display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LV_HOR_RES_MAX;
    disp_drv.ver_res = LV_VER_RES_MAX;
    disp_drv.flush_cb = lvgl_display_flush;
    disp_drv.rounder_cb = lvgl_port_rounder_callback;
    disp_drv.full_refresh = 1;                    // 1: Always make the whole screen redrawn
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    
    // Initialize touch input driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touchpad_read;
    lv_indev_drv_register(&indev_drv);
    
    // Create a simple label to test
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello Arduino and LVGL!");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    
    // Set up periodic timer for LVGL ticks
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &lvgl_tick_inc_cb,
        .name = "lvgl_tick"
    };
    if (esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LVGL tick timer");
        return mp_const_false;
    }
    
    // Start timer with period in microseconds
    if (esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start LVGL tick timer");
        return mp_const_false;
    }
    
    return mp_const_true;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lvgl_driver_init_obj, lvgl_driver_init);

// LVGL task handler
STATIC mp_obj_t lvgl_driver_loop(void) {
    lv_task_handler();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lvgl_driver_loop_obj, lvgl_driver_loop);

// Print function for LVGL
STATIC mp_obj_t lvgl_driver_print(mp_obj_t buf_obj) {
    const char *buf = mp_obj_str_get_str(buf_obj);
    lvgl_print(buf);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lvgl_driver_print_obj, lvgl_driver_print);

// Module cleanup
STATIC mp_obj_t lvgl_driver_deinit(void) {
    if (lvgl_tick_timer != NULL) {
        esp_timer_stop(lvgl_tick_timer);
        esp_timer_delete(lvgl_tick_timer);
        lvgl_tick_timer = NULL;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lvgl_driver_deinit_obj, lvgl_driver_deinit);

// Module globals table
STATIC const mp_rom_map_elem_t lvgl_driver_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_lvgl_driver) },
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&lvgl_driver_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_loop), MP_ROM_PTR(&lvgl_driver_loop_obj) },
    { MP_ROM_QSTR(MP_QSTR_print), MP_ROM_PTR(&lvgl_driver_print_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&lvgl_driver_deinit_obj) },
    
    // Constants
    { MP_ROM_QSTR(MP_QSTR_TICK_PERIOD_MS), MP_ROM_INT(EXAMPLE_LVGL_TICK_PERIOD_MS) },
    { MP_ROM_QSTR(MP_QSTR_BUFFER_SIZE), MP_ROM_INT(LVGL_BUF_LEN) },
};
STATIC MP_DEFINE_CONST_DICT(lvgl_driver_module_globals, lvgl_driver_module_globals_table);

// Module definition
const mp_obj_module_t lvgl_driver_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&lvgl_driver_module_globals,
};

// Register the module
MP_REGISTER_MODULE(MP_QSTR_lvgl_driver, lvgl_driver_user_cmodule);