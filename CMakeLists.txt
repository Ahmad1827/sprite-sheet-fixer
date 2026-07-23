cmake_minimum_required(VERSION 3.15)
project(SpriteSheetStudio)

set(CMAKE_CXX_STANDARD 17)

# SFML Dependency
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)
# Core Engine Logic
add_subdirectory(Core)

# UI Module
add_subdirectory(UI)