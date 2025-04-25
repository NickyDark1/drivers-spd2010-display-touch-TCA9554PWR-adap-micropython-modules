# modules/i2c_driver/micropython.cmake
add_library(usermod_i2c_driver INTERFACE)

target_sources(usermod_i2c_driver INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/i2c_driver.c
)

target_include_directories(usermod_i2c_driver INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(usermod INTERFACE usermod_i2c_driver)