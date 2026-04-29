cmake_minimum_required(VERSION 3.10)
project(TreeVisual VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(tree src/tree.cpp)

# 所有 Unix 类系统（Linux, macOS, 其他 BSD）都链接线程库
if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(tree Threads::Threads)
endif()

# Windows 特有链接库
if(WIN32)
    target_link_libraries(tree shlwapi ole32 shell32)
endif()

install(TARGETS tree DESTINATION bin)
