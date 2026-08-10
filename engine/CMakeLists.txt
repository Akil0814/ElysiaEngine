file(GLOB_RECURSE ENGINE CONFIGURE_DEPENDS "*.cpp")

add_library(engine_lib ${ENGINE})

target_include_directories(engine_lib PUBLIC ${PROJECT_SOURCE_DIR})
target_include_directories(engine_lib SYSTEM PUBLIC ${SDL2_INCLUDE_DIR})
target_link_libraries(engine_lib PUBLIC ${SDL2_LIBRARIES})
