# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(explorer_irblaster)

if(ENABLE_FRAM_STORAGE)
    add_compile_definitions(_INCLUDE_FRAM_)
    message(STATUS "Build Config: Persistência configurada para FRAM (_INCLUDE_FRAM_)")
else()
    add_compile_definitions(_INCLUDE_NVS_)
    message(STATUS "Build Config: Persistência configurada para NVS / Driver Externo")
endif()
