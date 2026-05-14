# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(COMPONENTS main) # "Trim" the build. Include the minimal set of components; main and anything it depends on.

# Set custom partition table before including project.cmake
set(COMPONENT_PARTITION_TABLE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/partitions.csv")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(display)
