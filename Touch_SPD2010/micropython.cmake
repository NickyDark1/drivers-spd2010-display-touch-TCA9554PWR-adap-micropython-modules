# modules/spd2010_touch/micropython.cmake
add_library(usermod_spd2010_touch INTERFACE)

target_sources(usermod_spd2010_touch INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/spd2010_touch.c
)

target_include_directories(usermod_spd2010_touch INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(usermod INTERFACE usermod_spd2010_touch)