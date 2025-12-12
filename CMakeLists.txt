cmake_minimum_required(VERSION 3.10)
project(NanoBroker)

if(POLICY CMP0148)
  cmake_policy(SET CMP0148 OLD)
endif()

set(CMAKE_CXX_STANDARD 17)

# Dependencies
find_package(OpenCV REQUIRED)
find_package(pybind11 REQUIRED)

# Include Directories (This allows #include <nanobroker/...>)
include_directories(${CMAKE_SOURCE_DIR}/include)

# --------------------------------------------------------
# 1. Build the Python Extension
# --------------------------------------------------------
pybind11_add_module(nanobroker src/python_bindings.cpp)
target_link_libraries(nanobroker PRIVATE rt pthread)

# --------------------------------------------------------
# 2. Build C++ Example (Producer)
# --------------------------------------------------------
add_executable(cpp_producer examples/cpp_producer/main.cpp)
target_link_libraries(cpp_producer rt pthread ${OpenCV_LIBS})

# --------------------------------------------------------
# 3. Build Admin Tool
# --------------------------------------------------------
add_executable(nanoadmin tools/nanoadmin.cpp)
target_link_libraries(nanoadmin rt pthread)