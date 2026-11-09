cmake_minimum_required(VERSION 3.21)
project(miniworld LANGUAGES C CXX)

set(SDL_SHARED OFF CACHE BOOL "Build shared SDL libraries")
set(SDL_STATIC ON CACHE BOOL "Build static SDL libraries")
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build SDL test library")
add_subdirectory(lib/sdl EXCLUDE_FROM_ALL)

set(GLM_BUILD_LIBRARY OFF CACHE BOOL "Use header-only GLM")
add_subdirectory(lib/glm EXCLUDE_FROM_ALL)

set(FLECS_STATIC ON CACHE BOOL "Build static Flecs")
set(FLECS_SHARED OFF CACHE BOOL "Build shared Flecs")
set(FLECS_TESTS OFF CACHE BOOL "Build Flecs tests")
add_subdirectory(lib/flecs EXCLUDE_FROM_ALL)

set(IMGUI_DIR "${PROJECT_SOURCE_DIR}/lib/imgui")
find_package(Freetype REQUIRED)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/misc/freetype/imgui_freetype.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3)
target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_FREETYPE)
target_link_libraries(imgui PRIVATE Freetype::Freetype)

add_library(miniworld_world STATIC
    src/actors/reflection.cpp
    src/actors/tree.cpp
    src/simulation/systems/forest.cpp
    src/simulation/systems/fire.cpp
    src/simulation/systems/recovery.cpp
    src/world/island_generator.cpp
    src/world/tree_placement.cpp
    src/simulation/simulation_clock.cpp
    src/simulation/entity_inspection.cpp
    src/simulation/entity_edits.cpp
    src/simulation/world.cpp
    src/simulation/simulation.cpp
)
target_include_directories(miniworld_world PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(miniworld_world PUBLIC cxx_std_17)
target_link_libraries(miniworld_world PUBLIC glm::glm flecs::flecs_static)

add_library(miniworld_effects STATIC
    src/effects/particle_system.cpp
    src/render/actor_scene.cpp
)
target_include_directories(miniworld_effects PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(miniworld_effects PUBLIC cxx_std_17)
target_link_libraries(miniworld_effects PUBLIC glm::glm)

add_library(miniworld_content STATIC src/content/catalog.cpp)
target_include_directories(miniworld_content PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(miniworld_content PUBLIC cxx_std_17)
target_link_libraries(miniworld_content PUBLIC glm::glm)

add_library(miniworld_textures STATIC src/render/texture_cache.cpp)
target_link_libraries(miniworld_textures PUBLIC miniworld_content SDL3::SDL3)

add_library(miniworld_gui STATIC
    src/gui/application_ui.cpp
    src/gui/asset_browser.cpp
    src/gui/entity_explorer.cpp
    src/gui/island_controls.cpp
    src/gui/simulation_controls.cpp
)
target_link_libraries(miniworld_gui PUBLIC imgui miniworld_world miniworld_textures PRIVATE Freetype::Freetype)

add_executable(miniworld
    src/main.cpp
    src/application.cpp
    src/render/map_renderer.cpp
    src/render/actor_renderer.cpp
    src/render/particle_renderer.cpp
    src/effects/fire_effects.cpp
)
target_link_libraries(miniworld PRIVATE miniworld_gui miniworld_effects)

# Complete development resources, with no stale files after additions/removals.
file(GLOB_RECURSE RESOURCE_ENTRIES CONFIGURE_DEPENDS LIST_DIRECTORIES true
    RELATIVE "${PROJECT_SOURCE_DIR}/res" "${PROJECT_SOURCE_DIR}/res/*")
file(REMOVE_RECURSE "${CMAKE_CURRENT_BINARY_DIR}/res")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/res")
foreach(resource IN LISTS RESOURCE_ENTRIES)
    if(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/res/${resource}")
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/res/${resource}")
    else()
        configure_file("${PROJECT_SOURCE_DIR}/res/${resource}"
            "${CMAKE_CURRENT_BINARY_DIR}/res/${resource}" COPYONLY)
    endif()
endforeach()

include(CTest)
if(BUILD_TESTING)
    foreach(suite IN ITEMS hex_grid island_generator tree_placement seed_fixtures
                           simulation_clock island_demo world_simulation fire_simulation recovery flecs_world entity_explorer)
        add_executable(${suite}_tests tests/${suite}_tests.cpp)
        target_link_libraries(${suite}_tests PRIVATE miniworld_world)
        add_test(NAME world.${suite} COMMAND ${suite}_tests)
    endforeach()
    foreach(suite IN ITEMS particle_system tree_picking)
        add_executable(${suite}_tests tests/${suite}_tests.cpp)
        target_link_libraries(${suite}_tests PRIVATE miniworld_effects)
        add_test(NAME effects.${suite} COMMAND ${suite}_tests)
    endforeach()
    add_executable(content_catalog_tests tests/content_catalog_tests.cpp)
    target_link_libraries(content_catalog_tests PRIVATE miniworld_content)
    target_compile_definitions(content_catalog_tests PRIVATE TEST_RESOURCE_ROOT="${CMAKE_CURRENT_BINARY_DIR}/res")
    add_test(NAME content.catalog COMMAND content_catalog_tests)
    add_executable(texture_cache_tests tests/texture_cache_tests.cpp src/render/actor_renderer.cpp src/render/particle_renderer.cpp)
    target_link_libraries(texture_cache_tests PRIVATE miniworld_textures miniworld_effects)
    add_test(NAME render.texture_cache COMMAND texture_cache_tests)
    add_executable(asset_browser_tests tests/asset_browser_tests.cpp src/render/actor_renderer.cpp src/render/particle_renderer.cpp)
    target_link_libraries(asset_browser_tests PRIVATE miniworld_gui miniworld_effects)
    add_test(NAME gui.asset_browser COMMAND asset_browser_tests)
    set_tests_properties(gui.asset_browser PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")
    add_executable(entity_explorer_gui_tests tests/entity_explorer_gui_tests.cpp)
    target_link_libraries(entity_explorer_gui_tests PRIVATE miniworld_gui)
    add_test(NAME gui.entity_explorer COMMAND entity_explorer_gui_tests)
    set_tests_properties(gui.entity_explorer PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")
    target_link_libraries(entity_explorer_tests PRIVATE miniworld_effects)
    target_link_libraries(particle_system_tests PRIVATE miniworld_world)
    add_executable(fire_integration_tests tests/fire_integration_tests.cpp src/effects/fire_effects.cpp)
    target_link_libraries(fire_integration_tests PRIVATE miniworld_world miniworld_effects)
    add_test(NAME world.fire_integration COMMAND fire_integration_tests)
    add_executable(particle_renderer_tests tests/particle_renderer_tests.cpp src/render/particle_renderer.cpp)
    target_link_libraries(particle_renderer_tests PRIVATE miniworld_effects SDL3::SDL3)
    add_test(NAME render.particle_renderer COMMAND particle_renderer_tests)
    add_executable(tree_renderer_tests tests/tree_renderer_tests.cpp
        src/render/actor_renderer.cpp src/render/particle_renderer.cpp)
    target_link_libraries(tree_renderer_tests PRIVATE miniworld_effects miniworld_textures)
    add_test(NAME render.tree_renderer COMMAND tree_renderer_tests)
    add_executable(map_renderer_tests tests/map_renderer_tests.cpp src/render/map_renderer.cpp)
    target_link_libraries(map_renderer_tests PRIVATE miniworld_world SDL3::SDL3)
    add_test(NAME render.map_renderer COMMAND map_renderer_tests)
    target_compile_definitions(flecs_world_tests PRIVATE
        TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
    )
    target_compile_definitions(seed_fixtures_tests PRIVATE
        TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
    )
endif()
