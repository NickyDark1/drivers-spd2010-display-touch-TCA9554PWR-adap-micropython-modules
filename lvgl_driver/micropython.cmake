# modules/lvgl_driver/micropython.cmake
add_library(usermod_lvgl_driver INTERFACE)

target_sources(usermod_lvgl_driver INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/lvgl_driver.c
)

target_include_directories(usermod_lvgl_driver INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../../lib/lvgl
)

target_link_libraries(usermod INTERFACE usermod_lvgl_driver)