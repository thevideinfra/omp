cmake_minimum_required(VERSION 3.21)
project(omamusic VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network Quick QuickControls2 Test)
qt_standard_project_setup(REQUIRES 6.5)

qt_add_library(omacore STATIC
    src/paths.cpp src/paths.h
    src/theme.cpp src/theme.h
    src/config.cpp src/config.h
    src/track.cpp src/track.h
    src/rowsmodel.cpp src/rowsmodel.h
    src/library.cpp src/library.h
)
target_include_directories(omacore PUBLIC src)
target_link_libraries(omacore PUBLIC Qt6::Core Qt6::Gui Qt6::Network)

qt_add_executable(omamusic src/main.cpp)
target_link_libraries(omamusic PRIVATE omacore Qt6::Widgets Qt6::Quick Qt6::QuickControls2)

enable_testing()
add_subdirectory(tests)
