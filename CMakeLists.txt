cmake_minimum_required(VERSION 3.16)
project(PruebaCMake CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Busca OpenCV en C:/opencv (carpeta de extracción)
file(GLOB OPENCV_CFG_DIRS "C:/opencv/build/x64/vc*/lib")
if(NOT OPENCV_CFG_DIRS)
  message(FATAL_ERROR "No encontré OpenCV en C:/opencv/build/x64/. Revisa la ruta de extracción.")
endif()

list(SORT OPENCV_CFG_DIRS)
list(GET OPENCV_CFG_DIRS -1 OpenCV_DIR)

find_package(OpenCV REQUIRED)

add_executable(PruebaCMake WIN32 PruebaCMake.cpp)
target_include_directories(PruebaCMake PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(PruebaCMake PRIVATE ${OpenCV_LIBS} comdlg32)