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
    add_library(scene_documentation_examples OBJECT
        "${generated_directory}/scene.cpp"
        "${generated_directory}/gameplay_scene.cpp"
        "${generated_directory}/game_objects.cpp"
        "${generated_directory}/animation_effects.cpp"
        "${generated_directory}/camera.cpp"
        "${generated_directory}/object_query.cpp"
        "${generated_directory}/damage.cpp")
    target_link_libraries(scene_documentation_examples PRIVATE engine_lib)
endfunction()
