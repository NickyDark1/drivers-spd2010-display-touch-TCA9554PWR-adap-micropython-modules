/*
 * SPD2010 Touch Driver implementation for MicroPython
 * Adapted from Touch_SPD2010.cpp/.h from Arduino implementation
 */

 #include "py/obj.h"
 #include "py/runtime.h"
 #include "py/mphal.h"
 #include "driver/gpio.h"
 #include "esp_log.h"
 
 #define SPD2010_ADDR                0x53
 #define EXAMPLE_PIN_NUM_TOUCH_INT   4
 #define EXAMPLE_PIN_NUM_TOUCH_RST   (-1)
 #define CONFIG_ESP_LCD_TOUCH_MAX_POINTS 5
 
 // Structures for touch data
 typedef struct {
     uint8_t id;
     uint16_t x;
     uint16_t y;
     uint8_t weight;
 } tp_report_t;
 
 typedef struct {
     tp_report_t rpt[10];
     uint8_t touch_num;
     uint8_t pack_code;
     uint8_t down;
     uint8_t up;
     uint8_t gesture;
     uint16_t down_x;
     uint16_t down_y;
     uint16_t up_x;
     uint16_t up_y;
 } SPD2010_Touch;
 
 typedef struct {
     uint8_t none0:1;
     uint8_t none1:1;
     uint8_t none2:1;
     uint8_t cpu_run:1;
     uint8_t tint_low:1;
     uint8_t tic_in_cpu:1;
     uint8_t tic_in_bios:1;
     uint8_t tic_busy:1;
 } tp_status_high_t;
 
 typedef struct {
     uint8_t pt_exist:1;
     uint8_t gesture:1;
     uint8_t key:1;
     uint8_t aux:1;
     uint8_t keep:1;
     uint8_t raw_or_pt:1;
     uint8_t none6:1;
     uint8_t none7:1;
 } tp_status_low_t;
 
 typedef struct {
     tp_status_low_t status_low;
     tp_status_high_t status_high;
     uint16_t read_len;
 } tp_status_t;
 
 typedef struct {
     uint8_t status;
     uint16_t next_packet_len;
 } tp_hdp_status_t;
 
 // Global variables
 static SPD2010_Touch touch_data = {0};
 static uint8_t Touch_interrupts = 0;
 
 // External function references
 extern mp_obj_t tca9554_set_exio(mp_obj_t pin_obj, mp_obj_t state_obj);
 extern mp_obj_t i2c_driver_read(mp_obj_t addr_obj, mp_obj_t reg_obj, mp_obj_t data_obj, mp_obj_t len_obj);
 extern mp_obj_t i2c_driver_write(mp_obj_t addr_obj, mp_obj_t reg_obj, mp_obj_t data_obj, mp_obj_t len_obj);
 
 // Special I2C read for touch (16-bit register address)
 STATIC mp_obj_t spd2010_touch_read(mp_obj_t addr_obj, mp_obj_t reg_obj, mp_obj_t len_obj) {
     uint8_t addr = mp_obj_get_int(addr_obj);
     uint16_t reg = mp_obj_get_int(reg_obj);
     uint32_t len = mp_obj_get_int(len_obj);
     
     // Prepare register address bytes
     uint8_t reg_data[2] = {(uint8_t)(reg >> 8), (uint8_t)reg};
     mp_obj_t reg_buf = mp_obj_new_bytearray(2, reg_data);
     
     // Prepare data buffer
     uint8_t *data_buf_ptr = m_new(uint8_t, len);
     mp_obj_t data_buf = mp_obj_new_bytearray(len, data_buf_ptr);
     
     // I2C write to set register address
     mp_obj_t write_args[] = {
         addr_obj,
         mp_obj_new_int(0), // Dummy register, as we're sending the register in the data
         reg_buf,
         mp_obj_new_int(2)
     };
     mp_obj_t write_result = i2c_driver_write(write_args[0], write_args[1], write_args[2], write_args[3]);
     
     if (!mp_obj_is_true(write_result)) {
         printf("The I2C transmission fails. - I2C Read Touch\r\n");
         m_del(uint8_t, data_buf_ptr, len);
         return mp_const_none;
     }
     
     // I2C read to get data
     mp_obj_t read_args[] = {
         addr_obj,
         mp_obj_new_int(0), // Dummy register, as we already set it
         data_buf,
         len_obj
     };
     mp_obj_t read_result = i2c_driver_read(read_args[0], read_args[1], read_args[2], read_args[3]);
     
     if (!mp_obj_is_true(read_result)) {
         printf("The I2C transmission fails. - I2C Read Touch Data\r\n");
         m_del(uint8_t, data_buf_ptr, len);
         return mp_const_none;
     }
     
     return data_buf;
 }
 
 // Special I2C write for touch (16-bit register address)
 STATIC mp_obj_t spd2010_touch_write(mp_obj_t addr_obj, mp_obj_t reg_obj, mp_obj_t data_obj, mp_obj_t len_obj) {
     uint8_t addr = mp_obj_get_int(addr_obj);
     uint16_t reg = mp_obj_get_int(reg_obj);
     uint32_t len = mp_obj_get_int(len_obj);
     
     // Get data buffer info
     mp_buffer_info_t data_info;
     mp_get_buffer_raise(data_obj, &data_info, MP_BUFFER_READ);
     
     // Create combined buffer: reg high byte, reg low byte, data...
     uint8_t *combined_buf_ptr = m_new(uint8_t, 2 + len);
     combined_buf_ptr[0] = (uint8_t)(reg >> 8);
     combined_buf_ptr[1] = (uint8_t)reg;
     memcpy(&combined_buf_ptr[2], data_info.buf, len);
     
     mp_obj_t combined_buf = mp_obj_new_bytearray(2 + len, combined_buf_ptr);
     
     // I2C write
     mp_obj_t write_args[] = {
         addr_obj,
         mp_obj_new_int(0), // Dummy register, as we're sending the register in the data
         combined_buf,
         mp_obj_new_int(2 + len)
     };
     mp_obj_t write_result = i2c_driver_write(write_args[0], write_args[1], write_args[2], write_args[3]);
     
     m_del(uint8_t, combined_buf_ptr, 2 + len);
     
     if (!mp_obj_is_true(write_result)) {
         printf("The I2C transmission fails. - I2C Write Touch\r\n");
         return mp_obj_new_bool(false);
     }
     
     return mp_obj_new_bool(true);
 }
 
 // ISR for touch interrupt
 static void touch_isr_handler(void *arg) {
     Touch_interrupts = true;
 }
 
 // Initialize touch controller
 STATIC mp_obj_t spd2010_touch_init(void) {
     // Reset touch controller
     mp_obj_t reset_result = mp_call_function_0(MP_OBJ_FROM_PTR(&spd2010_touch_reset_obj));
     
     // Configure GPIO for touch interrupt
     gpio_config_t io_conf = {
         .mode = GPIO_MODE_INPUT,
         .pull_up_en = GPIO_PULLUP_ENABLE,
         .intr_type = GPIO_INTR_NEGEDGE,
         .pin_bit_mask = (1ULL << EXAMPLE_PIN_NUM_TOUCH_INT),
     };
     gpio_config(&io_conf);
     
     // Install GPIO ISR service and add ISR handler
     gpio_install_isr_service(0);
     gpio_isr_handler_add(EXAMPLE_PIN_NUM_TOUCH_INT, touch_isr_handler, NULL);
     
     // Read touch configuration
     // In a real implementation, you'd want to add the read_fw_version function here
     
     return mp_obj_new_int(1);
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_0(spd2010_touch_init_obj, spd2010_touch_init);
 
 // Reset touch controller
 STATIC mp_obj_t spd2010_touch_reset(void) {
     mp_obj_t args[] = {
         mp_obj_new_int(1), // EXIO_PIN1
         mp_obj_new_int(0)  // Low
     };
     tca9554_set_exio(args[0], args[1]);
     mp_hal_delay_ms(50);
     
     args[1] = mp_obj_new_int(1); // High
     tca9554_set_exio(args[0], args[1]);
     mp_hal_delay_ms(50);
     
     return mp_obj_new_int(1);
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_0(spd2010_touch_reset_obj, spd2010_touch_reset);
 
 // Write touch point mode command
 STATIC mp_obj_t spd2010_write_tp_point_mode_cmd(void) {
     uint8_t data[2] = {0x00, 0x00};
     mp_obj_t data_obj = mp_obj_new_bytearray(2, data);
     
     mp_obj_t args[] = {
         mp_obj_new_int(SPD2010_ADDR),
         mp_obj_new_int(0x5000),  // Command register
         data_obj,
         mp_obj_new_int(2)
     };
     
     mp_obj_t result = spd2010_touch_write(args[0], args[1], args[2], args[3]);
     mp_hal_delay_us(200);
     
     return result;
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_0(spd2010_write_tp_point_mode_cmd_obj, spd2010_write_tp_point_mode_cmd);
 
 // Write touch start command
 STATIC mp_obj_t spd2010_write_tp_start_cmd(void) {
     uint8_t data[2] = {0x00, 0x00};
     mp_obj_t data_obj = mp_obj_new_bytearray(2, data);
     
     mp_obj_t args[] = {
         mp_obj_new_int(SPD2010_ADDR),
         mp_obj_new_int(0x4600),  // Command register
         data_obj,
         mp_obj_new_int(2)
     };
     
     mp_obj_t result = spd2010_touch_write(args[0], args[1], args[2], args[3]);
     mp_hal_delay_us(200);
     
     return result;
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_0(spd2010_write_tp_start_cmd_obj, spd2010_write_tp_start_cmd);
 
 // Write touch CPU start command
 STATIC mp_obj_t spd2010_write_tp_cpu_start_cmd(void) {
     uint8_t data[2] = {0x01, 0x00};
     mp_obj_t data_obj = mp_obj_new_bytearray(2, data);
     
     mp_obj_t args[] = {
         mp_obj_new_int(SPD2010_ADDR),
         mp_obj_new_int(0x0400),  // Command register
         data_obj,
         mp_obj_new_int(2)
     };
     
     mp_obj_t result = spd2010_touch_write(args[0], args[1], args[2], args[3]);
     mp_hal_delay_us(200);
     
     return result;
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_0(spd2010_write_tp_cpu_start_cmd_obj, spd2010_write_tp_cpu_start_cmd);
 
 // Write touch clear interrupt command
 STATIC mp_obj_t spd2010_write_tp_clear_int_cmd(void) {
     uint8_t data[2] = {0x01, 0x00};
     mp_obj_t data_obj = mp_obj_new_bytearray(2, data);
     
     mp_obj_t args[] = {
         mp_obj_new_int(SPD2010_ADDR),
         mp_obj_new_int(0x0200),  // Command register
         data_obj,
         mp_obj_new_int(2)
     };
     
     mp_obj_t result = spd2010_touch_write(args[0], args[1], args[2], args[3]);
     mp_hal_delay_us(200);
     
     return result;
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_0(spd2010_write_tp_clear_int_cmd_obj, spd2010_write_tp_clear_int_cmd);
 
 // Read touch status length
 STATIC mp_obj_t spd2010_read_tp_status_length(void) {
     mp_obj_t args[] = {
         mp_obj_new_int(SPD2010_ADDR),
         mp_obj_new_int(0x2000),  // Status register
         mp_obj_new_int(4)        // Read 4 bytes
     };
     
     mp_obj_t data = spd2010_touch_read(args[0], args[1], args[2]);
     if (data == mp_const_none) {
         return mp_const_none;
     }
     
     // Extract data from buffer
     mp_buffer_info_t buf_info;
     mp_get_buffer_raise(data, &buf_info, MP_BUFFER_READ);
     uint8_t *sample_data = (uint8_t *)buf_info.buf;
     
     // Create a status dictionary to return
     mp_obj_t status_dict = mp_obj_new_dict(3);
     
     // Status low byte
     uint8_t status_low = sample_data[0];
     mp_obj_dict_store(status_dict, mp_obj_new_str("pt_exist", 8), 
                      mp_obj_new_bool(status_low & 0x01));
     mp_obj_dict_store(status_dict, mp_obj_new_str("gesture", 7), 
                      mp_obj_new_bool(status_low & 0x02));
     mp_obj_dict_store(status_dict, mp_obj_new_str("aux", 3), 
                      mp_obj_new_bool(status_low & 0x08));
     
     // Status high byte
     uint8_t status_high = sample_data[1];
     mp_obj_dict_store(status_dict, mp_obj_new_str("tic_busy", 8), 
                      mp_obj_new_bool(status_high & 0x80));
     mp_obj_dict_store(status_dict, mp_obj_new_str("tic_in_bios", 11), 
                      mp_obj_new_bool(status_high & 0x40));
     mp_obj_dict_store(status_dict, mp_obj_new_str("tic_in_cpu", 10), 
                      mp_obj_new_bool(status_high & 0x20));
     mp_obj_dict_store(status_dict, mp_obj_new_str("tint_low", 8), 
                      mp_obj_new_bool(status_high & 0x10));
     mp_obj_dict_store(status_dict, mp_obj_new_str("cpu_run", 7), 
                      mp_obj_new_bool(status_high & 0x08));
     
     // Read length
     uint16_t read_len = (sample_data[3] << 8) | sample_data[2];
     mp_obj_dict_store(status_dict, mp_obj_new_str("read_len", 8), 
                      mp_obj_new_int(read_len));
     
     return status_dict;
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_0(spd2010_read_tp_status_length_obj, spd2010_read_tp_status_length);
 
 // Read touch data
 STATIC mp_obj_t spd2010_tp_read_data(void) {
     // Get touch status
     mp_obj_t status_dict = mp_call_function_0(MP_OBJ_FROM_PTR(&spd2010_read_tp_status_length_obj));
     
     if (status_dict == mp_const_none) {
         return mp_const_none;
     }
     
     // Extract status information
     mp_obj_t tic_in_bios = mp_obj_dict_get(status_dict, mp_obj_new_str("tic_in_bios", 11));
     mp_obj_t tic_in_cpu = mp_obj_dict_get(status_dict, mp_obj_new_str("tic_in_cpu", 10));
     mp_obj_t cpu_run = mp_obj_dict_get(status_dict, mp_obj_new_str("cpu_run", 7));
     mp_obj_t read_len = mp_obj_dict_get(status_dict, mp_obj_new_str("read_len", 8));
     mp_obj_t pt_exist = mp_obj_dict_get(status_dict, mp_obj_new_str("pt_exist", 8));
     mp_obj_t gesture = mp_obj_dict_get(status_dict, mp_obj_new_str("gesture", 7));
     mp_obj_t aux = mp_obj_dict_get(status_dict, mp_obj_new_str("aux", 3));
     
     // Process based on status
     if (mp_obj_is_true(tic_in_bios)) {
         // Clear interrupt
         mp_call_function_0(MP_OBJ_FROM_PTR(&spd2010_write_tp_clear_int_cmd_obj));
         // Start CPU
         mp_call_function_0(MP_OBJ_FROM_PTR(&spd2010_write_tp_cpu_start_cmd_obj));
     } 
     else if (mp_obj_is_true(tic_in_cpu)) {
         // Set point mode
         mp_call_function_0(MP_OBJ_FROM_PTR(&spd2010_write_tp_point_mode_cmd_obj));
         // Start touch
         mp_call_function_0(MP_OBJ_FROM_PTR(&spd2010_write_tp_start_cmd_obj));
         // Clear interrupt
         mp_call_function_0(MP_OBJ_FROM_PTR(&spd2010_write_tp_clear_int_cmd_obj));
     } 
     else if (mp_obj_is_true(cpu_run) && mp_obj_get_int(read_len) == 0) {
         // Just clear interrupt
         mp_call_function_0(MP_OBJ_FROM_PTR(&spd2010_write_tp_clear_int_cmd_obj));
     } 
     else if (mp_obj_is_true(pt_exist) || mp_obj_is_true(gesture)) {
         // Read touch point data - here we would parse the touch data
         // This is complex and would need a more detailed implementation
         
         // For now, just clear interrupt
         mp_call_function_0(MP_OBJ_FROM_PTR(&spd2010_write_tp_clear_int_cmd_obj));
     } 
     else if (mp_obj_is_true(cpu_run) && mp_obj_is_true(aux)) {
         // Just clear interrupt
         mp_call_function_0(MP_OBJ_FROM_PTR(&spd2010_write_tp_clear_int_cmd_obj));
     }
     
     return mp_const_none;
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_0(spd2010_tp_read_data_obj, spd2010_tp_read_data);
 
 // Read and process touch data
 STATIC mp_obj_t spd2010_touch_read_data(void) {
     // Process touch data based on interrupts/status
     mp_call_function_0(MP_OBJ_FROM_PTR(&spd2010_tp_read_data_obj));
     
     // In a full implementation, this would update the touch_data structure
     
     return mp_const_none;
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_0(spd2010_touch_read_data_obj, spd2010_touch_read_data);
 
 // Get touch coordinates
 STATIC mp_obj_t spd2010_touch_get_xy(void) {
     // Read touch data
     mp_call_function_0(MP_OBJ_FROM_PTR(&spd2010_touch_read_data_obj));
     
     // Prepare return dictionary
     mp_obj_t dict = mp_obj_new_dict(3);
     
     // Get number of touch points (clipped to max supported)
     uint8_t point_num = (touch_data.touch_num > CONFIG_ESP_LCD_TOUCH_MAX_POINTS) ? 
                           CONFIG_ESP_LCD_TOUCH_MAX_POINTS : touch_data.touch_num;
     
     // Add data to dictionary
     mp_obj_dict_store(dict, mp_obj_new_str("pressed", 7), mp_obj_new_bool(point_num > 0));
     mp_obj_dict_store(dict, mp_obj_new_str("points", 6), mp_obj_new_int(point_num));
     
     // If we have touch points, add first point coordinates
     if (point_num > 0) {
         mp_obj_dict_store(dict, mp_obj_new_str("x", 1), mp_obj_new_int(touch_data.rpt[0].x));
         mp_obj_dict_store(dict, mp_obj_new_str("y", 1), mp_obj_new_int(touch_data.rpt[0].y));
         mp_obj_dict_store(dict, mp_obj_new_str("weight", 6), mp_obj_new_int(touch_data.rpt[0].weight));
     } else {
         mp_obj_dict_store(dict, mp_obj_new_str("x", 1), mp_obj_new_int(0));
         mp_obj_dict_store(dict, mp_obj_new_str("y", 1), mp_obj_new_int(0));
         mp_obj_dict_store(dict, mp_obj_new_str("weight", 6), mp_obj_new_int(0));
     }
     
     // Clear available touch points count
     touch_data.touch_num = 0;
     
     return dict;
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_0(spd2010_touch_get_xy_obj, spd2010_touch_get_xy);
 
 // Module globals table
 STATIC const mp_rom_map_elem_t spd2010_touch_module_globals_table[] = {
     { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_spd2010_touch) },
     
     // Constants
     { MP_ROM_QSTR(MP_QSTR_SPD2010_ADDR), MP_ROM_INT(SPD2010_ADDR) },
     { MP_ROM_QSTR(MP_QSTR_TOUCH_INT_PIN), MP_ROM_INT(EXAMPLE_PIN_NUM_TOUCH_INT) },
     { MP_ROM_QSTR(MP_QSTR_MAX_TOUCH_POINTS), MP_ROM_INT(CONFIG_ESP_LCD_TOUCH_MAX_POINTS) },
     
     // Functions
     { MP_ROM_QSTR(MP_QSTR_Touch_Init), MP_ROM_PTR(&spd2010_touch_init_obj) },
     { MP_ROM_QSTR(MP_QSTR_SPD2010_Touch_Reset), MP_ROM_PTR(&spd2010_touch_reset_obj) },
     { MP_ROM_QSTR(MP_QSTR_write_tp_point_mode_cmd), MP_ROM_PTR(&spd2010_write_tp_point_mode_cmd_obj) },
     { MP_ROM_QSTR(MP_QSTR_write_tp_start_cmd), MP_ROM_PTR(&spd2010_write_tp_start_cmd_obj) },
     { MP_ROM_QSTR(MP_QSTR_write_tp_cpu_start_cmd), MP_ROM_PTR(&spd2010_write_tp_cpu_start_cmd_obj) },
     { MP_ROM_QSTR(MP_QSTR_write_tp_clear_int_cmd), MP_ROM_PTR(&spd2010_write_tp_clear_int_cmd_obj) },
     { MP_ROM_QSTR(MP_QSTR_read_tp_status_length), MP_ROM_PTR(&spd2010_read_tp_status_length_obj) },
     { MP_ROM_QSTR(MP_QSTR_tp_read_data), MP_ROM_PTR(&spd2010_tp_read_data_obj) },
     { MP_ROM_QSTR(MP_QSTR_Touch_Read_Data), MP_ROM_PTR(&spd2010_touch_read_data_obj) },
     { MP_ROM_QSTR(MP_QSTR_Touch_Get_xy), MP_ROM_PTR(&spd2010_touch_get_xy_obj) },
 };
 STATIC MP_DEFINE_CONST_DICT(spd2010_touch_module_globals, spd2010_touch_module_globals_table);
 
 // Module definition
 const mp_obj_module_t spd2010_touch_user_cmodule = {
     .base = { &mp_type_module },
     .globals = (mp_obj_dict_t *)&spd2010_touch_module_globals,
 };
 
 // Register the module
 MP_REGISTER_MODULE(MP_QSTR_spd2010_touch, spd2010_touch_user_cmodule);