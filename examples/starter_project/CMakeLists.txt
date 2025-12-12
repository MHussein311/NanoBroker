cmake_minimum_required(VERSION 3.10)

# User changes this name to their app name
project(MyNanoApp) 

set(CMAKE_CXX_STANDARD 17)

# --- THE MAGIC GLUE START ---
# This block asks Python where the NanoBroker headers are installed.
# It works automatically inside any virtual environment.
execute_process(
    COMMAND python3 -c "import nanobroker_config; print(nanobroker_config.get_include())"
    OUTPUT_VARIABLE NANOBROKER_INCLUDE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE PYTHON_RESULT
)

if(NOT "${PYTHON_RESULT}" STREQUAL "0")
    message(FATAL_ERROR "Could not find NanoBroker! Did you run 'pip install ../NanoBroker'?")
endif()

message(STATUS "Found NanoBroker headers at: ${NANOBROKER_INCLUDE}")
include_directories(${NANOBROKER_INCLUDE})
# --- THE MAGIC GLUE END ---

# Standard Dependencies
find_package(OpenCV REQUIRED)

# Define your executable
add_executable(my_producer main.cpp)
target_link_libraries(my_producer rt pthread ${OpenCV_LIBS})
