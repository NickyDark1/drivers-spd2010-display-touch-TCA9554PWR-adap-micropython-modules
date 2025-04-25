# modules/tca9554/micropython.cmake
add_library(usermod_tca9554 INTERFACE)

target_sources(usermod_tca9554 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/tca9554.c
)

target_include_directories(usermod_tca9554 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(usermod INTERFACE usermod_tca9554)