include(CMakeParseArguments)

function(add_elysia_test TARGET_NAME)
    set(options SDL)
    set(one_value_args LABEL)
    set(multi_value_args SOURCES LIBRARIES)
    cmake_parse_arguments(TEST "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    add_executable(${TARGET_NAME} ${TEST_SOURCES})
    target_include_directories(${TARGET_NAME} PRIVATE ${PROJECT_SOURCE_DIR})
    target_link_libraries(${TARGET_NAME} PRIVATE engine_lib ${TEST_LIBRARIES})

    if(ELYSIA_ENABLE_ASAN AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${TARGET_NAME} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(${TARGET_NAME} PRIVATE -fsanitize=address)
    endif()

    if(TEST_SDL AND WIN32)
        foreach(SDL2_RUNTIME_FILE IN LISTS SDL2_RUNTIME_FILES)
            if(EXISTS "${SDL2_RUNTIME_FILE}")
                add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${SDL2_RUNTIME_FILE}"
                        "$<TARGET_FILE_DIR:${TARGET_NAME}>"
                )
            endif()
        endforeach()
    endif()

    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
    if(TEST_LABEL)
        set_tests_properties(${TARGET_NAME} PROPERTIES LABELS "${TEST_LABEL}")
    endif()
endfunction()

find_package(Threads REQUIRED)

add_elysia_test(save_service_tests
    LABEL save
    SOURCES save/save_service_tests.cpp
)

add_elysia_test(glyph_run_layout_tests
    LABEL core
    SOURCES core/glyph_run_layout_tests.cpp
)

add_elysia_test(bootstrap_texture_cache_tests
    LABEL bootstrap
    SOURCES bootstrap/bootstrap_texture_cache_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES}
    SDL
)

add_elysia_test(builtin_asset_catalog_tests
    LABEL builtin
    SOURCES builtin/builtin_asset_catalog_tests.cpp
)
target_compile_definitions(builtin_asset_catalog_tests
    PRIVATE ELYSIA_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

add_elysia_test(builtin_asset_cache_tests
    LABEL builtin
    SOURCES builtin/builtin_asset_cache_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES}
    SDL
)
target_compile_definitions(builtin_asset_cache_tests
    PRIVATE ELYSIA_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

add_elysia_test(engine_character_tests
    LABEL builtin
    SOURCES builtin/engine_character_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES}
    SDL
)
target_compile_definitions(engine_character_tests
    PRIVATE ELYSIA_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

foreach(INPUT_TEST_TARGET IN ITEMS
    input_action_mapping_tests
    input_system_controller_lifecycle_tests
    input_system_pointer_coordinates_tests
    engine_gameplay_tests
    gameplay_scene_input_tests
)
    add_elysia_test(${INPUT_TEST_TARGET}
        LABEL input
        SOURCES input/${INPUT_TEST_TARGET}.cpp
        LIBRARIES ${SDL2_LIBRARIES}
        SDL
    )
endforeach()

add_elysia_test(termination_manager_tests
    LABEL application
    SOURCES application/termination_manager_tests.cpp
    LIBRARIES Threads::Threads
)

add_elysia_test(application_composition_tests
    LABEL application
    SOURCES application/application_composition_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES}
    SDL
)

add_elysia_test(application_presentation_settings_tests
    LABEL application
    SOURCES application/application_presentation_settings_tests.cpp
)

add_elysia_test(font_settings_tests
    LABEL typography
    SOURCES typography/font_settings_tests.cpp
)

add_elysia_test(application_sdl_presentation_tests
    LABEL application
    SOURCES application/application_sdl_presentation_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES}
    SDL
)

add_elysia_test(application_window_settings_tests
    LABEL application
    SOURCES application/application_window_settings_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES}
    SDL
)

add_elysia_test(camera_manager_tests
    LABEL camera
    SOURCES camera/camera_manager_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES}
    SDL
)

add_elysia_test(camera_projection_tests
    LABEL camera
    SOURCES camera/camera_projection_tests.cpp
)

add_elysia_test(camera_controller_tests
    LABEL camera
    SOURCES camera/camera_controller_tests.cpp
)

add_elysia_test(scene_core_tests
    LABEL scene
    SOURCES scene/scene_core_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES}
    SDL
)

add_elysia_test(game_object_query_tests
    LABEL object_query
    SOURCES object_query/game_object_query_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES}
    SDL
)

add_elysia_test(startup_loading_flow_tests
    LABEL scene
    SOURCES scene/startup_loading_flow_tests.cpp
)

add_elysia_test(startup_loading_scene_tests
    LABEL scene
    SOURCES scene/startup_loading_scene_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES}
    SDL
)

add_elysia_test(settings_scene_tests
    LABEL scene
    SOURCES scene/settings_scene_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES}
    SDL
)

add_elysia_test(testbed_scene_tests
    LABEL scene
    SOURCES scene/testbed_scene_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES}
    SDL
)
target_compile_definitions(testbed_scene_tests
    PRIVATE ELYSIA_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

add_elysia_test(elysia_scene_tests
    LABEL elysia
    SOURCES elysia/elysia_scene_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES}
    SDL
)
target_compile_definitions(elysia_scene_tests
    PRIVATE ELYSIA_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

add_elysia_test(elysia_animation_loader_tests
    LABEL elysia
    SOURCES elysia/elysia_animation_loader_tests.cpp
)
target_compile_definitions(elysia_animation_loader_tests PRIVATE SDL_MAIN_HANDLED)

add_elysia_test(application_failure_scene_tests
    LABEL scene
    SOURCES scene/application_failure_scene_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES}
    SDL
)
target_compile_definitions(application_failure_scene_tests
    PRIVATE ELYSIA_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

add_elysia_test(content_failure_tests
    LABEL loading
    SOURCES loading/content_failure_tests.cpp
)

add_elysia_test(physics_framework_contract_tests
    LABEL physics
    SOURCES physics/physics_framework_contract_tests.cpp
)

add_elysia_test(physics_service_tests
    LABEL physics
    SOURCES physics/physics_service_tests.cpp
)

add_elysia_test(physics_world_lifecycle_tests
    LABEL physics
    SOURCES physics/physics_world_lifecycle_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES}
    SDL
)

add_elysia_test(gameplay_collision_service_tests
    LABEL physics
    SOURCES physics/gameplay_collision_service_tests.cpp
)

add_elysia_test(sound_playback_scheduler_tests
    LABEL audio
    SOURCES audio/sound_playback_scheduler_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES}
    SDL
)

add_elysia_test(animation_effect_tests
    LABEL effects
    SOURCES effects/animation_effect_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES}
    SDL
)

add_elysia_test(floating_number_glyph_cache_tests
    LABEL effects
    SOURCES effects/floating_number_glyph_cache_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES} ${SDL2_TTF_LIBRARIES}
    SDL
)

add_elysia_test(floating_number_effect_tests
    LABEL effects
    SOURCES effects/floating_number_effect_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES} ${SDL2_TTF_LIBRARIES}
    SDL
)

foreach(LOADING_TEST_TARGET IN ITEMS
	animation_config_loader_tests
    atlas_build_preparer_tests
    content_load_pipeline_tests
    effect_definition_config_loader_tests
    horizontal_strip_animation_tests
	game_content_loader_config_tests
    resource_key_builder_tests
    resource_load_plan_validator_tests
    resource_request_assembler_tests
)
    add_elysia_test(${LOADING_TEST_TARGET}
        LABEL loading
        SOURCES loading/${LOADING_TEST_TARGET}.cpp
        LIBRARIES ${SDL2_LIBRARIES}
        SDL
    )
endforeach()

foreach(UI_TEST_TARGET IN ITEMS
    ui_layout_tests
    ui_style_tests
    ui_theme_defaults_tests
    ui_popup_lifecycle_tests
    ui_tooltip_visibility_tests
    ui_selection_controls_tests
    ui_focus_lifecycle_tests
    ui_focus_tree_tests
    ui_focus_routing_tests
    ui_presentation_tests
    ui_stroke_rendering_tests
    ui_callback_safety_tests
    ui_text_input_rendering_tests
    ui_number_rendering_tests
)
    add_elysia_test(${UI_TEST_TARGET}
        LABEL ui
        SOURCES ui/${UI_TEST_TARGET}.cpp
        LIBRARIES ${SDL2_LIBRARIES}
        SDL
    )
endforeach()

add_elysia_test(logging_tests
    LABEL tools
    SOURCES tools/logging_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES}
    SDL
)

add_elysia_test(logger_startup_phase_tests
    LABEL tools
    SOURCES tools/logger_startup_phase_tests.cpp
)
target_compile_definitions(logger_startup_phase_tests
    PRIVATE ELYSIA_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

add_elysia_test(random_generator_tests
    LABEL tools
    SOURCES tools/random_generator_tests.cpp
)

add_elysia_test(debug_draw_tests
    LABEL tools
    SOURCES tools/debug_draw_tests.cpp
)

add_elysia_test(debug_draw_projection_tests
    LABEL tools
    SOURCES tools/debug_draw_projection_tests.cpp
)

add_elysia_test(json_file_failure_tests
    LABEL loading
    SOURCES io/json_file_failure_tests.cpp
)

add_elysia_test(engine_i18n_assets_tests
    LABEL localization
    SOURCES localization/engine_i18n_assets_tests.cpp
)
target_compile_definitions(engine_i18n_assets_tests
    PRIVATE ELYSIA_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

add_elysia_test(project_i18n_assets_tests
    LABEL localization
    SOURCES localization/project_i18n_assets_tests.cpp
)
target_compile_definitions(project_i18n_assets_tests
    PRIVATE ELYSIA_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

add_elysia_test(localization_builtin_fallback_tests
    LABEL localization
    SOURCES localization/localization_builtin_fallback_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES}
    SDL
)
target_compile_definitions(localization_builtin_fallback_tests
    PRIVATE ELYSIA_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

add_elysia_test(font_resolver_tests
    LABEL typography
    SOURCES typography/font_resolver_tests.cpp
    LIBRARIES ${SDL2_LIBRARIES}
    SDL
)
target_compile_definitions(font_resolver_tests
    PRIVATE ELYSIA_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

foreach(CONFIG_TEST_TARGET IN ITEMS app_config_tests user_config_tests config_service_tests bootstrap_config_integration_tests)
    add_elysia_test(${CONFIG_TEST_TARGET}
        LABEL config
        SOURCES config/${CONFIG_TEST_TARGET}.cpp
        LIBRARIES ${SDL2_LIBRARIES}
        SDL
    )
endforeach()
target_compile_definitions(bootstrap_config_integration_tests
    PRIVATE ELYSIA_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
