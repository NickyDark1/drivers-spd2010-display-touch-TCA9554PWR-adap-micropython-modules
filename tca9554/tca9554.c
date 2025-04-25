/*
 * TCA9554PWR I/O Expander implementation for MicroPython
 * Adapted from TCA9554PWR.cpp/.h from Arduino implementation
 */

#include "py/obj.h"
#include "py/runtime.h"
#include "py/mphal.h"

// Definitions
#define TCA9554_ADDRESS     0x20
#define TCA9554_INPUT_REG   0x00
#define TCA9554_OUTPUT_REG  0x01
#define TCA9554_Polarity_REG 0x02
#define TCA9554_CONFIG_REG  0x03

// Forward declarations of external module functions
extern mp_obj_t i2c_driver_read(mp_obj_t addr_obj, mp_obj_t reg_obj, mp_obj_t data_obj, mp_obj_t len_obj);
extern mp_obj_t i2c_driver_write(mp_obj_t addr_obj, mp_obj_t reg_obj, mp_obj_t data_obj, mp_obj_t len_obj);

// Read TCA9554PWR register
STATIC mp_obj_t tca9554_read_exio(mp_obj_t reg_obj) {
    uint8_t reg = mp_obj_get_int(reg_obj);
    uint8_t buffer[1] = {0};
    mp_obj_t buf = mp_obj_new_bytearray(1, buffer);
    
    mp_obj_t args[] = {
        mp_obj_new_int(TCA9554_ADDRESS),
        reg_obj,
        buf,
        mp_obj_new_int(1)
    };
    
    mp_obj_t result = i2c_driver_read(args[0], args[1], args[2], args[3]);
    
    if (mp_obj_is_true(result)) {
        return mp_obj_new_int(buffer[0]);
    } else {
        printf("The I2C transmission fails. - I2C Read EXIO\r\n");
        return mp_obj_new_int(-1);
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tca9554_read_exio_obj, tca9554_read_exio);

// Write to TCA9554PWR register
STATIC mp_obj_t tca9554_write_exio(mp_obj_t reg_obj, mp_obj_t data_obj) {
    uint8_t data = mp_obj_get_int(data_obj);
    uint8_t buffer[1] = {data};
    mp_obj_t buf = mp_obj_new_bytearray(1, buffer);
    
    mp_obj_t args[] = {
        mp_obj_new_int(TCA9554_ADDRESS),
        reg_obj,
        buf,
        mp_obj_new_int(1)
    };
    
    mp_obj_t result = i2c_driver_write(args[0], args[1], args[2], args[3]);
    
    if (!mp_obj_is_true(result)) {
        printf("The I2C transmission fails. - I2C Write EXIO\r\n");
        return mp_obj_new_int(-1);
    }
    
    return mp_obj_new_int(0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(tca9554_write_exio_obj, tca9554_write_exio);

// Set pin mode
STATIC mp_obj_t tca9554_mode_exio(mp_obj_t pin_obj, mp_obj_t state_obj) {
    uint8_t pin = mp_obj_get_int(pin_obj);
    uint8_t state = mp_obj_get_int(state_obj);
    
    mp_obj_t bits_status = tca9554_read_exio(mp_obj_new_int(TCA9554_CONFIG_REG));
    
    if (mp_obj_get_int(bits_status) == -1) {
        return mp_obj_new_int(-1);
    }
    
    // In TCA9554, 1 = INPUT, 0 = OUTPUT
    uint8_t data;
    if (state == 1) { // INPUT
        data = (0x01 << (pin-1)) | mp_obj_get_int(bits_status);
    } else { // OUTPUT
        data = (~(0x01 << (pin-1))) & mp_obj_get_int(bits_status);
    }
    
    mp_obj_t result = tca9554_write_exio(mp_obj_new_int(TCA9554_CONFIG_REG), mp_obj_new_int(data));
    
    if (mp_obj_get_int(result) != 0) {
        printf("I/O Configuration Failure !!!\r\n");
    }
    
    return result;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(tca9554_mode_exio_obj, tca9554_mode_exio);

// Set all pins mode
STATIC mp_obj_t tca9554_mode_exios(mp_obj_t pinstate_obj) {
    uint8_t pinstate = mp_obj_get_int(pinstate_obj);
    mp_obj_t result = tca9554_write_exio(mp_obj_new_int(TCA9554_CONFIG_REG), pinstate_obj);
    
    if (mp_obj_get_int(result) != 0) {
        printf("I/O Configuration Failure !!!\r\n");
    }
    
    return result;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tca9554_mode_exios_obj, tca9554_mode_exios);

// Read pin status
STATIC mp_obj_t tca9554_read_exio_pin(mp_obj_t pin_obj) {
    uint8_t pin = mp_obj_get_int(pin_obj);
    mp_obj_t input_bits = tca9554_read_exio(mp_obj_new_int(TCA9554_INPUT_REG));
    
    if (mp_obj_get_int(input_bits) == -1) {
        return mp_obj_new_int(-1);
    }
    
    uint8_t bit_status = (mp_obj_get_int(input_bits) >> (pin-1)) & 0x01;
    return mp_obj_new_int(bit_status);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tca9554_read_exio_pin_obj, tca9554_read_exio_pin);

// Read all pins status
STATIC mp_obj_t tca9554_read_exios(size_t n_args, const mp_obj_t *args) {
    uint8_t reg = TCA9554_INPUT_REG;
    
    if (n_args == 1) {
        reg = mp_obj_get_int(args[0]);
    }
    
    return tca9554_read_exio(mp_obj_new_int(reg));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tca9554_read_exios_obj, 0, 1, tca9554_read_exios);

// Set pin output
STATIC mp_obj_t tca9554_set_exio(mp_obj_t pin_obj, mp_obj_t state_obj) {
    uint8_t pin = mp_obj_get_int(pin_obj);
    uint8_t state = mp_obj_get_int(state_obj);
    uint8_t data;
    
    if (state < 2 && pin < 9 && pin > 0) {
        mp_obj_t bits_status = tca9554_read_exio(mp_obj_new_int(TCA9554_OUTPUT_REG));
        
        if (mp_obj_get_int(bits_status) == -1) {
            return mp_obj_new_int(-1);
        }
        
        if (state == 1) { // HIGH
            data = (0x01 << (pin-1)) | mp_obj_get_int(bits_status);
        } else { // LOW
            data = (~(0x01 << (pin-1))) & mp_obj_get_int(bits_status);
        }
        
        mp_obj_t result = tca9554_write_exio(mp_obj_new_int(TCA9554_OUTPUT_REG), mp_obj_new_int(data));
        
        if (mp_obj_get_int(result) != 0) {
            printf("Failed to set GPIO!!!\r\n");
        }
        
        return result;
    } else {
        printf("Parameter error, please enter the correct parameter!\r\n");
        return mp_obj_new_int(-1);
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(tca9554_set_exio_obj, tca9554_set_exio);

// Set all pins output
STATIC mp_obj_t tca9554_set_exios(mp_obj_t pinstate_obj) {
    uint8_t pinstate = mp_obj_get_int(pinstate_obj);
    mp_obj_t result = tca9554_write_exio(mp_obj_new_int(TCA9554_OUTPUT_REG), pinstate_obj);
    
    if (mp_obj_get_int(result) != 0) {
        printf("Failed to set GPIO!!!\r\n");
    }
    
    return result;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tca9554_set_exios_obj, tca9554_set_exios);

// Toggle pin
STATIC mp_obj_t tca9554_set_toggle(mp_obj_t pin_obj) {
    uint8_t pin = mp_obj_get_int(pin_obj);
    mp_obj_t bits_status = tca9554_read_exio_pin(pin_obj);
    
    if (mp_obj_get_int(bits_status) == -1) {
        return mp_obj_new_int(-1);
    }
    
    uint8_t new_state = !mp_obj_get_int(bits_status);
    return tca9554_set_exio(pin_obj, mp_obj_new_int(new_state));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tca9554_set_toggle_obj, tca9554_set_toggle);

// Initialize TCA9554PWR
STATIC mp_obj_t tca9554_init(size_t n_args, const mp_obj_t *args) {
    uint8_t pinstate = 0; // Default all pins as OUTPUT
    
    if (n_args == 1) {
        pinstate = mp_obj_get_int(args[0]);
    }
    
    return tca9554_mode_exios(mp_obj_new_int(pinstate));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tca9554_init_obj, 0, 1, tca9554_init);

// Module globals table
STATIC const mp_rom_map_elem_t tca9554_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_tca9554) },
    
    // Constants
    { MP_ROM_QSTR(MP_QSTR_TCA9554_ADDRESS), MP_ROM_INT(TCA9554_ADDRESS) },
    { MP_ROM_QSTR(MP_QSTR_INPUT_REG), MP_ROM_INT(TCA9554_INPUT_REG) },
    { MP_ROM_QSTR(MP_QSTR_OUTPUT_REG), MP_ROM_INT(TCA9554_OUTPUT_REG) },
    { MP_ROM_QSTR(MP_QSTR_Polarity_REG), MP_ROM_INT(TCA9554_Polarity_REG) },
    { MP_ROM_QSTR(MP_QSTR_CONFIG_REG), MP_ROM_INT(TCA9554_CONFIG_REG) },
    
    { MP_ROM_QSTR(MP_QSTR_Low), MP_ROM_INT(0) },
    { MP_ROM_QSTR(MP_QSTR_High), MP_ROM_INT(1) },
    { MP_ROM_QSTR(MP_QSTR_EXIO_PIN1), MP_ROM_INT(1) },
    { MP_ROM_QSTR(MP_QSTR_EXIO_PIN2), MP_ROM_INT(2) },
    { MP_ROM_QSTR(MP_QSTR_EXIO_PIN3), MP_ROM_INT(3) },
    { MP_ROM_QSTR(MP_QSTR_EXIO_PIN4), MP_ROM_INT(4) },
    { MP_ROM_QSTR(MP_QSTR_EXIO_PIN5), MP_ROM_INT(5) },
    { MP_ROM_QSTR(MP_QSTR_EXIO_PIN6), MP_ROM_INT(6) },
    { MP_ROM_QSTR(MP_QSTR_EXIO_PIN7), MP_ROM_INT(7) },
    { MP_ROM_QSTR(MP_QSTR_EXIO_PIN8), MP_ROM_INT(8) },
    
    // Functions
    { MP_ROM_QSTR(MP_QSTR_I2C_Read_EXIO), MP_ROM_PTR(&tca9554_read_exio_obj) },
    { MP_ROM_QSTR(MP_QSTR_I2C_Write_EXIO), MP_ROM_PTR(&tca9554_write_exio_obj) },
    { MP_ROM_QSTR(MP_QSTR_Mode_EXIO), MP_ROM_PTR(&tca9554_mode_exio_obj) },
    { MP_ROM_QSTR(MP_QSTR_Mode_EXIOS), MP_ROM_PTR(&tca9554_mode_exios_obj) },
    { MP_ROM_QSTR(MP_QSTR_Read_EXIO), MP_ROM_PTR(&tca9554_read_exio_pin_obj) },
    { MP_ROM_QSTR(MP_QSTR_Read_EXIOS), MP_ROM_PTR(&tca9554_read_exios_obj) },
    { MP_ROM_QSTR(MP_QSTR_Set_EXIO), MP_ROM_PTR(&tca9554_set_exio_obj) },
    { MP_ROM_QSTR(MP_QSTR_Set_EXIOS), MP_ROM_PTR(&tca9554_set_exios_obj) },
    { MP_ROM_QSTR(MP_QSTR_Set_Toggle), MP_ROM_PTR(&tca9554_set_toggle_obj) },
    { MP_ROM_QSTR(MP_QSTR_TCA9554PWR_Init), MP_ROM_PTR(&tca9554_init_obj) },
};
STATIC MP_DEFINE_CONST_DICT(tca9554_module_globals, tca9554_module_globals_table);

// Module definition
const mp_obj_module_t tca9554_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&tca9554_module_globals,
};

// Register module
MP_REGISTER_MODULE(MP_QSTR_tca9554, tca9554_user_cmodule);