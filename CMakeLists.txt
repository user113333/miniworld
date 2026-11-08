cmake_minimum_required(VERSION 3.21)
project(miniworld LANGUAGES C CXX)

set(SDL_SHARED OFF CACHE BOOL "Build shared SDL libraries")
set(SDL_STATIC ON CACHE BOOL "Build static SDL libraries")
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build SDL test library")
add_subdirectory(lib/sdl EXCLUDE_FROM_ALL)

set(GLM_BUILD_LIBRARY OFF CACHE BOOL "Use header-only GLM")
add_subdirectory(lib/glm EXCLUDE_FROM_ALL)

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
    src/island_generator.cpp
    src/tree_placement.cpp
    src/simulation_clock.cpp
    src/world_simulation.cpp
    src/island_demo.cpp
)
target_include_directories(miniworld_world PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(miniworld_world PUBLIC cxx_std_17)
target_link_libraries(miniworld_world PUBLIC glm::glm)

add_library(miniworld_effects STATIC
    src/particle_system.cpp
    src/tree_scene.cpp
)
target_include_directories(miniworld_effects PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(miniworld_effects PUBLIC cxx_std_17)
target_link_libraries(miniworld_effects PUBLIC glm::glm)

add_executable(miniworld
    src/main.cpp
    src/application.cpp
    src/application_ui.cpp
    src/map_renderer.cpp
    src/island_controls.cpp
    src/tree_renderer.cpp
    src/particle_renderer.cpp
    src/fire_effects.cpp
)
target_compile_features(miniworld PRIVATE cxx_std_17)
target_link_libraries(miniworld PRIVATE imgui miniworld_world miniworld_effects)

configure_file(
    ${PROJECT_SOURCE_DIR}/res/fonts/CozetteVector.ttf
    ${CMAKE_CURRENT_BINARY_DIR}/res/fonts/CozetteVector.ttf
    COPYONLY
)
configure_file(
    ${PROJECT_SOURCE_DIR}/res/fonts/Cozette-LICENSE
    ${CMAKE_CURRENT_BINARY_DIR}/res/fonts/Cozette-LICENSE
    COPYONLY
)

foreach(tree IN ITEMS tree1 tree2 tree3 tree_growing tree_dead)
    configure_file(
        ${PROJECT_SOURCE_DIR}/res/map/${tree}.png
        ${CMAKE_CURRENT_BINARY_DIR}/res/map/${tree}.png
        COPYONLY
    )
endforeach()

include(CTest)
if(BUILD_TESTING)
    foreach(suite IN ITEMS hex_grid island_generator tree_placement seed_fixtures
                           simulation_clock island_demo world_simulation fire_simulation recovery)
        add_executable(${suite}_tests tests/${suite}_tests.cpp)
        target_link_libraries(${suite}_tests PRIVATE miniworld_world)
        add_test(NAME world.${suite} COMMAND ${suite}_tests)
    endforeach()
    foreach(suite IN ITEMS particle_system tree_picking)
        add_executable(${suite}_tests tests/${suite}_tests.cpp)
        target_link_libraries(${suite}_tests PRIVATE miniworld_effects)
        add_test(NAME effects.${suite} COMMAND ${suite}_tests)
    endforeach()
    target_link_libraries(particle_system_tests PRIVATE miniworld_world)
    add_executable(fire_integration_tests tests/fire_integration_tests.cpp src/fire_effects.cpp)
    target_link_libraries(fire_integration_tests PRIVATE miniworld_world miniworld_effects)
    add_test(NAME world.fire_integration COMMAND fire_integration_tests)
    add_executable(particle_renderer_tests tests/particle_renderer_tests.cpp src/particle_renderer.cpp)
    target_link_libraries(particle_renderer_tests PRIVATE miniworld_effects SDL3::SDL3)
    add_test(NAME render.particle_renderer COMMAND particle_renderer_tests)
    add_executable(tree_renderer_tests tests/tree_renderer_tests.cpp
        src/tree_renderer.cpp src/particle_renderer.cpp)
    target_link_libraries(tree_renderer_tests PRIVATE miniworld_effects SDL3::SDL3)
    add_test(NAME render.tree_renderer COMMAND tree_renderer_tests)
    add_executable(map_renderer_tests tests/map_renderer_tests.cpp src/map_renderer.cpp)
    target_link_libraries(map_renderer_tests PRIVATE miniworld_world SDL3::SDL3)
    add_test(NAME render.map_renderer COMMAND map_renderer_tests)
    target_compile_definitions(seed_fixtures_tests PRIVATE
        TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
    )
endif()
