cmake_minimum_required(VERSION 3.10)
project(TreeVisual VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===== Source =====
add_executable(tree src/tree.cpp)

# ===== Static resource path (compile-time fallback) =====
get_filename_component(PROJECT_ROOT "${CMAKE_SOURCE_DIR}" ABSOLUTE)
set(WEB_ROOT_PATH "${PROJECT_ROOT}/src/web")
target_compile_definitions(tree PRIVATE WEB_ROOT_PATH="${WEB_ROOT_PATH}")

# ===== Unix threading =====
if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(tree Threads::Threads)
endif()

# ===== Windows libraries =====
if(WIN32)
    target_link_libraries(tree ws2_32 shlwapi ole32 shell32)
endif()

# ===== Installation =====
install(TARGETS tree DESTINATION bin)
install(DIRECTORY src/web/ DESTINATION share/treevisual/web)
