cmake_minimum_required(VERSION 3.22)
project(FilterApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "C:/Qt/aqt/6.8.0/6.5.3/msvc2019_64" CACHE PATH "Qt installation path")

add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets PrintSupport)

if(MSVC)
    add_compile_options("/utf-8")
endif()

if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
endif()

qt_add_executable(FilterApp
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/qcustomplot.h
    src/qcustomplot.cpp
    src/filters/filter_base.h
    src/filters/moving_average_filter.h
    src/filters/moving_average_filter.cpp
    src/filters/exponential_filter.h
    src/filters/exponential_filter.cpp
    src/fft.h
    src/fft.cpp
)

set_target_properties(FilterApp PROPERTIES WIN32_EXECUTABLE TRUE)

target_link_libraries(FilterApp PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::PrintSupport
    Qt6::EntryPointPrivate
)

if(UNIX AND NOT APPLE)
    target_link_libraries(FilterApp PRIVATE Threads::Threads)
endif()

target_include_directories(FilterApp PRIVATE src)