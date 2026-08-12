add_library(imgui_lib STATIC
    imgui.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    imgui_impl_sdl2.cpp
    imgui_impl_sdlrenderer2.cpp
)

target_include_directories(imgui_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SDL2_INCLUDE_DIR}
)

target_link_libraries(imgui_lib PUBLIC ${SDL2_LIBRARIES})
