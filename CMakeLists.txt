cmake_minimum_required(VERSION 3.10)
project(TreeVisual VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
endif()

add_executable(tree src/tree.cpp)

if(UNIX AND NOT APPLE)
    target_link_libraries(tree Threads::Threads)
endif()

if(WIN32)
    target_link_libraries(tree shlwapi ole32 shell32)
endif()

install(TARGETS tree DESTINATION bin)
