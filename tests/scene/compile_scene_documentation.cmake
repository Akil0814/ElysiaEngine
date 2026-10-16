# Compile the Markdown itself, so examples cannot silently diverge from a copied fixture.
# Include this file from tests/CMakeLists.txt after engine_lib exists, then call:
#   elysia_add_scene_documentation_examples()
function(elysia_extract_documentation_cpp markdown output block_count)
    file(READ "${markdown}" remaining)
    string(REPLACE "\r\n" "\n" remaining "${remaining}")
    set(generated "// Generated from ${markdown}; edit the Markdown source.\n#include <type_traits>\n")
    set(consumed_lines 0)
    foreach(index RANGE 1 ${block_count})
        string(FIND "${remaining}" "```cpp\n" opening)
        if(opening EQUAL -1)
            message(FATAL_ERROR "Missing C++ block ${index} in ${markdown}")
        endif()
        math(EXPR content_start "${opening} + 7")
        string(SUBSTRING "${remaining}" 0 ${content_start} prefix)
        string(REGEX REPLACE "[^\n]" "" prefix_newlines "${prefix}")
        string(LENGTH "${prefix_newlines}" prefix_line_count)
        math(EXPR source_line "${consumed_lines} + ${prefix_line_count} + 1")
        string(SUBSTRING "${remaining}" ${content_start} -1 remaining)
        string(FIND "${remaining}" "\n```" closing)
        if(closing EQUAL -1)
            message(FATAL_ERROR "Unclosed C++ block ${index} in ${markdown}")
        endif()
        string(SUBSTRING "${remaining}" 0 ${closing} code)
        string(APPEND generated "#line ${source_line} \"${markdown}\"\n${code}\n")
        math(EXPR fence_end "${closing} + 4")
        string(SUBSTRING "${remaining}" 0 ${fence_end} extracted)
        string(REGEX REPLACE "[^\n]" "" extracted_newlines "${extracted}")
        string(LENGTH "${extracted_newlines}" extracted_line_count)
        math(EXPR consumed_lines "${consumed_lines} + ${prefix_line_count} + ${extracted_line_count}")
        string(SUBSTRING "${remaining}" ${fence_end} -1 remaining)
    endforeach()
    foreach(example_type IN LISTS ARGN)
        string(APPEND generated "static_assert(!std::is_abstract_v<${example_type}>);\n")
    endforeach()
    file(WRITE "${output}" "${generated}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${markdown}")
endfunction()

function(elysia_add_scene_documentation_examples)
    set(documentation_root "${PROJECT_SOURCE_DIR}/docs/zh-CN/user-guide")
    set(generated_directory "${CMAKE_CURRENT_BINARY_DIR}/scene_documentation_examples")
    file(MAKE_DIRECTORY "${generated_directory}")
    elysia_extract_documentation_cpp("${documentation_root}/game-initialization.md"
        "${generated_directory}/game_initialization.cpp" 3
        initialization_example::EntryScene initialization_example::GameModule)
    elysia_extract_documentation_cpp("${documentation_root}/scene/scene.md"
        "${generated_directory}/scene.cpp" 1 RoomScene)
    elysia_extract_documentation_cpp("${documentation_root}/gameplay/scene.md"
        "${generated_directory}/gameplay_scene.cpp" 1 BattleScene)
    elysia_extract_documentation_cpp("${documentation_root}/scene/game-objects.md"
        "${generated_directory}/game_objects.cpp" 1 MovingBlock)
    elysia_extract_documentation_cpp("${documentation_root}/systems/animation-and-effects.md"
        "${generated_directory}/animation_effects.cpp" 1 AnimatedItem)
    elysia_extract_documentation_cpp("${documentation_root}/systems/camera.md"
        "${generated_directory}/camera.cpp" 1 FollowScene)
    elysia_extract_documentation_cpp("${documentation_root}/systems/object-query.md"
        "${generated_directory}/object_query.cpp" 1)
    elysia_extract_documentation_cpp("${documentation_root}/gameplay/damage.md"
        "${generated_directory}/damage.cpp" 2 damage_example::Fighter damage_example::DamageScene)
    elysia_extract_documentation_cpp("${PROJECT_SOURCE_DIR}/docs/zh-CN/architecture/subsystems/save.md"
        "${generated_directory}/save.cpp" 1)
    elysia_extract_documentation_cpp("${documentation_root}/builtin-scenes/application-failure.md"
        "${generated_directory}/application_failure.cpp" 1 FailureExampleScene)
    elysia_extract_documentation_cpp("${documentation_root}/content/configuration.md"
        "${generated_directory}/configuration.cpp" 2)
    elysia_extract_documentation_cpp("${documentation_root}/systems/physics.md"
        "${generated_directory}/physics.cpp" 4 FallingBox ConfiguredPhysicsScene)
    elysia_extract_documentation_cpp("${documentation_root}/gameplay/collision.md"
        "${generated_directory}/collision.cpp" 3 CollisionExampleScene)
    elysia_extract_documentation_cpp("${documentation_root}/content/resources.md"
        "${generated_directory}/resources.cpp" 1)
    elysia_extract_documentation_cpp("${documentation_root}/content/save.md"
        "${generated_directory}/user_save.cpp" 1)
    elysia_extract_documentation_cpp("${documentation_root}/builtin-scenes/startup-loading.md"
        "${generated_directory}/startup_loading.cpp" 1)
    elysia_extract_documentation_cpp("${documentation_root}/builtin-scenes/settings.md"
        "${generated_directory}/settings.cpp" 1 SettingsExampleScene)
    elysia_extract_documentation_cpp("${documentation_root}/systems/audio.md"
        "${generated_directory}/audio.cpp" 1)
    elysia_extract_documentation_cpp("${documentation_root}/systems/localization.md"
        "${generated_directory}/localization.cpp" 1)
    elysia_extract_documentation_cpp("${documentation_root}/tools/debug-draw.md"
        "${generated_directory}/debug_draw.cpp" 1)
    elysia_extract_documentation_cpp("${documentation_root}/systems/time-and-timers.md"
        "${generated_directory}/time_and_timers.cpp" 2)
    elysia_extract_documentation_cpp("${documentation_root}/tools/utilities.md"
        "${generated_directory}/utilities.cpp" 2)
    elysia_extract_documentation_cpp("${documentation_root}/gameplay/control.md"
        "${generated_directory}/control.cpp" 1)
    elysia_extract_documentation_cpp("${documentation_root}/systems/input.md"
        "${generated_directory}/input.cpp" 2 ShortcutScene)
    elysia_extract_documentation_cpp("${documentation_root}/ui/README.md"
        "${generated_directory}/ui.cpp" 2 ExitMenuScene)
    add_library(scene_documentation_examples OBJECT
        "${generated_directory}/game_initialization.cpp"
        "${generated_directory}/scene.cpp"
        "${generated_directory}/gameplay_scene.cpp"
        "${generated_directory}/game_objects.cpp"
        "${generated_directory}/animation_effects.cpp"
        "${generated_directory}/camera.cpp"
        "${generated_directory}/object_query.cpp"
        "${generated_directory}/damage.cpp"
        "${generated_directory}/save.cpp"
        "${generated_directory}/application_failure.cpp"
        "${generated_directory}/configuration.cpp"
        "${generated_directory}/physics.cpp"
        "${generated_directory}/collision.cpp"
        "${generated_directory}/resources.cpp"
        "${generated_directory}/user_save.cpp"
        "${generated_directory}/startup_loading.cpp"
        "${generated_directory}/settings.cpp"
        "${generated_directory}/audio.cpp"
        "${generated_directory}/localization.cpp"
        "${generated_directory}/debug_draw.cpp"
        "${generated_directory}/time_and_timers.cpp"
        "${generated_directory}/utilities.cpp"
        "${generated_directory}/control.cpp"
        "${generated_directory}/input.cpp"
        "${generated_directory}/ui.cpp")
    target_link_libraries(scene_documentation_examples PRIVATE engine_lib)
endfunction()
