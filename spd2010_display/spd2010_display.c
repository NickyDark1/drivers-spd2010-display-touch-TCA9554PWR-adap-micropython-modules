/*
 * SPD2010 Display Driver implementation for MicroPython
 * Adapted from Display_SPD2010.cpp/.h from Arduino implementation
 */

 
#include "esp_lcd_spd2010.h"
// #include "drivers/esp_lcd_spd2010.h"


 #include "py/obj.h"
 #include "py/runtime.h"
 #include "py/mphal.h"
 #include "driver/gpio.h"
 #include "driver/spi_master.h"
 #include "esp_lcd_panel_io.h"
 #include "esp_lcd_panel_ops.h"
 #include "esp_log.h"
 
 // Display definitions
 #define EXAMPLE_LCD_WIDTH           412
 #define EXAMPLE_LCD_HEIGHT          412
 #define EXAMPLE_LCD_COLOR_BITS      16
 
 // SPI configuration
 #define ESP_PANEL_LCD_SPI_MODE      3
 #define ESP_PANEL_LCD_SPI_CLK_HZ    (40 * 1000 * 1000)
 #define ESP_PANEL_LCD_SPI_CS        21
 #define ESP_PANEL_LCD_SPI_TE        18
 #define ESP_PANEL_LCD_SPI_IO_SCK    40
 #define ESP_PANEL_LCD_SPI_IO_DATA0  46
 #define ESP_PANEL_LCD_SPI_IO_DATA1  45
 #define ESP_PANEL_LCD_SPI_IO_DATA2  42
 #define ESP_PANEL_LCD_SPI_IO_DATA3  41
 
 // For PWM Backlight
 #define LCD_Backlight_PIN           5
 #define Backlight_MAX               100
 #define PWM_FREQ                    20000
 #define PWM_RESOLUTION              10
 
 // Global variables
 static esp_lcd_panel_handle_t panel_handle = NULL;
 static uint8_t LCD_Backlight = 60;
 static ledc_channel_config_t ledc_channel;
 static bool display_initialized = false;
 
 // External function references
 extern mp_obj_t tca9554_set_exio(mp_obj_t pin_obj, mp_obj_t state_obj);
 
 // Reset the SPD2010 display
 STATIC mp_obj_t spd2010_display_reset(void) {
     // Reset using TCA9554 pin 2
     mp_obj_t args[] = {
         mp_obj_new_int(2), // EXIO_PIN2
         mp_obj_new_int(0)  // Low
     };
     tca9554_set_exio(args[0], args[1]);
     mp_hal_delay_ms(50);
     
     args[1] = mp_obj_new_int(1); // High
     tca9554_set_exio(args[0], args[1]);
     mp_hal_delay_ms(50);
     
     return mp_const_none;
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_0(spd2010_display_reset_obj, spd2010_display_reset);
 
 // QSPI initialization for LCD
 STATIC mp_obj_t spd2010_qspi_init(void) {
     spi_bus_config_t bus_config = {
         .data0_io_num = ESP_PANEL_LCD_SPI_IO_DATA0,
         .data1_io_num = ESP_PANEL_LCD_SPI_IO_DATA1,
         .sclk_io_num = ESP_PANEL_LCD_SPI_IO_SCK,
         .data2_io_num = ESP_PANEL_LCD_SPI_IO_DATA2,
         .data3_io_num = ESP_PANEL_LCD_SPI_IO_DATA3,
         .data4_io_num = -1,
         .data5_io_num = -1,
         .data6_io_num = -1,
         .data7_io_num = -1,
         .max_transfer_sz = 65535,
         .flags = SPICOMMON_BUSFLAG_MASTER,
         .intr_flags = 0,
     };
     
     if (spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO) != ESP_OK) {
         printf("The SPI initialization failed.\r\n");
         return mp_obj_new_bool(false);
     }
     
     printf("The SPI initialization succeeded.\r\n");
     return mp_obj_new_bool(true);
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_0(spd2010_qspi_init_obj, spd2010_qspi_init);
 
 // SPD2010 display initialization
 STATIC mp_obj_t spd2010_display_init(void) {
     // Reset display
     spd2010_display_reset();
     
     // Set TE pin as output
     gpio_config_t io_conf = {
         .mode = GPIO_MODE_OUTPUT,
         .pin_bit_mask = (1ULL << ESP_PANEL_LCD_SPI_TE),
         .pull_down_en = 0,
         .pull_up_en = 0,
         .intr_type = GPIO_INTR_DISABLE,
     };
     gpio_config(&io_conf);
     
     // Initialize QSPI
     mp_obj_t qspi_result = spd2010_qspi_init();
     if (!mp_obj_is_true(qspi_result)) {
         printf("SPD2010 Failed to be initialized\r\n");
         return mp_obj_new_bool(false);
     }
     
     // Configure LCD panel IO over SPI
     esp_lcd_panel_io_spi_config_t io_config = {
         .cs_gpio_num = ESP_PANEL_LCD_SPI_CS,
         .dc_gpio_num = -1,
         .spi_mode = ESP_PANEL_LCD_SPI_MODE,
         .pclk_hz = ESP_PANEL_LCD_SPI_CLK_HZ,
         .trans_queue_depth = 10,
         .on_color_trans_done = NULL,
         .user_ctx = NULL,
         .lcd_cmd_bits = 32,
         .lcd_param_bits = 8,
         .flags = {
             .dc_low_on_data = 0,
             .octal_mode = 0,
             .quad_mode = 1,
             .sio_mode = 0,
             .lsb_first = 0,
             .cs_high_active = 0,
         },
     };
     
     esp_lcd_panel_io_handle_t io_handle = NULL;
     if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle) != ESP_OK) {
         printf("Failed to set LCD communication parameters -- SPI\r\n");
         return mp_obj_new_bool(false);
     } else {
         printf("LCD communication parameters are set successfully -- SPI\r\n");
     }
     
     // Configurar el panel SPD2010
    printf("Install LCD driver of spd2010\r\n");
    spd2010_vendor_config_t vendor_config = {
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,  // Ya hicimos el reset con el TCA9554
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel = EXAMPLE_LCD_COLOR_BITS,
        .vendor_config = (void *)&vendor_config,
    };
    
    ESP_LOGI("SPD2010", "Initializing panel driver");
    esp_lcd_new_panel_spd2010(io_handle, &panel_config, &panel_handle);
    
    // Inicializar el panel
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_disp_on_off(panel_handle, true);
    
    printf("spd2010 LCD OK\r\n");
    display_initialized = true;
    return mp_obj_new_bool(true);
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_0(spd2010_display_init_obj, spd2010_display_init);
 
 // Add a window to the LCD (draw bitmap)
 STATIC mp_obj_t spd2010_display_add_window(mp_obj_t x_start_obj, mp_obj_t y_start_obj, mp_obj_t x_end_obj, mp_obj_t y_end_obj, mp_obj_t color_obj) {
     if (!display_initialized) {
         printf("Display not initialized\r\n");
         return mp_const_none;
     }
     
     int x_start = mp_obj_get_int(x_start_obj);
     int y_start = mp_obj_get_int(y_start_obj);
     int x_end = mp_obj_get_int(x_end_obj);
     int y_end = mp_obj_get_int(y_end_obj);
     
     // Get buffer info
     mp_buffer_info_t color_info;
     mp_get_buffer_raise(color_obj, &color_info, MP_BUFFER_READ);
     
     // Calculate size
     uint32_t size = (x_end - x_start + 1) * (y_end - y_start + 1);
     uint16_t *color = (uint16_t *)color_info.buf;
     
     // Swap bytes for each color value (endian conversion)
     for (size_t i = 0; i < size; i++) {
         color[i] = (((color[i] >> 8) & 0xFF) | ((color[i] << 8) & 0xFF00));
     }
     
     // Adjust end points for esp_lcd_panel_draw_bitmap
     x_end += 1;
     y_end += 1;
     
     // Clip to screen bounds
     if (x_end > EXAMPLE_LCD_WIDTH)
         x_end = EXAMPLE_LCD_WIDTH;
     if (y_end > EXAMPLE_LCD_HEIGHT)
         y_end = EXAMPLE_LCD_HEIGHT;
     
     // Draw to the panel if initialized
     if (panel_handle != NULL) {
         esp_lcd_panel_draw_bitmap(panel_handle, x_start, y_start, x_end, y_end, color);
     }
     
     return mp_const_none;
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_5(spd2010_display_add_window_obj, spd2010_display_add_window);
 
 // Initialize backlight control
 STATIC mp_obj_t spd2010_backlight_init(void) {
     // Initialize LEDC for PWM control of backlight
     ledc_timer_config_t ledc_timer = {
         .duty_resolution = LEDC_TIMER_10_BIT,
         .freq_hz = PWM_FREQ,
         .speed_mode = LEDC_HIGH_SPEED_MODE,
         .timer_num = LEDC_TIMER_0,
         .clk_cfg = LEDC_AUTO_CLK,
     };
     ledc_timer_config(&ledc_timer);
     
     ledc_channel.channel = LEDC_CHANNEL_0;
     ledc_channel.duty = 0;
     ledc_channel.gpio_num = LCD_Backlight_PIN;
     ledc_channel.speed_mode = LEDC_HIGH_SPEED_MODE;
     ledc_channel.hpoint = 0;
     ledc_channel.timer_sel = LEDC_TIMER_0;
     ledc_channel_config(&ledc_channel);
     
     // Set initial backlight level
     uint32_t duty = LCD_Backlight * 10;
     if (duty > 1000) duty = 1024;
     ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, duty);
     ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
     
     return mp_const_none;
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_0(spd2010_backlight_init_obj, spd2010_backlight_init);
 
 // Set backlight level
 STATIC mp_obj_t spd2010_set_backlight(mp_obj_t light_obj) {
     uint8_t light = mp_obj_get_int(light_obj);
     
     if (light > Backlight_MAX || light < 0) {
         printf("Set Backlight parameters in the range of 0 to 100 \r\n");
     } else {
         uint32_t backlight = light * 10;
         if (backlight == 1000)
             backlight = 1024;
         
         // Set PWM duty cycle
         ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, backlight);
         ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
         
         // Update global
         LCD_Backlight = light;
     }
     
     return mp_const_none;
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_1(spd2010_set_backlight_obj, spd2010_set_backlight);
 
 // Full LCD initialization
 STATIC mp_obj_t spd2010_lcd_init(void) {
     // Initialize display
     spd2010_display_init();
     
     // Configurar el panel SPD2010
    // printf("Install LCD driver of spd2010\r\n");
    spd2010_vendor_config_t vendor_config = {
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,  // Ya hicimos el reset con el TCA9554
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel = EXAMPLE_LCD_COLOR_BITS,
        .vendor_config = (void *)&vendor_config,
    };
    
    esp_lcd_new_panel_spd2010(io_handle, &panel_config, &panel_handle);
    
    // Inicializar el panel
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_disp_on_off(panel_handle, true);
     
     return mp_const_none;
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_0(spd2010_lcd_init_obj, spd2010_lcd_init);
 
 // Module globals table
 STATIC const mp_rom_map_elem_t spd2010_display_module_globals_table[] = {
     { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_spd2010_display) },
     
     // Constants
     { MP_ROM_QSTR(MP_QSTR_LCD_WIDTH), MP_ROM_INT(EXAMPLE_LCD_WIDTH) },
     { MP_ROM_QSTR(MP_QSTR_LCD_HEIGHT), MP_ROM_INT(EXAMPLE_LCD_HEIGHT) },
     { MP_ROM_QSTR(MP_QSTR_LCD_COLOR_BITS), MP_ROM_INT(EXAMPLE_LCD_COLOR_BITS) },
     { MP_ROM_QSTR(MP_QSTR_LCD_Backlight_PIN), MP_ROM_INT(LCD_Backlight_PIN) },
     { MP_ROM_QSTR(MP_QSTR_Backlight_MAX), MP_ROM_INT(Backlight_MAX) },
     
     // Functions
     { MP_ROM_QSTR(MP_QSTR_SPD2010_Reset), MP_ROM_PTR(&spd2010_display_reset_obj) },
     { MP_ROM_QSTR(MP_QSTR_QSPI_Init), MP_ROM_PTR(&spd2010_qspi_init_obj) },
     { MP_ROM_QSTR(MP_QSTR_SPD2010_Init), MP_ROM_PTR(&spd2010_display_init_obj) },
     { MP_ROM_QSTR(MP_QSTR_LCD_addWindow), MP_ROM_PTR(&spd2010_display_add_window_obj) },
     { MP_ROM_QSTR(MP_QSTR_Backlight_Init), MP_ROM_PTR(&spd2010_backlight_init_obj) },
     { MP_ROM_QSTR(MP_QSTR_Set_Backlight), MP_ROM_PTR(&spd2010_set_backlight_obj) },
     { MP_ROM_QSTR(MP_QSTR_LCD_Init), MP_ROM_PTR(&spd2010_lcd_init_obj) },
 };
 STATIC MP_DEFINE_CONST_DICT(spd2010_display_module_globals, spd2010_display_module_globals_table);
 
 // Module definition
 const mp_obj_module_t spd2010_display_user_cmodule = {
     .base = { &mp_type_module },
     .globals = (mp_obj_dict_t *)&spd2010_display_module_globals,
 };
 
 // Register the module
 MP_REGISTER_MODULE(MP_QSTR_spd2010_display, spd2010_display_user_cmodule);