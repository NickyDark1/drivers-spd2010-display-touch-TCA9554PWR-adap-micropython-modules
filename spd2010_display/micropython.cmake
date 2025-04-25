# modules/spd2010_display/micropython.cmake
add_library(usermod_spd2010_display INTERFACE)

# Incluir los archivos fuente (el principal y el driver)
target_sources(usermod_spd2010_display INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/spd2010_display.c
    ${CMAKE_CURRENT_LIST_DIR}/drivers/esp_lcd_spd2010.c
)

# Incluir directorios para los encabezados
target_include_directories(usermod_spd2010_display INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/drivers
)

# Definiciones para el driver LCD
target_compile_definitions(usermod_spd2010_display INTERFACE
    ESP_LCD_SPD2010_VER_MAJOR=1
    ESP_LCD_SPD2010_VER_MINOR=0
    ESP_LCD_SPD2010_VER_PATCH=0
)

# Vinculación al módulo de usuario
target_link_libraries(usermod INTERFACE usermod_spd2010_display)