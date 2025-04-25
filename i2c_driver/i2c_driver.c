/*
 * I2C Driver implementation for MicroPython
 * Adapted from I2C_Driver.cpp/.h from Arduino implementation
 */

 #include "py/obj.h"
 #include "py/runtime.h"
 #include "py/mphal.h"
 #include "driver/i2c.h"
 #include "esp_log.h"
 
 #define I2C_MASTER_FREQ_HZ  (400000)
 #define I2C_SCL_PIN         10
 #define I2C_SDA_PIN         11
 #define I2C_PORT            I2C_NUM_0
 
 // Initialize I2C bus with default settings
 STATIC mp_obj_t i2c_driver_init(void) {
     i2c_config_t conf = {
         .mode = I2C_MODE_MASTER,
         .sda_io_num = I2C_SDA_PIN,
         .scl_io_num = I2C_SCL_PIN,
         .sda_pullup_en = GPIO_PULLUP_ENABLE,
         .scl_pullup_en = GPIO_PULLUP_ENABLE,
         .master.clk_speed = I2C_MASTER_FREQ_HZ,
     };
     
     ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &conf));
     ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0));
     
     return mp_const_none;
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_0(i2c_driver_init_obj, i2c_driver_init);
 
 // Read data from I2C device
 STATIC mp_obj_t i2c_driver_read(mp_obj_t driver_addr_obj, mp_obj_t reg_addr_obj, mp_obj_t reg_data_obj, mp_obj_t length_obj) {
     uint8_t driver_addr = mp_obj_get_int(driver_addr_obj);
     uint8_t reg_addr = mp_obj_get_int(reg_addr_obj);
     mp_buffer_info_t reg_data_info;
     mp_get_buffer_raise(reg_data_obj, &reg_data_info, MP_BUFFER_WRITE);
     uint32_t length = mp_obj_get_int(length_obj);
     
     esp_err_t ret;
     i2c_cmd_handle_t cmd = i2c_cmd_link_create();
     i2c_master_start(cmd);
     i2c_master_write_byte(cmd, (driver_addr << 1) | I2C_MASTER_WRITE, true);
     i2c_master_write_byte(cmd, reg_addr, true);
     i2c_master_stop(cmd);
     ret = i2c_master_cmd_begin(I2C_PORT, cmd, 1000 / portTICK_PERIOD_MS);
     i2c_cmd_link_delete(cmd);
     
     if (ret != ESP_OK) {
         printf("The I2C transmission fails. - I2C Read\r\n");
         return mp_obj_new_bool(false);
     }
     
     cmd = i2c_cmd_link_create();
     i2c_master_start(cmd);
     i2c_master_write_byte(cmd, (driver_addr << 1) | I2C_MASTER_READ, true);
     if (length > 1) {
         i2c_master_read(cmd, reg_data_info.buf, length - 1, I2C_MASTER_ACK);
     }
     i2c_master_read_byte(cmd, reg_data_info.buf + length - 1, I2C_MASTER_NACK);
     i2c_master_stop(cmd);
     ret = i2c_master_cmd_begin(I2C_PORT, cmd, 1000 / portTICK_PERIOD_MS);
     i2c_cmd_link_delete(cmd);
     
     if (ret != ESP_OK) {
         printf("The I2C transmission fails. - I2C Read Data\r\n");
         return mp_obj_new_bool(false);
     }
     
     return mp_obj_new_bool(true);
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_4(i2c_driver_read_obj, i2c_driver_read);
 
 // Write data to I2C device
 STATIC mp_obj_t i2c_driver_write(mp_obj_t driver_addr_obj, mp_obj_t reg_addr_obj, mp_obj_t reg_data_obj, mp_obj_t length_obj) {
     uint8_t driver_addr = mp_obj_get_int(driver_addr_obj);
     uint8_t reg_addr = mp_obj_get_int(reg_addr_obj);
     mp_buffer_info_t reg_data_info;
     mp_get_buffer_raise(reg_data_obj, &reg_data_info, MP_BUFFER_READ);
     uint32_t length = mp_obj_get_int(length_obj);
     
     i2c_cmd_handle_t cmd = i2c_cmd_link_create();
     i2c_master_start(cmd);
     i2c_master_write_byte(cmd, (driver_addr << 1) | I2C_MASTER_WRITE, true);
     i2c_master_write_byte(cmd, reg_addr, true);
     i2c_master_write(cmd, reg_data_info.buf, length, true);
     i2c_master_stop(cmd);
     esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, 1000 / portTICK_PERIOD_MS);
     i2c_cmd_link_delete(cmd);
     
     if (ret != ESP_OK) {
         printf("The I2C transmission fails. - I2C Write\r\n");
         return mp_obj_new_bool(false);
     }
     
     return mp_obj_new_bool(true);
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_4(i2c_driver_write_obj, i2c_driver_write);
 
 // Module globals table
 STATIC const mp_rom_map_elem_t i2c_driver_module_globals_table[] = {
     { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_i2c_driver) },
     { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&i2c_driver_init_obj) },
     { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&i2c_driver_read_obj) },
     { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&i2c_driver_write_obj) },
     
     // Constants
     { MP_ROM_QSTR(MP_QSTR_SCL_PIN), MP_ROM_INT(I2C_SCL_PIN) },
     { MP_ROM_QSTR(MP_QSTR_SDA_PIN), MP_ROM_INT(I2C_SDA_PIN) },
     { MP_ROM_QSTR(MP_QSTR_FREQ_HZ), MP_ROM_INT(I2C_MASTER_FREQ_HZ) },
 };
 STATIC MP_DEFINE_CONST_DICT(i2c_driver_module_globals, i2c_driver_module_globals_table);
 
 // Module definition
 const mp_obj_module_t i2c_driver_user_cmodule = {
     .base = { &mp_type_module },
     .globals = (mp_obj_dict_t *)&i2c_driver_module_globals,
 };
 
 // Register module
 MP_REGISTER_MODULE(MP_QSTR_i2c_driver, i2c_driver_user_cmodule);